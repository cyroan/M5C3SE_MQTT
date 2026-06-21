#ifndef OTA_MGR_H
#define OTA_MGR_H

#include <Arduino.h>

extern const char* updateUrl;
extern const char* versionUrl;

void updateFirmware();

#endif // OTA_MGR_H
