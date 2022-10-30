#ifndef __MODEL_DEFS_H__
#define __MODEL_DEFS_H__

#include <memory>

#include "model.h"

extern std::unique_ptr<Model<EspAbstraction>> car_model;
extern std::unique_ptr<Model<EspAbstraction>> display_model;
extern std::unique_ptr<Model<EspAbstraction>> car_ext_model;
extern std::unique_ptr<Model<EspAbstraction>> display_ext_model;

void InitModels(QueueHandle_t twai_q, QueueHandle_t uart_q);

template <typename M>
void LightsOff(M *m) {
  m->SetCan(false);
  m->Update(0x525, {.size = 5, .bytes = {0x8d, 0x46, 0x46, 0x1e, 0x1e}});
  m->esp.Delay(1000);
  m->Update(0x525, {.size = 5, .bytes = {0x8c, 0x46, 0x46, 0x1e, 0x1e}});
  m->SetCan(true);
}

template <typename M>
void DoorOff(M *m) {
  m->SetCan(false);
  m->Update(0x526, {.size = 4, .bytes = {0x46, 0x00, 0x46, 0x46}});
  m->esp.Delay(110);
  m->SetCan(true);
}

template <typename M>
void DoorOn(M *m) {
  m->SetCan(false);
  m->Update(0x526, {.size = 4, .bytes = {0x46, 0x08, 0x46, 0x46}});
  m->esp.Delay(110);
  m->SetCan(true);
}

#endif  // __MODEL_DEFS_H__