#ifndef __PROPS_LOGGER_H__
#define __PROPS_LOGGER_H__

#include <stdlib.h>

#include <set>

#include "Arduino.h"
#include "freertos/semphr.h"
#include "snoop_buffer.h"

// Records property _changes_ on the model.
struct PropsLogger {
  PropsLogger() : buf(10*(1<<10)) {
    props_mu = xSemaphoreCreateMutex();
    if (props_mu == nullptr) {
      abort();
    }
    filtered_props = {
        0x53a,  // Ignore this timer property to reduce noise.
    };
    // selected_props = {0x525, 0x526, 0x527, 0x527, 0x528, };
  }

  void filter(uint32_t prop) {
    xSemaphoreTake(props_mu, portMAX_DELAY);
    filtered_props.insert(prop);
    xSemaphoreGive(props_mu);
  }
  void log(uint32_t prop, size_t len, const uint8_t *new_v) {
    xSemaphoreTake(props_mu, portMAX_DELAY);
    if (filtered_props.count(prop) > 0) {
      xSemaphoreGive(props_mu);
      return;
    }
    xSemaphoreGive(props_mu);

    auto d = SnoopData{
        .message = {
            .has_prop = true,
            .prop = prop,
            .has_value = true,
            .value = {
                .size = static_cast<uint8_t>(len),
            }
        },
        .metadata = {
            .recv_us=micros(),
             .source = Metadata_Source_UNKNOWN,
             },
    };
    memcpy(d.message.value.bytes, new_v, len);
    buf.Snoop(d);

/*
    Serial.printf("[%05d] 0x%04x: ", millis(), prop);
    for (int i = 0; i < len; i++) {
      Serial.printf("0x%02x ", new_v[i]);
    }
    Serial.println();
    */
  }

  SnoopBuffer buf;
  SemaphoreHandle_t props_mu;
  std::set<uint32_t> filtered_props;
};

#endif  // __PROPS_LOGGER_H__