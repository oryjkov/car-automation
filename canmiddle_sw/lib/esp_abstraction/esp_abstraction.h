#ifndef __ESP_ABSTRACTION__
#define __ESP_ABSTRACTION__

#include "message.pb.h"

#define EXAMPLE_TAG "TWAI Self Test"

enum EventType {
  CAN_MESSAGE = 1,
  UART_MESSAGE = 2,
};

// Common representation of a message that is sent or received (UART or TWAI bus).
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

struct LockAbstraction {
  void Delay(int ms);
  void Lock();
  void Unlock();
  int64_t Micros();

  LockAbstraction();
  ~LockAbstraction();

 private:
  SemaphoreHandle_t sem;
};

struct QueueAbstraction {
  void Enqueue(const CanMessage& msg);

  QueueAbstraction(QueueHandle_t q);
  ~QueueAbstraction();

 private:
  QueueHandle_t q;
};

#else

#include <gmock/gmock.h>

struct MockLockAbstraction {
  MOCK_METHOD(void, Delay, (int ms), ());
  MOCK_METHOD(void, Lock, (), ());
  MOCK_METHOD(void, Unlock, (), ());
  MOCK_METHOD(int64_t, Micros, (), ());
};
struct MockQueueAbstraction {
  MOCK_METHOD(void, Enqueue, (const CanMessage& msg), ());
};
#endif

#endif  // __ESP_ABSTRACTION__