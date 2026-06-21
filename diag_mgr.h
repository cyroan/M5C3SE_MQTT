#ifndef DIAG_MGR_H
#define DIAG_MGR_H

#include <Arduino.h>
#include <ArduinoJson.h>

extern bool conditionActive[12];
extern bool alarmX, alarmY, alarmZ;
extern float addr0, addr1, addr2, addr3, addr4;
extern const char* conditionNames[12];

void resetDiagnosis();
bool parseMqttJsonPayload(const char* topic, byte* payload, unsigned int length);

#endif // DIAG_MGR_H
