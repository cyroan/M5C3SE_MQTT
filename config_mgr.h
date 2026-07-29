#ifndef CONFIG_MGR_H
#define CONFIG_MGR_H

#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>

// Pin Definitions
#define SD_SPI_SCK_PIN  36
#define SD_SPI_MISO_PIN 35
#define SD_SPI_MOSI_PIN 37
#define SD_SPI_CS_PIN   4

#define LAN_SPI_SCK_PIN  7
#define LAN_SPI_MISO_PIN 8
#define LAN_SPI_MOSI_PIN 6
#define LAN_CS_PIN       1
#define LAN_RST_PIN      0
#define LAN_INT_PIN      10

enum NetworkType { NET_WIFI, NET_LAN_DHCP, NET_LAN_STATIC };

#include <vector>

extern const char* wifiConfigFile;
extern const char* lanConfigFile;
extern const char* mqttConfigFile;
extern const char* netPrefFile;
extern const char* mqttListFile;

extern String mqttServer;
extern int mqttPort;
extern String mqttTopicSub;
extern String mqttTopicPub;
extern std::vector<String> mqttTopicList;

extern String storedOtaSsid;
extern String storedOtaPass;
extern String storedRunSsid;
extern String storedRunPass;

extern String lanIP;
extern String lanGW;
extern String lanMask;
extern String lanDNS;
extern NetworkType activeNet;
extern bool showSpeed;

extern bool sdAvailable;
extern byte mac[];

bool initSD();
void saveWiFiConfig(bool isOtaMode, const String& selectedSSID, const String& wifiPassword);
void loadWiFiConfig();
void saveLANConfig();
void loadLANConfig();
void saveMQTTConfig();
void loadMQTTConfig();
void saveNetPref();
void loadNetPref();
void saveGuiConfig();
void loadGuiConfig();
void loadMqttTopicList();

#endif // CONFIG_MGR_H
