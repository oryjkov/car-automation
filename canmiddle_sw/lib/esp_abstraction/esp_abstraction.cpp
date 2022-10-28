#include "esp_abstraction.h"

#if defined(ARDUINO)

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

void EspAbstraction::Delay(int ms) { vTaskDelay(pdTICKS_TO_MS(ms)); };
void EspAbstraction::Lock() { xSemaphoreTake(sem, portMAX_DELAY); }
void EspAbstraction::Unlock() { xSemaphoreGive(sem); }
void EspAbstraction::Enqueue(const CanMessage &msg) {
  QueueElement *e = reinterpret_cast<QueueElement *>(malloc(sizeof(QueueElement)));
  if (e == nullptr) {
    ESP_LOGE(EXAMPLE_TAG, "malloc failed, free heap: %d\r\n", esp_get_free_heap_size());
    abort();
  }
  *e = {
      .type = CAN_MESSAGE,
      .msg = msg,
      .event_us = esp_timer_get_time(),
  };
  if (xQueueSendToBack(q, &e, 0) != pdTRUE) {
    free(e);
    ESP_LOGW(EXAMPLE_TAG, "failed to add can message to the event queue");
  }
}

EspAbstraction::EspAbstraction(QueueHandle_t q) : q(q) {
  sem = xSemaphoreCreateMutex();
  if (sem == nullptr) {
    abort();
  }
}
EspAbstraction::~EspAbstraction() { vSemaphoreDelete(sem); }

#endif
