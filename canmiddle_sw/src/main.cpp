#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <pb_decode.h>
#include <pb_encode.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "go.h"
#include "message.pb.h"
#include "model_defs.h"
#include "secrets.h"
#include "util.h"

uint8_t hexdigit(char hex) { return (hex <= '9') ? hex - '0' : toupper(hex) - 'A' + 10; }

uint8_t hexbyte(const char *hex) { return (hexdigit(*hex) << 4) | hexdigit(*(hex + 1)); }

AsyncWebServer server(80);

const char *ssid = SECRET_WIFI_SSID;
const char *password = SECRET_WIFI_PASSWORD;

Preferences pref;

void canbus_check();
void master_loop(bool passthrough);
void start_send_state();
void stop_send_state();

enum Role : uint32_t {
  MASTER = 1,
  SLAVE = 2,
  // Sends on CAN and prints what it gets on CAN.
  DEBUGGER = 3,
  MIRROR_ODD = 4,
};

Role r;

void program_as(Role r) {
  pref.begin("canmiddle", false);
  int x = pref.getUInt("role", 0);
  Serial.printf("read role: %d, want: %d\r\n", x, r);
  if (x == r) {
    Serial.println("already programmed as required.");
    return;
  }
  delay(5000);

  pref.clear();
  pref.putUInt("role", r);
  pref.end();

  Serial.printf("programmed as: %d\r\n", r);
}

void setup() {
  Serial.begin(115200);
  Serial.println("serial port initialized");

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set(EXAMPLE_TAG, ESP_LOG_INFO);

  esp_chip_info_t chip;
  esp_chip_info(&chip);
  Serial.printf("chip info: model: %d, cores: %d, revision: %d\r\n", chip.model, chip.cores,
                chip.revision);
  // program_as(DEBUGGER);

  pref.begin("canmiddle", true);
  r = static_cast<Role>(pref.getUInt("role", 0));

  uart_config_t uart_config = {
      .baud_rate = 5000000,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 1024 * 2, 0, 0, NULL, 0));
  ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
  ESP_ERROR_CHECK(
      uart_set_pin(2, GPIO_NUM_17, GPIO_NUM_16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

  pinMode(LED_BUILTIN, OUTPUT);

  Serial.printf("running as role: %d\r\n", r);
  if (r == MASTER) {
    get_snoop_buffer()->Init();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      return;
    }

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncResponseStream *response = request->beginResponseStream("text/plain");
      print_stats(response, get_stats());
      request->send(response);
    });
    server.on("/filter", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (!request->hasParam("k")) {
        request->send(500, "text/plain", "k is required\r\n");
        return;
      }
      uint32_t key;
      sscanf(request->getParam("k")->value().c_str(), "%x", &key);
      xSemaphoreTake(props_mu, portMAX_DELAY);
      filtered_props.insert(key);
      xSemaphoreGive(props_mu);
      request->send(500, "text/plain", "ok\r\n");
    });
    server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (!request->hasParam("k") || !request->hasParam("v")) {
        request->send(500, "text/plain", "k,v are required\r\n");
        return;
      }
      uint32_t key;
      sscanf(request->getParam("k")->value().c_str(), "%x", &key);
      if (request->getParam("v")->value().length() > 16) {
        request->send(500, "text/plain", "v is too long\r\n");
        return;
      }
      if (request->getParam("v")->value().length() % 2 != 0) {
        request->send(500, "text/plain", "v must be of even length\r\n");
        return;
      }
      auto param_v = request->getParam("v")->value();
      Value val;
      val.size = param_v.length() / 2;
      for (int i = 0; i < val.size; i++) {
        val.bytes[i] = hexbyte(param_v.c_str() + i * 2);
      }
      auto *m = display_model.get();
      go([=]() { DebugSet(m, key, val); });

      request->send(500, "text/plain", "ok\r\n");
    });
    server.on("/setLight", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (!request->hasParam("light")) {
        request->send(500, "text/plain", "light is required");
        return;
      }
      auto light = request->getParam("light")->value();
      int i = 70;
      if (request->hasParam("i")) {
        auto p = request->getParam("i");
        if (p != nullptr) {
          i = p->value().toInt();
        }
      }
      bool off = (i == 0);
      if (i < 1) {
        i = 1;
      }
      if (i > 100) {
        i = 100;
      }
      auto *m = display_model.get();
      if (light == "Door") {
        go([=]() { LightDoor(m, i, off); });
      } else if (light == "OutsideKitchen") {
        go([=]() { LightOutsideKitchen(m, i, off); });
      } else if (light == "Tailgate") {
        go([=]() { LightTailgate(m, i, off); });
      } else if (light == "InsideKitchen") {
        go([=]() { LightInsideKitchen(m, i, off); });
      } else {
        request->send(500, "text/plain", "light unknown");
        return;
      }
      request->send(200, "text/plain", "ok");
    });
    server.on("/doorOff", HTTP_GET, [](AsyncWebServerRequest *request) {
      auto *m = display_model.get();
      go([=]() { DoorOff(m); });
      request->send(200, "text/plain", "ligts off");
    });
    server.on("/lightsOff", HTTP_GET, [](AsyncWebServerRequest *request) {
      auto *m = display_model.get();
      go([=]() { LightsOff(m); });
      request->send(200, "text/plain", "ligts off");
    });
    server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request) {
      start_send_state();
      request->send(200, "text/plain", "Started");
    });
    server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
      stop_send_state();
      request->send(200, "text/plain", "Stopped");
    });
    server.on("/get_snoop", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (get_snoop_buffer()->IsActive()) {
        request->send(400, "text/plain", "snoop is still active");
      } else {
        AsyncWebServerResponse *response =
            request->beginResponse_P(200, "application/octet-stream", get_snoop_buffer()->buffer,
                                     get_snoop_buffer()->position);
        request->send(response);
      }
    });
    server.on("/start_snoop", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncResponseStream *response = request->beginResponseStream("text/plain");
      if (get_snoop_buffer()->IsActive()) {
        response->printf("already running for another %d ms, snooped %d bytes\r\n",
                         get_snoop_buffer()->TimeRemainingMs(), get_snoop_buffer()->position);
      } else {
        uint32_t duration_ms = 1000;
        if (request->hasParam("duration_ms")) {
          auto p = request->getParam("duration_ms");
          int d = p->value().toInt();
          if ((d > 0) && (d < 10000)) {
            duration_ms = d;
          }
        }
        get_snoop_buffer()->Activate(duration_ms);
        get_snoop_buffer()->position = 0;
        response->printf("Snoop started. will run for %d buffer available: %d", duration_ms,
                         snoop_buffer_max_size);
      }
      request->send(response);
    });
    server.begin();

    digitalWrite(LED_BUILTIN, 1);
    Serial.println("master_loop");
    master_loop(false);
    Serial.println("master_loop returned");
  } else if (r == SLAVE) {
    digitalWrite(LED_BUILTIN, 0);
    master_loop(true);
  } else if (r == DEBUGGER) {
    digitalWrite(LED_BUILTIN, 0);
  } else if (r == MIRROR_ODD) {
    digitalWrite(LED_BUILTIN, 0);
  } else {
    Serial.printf("role unknown: %d\n", r);
    while (1) {
    }
  }
}

void loop_master() {
  // placeholder
  delay(1000);
}

void loop_slave() {
  // placeholder
  delay(1000);
}

void loop() {
  if (r == MASTER) {
    loop_master();
  } else if (r == SLAVE) {
    loop_slave();
  } else if (r == DEBUGGER) {
    delay(1000);
    // loop_mirror(0);
    //  loop_debugger();
  } else if (r == MIRROR_ODD) {
    delay(1000);
    // loop_mirror(1);
  } else {
    while (1) {
    }
  }
}