#include "config_mgr.h"

const char* wifiConfigFile = "/wifi_config.txt";
const char* lanConfigFile = "/lan_config.txt";
const char* mqttConfigFile = "/mqtt_config.txt";
const char* netPrefFile = "/net_pref.txt";

String mqttServer = "mqtt.m5stack.com";
int mqttPort = 1883;
String mqttTopicSub = "Prowave/#";
String mqttTopicPub = "Pro/T";

String storedOtaSsid = "";
String storedOtaPass = "";
String storedRunSsid = "";
String storedRunPass = "";

String lanIP = "192.168.1.100";
String lanGW = "192.168.1.1";
String lanMask = "255.255.255.0";
String lanDNS = "1.1.1.1";
NetworkType activeNet = NET_WIFI;
bool showSpeed = true;

bool sdAvailable = false;
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x89};

bool initSD() {
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    sdAvailable = SD.begin(SD_SPI_CS_PIN, SPI, 25000000);
    return sdAvailable;
}

void saveWiFiConfig(bool isOtaMode, const String& selectedSSID, const String& wifiPassword) {
    if (!sdAvailable) return;
    File file = SD.open(wifiConfigFile, FILE_WRITE);
    if (file) {
        if (isOtaMode) {
            storedOtaSsid = selectedSSID;
            storedOtaPass = wifiPassword;
        } else {
            storedRunSsid = selectedSSID;
            storedRunPass = wifiPassword;
        }
        file.println(storedOtaSsid);
        file.println(storedOtaPass);
        file.println(storedRunSsid);
        file.println(storedRunPass);
        file.close();
    }
}

void loadWiFiConfig() {
    if (!sdAvailable || !SD.exists(wifiConfigFile)) return;
    File file = SD.open(wifiConfigFile, FILE_READ);
    if (file) {
        storedOtaSsid = file.readStringUntil('\n'); storedOtaSsid.trim();
        storedOtaPass = file.readStringUntil('\n'); storedOtaPass.trim();
        storedRunSsid = file.readStringUntil('\n'); storedRunSsid.trim();
        storedRunPass = file.readStringUntil('\n'); storedRunPass.trim();
        file.close();
    }
}

void saveLANConfig() {
    if (!sdAvailable) return;
    File file = SD.open(lanConfigFile, FILE_WRITE);
    if (file) {
        file.println(activeNet == NET_LAN_DHCP ? "1" : "0");
        file.println(lanIP);
        file.println(lanGW);
        file.println(lanMask);
        file.println(lanDNS);
        file.close();
    }
}

void loadLANConfig() {
    if (!sdAvailable || !SD.exists(lanConfigFile)) return;
    File file = SD.open(lanConfigFile, FILE_READ);
    if (file) {
        String dhStr = file.readStringUntil('\n'); dhStr.trim();
        if (dhStr == "1") activeNet = NET_LAN_DHCP; else activeNet = NET_LAN_STATIC;
        lanIP = file.readStringUntil('\n'); lanIP.trim();
        lanGW = file.readStringUntil('\n'); lanGW.trim();
        lanMask = file.readStringUntil('\n'); lanMask.trim();
        lanDNS = file.readStringUntil('\n'); lanDNS.trim();
        file.close();
    }
}

void saveMQTTConfig() {
    if (!sdAvailable) return;
    File file = SD.open(mqttConfigFile, FILE_WRITE);
    if (file) {
        file.println(mqttServer);
        file.println(String(mqttPort));
        file.println(mqttTopicSub);
        file.println(mqttTopicPub);
        file.close();
    }
}

void loadMQTTConfig() {
    if (!sdAvailable || !SD.exists(mqttConfigFile)) return;
    File file = SD.open(mqttConfigFile, FILE_READ);
    if (file) {
        mqttServer = file.readStringUntil('\n'); mqttServer.trim();
        String pStr = file.readStringUntil('\n'); pStr.trim();
        if (pStr != "") mqttPort = pStr.toInt();
        mqttTopicSub = file.readStringUntil('\n'); mqttTopicSub.trim();
        mqttTopicPub = file.readStringUntil('\n'); mqttTopicPub.trim();
        file.close();
    }
}

void saveNetPref() {
    if (!sdAvailable) return;
    File file = SD.open(netPrefFile, FILE_WRITE);
    if (file) {
        file.println(String((int)activeNet));
        file.close();
    }
}

void loadNetPref() {
    if (!sdAvailable || !SD.exists(netPrefFile)) return;
    File file = SD.open(netPrefFile, FILE_READ);
    if (file) {
        String s = file.readStringUntil('\n'); s.trim();
        if (s != "") activeNet = (NetworkType)s.toInt();
        file.close();
    }
}

void saveGuiConfig() {
    if (!sdAvailable) return;
    File file = SD.open("/gui_pref.txt", FILE_WRITE);
    if (file) {
        file.println(showSpeed ? "1" : "0");
        file.close();
    }
}

void loadGuiConfig() {
    if (!sdAvailable || !SD.exists("/gui_pref.txt")) return;
    File file = SD.open("/gui_pref.txt", FILE_READ);
    if (file) {
        String s = file.readStringUntil('\n'); s.trim();
        if (s != "") showSpeed = (s == "1");
        file.close();
    }
}
