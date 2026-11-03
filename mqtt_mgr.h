#ifndef MQTT_MGR_H
#define MQTT_MGR_H

#include <Arduino.h>
#include <PubSubClient.h>
#include "config_mgr.h"
#include "network_mgr.h"
#include "diag_mgr.h"

struct MqttMsg { 
    String topic; 
    String payload; 
    String timestamp; 
};

extern MqttMsg msgHistory[10];
extern int historyWriteIdx;
extern int historyViewIdx;
extern int historyCount;
extern unsigned long lastPublishTime;
extern PubSubClient mqttClient;

extern String latestSN;
extern String latestDataTime;
extern float latestRSSI;
extern float latestDB;
extern bool hasDbRssiData;

void initMqtt();
void reconnectMqtt();
void mqttLoop();
void publishUptime();
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool parseAdvantechPayload(const char* topic, byte* payload, unsigned int length);

#endif // MQTT_MGR_H
