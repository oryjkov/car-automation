#ifndef __MODEL_DEFS_H__
#define __MODEL_DEFS_H__

#include <memory>
#include <set>

#include "model.h"
#include "Arduino.h"

void HandlePropUpdate(uint32_t prop, size_t len, const uint8_t *new_v, const uint8_t *old_v);

typedef Model<EspAbstraction> ConcreteModel;

extern std::unique_ptr<ConcreteModel> car_model;
extern std::unique_ptr<ConcreteModel> display_model;
extern std::unique_ptr<ConcreteModel> car_ext_model;
extern std::unique_ptr<ConcreteModel> display_ext_model;

extern SemaphoreHandle_t props_mu;
extern std::set<uint32_t> filtered_props;

void InitModels(QueueHandle_t twai_q, QueueHandle_t uart_q);

template <typename M>
void DebugSet(M *m, uint32_t prop, const Value v) {
  m->DisableCanFor(500);
  m->Update(prop, v);
}

void SetLight(const String &name, uint8_t i, bool off);
void LightsOff();

void SetFridgeState(bool on);
void SetFridgePower(uint32_t power);

#endif  // __MODEL_DEFS_H__