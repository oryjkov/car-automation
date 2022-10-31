#include <stdint.h>

#include <memory>
#include <set>

#include "Arduino.h"
#include "esp_abstraction.h"
#include "model.h"
#include "mqtt.h"
#include "go.h"

std::unique_ptr<Model<EspAbstraction>> car_model{};
std::unique_ptr<Model<EspAbstraction>> display_model{};
std::unique_ptr<Model<EspAbstraction>> car_ext_model{};
std::unique_ptr<Model<EspAbstraction>> display_ext_model{};

SemaphoreHandle_t props_mu;
std::set<uint32_t> filtered_props;

void InitModels(QueueHandle_t twai_q, QueueHandle_t uart_q) {
  car_model =
      std::unique_ptr<Model<EspAbstraction>>(
          new Model<EspAbstraction>(
              {
                  .props =
                      {
                          // clang-format off
{ .prop = DELAY_ONLY_PROP, .send_delay_ms = 82, .val={}, .iteration = 1 },  // offset
{ .prop = 0x052a, .send_delay_ms =  0, .val = { .size = 1, .bytes = { 0x3a,   } } },
{ .prop = 0x0532, .send_delay_ms =  0, .val = { .size = 5, .bytes = { 0x00, 0x04, 0x01, 0x04, 0x01,   } } },
{ .prop = 0x053c, .send_delay_ms =  0, .val = { .size = 4, .bytes = { 0x4a, 0x00, 0x25, 0x00,   } } },
{ .prop = 0x0528, .send_delay_ms = 10, .val = { .size = 4, .bytes = { 0x02, 0x46, 0x46, 0x00,   } } },
{ .prop = 0x0530, .send_delay_ms =  0, .val = { .size = 1, .bytes = { 0x01,   } } },
{ .prop = 0x053b, .send_delay_ms =  0, .val = { .size = 6, .bytes = { 0x02, 0x24, 0xa4, 0x00, 0x20, 0x00,   } } },
{ .prop = 0x0527, .send_delay_ms = 10, .val = { .size = 6, .bytes = { 0x46, 0x46, 0x46, 0x1e, 0x1e, 0x46,   } } },
{ .prop = 0x052e, .send_delay_ms =  0, .val = { .size = 1, .bytes = { 0x75,   } } },
{ .prop = 0x053a, .send_delay_ms =  0, .val = { .size = 6, .bytes = { 0x16, 0x11, 0x09, 0x38, 0x19, 0x11,   } } },
{ .prop = 0x0524, .send_delay_ms = 10, .val = { .size = 1, .bytes = { 0x70,   } } },
{ .prop = 0x052d, .send_delay_ms =  0, .val = { .size = 1, .bytes = { 0x78,   } } },
{ .prop = 0x0537, .send_delay_ms =  0, .val = { .size = 3, .bytes = { 0x00, 0x10, 0x00,   } } },
{ .prop = 0x0523, .send_delay_ms = 10, .val = { .size = 2, .bytes = { 0xf0, 0x00,   } } },
{ .prop = 0x0535, .send_delay_ms =  0, .val = { .size = 1, .bytes = { 0x01,   } } },
{ .prop = 0x053d, .send_delay_ms =  0, .val = { .size = 3, .bytes = { 0xfe, 0xff, 0x06,   } } },
{ .prop = 0x0521, .send_delay_ms = 10, .val = { .size = 1, .bytes = { 0x00,   } } },
{ .prop = 0x052c, .send_delay_ms =  0, .val = { .size = 7, .bytes = { 0x05, 0x0c, 0x2f, 0x39, 0x01, 0x02, 0x54,   } } },
{ .prop = 0x0533, .send_delay_ms =  0, .val = { .size = 3, .bytes = { 0x00, 0x37, 0x37,   } } },
{ .prop = DELAY_ONLY_PROP, .send_delay_ms =  40, },
                          // clang-format on
                      },
                  .esp = EspAbstraction(uart_q),
                  .can_enable_at_us = 0,
              }));

  car_ext_model = std::unique_ptr<Model<EspAbstraction>>(new Model<EspAbstraction>({
      .props = {
              // clang-format off
{ .prop = 0x1b00002c, .send_delay_ms =  22, .val = { .size = 8, .bytes = { 0x2c, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 1 },
{ .prop = 0x1b00002c, .send_delay_ms = 200, .val = { .size = 8, .bytes = { 0x2c, 0x00, 0x01, 0x01, 0x04, 0x00, 0x00, 0x00, } } },
              // clang-format on
      },
      .esp = EspAbstraction(uart_q),
      .can_enable_at_us = 0,
  }));

  display_model = std::unique_ptr<Model<EspAbstraction>>(new Model<EspAbstraction>({
      .props =
          {
              // clang-format off
{ .prop = DELAY_ONLY_PROP, .send_delay_ms =  12, .val={}, .iteration = 1 },  // offset
{ .prop = 0x0520, .send_delay_ms =  0, .val = { .size = 1, .bytes = { 0x00, } } },
{ .prop = 0x0522, .send_delay_ms =  0, .val = { .size = 2, .bytes = { 0x00, 0x00, } } },
{ .prop = 0x0525, .send_delay_ms =  0, .val = { .size = 5, .bytes = { 0x8c, 0x46, 0x46, 0x1e, 0x1e, } } },
{ .prop = 0x0526, .send_delay_ms =  0, .val = { .size = 4, .bytes = { 0x46, 0x00, 0x46, 0x46, } } },
{ .prop = 0x0529, .send_delay_ms =  0, .val = { .size = 4, .bytes = { 0x03, 0x01, 0x01, 0x00, } } },
{ .prop = 0x052b, .send_delay_ms =  0, .val = { .size = 1, .bytes = { 0x00, } } },
{ .prop = 0x052f, .send_delay_ms =  0, .val = { .size = 1, .bytes = { 0x00, } } },
{ .prop = 0x0531, .send_delay_ms =  0, .val = { .size = 4, .bytes = { 0x00, 0x00, 0x04, 0x01, } } },
{ .prop = 0x0534, .send_delay_ms =  0, .val = { .size = 2, .bytes = { 0x02, 0x00, } } },
{ .prop = 0x0536, .send_delay_ms =  0, .val = { .size = 3, .bytes = { 0x10, 0x00, 0x00, } } },
{ .prop = 0x0538, .send_delay_ms =  1, .val = { .size = 4, .bytes = { 0x20, 0x25, 0x80, 0x00, } } },
{ .prop = 0x0539, .send_delay_ms =  0, .val = { .size = 6, .bytes = { 0x24, 0x02, 0x24, 0x00, 0x00, 0x00, } } },
{ .prop = DELAY_ONLY_PROP, .send_delay_ms =  99, },
              // clang-format on
          },
      .esp = EspAbstraction(twai_q),
      .can_enable_at_us = 0,
  }));

  display_ext_model = std::unique_ptr<Model<EspAbstraction>>(new Model<EspAbstraction>({
              // clang-format off
      .props = {
{ .prop = 0x1b000046, .send_delay_ms =   0, .val = { .size = 8, .bytes = { 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 1 },
{ .prop = 0x1b000046, .send_delay_ms =  10, .val = { .size = 8, .bytes = { 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 2 },
{ .prop = 0x1b000046, .send_delay_ms =  10, .val = { .size = 8, .bytes = { 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 3 },
{ .prop = 0x1b000046, .send_delay_ms =  10, .val = { .size = 8, .bytes = { 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 4 },
{ .prop = 0x1b000046, .send_delay_ms =  10, .val = { .size = 8, .bytes = { 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 5 },
{ .prop = 0x1b000046, .send_delay_ms =  10, .val = { .size = 8, .bytes = { 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 6 },
{ .prop = 0x1b000046, .send_delay_ms =  10, .val = { .size = 8, .bytes = { 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 7 },
{ .prop = 0x1b000046, .send_delay_ms =  10, .val = { .size = 8, .bytes = { 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 8 },
{ .prop = 0x1b000046, .send_delay_ms =  10, .val = { .size = 8, .bytes = { 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 9 },
{ .prop = 0x1b000046, .send_delay_ms =  10, .val = { .size = 8, .bytes = { 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 10 },
{ .prop = 0x1b000046, .send_delay_ms = 100, .val = { .size = 8, .bytes = { 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, } } },
          },
              // clang-format on
      .esp = EspAbstraction(twai_q),
      .can_enable_at_us = 0,
  }));

  props_mu = xSemaphoreCreateMutex();
  if (props_mu == nullptr) {
    abort();
  }
  filtered_props = {
      0x53a,
  };
  // selected_props = {0x525, 0x526, 0x527, 0x527, 0x528, };
}

void PublishOnOff(bool state) {
  String s = (state) ? "ON" : "OFF";
  getMqttClient()->publish("car/light/outside_kitchen/status", 2, true, s.c_str(), s.length());
}

// Gets a bit from a byte array.
bool getBit(uint8_t bit_num, const uint8_t *d) {
  uint8_t byte_num = bit_num % 8;
  bit_num = bit_num / 8;

  return (d[byte_num] & (1 << bit_num)) == 0;
}

void HandlePropUpdate(uint32_t prop, size_t len, const uint8_t *new_v, const uint8_t *old_v) {
  if (memcmp(old_v, new_v, len) == 0) {
    return;
  }

  if (prop == 0x528) {
    // Light status update.

    if (getBit(0, new_v) != getBit(0, old_v)) {
      // Outside Kitchen light changes.
      go([=]() { PublishOnOff(getBit(0, old_v)); });
    }
  }

  xSemaphoreTake(props_mu, portMAX_DELAY);
  if (filtered_props.count(prop) == 0) {
    Serial.printf("[%05d] 0x%04x: ", millis(), prop);
    for (int i = 0; i < len; i++) {
      Serial.printf("0x%02x ", new_v[i]);
    }
    Serial.println();
  }
  xSemaphoreGive(props_mu);
}

void LightsOff() {
  display_model->DisableCanFor(1100);
  display_model->Update(0x525, {.size = 5, .bytes = {0x8d, 0x46, 0x46, 0x1e, 0x1e}});
  display_model->esp.Delay(300);
  // The following clears out the active light (and also turns that off).
  // This line is only necessary when byte 3 of 0x528 is non-zero (i.e.
  // there is an active light).
  display_model->Update(0x526, {.size = 4, .bytes = {0x46, 0x00, 0x46, 0x46}});
  display_model->esp.Delay(700);
  display_model->Update(0x525, {.size = 5, .bytes = {0x8c, 0x46, 0x46, 0x1e, 0x1e}});
}

void LightDoor(uint8_t i, bool off) {
  display_model->DisableCanFor(600);
  uint8_t mask[] = {0x00, 0xff, 0x00, 0xff};
  uint8_t data[] = {0x00, 0x08, 0x00, i};
  display_model->UpdateMasked(0x526, 4, mask, data);
  if (off) {
    display_model->esp.Delay(300);
    uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    display_model->UpdateMasked(0x526, 4, mask, data);
  }
}
void LightOutsideKitchen(uint8_t i, bool off) {
  display_model->DisableCanFor(600);
  uint8_t mask[] = {0x00, 0xff, 0xff, 0x00};
  uint8_t data[] = {0x00, 0x07, i, 0x00};
  display_model->UpdateMasked(0x526, 4, mask, data);
  if (off) {
    display_model->esp.Delay(300);
    uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    display_model->UpdateMasked(0x526, 4, mask, data);
  }
}
void LightTailgate(uint8_t i, bool off) {
  display_model->DisableCanFor(600);
  uint8_t mask[] = {0xff, 0xff, 0x00, 0x00};
  uint8_t data[] = {i, 0x06, 0x00, 0x00};
  display_model->UpdateMasked(0x526, 4, mask, data);
  if (off) {
    display_model->esp.Delay(300);
    uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    display_model->UpdateMasked(0x526, 4, mask, data);
  }
}
void LightInsideKitchen(uint8_t i, bool off) {
  display_model->DisableCanFor(600);
  uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
  uint8_t data[] = {0x00, 0x02, 0x00, 0x00};
  display_model->UpdateMasked(0x526, 4, mask, data);
  uint8_t mask2[] = {0x00, 0xff, 0x00, 0x00, 0x00};
  uint8_t data2[] = {0x00, i, 0x00, 0x00, 0x00};
  display_model->UpdateMasked(0x525, 5, mask2, data2);
  if (off) {
    display_model->esp.Delay(300);
    uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    display_model->UpdateMasked(0x526, 4, mask, data);
  }
}
