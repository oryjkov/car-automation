#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <Ticker.h>
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

#define MQTT_HOST IPAddress(192, 168, 158, 147)
#define MQTT_PORT 1883

const char *ssid = SECRET_WIFI_SSID;
const char *password = SECRET_WIFI_PASSWORD;

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer,
                 0);  // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  uint16_t packetIdSub = mqttClient.subscribe("test/lol", 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);
  mqttClient.publish("test/lol", 0, true, "test 1");
  Serial.println("Publishing at QoS 0");
  uint16_t packetIdPub1 = mqttClient.publish("test/lol", 1, true, "test 2");
  Serial.print("Publishing at QoS 1, packetId: ");
  Serial.println(packetIdPub1);
  uint16_t packetIdPub2 = mqttClient.publish("test/lol", 2, true, "test 3");
  Serial.print("Publishing at QoS 2, packetId: ");
  Serial.println(packetIdPub2);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total) {
  Serial.println("Publish received.");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  dup: ");
  Serial.println(properties.dup);
  Serial.print("  retain: ");
  Serial.println(properties.retain);
  Serial.print("  len: ");
  Serial.println(len);
  Serial.print("  index: ");
  Serial.println(index);
  Serial.print("  total: ");
  Serial.println(total);
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void canbus_check();
void master_loop(bool passthrough);
void start_send_state();
void stop_send_state();

Preferences pref;
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
    mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0,
                                      reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
    wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0,
                                      reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

    WiFi.onEvent(WiFiEvent);
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onSubscribe(onMqttSubscribe);
    mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.onPublish(onMqttPublish);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);

    connectToWifi();
    get_snoop_buffer()->Init();

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

void loop() { delay(1000); }