#ifndef __MODEL_DEFS_H__
#define __MODEL_DEFS_H__

#include <memory>

#include "model.h"

extern std::unique_ptr<Model<EspAbstraction>> car_model;
extern std::unique_ptr<Model<EspAbstraction>> display_model;
extern std::unique_ptr<Model<EspAbstraction>> car_ext_model;
extern std::unique_ptr<Model<EspAbstraction>> display_ext_model;

void InitModels(QueueHandle_t twai_q, QueueHandle_t uart_q);

#endif  // __MODEL_DEFS_H__