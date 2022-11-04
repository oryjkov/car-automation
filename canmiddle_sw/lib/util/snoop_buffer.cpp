#include "snoop_buffer.h"

#include <pb_encode.h>

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define TAG "snoop_buffer"

SnoopBuffer snoop_buffer;

SnoopBuffer::SnoopBuffer(size_t buf_size) {
  buffer = reinterpret_cast<uint8_t *>(malloc(snoop_buffer_max_size));
  if (buffer == nullptr) {
    abort();
  }
  mu = xSemaphoreCreateMutex();
  if (mu == nullptr) {
    abort();
  }
}
int32_t SnoopBuffer::TimeRemainingMs() {
  int32_t rv;
  xSemaphoreTake(mu, portMAX_DELAY);
  rv = millis() - end_ms;
  xSemaphoreGive(mu);
  return rv;
}

bool SnoopBuffer::IsActive() {
  bool rv;
  if (mu == nullptr) {
    return false;
  }
  xSemaphoreTake(mu, portMAX_DELAY);
  rv = (end_ms > millis());
  xSemaphoreGive(mu);
  return rv;
}

void SnoopBuffer::Activate(int32_t for_ms) {
  xSemaphoreTake(mu, portMAX_DELAY);
  end_ms = millis() + for_ms;
  xSemaphoreGive(mu);
}

void SnoopBuffer::Unlock() { xSemaphoreGive(mu); }
void SnoopBuffer::Lock() { xSemaphoreTake(mu, portMAX_DELAY); }
bool SnoopBuffer::Snoop(const SnoopData &s) {
  xSemaphoreTake(mu, portMAX_DELAY);
  size_t start_from = snoop_buffer.position;
  uint8_t *length_field = snoop_buffer.buffer + snoop_buffer.position;
  start_from += 1;
  pb_ostream_t stream =
      pb_ostream_from_buffer(snoop_buffer.buffer + start_from, snoop_buffer_max_size - start_from);
  bool status = pb_encode(&stream, &SnoopData_msg, &s);
  if (!status) {
    ESP_LOGW(TAG, "Encoding failed: %s\r\n", PB_GET_ERROR(&stream));
    xSemaphoreGive(mu);
    return false;
  }

  *length_field = stream.bytes_written;
  snoop_buffer.position += stream.bytes_written + 1;
  xSemaphoreGive(mu);
  return true;
}
