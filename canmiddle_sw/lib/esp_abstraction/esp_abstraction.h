#ifndef __ESP_ABSTRACTION__
#define __ESP_ABSTRACTION__

#include "message.pb.h"

#define EXAMPLE_TAG "TWAI Self Test"

enum EventType {
  CAN_MESSAGE = 1,
  UART_MESSAGE = 2,
};
typedef struct QUEUE_ELEMENT {
  EventType type;
  CanMessage msg;
  int64_t event_us;
} QueueElement;


template <typename Mutex>
struct LockGuard {
  LockGuard(Mutex& mutex) : _ref(mutex) {
    _ref.Lock();
  };
  ~LockGuard() {
    _ref.Unlock();
  }

 private:
  LockGuard(const LockGuard&);  // or use c++0x ` = delete`
  Mutex& _ref;
};

#if defined(ARDUINO)

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

struct EspAbstraction {
  void Delay(int ms);
  void Lock();
  void Unlock();
  void Enqueue(const CanMessage& msg);
  int64_t Micros();

  EspAbstraction(QueueHandle_t q);
  ~EspAbstraction();

 private:
  SemaphoreHandle_t sem;
  QueueHandle_t q;
};

#else

#include <gmock/gmock.h>

struct MockEspAbstraction {
  MOCK_METHOD(void, Delay, (int ms), ());
  MOCK_METHOD(void, Lock, (), ());
  MOCK_METHOD(void, Unlock, (), ());
  MOCK_METHOD(void, Enqueue, (const CanMessage& msg), ());
  MOCK_METHOD(int64_t, Micros, (), ());
};
#endif

#endif  // __ESP_ABSTRACTION__