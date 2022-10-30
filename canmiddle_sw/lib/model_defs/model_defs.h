#ifndef __MODEL_DEFS_H__
#define __MODEL_DEFS_H__

#include <memory>
#include <set>

#include "model.h"

extern std::unique_ptr<Model<EspAbstraction>> car_model;
extern std::unique_ptr<Model<EspAbstraction>> display_model;
extern std::unique_ptr<Model<EspAbstraction>> car_ext_model;
extern std::unique_ptr<Model<EspAbstraction>> display_ext_model;

extern SemaphoreHandle_t props_mu;
extern std::set<uint32_t> filtered_props;

void InitModels(QueueHandle_t twai_q, QueueHandle_t uart_q);

template <typename M>
void LightsOff(M *m) {
  m->DisableCanFor(1100);
  m->Update(0x525, {.size = 5, .bytes = {0x8d, 0x46, 0x46, 0x1e, 0x1e}});
  m->esp.Delay(300);
  // The following clears out the active light (and also turns that off).
  // This line is only necessary when byte 3 of 0x528 is non-zero (i.e.
  // there is an active light).
  m->Update(0x526, {.size = 4, .bytes = {0x46, 0x00, 0x46, 0x46}});
  m->esp.Delay(700);
  m->Update(0x525, {.size = 5, .bytes = {0x8c, 0x46, 0x46, 0x1e, 0x1e}});
}

template <typename M>
void DebugSet(M *m, uint32_t prop, const Value v) {
  m->DisableCanFor(500);
  m->Update(prop, v);
}

template <typename M>
void DoorOff(M *m) {
  m->DisableCanFor(500);
  m->Update(0x526, {.size = 4, .bytes = {0x46, 0x00, 0x46, 0x46}});
}

template <typename M>
void LightOn(M *m, uint8_t x, uint8_t i) {
  m->DisableCanFor(500);
  // Doing door on blocks the physical buttons?!
  m->Update(0x526, {.size = 4, .bytes = {0x46, x, 0x46, i}});
}

template <typename M>
void LightDoor(M *m, uint8_t i, bool off) {
  m->DisableCanFor(600);
  uint8_t mask[] = {0x00, 0xff, 0x00, 0xff};
  uint8_t data[] = {0x00, 0x08, 0x00, i};
  m->UpdateMask(0x526, 4, mask, data);
  if (off) {
    m->esp.Delay(300);
    uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    m->UpdateMask(0x526, 4, mask, data);
  }
}
template <typename M>
void LightOutsideKitchen(M *m, uint8_t i, bool off) {
  m->DisableCanFor(600);
  uint8_t mask[] = {0x00, 0xff, 0xff, 0x00};
  uint8_t data[] = {0x00, 0x07, i, 0x00};
  m->UpdateMask(0x526, 4, mask, data);
  if (off) {
    m->esp.Delay(300);
    uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    m->UpdateMask(0x526, 4, mask, data);
  }
}
template <typename M>
void LightTailgate(M *m, uint8_t i, bool off) {
  m->DisableCanFor(600);
  uint8_t mask[] = {0xff, 0xff, 0x00, 0x00};
  uint8_t data[] = {i, 0x06, 0x00, 0x00};
  m->UpdateMask(0x526, 4, mask, data);
  if (off) {
    m->esp.Delay(300);
    uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    m->UpdateMask(0x526, 4, mask, data);
  }
}
template <typename M>
void LightInsideKitchen(M *m, uint8_t i, bool off) {
  m->DisableCanFor(600);
  uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
  uint8_t data[] = {0x00, 0x02, 0x00, 0x00};
  m->UpdateMask(0x526, 4, mask, data);
  uint8_t mask2[] = {0x00, 0xff, 0x00, 0x00, 0x00};
  uint8_t data2[] = {0x00, i, 0x00, 0x00, 0x00};
  m->UpdateMask(0x525, 5, mask2, data2);
  if (off) {
    m->esp.Delay(300);
    uint8_t mask[] = {0x00, 0xff, 0x00, 0x00};
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    m->UpdateMask(0x526, 4, mask, data);
  }
}

#endif  // __MODEL_DEFS_H__