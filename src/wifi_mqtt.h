#pragma once

#include <WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include "config.h"

extern WiFiClient mqttClient;
extern Adafruit_MQTT_Client mqtt;

// Publish feeds
extern Adafruit_MQTT_Publish pubTemp;
extern Adafruit_MQTT_Publish pubHumidity;
extern Adafruit_MQTT_Publish pubAirPct;
extern Adafruit_MQTT_Publish pubAirAlert;
extern Adafruit_MQTT_Publish pubFlow;
extern Adafruit_MQTT_Publish pubValveStatus;
extern Adafruit_MQTT_Publish pubPumpStatus;
extern bool anyPendingCommand;

// Functions
void connectWiFi();
void checkMQTT();
void readMQTTSubscriptions();