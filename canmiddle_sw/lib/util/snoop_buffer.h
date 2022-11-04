#ifndef __SNOOP_BUFFER__
#define __SNOOP_BUFFER__

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "message.pb.h"

constexpr size_t snoop_buffer_max_size = 50 * (1 << 10);

struct SnoopBuffer {
  SnoopBuffer(size_t buf_size = snoop_buffer_max_size);
  void Lock();
  void Unlock();
  bool IsActive();
  bool Snoop(const SnoopData &s);
  void Activate(int32_t for_ms);
  int32_t TimeRemainingMs();

  uint8_t *buffer;
  size_t position = 0;

 private:
  uint32_t end_ms = 0;

  xSemaphoreHandle mu = nullptr;
};

extern SnoopBuffer snoop_buffer;
#endif  // __SNOOP_BUFFER__