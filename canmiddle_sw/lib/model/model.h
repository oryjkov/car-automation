#ifndef __MODEL_H__
#define __MODEL_H__

#include <stdlib.h>

#include <set>
#include <vector>

#include "esp_abstraction.h"

using std::vector;

struct Value {
  size_t size;
  uint8_t bytes[8];
};

// This function will be called for every change of a model's property.
// It needs to be defined externally. There are two definitions. One in model_defs another
// in tests.
extern void HandlePropUpdate(uint32_t prop, size_t len, const uint8_t *new_v, const uint8_t *old_v);

// Property value used to indicate that nothing is to be sent, only trigger a delay.
constexpr uint32_t DELAY_ONLY_PROP = 0;

// A property is just a pair of (identifier, value). It is used to model the
// properties exchanged on the CAN bus between the car and the display.
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

// Models the state of an element - either the car or display (each has two models
// which is done to keep the extended CAN properties separate. Extended properties
// appear to be used as keep-alives/identifications and are not updated).
template <typename A>
struct Model {
  // Main properties in this model.
  vector<Prop> props;

  // Abstraction of the ESP32.
  A esp;

  int64_t can_enable_at_us;

  // Serializes the complete internal state into messages sent out on the queue.
  // Iteration starts at 1.
  // bool SendState();
  bool SendState(uint32_t iteration);

  // Updates the internal state from the incoming message.
  bool UpdateState(const CanMessage &msg);

  // Manual updates. Those are not stopped.
  bool Update(uint32_t prop, const Value &v);
  // Like Update(), but only sets bits that are set in mask. mask[] and data[] must
  // be of length len.
  bool UpdateMasked(uint32_t prop, size_t len, uint8_t mask[], uint8_t data[]);

  // Temporarily disables processing of CAN updates (those from the UpdateState()) function.
  void DisableCanFor(uint32_t ms);
  bool canEnabled();
};

template <typename A>
void Model<A>::DisableCanFor(uint32_t ms) {
  LockGuard<A> l(esp);
  can_enable_at_us = esp.Micros() + ms*1000;
}

template <typename A>
bool Model<A>::canEnabled() {
  return (esp.Micros() > can_enable_at_us);
}

void HandlePropUpdate(uint32_t prop, size_t len, const uint8_t *new_v, const uint8_t *old_v);

template <typename A>
bool Model<A>::UpdateMasked(uint32_t p, size_t len, uint8_t mask[], uint8_t data[]) {
  for (auto &prop : props) {
    if (prop.prop == p) {
      // Look the other way, as long as mask and data are long enough.
      if (prop.val.size > len) {
        abort();
      }

      LockGuard<A> l(esp);
      for (int i = 0; i < prop.val.size; i++) {
        prop.val.bytes[i] = prop.val.bytes[i] & (~mask[i]) | data[i] & mask[i];
      }
      // Force printing by passing in mask
      HandlePropUpdate(p, prop.val.size, prop.val.bytes, mask);
      return true;
    }
  }
  return true;
}

template <typename A>
bool Model<A>::Update(uint32_t p, const Value &new_value) {
  for (auto &prop : props) {
    if (prop.prop == p) {
      if (prop.val.size != new_value.size) {
        return false;
      }
      HandlePropUpdate(p, prop.val.size, new_value.bytes, prop.val.bytes);

      LockGuard<A> l(esp);
      memcpy(prop.val.bytes, new_value.bytes, new_value.size);
      return true;
    }
  }
  return true;
}

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
      if (canEnabled()) {
        HandlePropUpdate(msg.prop, msg.value.size, msg.value.bytes, prop.val.bytes);
        memcpy(prop.val.bytes, msg.value.bytes, msg.value.size);
      }
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

    if (prop.prop != DELAY_ONLY_PROP) {
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