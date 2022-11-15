#include "io_abstraction.h"

#if defined(ARDUINO)

#include "driver/twai.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "snoop_buffer.h"
#include "util.h"

constexpr gpio_num_t TX_GPIO_NUM = GPIO_NUM_25;
constexpr gpio_num_t RX_GPIO_NUM = GPIO_NUM_26;

#define UPDATE_TASK_PRIO 7
#define TX_TASK_PRIO 8     // Receiving task priority
#define RX_TASK_PRIO 9     // Receiving task priority
#define CTRL_TASK_PRIO 10  // Control task priority

static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

static void twai_tx_task(void *arg) {
  IOAbstraction *io = reinterpret_cast<IOAbstraction *>(arg);
  while (true) {
    QueueElement *e = nullptr;
    if (xQueueReceive(io->twai_tx_queue, &e, portMAX_DELAY) != pdTRUE) {
      ESP_LOGE(EXAMPLE_TAG, "queue recv failed on twai tx queue");
      continue;
    }

    io->snoop_buffer->Snoop({.message = e->msg,
                             .metadata = {.recv_us = static_cast<uint64_t>(e->event_us),
                                          .source = Metadata_Source_MASTER_TX}});

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
    free(e);
  }
  vTaskDelete(NULL);
}

static void uart_tx_task(void *arg) {
  IOAbstraction *io = reinterpret_cast<IOAbstraction *>(arg);
  while (true) {
    QueueElement *e = nullptr;
    if (xQueueReceive(io->uart_tx_queue, &e, portMAX_DELAY) != pdTRUE) {
      ESP_LOGE(EXAMPLE_TAG, "queue recv failed on uart tx queue");
      continue;
    }

    io->snoop_buffer->Snoop({.message = e->msg,
                             .metadata = {.recv_us = static_cast<uint64_t>(e->event_us),
                                          .source = Metadata_Source_SLAVE_TX}});

    if (!send_over_uart(e->msg)) {
      ESP_LOGE(EXAMPLE_TAG, "uart send failed");
    }
    free(e);
  }
  vTaskDelete(NULL);
}

static void twai_receive_task(void *arg) {
  IOAbstraction *io = reinterpret_cast<IOAbstraction *>(arg);
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
        .type = CAN_MESSAGE,
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
    io->snoop_buffer->Snoop({.message = e->msg,
                             .metadata = {.recv_us = static_cast<uint64_t>(e->event_us),
                                          .source = Metadata_Source_MASTER}});

    if (xQueueSendToBack(io->rx_queue, &e, 0) != pdTRUE) {
      free(e);
      ESP_LOGW(EXAMPLE_TAG, "failed to add can message to the event queue");
      get_stats()->can_rx_err += 1;
    }
    get_stats()->can_pkt_rx += 1;
  }
  vTaskDelete(NULL);
}

static void uart_receive_task(void *arg) {
  IOAbstraction *io = reinterpret_cast<IOAbstraction *>(arg);
  while (true) {
    QueueElement *e = reinterpret_cast<QueueElement *>(malloc(sizeof(QueueElement)));
    if (e == nullptr) {
      ESP_LOGE(EXAMPLE_TAG, "malloc failed, free heap: %d\r\n", esp_get_free_heap_size());
      abort();
    }
    e->type = UART_MESSAGE;
    if (!recv_over_uart(&e->msg)) {
      ESP_LOGE(EXAMPLE_TAG, "uart recv failed");
      free(e);
      continue;
    }
    e->event_us = esp_timer_get_time();
    ESP_LOGI(EXAMPLE_TAG, "Received msg on uart, prop: %d", e->msg.prop);

    io->snoop_buffer->Snoop({.message = e->msg,
                             .metadata = {.recv_us = static_cast<uint64_t>(e->event_us),
                                          .source = Metadata_Source_SLAVE}});

    if (xQueueSendToBack(io->rx_queue, &e, 0) != pdTRUE) {
      free(e);
      ESP_LOGW(EXAMPLE_TAG, "failed to add uart message to the event queue");
      get_stats()->ser_drops += 1;
    }
  }
  vTaskDelete(NULL);
}

static void event_loop(void *arg) {
  IOAbstraction *io = reinterpret_cast<IOAbstraction *>(arg);

  ESP_LOGI(EXAMPLE_TAG, "starting event loop");
  while (1) {
    QueueElement *e = nullptr;
    if (xQueueReceive(io->rx_queue, &e, portMAX_DELAY) != pdTRUE) {
      ESP_LOGE(EXAMPLE_TAG, "queue recv failed");
      continue;
    }
    ESP_LOGI(EXAMPLE_TAG, "received an event of type %d", e->type);
    io->recv_cb(e);
  }
}

IOAbstraction::IOAbstraction(RecvCallback *recv_cb, SnoopBuffer<LockAbstraction> *snoop_buffer)
    : recv_cb(recv_cb), snoop_buffer(snoop_buffer) {
  const twai_filter_config_t accept_all_filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  const twai_general_config_t normal_config =
      TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NORMAL);
  ESP_ERROR_CHECK(twai_driver_install(&normal_config, &t_config, &accept_all_filter));
  ESP_LOGI(EXAMPLE_TAG, "Driver installed");

  esp_log_level_set(EXAMPLE_TAG, ESP_LOG_INFO);

  //
  rx_queue = xQueueCreate(10, sizeof(void *));
  if (rx_queue == 0) {
    ESP_LOGE(EXAMPLE_TAG, "cant create an rx queue");
    abort();
  }
  twai_tx_queue = xQueueCreate(10, sizeof(void *));
  if (twai_tx_queue == 0) {
    ESP_LOGE(EXAMPLE_TAG, "cant create twai tx queue");
    abort();
  }
  uart_tx_queue = xQueueCreate(10, sizeof(void *));
  if (uart_tx_queue == 0) {
    ESP_LOGE(EXAMPLE_TAG, "cant create a uart tx queue");
    abort();
  }

  ESP_ERROR_CHECK(twai_start());
  xTaskCreatePinnedToCore(event_loop, "loop", 4096, NULL, CTRL_TASK_PRIO, NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(twai_tx_task, "TWAI_tx", 4096, this, TX_TASK_PRIO, NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(uart_tx_task, "UART_tx", 4096, this, TX_TASK_PRIO, NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(twai_receive_task, "TWAI_rx", 4096, this, RX_TASK_PRIO, NULL,
                          tskNO_AFFINITY);
  xTaskCreatePinnedToCore(uart_receive_task, "UART_rx", 4096, this, RX_TASK_PRIO, NULL,
                          tskNO_AFFINITY);
}

// Puts message on the UART tx queue.
void IOAbstraction::SendOverUART(QueueElement *e) {
  // Queue up for transmission on the UART bus.
  if (xQueueSendToBack(uart_tx_queue, &e, 0) != pdTRUE) {
    free(e);
    ESP_LOGW(EXAMPLE_TAG, "failed to queue uart tx");
  }
}

// Puts message on the TWAI tx queue.
void IOAbstraction::SendOverTWAI(QueueElement *e) {
  // Queue up for transmission on the TWAI bus.
  if (xQueueSendToBack(twai_tx_queue, &e, 0) != pdTRUE) {
    free(e);
    ESP_LOGW(EXAMPLE_TAG, "failed to queue twai tx");
  }
}
#else

#endif  // defined(ARDUINO)