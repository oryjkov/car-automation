#ifndef __MQTT_H__
#define __MQTT_H__

#include "Arduino.h"
#include <AsyncMqttClient.h>

#include "model.h"
#include "mqtt.h"
#include "model_defs.h"

AsyncMqttClient *getMqttClient();

void mqttClientStart(IPAddress ip, uint32_t port);
void connectToMqtt();

#endif // __MQTT_H__