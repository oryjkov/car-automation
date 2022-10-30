#ifndef __GO_H__
#define __GO_H__

#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct GoHelper {
  std::function<void(void)> f;
};

void goHelper(void *args);
void go(std::function<void(void)> f);

#endif  // __GO_H__