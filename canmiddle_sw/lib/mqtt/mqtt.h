#ifndef __MQTT_H__
#define __MQTT_H__

#include "Arduino.h"
#include <AsyncMqttClient.h>

AsyncMqttClient *getMqttClient();

void mqttClientStart(IPAddress ip, uint32_t port);
void connectToMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttSubscribe(uint16_t packetId, uint8_t qos);
void onMqttUnsubscribe(uint16_t packetId);

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties,
                   size_t payload_len, size_t index, size_t total);

void onMqttPublish(uint16_t packetId);

#endif // __MQTT_H__