#ifndef __MODEL_H__
#define __MODEL_H__

#include <stdlib.h>

#include <vector>

#include "esp_abstraction.h"

using std::vector;

struct Value {
  size_t size;
  uint8_t bytes[8];
};

struct Prop {
  // property id
  const uint32_t prop;
  // Delay used when sending this state. Measured from the previous propery.
  const uint32_t send_delay_ms;
  // Property value.
  Value val;
  // Only needed for the external props to represent:
  //  - more frequent prop updates initially (display model)
  //  - different initial values (car model)
  const uint32_t iteration;
};

template <typename A>
struct Model {
  // Main properties in this model.
  vector<Prop> props;

  // Abstraction of the ESP32.
  A esp;

  // Serializes the complete internal state into messages sent out on the queue.
  // Iteration starts at 1.
  // bool SendState();
  bool SendState(uint32_t iteration);

  // Updates the internal state from the incoming message.
  bool UpdateState(const CanMessage &msg);
};

template <typename A>
bool Model<A>::UpdateState(const CanMessage &msg) {
  if (!msg.has_prop || !msg.has_value) {
    return false;
  }
  for (auto &prop : props) {
    if (prop.prop == msg.prop) {
      if (prop.val.size != msg.value.size) {
        return false;
      }
      LockGuard<A> l(esp);
      memcpy(prop.val.bytes, msg.value.bytes, msg.value.size);
      return true;
    }
  }
  return false;
}

template <typename A>
bool Model<A>::SendState(uint32_t iteration) {
  for (const auto &prop : props) {
    if (prop.iteration > 0 && prop.iteration != iteration) {
      // skip this one-off property, not its time.
      continue;
    }
    esp.Delay(prop.send_delay_ms);

    if (prop.prop != 0x0) {
      LockGuard<A> l(esp);
      CanMessage m;
      m.has_prop = true;
      m.prop = prop.prop;
      m.has_extended = prop.prop > 1 << 11;
      m.extended = prop.prop > 1 << 11;
      m.has_value = true;
      m.value.size = prop.val.size;
      memcpy(m.value.bytes, prop.val.bytes, prop.val.size);

      esp.Enqueue(m);
    }

    if (prop.iteration > 0 && prop.iteration == iteration) {
      // in the one-off realm and this one was just send.
      break;
    }
  }
  return true;
};

#endif  // __MODEL_H__