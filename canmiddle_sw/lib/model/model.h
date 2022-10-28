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
  uint32_t prop;
  // Delay used when sending this state. Measured from the previous propery.
  uint32_t send_delay_ms;
  Value val;
};

template <typename A>
struct Model {
  // Main properties in this model.
  vector<Prop> props;
  A esp;

  // Serializes the complete internal state into messages sent out on the queue.
  bool SendState();

  // Updates the internal state from the incoming message.
  bool UpdateState(const CanMessage &msg);
};

template <typename A>
bool Model<A>::UpdateState(const CanMessage &msg) {
  if (!msg.has_prop || !msg.has_value) {
    return false;
  }
  LockGuard<A> l(esp);
  for (auto &prop : props) {
    if (prop.prop == msg.prop) {
      if (prop.val.size != msg.value.size) {
        return false;
      }
      memcpy(prop.val.bytes, msg.value.bytes, msg.value.size);
      return true;
    }
  }
  return false;
}

template <typename A>
bool Model<A>::SendState() {
  LockGuard<A> l(esp);
  for (const auto &prop : props) {
    esp.Delay(prop.send_delay_ms);
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
  return true;
};

#endif  // __MODEL_H__