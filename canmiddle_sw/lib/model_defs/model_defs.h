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
void DebugSet(M *m, uint32_t prop, const Value v) {
  m->DisableCanFor(500);
  m->Update(prop, v);
}

void LightsOff();
void LightDoor(uint8_t i, bool off);
void LightOutsideKitchen(uint8_t i, bool off);
void LightTailgate(uint8_t i, bool off);
void LightInsideKitchen(uint8_t i, bool off);

#endif  // __MODEL_DEFS_H__