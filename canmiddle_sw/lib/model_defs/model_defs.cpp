#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "Arduino.h"
#include "esp_abstraction.h"
#include "io_abstraction.h"
#include "go.h"
#include "model.h"
#include "mqtt.h"
#include "props_logger.h"

std::unique_ptr<ConcreteModel> car_model;
std::unique_ptr<ConcreteModel> display_model;
std::unique_ptr<ConcreteModel> car_ext_model;
std::unique_ptr<ConcreteModel> display_ext_model;

PropsLogger props_logger;

void InitModels(IOAbstraction *io) {
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
                  .esp = EspAbstraction(io->twai_tx_queue),
                  .can_enable_at_us = 0,
              }));

  car_ext_model = std::unique_ptr<Model<EspAbstraction>>(new Model<EspAbstraction>({
      .props = {
              // clang-format off
{ .prop = 0x1b00002c, .send_delay_ms =  22, .val = { .size = 8, .bytes = { 0x2c, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, } }, .iteration = 1 },
{ .prop = 0x1b00002c, .send_delay_ms = 200, .val = { .size = 8, .bytes = { 0x2c, 0x00, 0x01, 0x01, 0x04, 0x00, 0x00, 0x00, } } },
              // clang-format on
      },
      .esp = EspAbstraction(io->twai_tx_queue),
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
      .esp = EspAbstraction(io->twai_tx_queue),
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
      .esp = EspAbstraction(io->twai_tx_queue),
      .can_enable_at_us = 0,
  }));

  props_logger = PropsLogger();
}

void PublishOnOff(const String &light, bool state) {
  String s = (state) ? "ON" : "OFF";
  getMqttClient()->publish(String("car/light/" + light + "/status").c_str(), 2, true, s.c_str(),
                           s.length());
}

void PublishBrightness(const String &light, uint32_t brightness) {
  String b = String(brightness);
  getMqttClient()->publish(String("car/light/" + light + "/brightness").c_str(), 2, true, b.c_str(),
                           b.length());
}

// Structure containing the CAN properties for getting/setting lights.
struct LightDescriptor {
  String light;            // String name - same as what's used in the mqtt topics.
  size_t state_bit;        // from prop 0x528
  size_t brightness_prop;  // either 0x527 or 0x528
  size_t brightness_byte;  // byte num in the prop above. Only take 7 lower bits in that byte.

  uint8_t selector;            // Selector that is written to byte 1 of 0x526.
  size_t set_brightness_prop;  // Property for setting the brightness.
  size_t
      set_brightness_byte;  // Byte number for brightness. Only 7 lowest bits of that byte are used.
};
std::map<LightsEnum, LightDescriptor> lmp = {
    {OUTSIDE_KITCHEN,
     {
         .light = "outside_kitchen",
         .state_bit = 0,
         .brightness_prop = 0x528,
         .brightness_byte = 1,
         .selector = 7,
         .set_brightness_prop = 0x526,
         .set_brightness_byte = 2,
     }},
    {
        INSIDE_KITCHEN,
        {
            .light = "inside_kitchen",
            .state_bit = 5,
            .brightness_prop = 0x527,
            .brightness_byte = 1,
            .selector = 2,
            .set_brightness_prop = 0x525,
            .set_brightness_byte = 1,
        },
    },
    {
        DOOR,
        {
            .light = "door",
            .state_bit = 8,
            .brightness_prop = 0x528,
            .brightness_byte = 2,
            .selector = 8,
            .set_brightness_prop = 0x526,
            .set_brightness_byte = 3,
        },
    },
    {
        TAILGATE,
        {
            .light = "tailgate",
            .state_bit = 1,
            .brightness_prop = 0x527,
            .brightness_byte = 5,
            .selector = 6,
            .set_brightness_prop = 0x526,
            .set_brightness_byte = 0,
        },
    },
};

void HandlePropUpdate(uint32_t prop, size_t len, const uint8_t *new_v, const uint8_t *old_v) {
  if (memcmp(old_v, new_v, len) == 0) {
    return;
  }

  // lights
  for (const auto iter : lmp) {
    const LightDescriptor &l = iter.second;
    if (prop == 0x528 && (getBit(l.state_bit, new_v) != getBit(l.state_bit, old_v))) {
      bool s = getBit(l.state_bit, new_v);
      go([=]() { PublishOnOff(l.light, s); });
    }
    if (prop == l.brightness_prop && (getBrightnessByte(l.brightness_byte, new_v) !=
                                      getBrightnessByte(l.brightness_byte, old_v))) {
      uint8_t b = getBrightnessByte(l.brightness_byte, new_v);
      go([=]() { PublishBrightness(l.light, b); });
    }
  }
  // Check for the physical light button press which sets the selector to 0.
  if (prop == 0x528 && (getByte(3, new_v) != getByte(3, old_v))) {
    if (getByte(3, new_v) == 0) {
      display_model->DisableCanFor(600);
      uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
      uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
      display_model->UpdateMasked(0x526, 4, mask, data);
    }
  }

  // fridge
  if (prop == 0x532) {
    if (getBit(7, new_v) != getBit(7, old_v)) {
      bool s = getBit(7, new_v);
      String state = (s) ? "ON" : "OFF";
      String topic = "car/fridge/state";
      go([=]() {
        getMqttClient()->publish(topic.c_str(), 2, true, state.c_str(), state.length());
      });
    }
    if (getBits(2, 3, new_v) != getBits(2, 3, old_v)) {
      uint32_t power_setting = getBits(2, 3, new_v);
      String ps = String(power_setting);
      String topic = "car/fridge/percentage_state";

      go([=]() { getMqttClient()->publish(topic.c_str(), 2, true, ps.c_str(), ps.length()); });
    }
  }

  props_logger.log(prop, len, new_v);
}

void SetFridgePower(uint32_t power) {
  if (power < 0 || power > 6) {
    return;
  }
  display_model->DisableCanFor(600);
  uint8_t mask[] = {7 << 1, 0x00, 0x00, 0x00};
  uint8_t data[] = {static_cast<uint8_t>(power << 1), 0x00, 0x00, 0x00};
  display_model->UpdateMasked(0x531, 4, mask, data);
}

void SetFridgeState(bool on) {
  display_model->DisableCanFor(600);
  uint8_t mask[] = {1, 0x00, 0x00, 0x00};
  uint8_t data[] = {static_cast<uint8_t>(on), 0x00, 0x00, 0x00};
  display_model->UpdateMasked(0x531, 4, mask, data);
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

void SetLight(LightsEnum light, uint8_t brightness, bool off) {
  auto search = lmp.find(light);

  if (search == lmp.end()) {
    return;
  }
  display_model->DisableCanFor(600);
  const LightDescriptor &l = search->second;

  // Set Selector.
  uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
  uint8_t data[] = {0x00, l.selector, 0x00, 0x00};
  display_model->UpdateMasked(0x526, 4, mask, data);

  // Set brightness. Mask and data have lenght of 5, as this is the max (prop 0x525).
  uint8_t mask2[] = {0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t data2[] = {0x00, 0x00, 0x00, 0x00, 0x00};
  mask2[l.set_brightness_byte] = 0x7f;
  data2[l.set_brightness_byte] = brightness;
  // Pass in the lenght of 5, even though if l.brightness_prop==0x526, it is 4.
  display_model->UpdateMasked(l.set_brightness_prop, 5, mask2, data2);

  if (off) {
    // OFF is always the same, but we have to let the selector we just set propagate.
    display_model->esp.Delay(300);
    uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    display_model->UpdateMasked(0x526, 4, mask, data);
  }
}
