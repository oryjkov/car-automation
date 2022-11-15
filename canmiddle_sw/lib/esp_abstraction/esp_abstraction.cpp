#include "esp_abstraction.h"

#include "message.pb.h"

#if defined(ARDUINO)

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

void LockAbstraction::Delay(int ms) { vTaskDelay(pdTICKS_TO_MS(ms)); };
void LockAbstraction::Lock() { xSemaphoreTake(sem, portMAX_DELAY); }
void LockAbstraction::Unlock() { xSemaphoreGive(sem); }
int64_t LockAbstraction::Micros() { return esp_timer_get_time(); }
LockAbstraction::LockAbstraction() {
  sem = xSemaphoreCreateMutex();
  if (sem == nullptr) {
    abort();
  }
}
LockAbstraction::~LockAbstraction() { vSemaphoreDelete(sem); }

void QueueAbstraction::Enqueue(const CanMessage &msg) {
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

QueueAbstraction::QueueAbstraction(QueueHandle_t q) : q(q) {}
QueueAbstraction::~QueueAbstraction() { vSemaphoreDelete(q); }

#endif
