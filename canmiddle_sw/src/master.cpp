#define LOG_LOCAL_LEVEL ESP_LOG_INFO

#include <stdio.h>
#include <stdlib.h>

#include "driver/twai.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "go.h"
#include "io_abstraction.h"
#include "message.pb.h"
#include "model.h"
#include "model_defs.h"
#include "util.h"

#define UPDATE_TASK_PRIO 7

bool passthrough = false;
IOAbstraction *io;

enum Command {
  START,
  STOP,
};
// Queue used to start/stop the task that periodically send model state on the can bus.
QueueHandle_t control_q;

// Calls send state on the model passed in the argument.
// Can be started/stopped via a message on control_q.
static void send_state(void *arg) {
  Model<EspAbstraction> *model = reinterpret_cast<Model<EspAbstraction> *>(arg);
  Command cmd;
  xQueuePeek(control_q, &cmd, portMAX_DELAY);
  uint32_t iteration = 1;
  while (1) {
    if (cmd == STOP) {
      xQueuePeek(control_q, &cmd, portMAX_DELAY);
      iteration = 1;
      delay(10);
      continue;
    }
    // This function is slow, so state stopping will only happen after sending is done.
    model->SendState(iteration);
    iteration = (iteration > 100) ? 100 : iteration + 1;
    // Try to read a command. If nothing read, just continue with what we had.
    // If something is read and it is a STOP, then the next loop will block.
    xQueuePeek(control_q, &cmd, 0);
  }
}

void toggle_send_state(Command c) {
  Command unused_cmd;
  xQueueReceive(control_q, &unused_cmd, 0);
  if (xQueueSendToBack(control_q, &c, 0) != pdTRUE) {
    abort();
  }
}

void start_send_state() { toggle_send_state(START); }

void stop_send_state() { toggle_send_state(STOP); }

void QueueRecvCallback(QueueElement *e) {
  if (e->type == CAN_MESSAGE) {
    if (passthrough) {
      io->SendOverUART(e);
    } else {
      // Update the Car model with the incoming value.
      car_model->UpdateState(e->msg);
      free(e);
    }
  } else if (e->type == UART_MESSAGE) {
    if (passthrough) {
      io->SendOverTWAI(e);
    } else {
      // Update the Display model with the incoming value.
      display_model->UpdateState(e->msg);
      // Magic message that starts off comms.
      if (e->msg.prop == 0x1b000046) {
        start_send_state();
      }
      free(e);
    }
  } else {
    free(e);
    ESP_LOGE(EXAMPLE_TAG, "wrong event queue element type");
    abort();
  }
}

void master_loop() {
  SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();

  IOAbstraction ioa(QueueRecvCallback);
  io = &ioa;
  InitModels(io);

  control_q = xQueueCreate(1, sizeof(Command));
  if (control_q == 0) {
    ESP_LOGE(EXAMPLE_TAG, "cant create an queue");
    abort();
  }

  xTaskCreatePinnedToCore(send_state, "upd_car", 4096, car_model.get(), UPDATE_TASK_PRIO, NULL,
                          tskNO_AFFINITY);
  xTaskCreatePinnedToCore(send_state, "upd_ext_car", 4096, car_ext_model.get(), UPDATE_TASK_PRIO,
                          NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(send_state, "upd_disp", 4096, display_model.get(), UPDATE_TASK_PRIO, NULL,
                          tskNO_AFFINITY);
  xTaskCreatePinnedToCore(send_state, "upd_ext_disp", 4096, display_ext_model.get(),
                          UPDATE_TASK_PRIO, NULL, tskNO_AFFINITY);

  xSemaphoreTake(done_sem, portMAX_DELAY);
  // event_loop();
  ESP_ERROR_CHECK(twai_stop());
  ESP_ERROR_CHECK(twai_driver_uninstall());
}