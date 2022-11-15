#ifndef __SNOOP_BUFFER__
#define __SNOOP_BUFFER__

#include <pb_encode.h>

#include "esp_abstraction.h"
#include "message.pb.h"

template <typename LockAbstraction>
struct SnoopBuffer {
  uint8_t *buffer = nullptr;
  size_t position = 0;

  SnoopBuffer(size_t buf_size) : buf_size(buf_size) {
    buffer = reinterpret_cast<uint8_t *>(malloc(buf_size));
    if (buffer == nullptr) {
      abort();
    }
  }
  int32_t TimeRemainingMs() {
    LockGuard<LockAbstraction> l(lock_abs);
    return end_ms - lock_abs.Millis();
  }

  bool IsActive() {
    LockGuard<LockAbstraction> l(lock_abs);
    return isActive();
  }

  bool Activate(int32_t for_ms) {
    LockGuard<LockAbstraction> l(lock_abs);
    if (isActive()) {
      return false;
    }
    position = 0;
    end_ms = lock_abs.Millis() + for_ms;
    return true;
  }

  void Unlock() { lock_abs.Unlock(); }
  void Lock() { lock_abs.Lock(); }
  bool Snoop(const SnoopData &s) {
    LockGuard<LockAbstraction> l(lock_abs);
    if (!isActive()) {
      return false;
    }
    size_t start_from = position;
    uint8_t *length_field = buffer + position;
    start_from += 1;
    pb_ostream_t stream = pb_ostream_from_buffer(buffer + start_from, buf_size - start_from);
    bool status = pb_encode(&stream, &SnoopData_msg, &s);
    if (!status) {
      // ESP_LOGW(TAG, "Encoding failed: %s\r\n", PB_GET_ERROR(&stream));
      return false;
    }

    *length_field = stream.bytes_written;
    position += stream.bytes_written + 1;
    return true;
  }

  size_t Capacity() const { return buf_size; };

 private:
  const size_t buf_size = 0;
  uint32_t end_ms = 0;
  SnoopBuffer(const SnoopBuffer &);  // or use c++0x ` = delete`
  bool isActive() { return (end_ms > lock_abs.Millis()); };

 public:
  LockAbstraction lock_abs;
};

#endif  // __SNOOP_BUFFER__