#include "mqtt.h"

#include <AsyncMqttClient.h>
#include <WiFi.h>

#include "Arduino.h"
#include "go.h"
#include "model.h"
#include "model_defs.h"

AsyncMqttClient mqttClient;

TimerHandle_t mqttReconnectTimer;

AsyncMqttClient *getMqttClient() { return &mqttClient; }

static String hassConfig[][2] = {
    {
        "fan",
        R"END(
{
  "name": "Fridge",
  "command_topic": "car/fridge/command",
  "state_topic": "car/fridge/state",
  "percentage_state_topic": "car/fridge/percentage_state",
  "percentage_command_topic": "car/fridge/percentage_command",
  "speed_range_min": 1,
  "speed_range_max": 6
}
)END",
    },
    {
        "button",
        R"END(
{
  "name": "Restart CAN controller",
  "command_topic": "car/controller/restart"
}
)END",
    },
    {
        "button",
        R"END(
{
  "name": "Start CAN comms",
  "command_topic": "car/controller/can/start"
}
)END",
    },
    {
        "button",
        R"END(
{
  "name": "Stop CAN comms",
  "command_topic": "car/controller/can/stop"
}
)END",
    },
    {
        "light",
        R"END(
{
  "name": "Door light",
  "state_topic": "car/light/door/status",
  "command_topic": "car/light/door/switch",
  "brightness_state_topic": "car/light/door/brightness",
  "brightness_command_topic": "car/light/door/brightness/set",
  "brightness_scale": 100,
  "on_command_type": "brightness",
  "payload_off": "OFF",
  "optimistic": false,
  "dev": { "identifiers": "l1" }
}
)END",
    },
    {
        "light",
        R"END(
{
  "name": "Tailgate light",
  "state_topic": "car/light/tailgate/status",
  "command_topic": "car/light/tailgate/switch",
  "brightness_state_topic": "car/light/tailgate/brightness",
  "brightness_command_topic": "car/light/tailgate/brightness/set",
  "brightness_scale": 100,
  "on_command_type": "brightness",
  "payload_off": "OFF",
  "optimistic": false,
  "dev": { "identifiers": "l2" }
}
)END",
    },
    {
        "light",
        R"END(
{
  "name": "Inside Kitchen light",
  "state_topic": "car/light/inside_kitchen/status",
  "command_topic": "car/light/inside_kitchen/switch",
  "brightness_state_topic": "car/light/inside_kitchen/brightness",
  "brightness_command_topic": "car/light/inside_kitchen/brightness/set",
  "brightness_scale": 100,
  "on_command_type": "brightness",
  "payload_off": "OFF",
  "optimistic": false,
  "dev": { "identifiers": "l3" }
}
)END",
    },
    {
        "light",
        R"END(
{
  "name": "Outside Kitchen light",
  "state_topic": "car/light/outside_kitchen/status",
  "command_topic": "car/light/outside_kitchen/switch",
  "brightness_state_topic": "car/light/outside_kitchen/brightness",
  "brightness_command_topic": "car/light/outside_kitchen/brightness/set",
  "brightness_scale": 100,
  "on_command_type": "brightness",
  "payload_off": "OFF",
  "optimistic": false,
  "dev": { "identifiers": "l4" }
}
)END",
    },
};

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);

  // Device discovery. Pubilsh with retain since HASS requires that.
  int i = 0;
  for (const auto &hc : hassConfig) {
    i += 1;
    getMqttClient()->publish(
        (String("homeassistant/" + hc[0] + "/l") + String(i) + String("/config")).c_str(), 2, true,
        hc[1].c_str());
  }

  mqttClient.subscribe("car/controller/restart", 2);
  mqttClient.subscribe("car/controller/can/stop", 2);
  mqttClient.subscribe("car/controller/can/start", 2);

  mqttClient.subscribe("car/fridge/command", 2);
  mqttClient.subscribe("car/fridge/percentage_command", 2);

  mqttClient.subscribe("car/light/door/switch", 2);
  mqttClient.subscribe("car/light/door/brightness/set", 2);

  mqttClient.subscribe("car/light/outside_kitchen/switch", 2);
  mqttClient.subscribe("car/light/outside_kitchen/brightness/set", 2);

  mqttClient.subscribe("car/light/tailgate/switch", 2);
  mqttClient.subscribe("car/light/tailgate/brightness/set", 2);

  mqttClient.subscribe("car/light/inside_kitchen/switch", 2);
  mqttClient.subscribe("car/light/inside_kitchen/brightness/set", 2);
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

int ParseBrightness(const String &payload) {
  int b = payload.toInt();
  if (b > 100) {
    b = 100;
  }
  if (b < 1) {
    b = 1;
  }
  return b;
}

extern void start_send_state();
extern void stop_send_state();

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties,
                   size_t payload_len, size_t index, size_t total) {
  String payload_str = String(payload, payload_len);
  String topic_str = String(topic);
  Serial.print("message on '");
  Serial.print(topic_str);
  Serial.print("' payload: '");
  Serial.print(payload_str);
  Serial.println("'");

  if (topic_str == "car/controller/restart") {
    abort();
  } else if (topic_str == "car/controller/start") {
      start_send_state();
  } else if (topic_str == "car/controller/stop") {
      stop_send_state();
  } else if (topic_str == "car/fridge/command") {
    bool s = (payload_str == "ON");
    go([=]() { SetFridgeState(s); });
  } else if (topic_str == "car/fridge/percentage_command") {
    uint32_t p = payload_str.toInt();
    go([=]() { SetFridgePower(p); });
  } else if (topic_str == "car/light/door/brightness/set") {
    int brightness = ParseBrightness(payload_str);
    go([=]() { SetLight("door", brightness, false); });
  } else if (topic_str == "car/light/door/switch") {
    if (payload_str == "OFF") {
      go([=]() { SetLight("door", 70, true); });
    }
  } else if (topic_str == "car/light/outside_kitchen/brightness/set") {
    int brightness = ParseBrightness(payload_str);
    go([=]() { SetLight("outside_kitchen", brightness, false); });
  } else if (topic_str == "car/light/outside_kitchen/switch") {
    if (payload_str == "OFF") {
      go([=]() { SetLight("outside_kitchen", 70, true); });
    }
  } else if (topic_str == "car/light/tailgate/brightness/set") {
    int brightness = ParseBrightness(payload_str);
    go([=]() { SetLight("tailgate", brightness, false); });
  } else if (topic_str == "car/light/tailgate/switch") {
    if (payload_str == "OFF") {
      go([=]() { SetLight("tailgate", 70, true); });
    }
  } else if (topic_str == "car/light/inside_kitchen/brightness/set") {
    int brightness = ParseBrightness(payload_str);
    go([=]() { SetLight("inside_kitchen", brightness, false); });
  } else if (topic_str == "car/light/inside_kitchen/switch") {
    if (payload_str == "OFF") {
      go([=]() { SetLight("inside_kitchen", 70, true); });
    }
  } else {
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void mqttClientStart(IPAddress ip, uint32_t port) {
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(ip, port);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}
