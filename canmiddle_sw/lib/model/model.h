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

// Gets a bit from a byte array.
inline bool getBit(uint8_t bit_num, const uint8_t *d) {
  uint8_t byte_num = bit_num / 8;
  bit_num = bit_num % 8;
  // bit 0 is the most significant bit.
  bit_num = 7 - bit_num;

  return (d[byte_num] & (1 << bit_num)) != 0;
}
inline uint8_t getByte(uint8_t byte_num, const uint8_t *d) { return d[byte_num]; }
inline uint8_t getBrightnessByte(uint8_t byte_num, const uint8_t *d) { return d[byte_num] & 0x7f; }
inline uint32_t getBits(uint8_t bit_num, uint8_t bit_count, const uint8_t *d) {
  uint32_t result = 0;
  for (int i = 0; i < bit_count; i++) {
    result = (result << 1) | getBit(bit_num + i, d);
  }
  return result;
}

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
// appear to be used as keep-alives/identifications and are not updated during operation).
template <typename LockAbstraction, typename QueueAbstraction>
struct Model {
  // Main properties in this model.
  vector<Prop> props;

  // Abstraction of the ESP32.
  LockAbstraction lock_abs;
  QueueAbstraction queue_abs;

  int64_t can_enable_at_us;

  // Serializes the complete internal state into messages sent out on the queue.
  // Iteration starts at 1.
  bool SendState(uint32_t iteration);

  // Updates the internal state from the incoming message.
  bool UpdateState(const CanMessage &msg);

  // Manual updates. Those are not stopped.
  bool Update(uint32_t prop, const Value &v);
  // Like Update(), but only sets bits that are set in mask. mask[] and data[] must
  // be of length len.
  bool UpdateMasked(uint32_t prop, size_t len, uint8_t mask[], uint8_t data[]);

  // Temporarily disables processing of updates (those from the UpdateState()) function.
  void DisableUpdatesFor(uint32_t ms);
  bool updatesEnabled();

  void DelayMs(int ms) { lock_abs.Delay(ms);}
};

template <typename LockAbstraction, typename QueueAbstraction>
void Model<LockAbstraction, QueueAbstraction>::DisableUpdatesFor(uint32_t ms) {
  LockGuard<LockAbstraction> l(lock_abs);
  can_enable_at_us = lock_abs.Micros() + ms * 1000;
}

template <typename LockAbstraction, typename QueueAbstraction>
bool Model<LockAbstraction, QueueAbstraction>::updatesEnabled() {
  return (lock_abs.Micros() > can_enable_at_us);
}

void HandlePropUpdate(uint32_t prop, size_t len, const uint8_t *new_v, const uint8_t *old_v);

template <typename LockAbstraction, typename QueueAbstraction>
bool Model<LockAbstraction, QueueAbstraction>::UpdateMasked(uint32_t p, size_t len, uint8_t mask[], uint8_t data[]) {
  for (auto &prop : props) {
    if (prop.prop == p) {
      // Look the other way, as long as mask and data are long enough.
      if (prop.val.size > len) {
        abort();
      }

      LockGuard<LockAbstraction> l(lock_abs);
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

template <typename LockAbstraction, typename QueueAbstraction>
bool Model<LockAbstraction, QueueAbstraction>::Update(uint32_t p, const Value &new_value) {
  for (auto &prop : props) {
    if (prop.prop == p) {
      if (prop.val.size != new_value.size) {
        return false;
      }
      HandlePropUpdate(p, prop.val.size, new_value.bytes, prop.val.bytes);

      LockGuard<LockAbstraction> l(lock_abs);
      memcpy(prop.val.bytes, new_value.bytes, new_value.size);
      return true;
    }
  }
  return true;
}

template <typename LockAbstraction, typename QueueAbstraction>
bool Model<LockAbstraction, QueueAbstraction>::UpdateState(const CanMessage &msg) {
  if (!msg.has_prop || !msg.has_value) {
    return false;
  }
  for (auto &prop : props) {
    if (prop.prop == msg.prop) {
      if (prop.val.size != msg.value.size) {
        return false;
      }
      LockGuard<LockAbstraction> l(lock_abs);
      if (updatesEnabled()) {
        HandlePropUpdate(msg.prop, msg.value.size, msg.value.bytes, prop.val.bytes);
        memcpy(prop.val.bytes, msg.value.bytes, msg.value.size);
      }
      return true;
    }
  }
  return false;
}

template <typename LockAbstraction, typename QueueAbstraction>
bool Model<LockAbstraction, QueueAbstraction>::SendState(uint32_t iteration) {
  for (const auto &prop : props) {
    if (prop.iteration > 0 && prop.iteration != iteration) {
      // skip this one-off property, not its time.
      continue;
    }
    lock_abs.Delay(prop.send_delay_ms);

    if (prop.prop != DELAY_ONLY_PROP) {
      LockGuard<LockAbstraction> l(lock_abs);
      CanMessage m;
      m.has_prop = true;
      m.prop = prop.prop;
      m.has_extended = prop.prop > 1 << 11;
      m.extended = prop.prop > 1 << 11;
      m.has_value = true;
      m.value.size = prop.val.size;
      memcpy(m.value.bytes, prop.val.bytes, prop.val.size);

      queue_abs.Enqueue(m);
    }

    if (prop.iteration > 0 && prop.iteration == iteration) {
      // in the one-off realm and this one was just sent.
      break;
    }
  }
  return true;
};

#endif  // __MODEL_H__