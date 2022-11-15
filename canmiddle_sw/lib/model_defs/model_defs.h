#ifndef __MODEL_DEFS_H__
#define __MODEL_DEFS_H__

#include <memory>
#include <set>

#include "model.h"
#include "Arduino.h"
#include "io_abstraction.h"
#include "esp_abstraction.h"
#include "props_logger.h"

void HandlePropUpdate(uint32_t prop, size_t len, const uint8_t *new_v, const uint8_t *old_v);

typedef Model<LockAbstraction, QueueAbstraction> ConcreteModel;

extern std::unique_ptr<ConcreteModel> car_model;
extern std::unique_ptr<ConcreteModel> display_model;
extern std::unique_ptr<ConcreteModel> car_ext_model;
extern std::unique_ptr<ConcreteModel> display_ext_model;

extern PropsLogger props_logger;

void InitModels(IOAbstraction *io);

template <typename M>
void DebugSet(M *m, uint32_t prop, const Value v) {
  m->DisableUpdatesFor(500);
  m->Update(prop, v);
}

enum LightsEnum {
  OUTSIDE_KITCHEN = 1,
  INSIDE_KITCHEN = 2,
  DOOR = 3,
  TAILGATE = 4,
};
void SetLight(LightsEnum light, uint8_t brightness, bool off);
void LightsOff();

void SetFridgeState(bool on);
void SetFridgePower(uint32_t power);

#endif  // __MODEL_DEFS_H__