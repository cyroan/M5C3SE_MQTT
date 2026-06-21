#ifndef NETWORK_MGR_H
#define NETWORK_MGR_H

#include <Arduino.h>
#include <WiFi.h>
#include <M5Module_LAN.h>
#include "config_mgr.h"

extern WiFiClient wifiClient;
extern EthernetClient ethClient;
extern M5Module_LAN LAN;

void startNetworkConnection(const String& selectedSSID, const String& wifiPassword);
bool isNetworkConnected();
String getLocalIPString();

#endif // NETWORK_MGR_H
