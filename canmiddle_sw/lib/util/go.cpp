#include "go.h"

void goHelper(void *args) {
  GoHelper *h = reinterpret_cast<GoHelper *>(args);
  h->f();
  delete h;
  vTaskDelete(nullptr);
}

void go(std::function<void(void)> f) {
  GoHelper *gh = new GoHelper;
  gh->f = f;
  TaskHandle_t h;
  xTaskCreate(goHelper, "goH", 4096, gh, tskIDLE_PRIORITY, &h);
}

