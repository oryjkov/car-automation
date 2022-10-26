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
#include "message.pb.h"
#include "util.h"
/* --------------------- Definitions and static variables ------------------ */

// Example Configurations
constexpr gpio_num_t TX_GPIO_NUM = GPIO_NUM_25;
constexpr gpio_num_t RX_GPIO_NUM = GPIO_NUM_26;
#define RX_TASK_PRIO 9     // Receiving task priority
#define CTRL_TASK_PRIO 10  // Control task priority
#define EXAMPLE_TAG "TWAI Self Test"

static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

static SemaphoreHandle_t tx_sem;
static SemaphoreHandle_t rx_sem;
static SemaphoreHandle_t ctrl_sem;
static SemaphoreHandle_t done_sem;

static QueueHandle_t event_queue;
enum EventType {
  CAN_MESSAGE = 1,
  UART_MESSAGE = 2,
};
typedef struct QUEUE_ELEMENT {
  EventType event;
  CanMessage msg;
  int64_t event_us;
} QueueElement;

static void print_stats(void *arg) {
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    print_stats_ser(3000);
  }
}

static void twai_receive_task(void *arg) {
  while (true) {
    twai_message_t rx_message;
    ESP_LOGI(EXAMPLE_TAG, "recv on can");
    ESP_ERROR_CHECK(twai_receive(&rx_message, portMAX_DELAY));
    ESP_LOGI(EXAMPLE_TAG, "Msg received - Id: %d Data = %d", rx_message.identifier,
             rx_message.data[0]);
    QueueElement *e = reinterpret_cast<QueueElement *>(malloc(sizeof(QueueElement)));
    if (e == nullptr) {
      ESP_LOGE(EXAMPLE_TAG, "malloc failed, free heap: %d\r\n", esp_get_free_heap_size());
      abort();
    }
    *e = {
        .event = CAN_MESSAGE,
        .msg =
            {
                .has_prop = true,
                .prop = rx_message.identifier,
                .has_extended = rx_message.extd,
                .extended = rx_message.extd,
                .has_value = true,
                .value = {.size = rx_message.data_length_code},
            },
        .event_us = esp_timer_get_time(),
    };
    memcpy(e->msg.value.bytes, rx_message.data, rx_message.data_length_code);
    if (xQueueSendToBack(event_queue, &e, 0) != pdTRUE) {
      free(e);
      ESP_LOGW(EXAMPLE_TAG, "failed to add can message to the event queue");
      get_stats()->can_rx_err += 1;
    }
    get_stats()->can_pkt_rx += 1;
  }
  vTaskDelete(NULL);
}

static void uart_receive_task(void *arg) {
  while (true) {
    QueueElement *e = reinterpret_cast<QueueElement *>(malloc(sizeof(QueueElement)));
    if (e == nullptr) {
      ESP_LOGE(EXAMPLE_TAG, "malloc failed, free heap: %d\r\n", esp_get_free_heap_size());
      abort();
    }
    e->event = UART_MESSAGE;
    if (!recv_over_uart(&e->msg)) {
      ESP_LOGE(EXAMPLE_TAG, "uart recv failed");
      free(e);
      continue;
    }
    e->event_us = esp_timer_get_time();
    ESP_LOGI(EXAMPLE_TAG, "Received msg on uart, prop: %d", e->msg.prop);
    if (xQueueSendToBack(event_queue, &e, 0) != pdTRUE) {
      free(e);
      ESP_LOGW(EXAMPLE_TAG, "failed to add uart message to the event queue");
      get_stats()->ser_drops += 1;
    }
  }
  vTaskDelete(NULL);
}

static void event_loop(void *arg) {
  ESP_LOGI(EXAMPLE_TAG, "starting event loop");
  while (1) {
    QueueElement *e = nullptr;
    if (xQueueReceive(event_queue, &e, portMAX_DELAY) != pdTRUE) {
      ESP_LOGE(EXAMPLE_TAG, "queue recv failed");
      continue;
    }
    ESP_LOGI(EXAMPLE_TAG, "received an event of type %d", e->event);
    if (e->event == CAN_MESSAGE) {
      if (!send_over_uart(e->msg)) {
        ESP_LOGE(EXAMPLE_TAG, "uart send failed");
      }
      if (get_snoop_buffer()->end_ms > millis()) {
        add_to_snoop_buffer(
            {.message = e->msg,
             .metadata = {.recv_us = static_cast<uint64_t>(e->event_us), .source = Metadata_Source_MASTER}});
      }
    } else if (e->event == UART_MESSAGE) {
      twai_message_t tx_msg = {
          .extd = e->msg.has_extended,
          .identifier = e->msg.prop,
          .data_length_code = static_cast<uint8_t>(e->msg.value.size),
      };
      memcpy(tx_msg.data, e->msg.value.bytes, e->msg.value.size);
      esp_err_t err = twai_transmit(&tx_msg, 0);
      if (err != ESP_OK) {
        get_stats()->can_tx_err += 1;
      }
      get_stats()->can_pkt_tx += 1;

      if (get_snoop_buffer()->end_ms > millis()) {
        add_to_snoop_buffer(
            {.message = e->msg,
             .metadata = {.recv_us = static_cast<uint64_t>(e->event_us), .source = Metadata_Source_SLAVE}});
      }
    } else {
      ESP_LOGE(EXAMPLE_TAG, "wrong event queue element type");
      abort();
    }
    free(e);
  }
}

void master_loop() {
  done_sem = xSemaphoreCreateBinary();

  const twai_filter_config_t accept_all_filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  const twai_general_config_t normal_config =
      TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NORMAL);
  ESP_ERROR_CHECK(twai_driver_install(&normal_config, &t_config, &accept_all_filter));
  ESP_LOGI(EXAMPLE_TAG, "Driver installed");

  esp_log_level_set(EXAMPLE_TAG, ESP_LOG_INFO);
  event_queue = xQueueCreate(10, sizeof(void *));
  if (event_queue == 0) {
    ESP_LOGE(EXAMPLE_TAG, "cant create an event queue");
    abort();
  }
  ESP_LOGE(EXAMPLE_TAG, "preparing event loop");
  ESP_ERROR_CHECK(twai_start());
  xTaskCreatePinnedToCore(twai_receive_task, "TWAI_rx", 4096, NULL, RX_TASK_PRIO, NULL,
                          tskNO_AFFINITY);
  xTaskCreatePinnedToCore(uart_receive_task, "UART_rx", 4096, NULL, RX_TASK_PRIO, NULL,
                          tskNO_AFFINITY);
  xTaskCreatePinnedToCore(event_loop, "loop", 4096, NULL, CTRL_TASK_PRIO, NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(print_stats, "stats", 4096, NULL, RX_TASK_PRIO, NULL, tskNO_AFFINITY);
  xSemaphoreTake(done_sem, portMAX_DELAY);
  // event_loop();
  ESP_ERROR_CHECK(twai_stop());
  ESP_ERROR_CHECK(twai_driver_uninstall());
}