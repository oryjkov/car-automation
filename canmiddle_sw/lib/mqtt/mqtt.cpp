#include "Arduino.h"
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include "go.h"
#include "mqtt.h"
#include "model_defs.h"

AsyncMqttClient mqttClient;

TimerHandle_t mqttReconnectTimer;

AsyncMqttClient *getMqttClient() {
    return &mqttClient;
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);

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

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties,
                   size_t payload_len, size_t index, size_t total) {
  String payload_str = String(payload, payload_len);
  String topic_str = String(topic);
  Serial.print("message on '");
  Serial.print(topic_str);
  Serial.print("' payload: '");
  Serial.print(payload_str);
  Serial.println("'");

  if (topic_str == "car/light/door/brightness/set") {
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
