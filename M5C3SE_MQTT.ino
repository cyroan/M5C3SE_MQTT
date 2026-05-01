#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <PubSubClient.h> 
#include <time.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <LittleFS.h>
#include <M5Module_LAN.h>
#include "version.h"

// --- Configuration ---
const char* updateUrl = "https://raw.githubusercontent.com/cyroan/M5C3SE_MQTT/master/M5C3SE_MQTT.ino.m5stack_cores3.bin";
const char* versionUrl = "https://raw.githubusercontent.com/cyroan/M5C3SE_MQTT/master/version.txt";
const char* wifiConfigFile = "/wifi_config.txt";
const char* lanConfigFile = "/lan_config.txt";

// --- SD Card Pins for CoreS3 ---
#define SD_SPI_SCK_PIN  36
#define SD_SPI_MISO_PIN 35
#define SD_SPI_MOSI_PIN 37
#define SD_SPI_CS_PIN   4

// --- LAN Module 13.2 Pins for CoreS3 ---
#define LAN_SPI_SCK_PIN  7
#define LAN_SPI_MISO_PIN 8
#define LAN_SPI_MOSI_PIN 6
#define LAN_CS_PIN       1
#define LAN_RST_PIN      0
#define LAN_INT_PIN      10

// --- MQTT Configuration ---
const char* mqttServer = "mqtt.m5stack.com";
const int mqttPort = 1883;
const char* mqttTopicSub = "Advantech/#";
const char* mqttTopicPub = "Advantech/T";
String lastMqttMsg = "Waiting for data...";
String lastMqttTopic = "None";

unsigned long lastPublishTime = 0; 
bool sdAvailable = false;

WiFiClient wifiClient;
EthernetClient ethClient;
PubSubClient mqttClient;

enum NetworkType { NET_WIFI, NET_LAN };
NetworkType activeNet = NET_WIFI;

enum State {
    STATE_BOOT,
    STATE_SELECT_NET_TYPE,
    STATE_SCAN_WIFI,
    STATE_SELECT_SSID,
    STATE_INPUT_PASSWORD,
    STATE_LAN_CONFIG,
    STATE_LAN_STATIC_INPUT,
    STATE_CONNECTING,
    STATE_OTA,
    STATE_RUNNING
};

State currentState = STATE_BOOT;
unsigned long stateTimer = 0;
String selectedSSID = "";
String wifiPassword = "";
String storedOtaSsid = "", storedOtaPass = "";
String storedRunSsid = "", storedRunPass = "";

// LAN Settings
bool lanUseDhcp = true;
String lanIP = "192.168.1.100";
String lanGW = "192.168.1.1";
String lanMask = "255.255.255.0";
String lanDNS = "1.1.1.1";
int lanInputIdx = 0; 

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x89};
M5Module_LAN LAN;

int scanCount = 0;
int selectedSsidIdx = 0;
int kbdPage = 0;
bool isOtaMode = false;

// --- Config Helpers ---
void saveWiFiConfig() {
    if (!sdAvailable) return;
    File file = SD.open(wifiConfigFile, FILE_WRITE);
    if (file) {
        if (isOtaMode) { storedOtaSsid = selectedSSID; storedOtaPass = wifiPassword; }
        else { storedRunSsid = selectedSSID; storedRunPass = wifiPassword; }
        file.println(storedOtaSsid); file.println(storedOtaPass);
        file.println(storedRunSsid); file.println(storedRunPass);
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
        file.println(lanUseDhcp ? "1" : "0");
        file.println(lanIP); file.println(lanGW);
        file.println(lanMask); file.println(lanDNS);
        file.close();
    }
}

void loadLANConfig() {
    if (!sdAvailable || !SD.exists(lanConfigFile)) return;
    File file = SD.open(lanConfigFile, FILE_READ);
    if (file) {
        String dhStr = file.readStringUntil('\n'); dhStr.trim();
        lanUseDhcp = (dhStr == "1");
        lanIP = file.readStringUntil('\n'); lanIP.trim();
        lanGW = file.readStringUntil('\n'); lanGW.trim();
        lanMask = file.readStringUntil('\n'); lanMask.trim();
        lanDNS = file.readStringUntil('\n'); lanDNS.trim();
        file.close();
    }
}

// Keyboard
const char* kbdMap0 = "ABCDEFGHIJKLMNOPQRSTUVW<>"; 
const char* kbdMap1 = "XYZ0123456789.-_ \x01\n";
const int KBD_COLS = 5, KBD_ROWS = 5, BTN_W = 60, BTN_H = 38, KBD_X = 10, KBD_Y = 45;

void drawButton(int x, int y, int w, int h, const char* label, uint32_t color, int textSize = 2) {
    M5.Display.fillRoundRect(x, y, w, h, 6, color);
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(textSize);
    M5.Display.drawCenterString(label, x + w / 2, y + h / 2 - (textSize * 4));
}

void drawKeyboard() {
    M5.Display.fillRect(0, KBD_Y - 5, 320, 240 - (KBD_Y - 5), BLACK);
    const char* map = (kbdPage == 0) ? kbdMap0 : kbdMap1;
    for (int i = 0; i < strlen(map); i++) {
        int r = i / KBD_COLS, c = i % KBD_COLS;
        int bx = KBD_X + c * (BTN_W + 2), by = KBD_Y + r * (BTN_H + 2);
        char key = map[i];
        if (key == '<') drawButton(bx, by, BTN_W, BTN_H, "BS", RED);
        else if (key == '>') drawButton(bx, by, BTN_W, BTN_H, "NEXT", ORANGE);
        else if (key == '\x01') drawButton(bx, by, BTN_W, BTN_H, "BACK", ORANGE);
        else if (key == '\n') drawButton(bx, by, BTN_W, BTN_H, "OK", GREEN);
        else { char lbl[2] = {key, 0}; drawButton(bx, by, BTN_W, BTN_H, lbl, BLUE); }
    }
}

void mqttCallback(char* topic, byte* payload, unsigned long length) {
    lastMqttTopic = String(topic); lastMqttMsg = "";
    for (int i = 0; i < length; i++) lastMqttMsg += (char)payload[i];
    auto dt = M5.Rtc.getDateTime();
    if (sdAvailable) {
        File file = SD.open("/RECEIVR.TXT", FILE_APPEND);
        if (file) {
            file.printf("[%04d-%02d-%02d %02d:%02d:%02d] T: %s | M: %s\n", dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes, dt.time.seconds, topic, lastMqttMsg.c_str());
            file.close();
        }
    }
    if (currentState == STATE_RUNNING) updateRunningUI();
}

void reconnectMqtt() {
    if (!mqttClient.connected()) {
        M5.Display.setCursor(10, 215); M5.Display.setTextSize(1); M5.Display.setTextColor(WHITE);
        M5.Display.print("MQTT Connecting...");
        String clientId = "M5-C3SE-" + String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str())) {
            mqttClient.subscribe(mqttTopicSub);
            M5.Display.fillRect(0, 210, 320, 30, DARKGREY); 
        }
    }
}

void updateClock() {
    static int ls = -1; auto dt = M5.Rtc.getDateTime();
    if (dt.time.seconds == ls) return; ls = dt.time.seconds;
    M5.Display.fillRect(200, 5, 115, 25, DARKGREY); 
    M5.Display.setTextColor(WHITE); M5.Display.setTextSize(2);
    M5.Display.setCursor(210, 10); M5.Display.printf("%02d:%02d:%02d", dt.time.hours, dt.time.minutes, dt.time.seconds);
}

void updateRunningUI() {
    M5.Display.fillRect(0, 40, 320, 160, BLACK);
    M5.Display.setTextColor(YELLOW); M5.Display.setTextSize(2); M5.Display.setCursor(10, 45); M5.Display.println("Last Topic:");
    M5.Display.setTextColor(WHITE); M5.Display.setCursor(10, 68); M5.Display.println(lastMqttTopic);
    M5.Display.drawLine(10, 95, 310, 95, DARKGREY);
    M5.Display.setTextColor(CYAN); M5.Display.setCursor(10, 105); M5.Display.println("Message:");
    M5.Display.setTextColor(WHITE); M5.Display.setCursor(10, 130); M5.Display.println(lastMqttMsg);
    M5.Display.fillRect(5, 210, 310, 25, DARKGREY); 
    M5.Display.setTextSize(1); M5.Display.setCursor(10, 215);
    M5.Display.setTextColor(sdAvailable ? GREEN : RED); M5.Display.printf("SD:%s | ", sdAvailable ? "OK" : "ERR");
    M5.Display.setTextColor(WHITE);
    if (activeNet == NET_WIFI) M5.Display.printf("WiFi:%s", WiFi.SSID().c_str());
    else M5.Display.printf("LAN:%s", Ethernet.localIP().toString().c_str());
    updateClock();
}

bool initSD() {
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    sdAvailable = SD.begin(SD_SPI_CS_PIN, SPI, 25000000);
    return sdAvailable;
}

void setup() {
    auto cfg = M5.config(); M5.begin(cfg);
    M5.Display.setRotation(1);
    initSD(); loadWiFiConfig(); loadLANConfig();
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    enterState(STATE_BOOT);
}

void drawSsidList() {
    M5.Display.fillScreen(BLACK); M5.Display.setTextSize(2); M5.Display.setCursor(10, 5);
    M5.Display.setTextColor(ORANGE); M5.Display.println("Select SSID:");
    int startY = 35;
    for (int i = 0; i < min(scanCount, 5); i++) {
        String ssid = WiFi.SSID(i);
        bool isStored = (isOtaMode && ssid == storedOtaSsid) || (!isOtaMode && ssid == storedRunSsid);
        if (i == selectedSsidIdx) { M5.Display.fillRect(0, startY + i * 30, 320, 28, BLUE); M5.Display.setTextColor(WHITE); }
        else M5.Display.setTextColor(isStored ? GREEN : LIGHTGREY);
        M5.Display.setCursor(15, startY + 5 + i * 30); M5.Display.printf("%s%s", ssid.c_str(), isStored ? " *" : "");
    }
    drawButton(5, 190, 95, 45, "UP", DARKGREY); drawButton(110, 190, 100, 45, "SELECT", GREEN); drawButton(220, 190, 95, 45, "DOWN", DARKGREY);
}

void updatePasswordDisplay() {
    M5.Display.fillRect(0, 0, 320, 40, DARKGREY); M5.Display.setCursor(5, 5);
    M5.Display.setTextSize(1); M5.Display.setTextColor(WHITE);
    M5.Display.printf("Net:%s | SSID:%s", activeNet == NET_WIFI ? "WiFi" : "LAN", selectedSSID.c_str());
    M5.Display.setCursor(5, 20); M5.Display.setTextSize(2); M5.Display.printf("PW:%s", wifiPassword.c_str());
}

void updateLanStaticDisplay() {
    M5.Display.fillRect(0, 0, 320, 45, DARKGREY);
    M5.Display.setTextSize(1); M5.Display.setCursor(5, 5);
    String lbls[] = {"IP:", "GW:", "MSK:", "DNS:"}; String vls[] = {lanIP, lanGW, lanMask, lanDNS};
    for(int i=0; i<4; i++) {
        M5.Display.setTextColor(i == lanInputIdx ? YELLOW : WHITE);
        M5.Display.printf("%s%s ", lbls[i].c_str(), vls[i].c_str());
    }
}

void updateFirmware() {
    M5.Display.fillScreen(BLACK); M5.Display.setCursor(0, 0); M5.Display.println("Checking for updates (WiFi)...");
    WiFiClientSecure sc; sc.setInsecure(); HTTPClient h;
    if (h.begin(sc, versionUrl)) {
        if (h.GET() == 200) {
            String nv = h.getString(); nv.trim();
            if (nv.equals(FIRMWARE_VERSION)) { M5.Display.println("Latest."); delay(2000); enterState(STATE_RUNNING); return; }
            h.end(); M5.Display.println("Updating...");
            httpUpdate.onProgress([](int c, int t){ M5.Display.fillRect(0,180,320,30,BLACK); M5.Display.setCursor(0,180); M5.Display.printf("Progress: %d%%", (c*100)/t); });
            httpUpdate.rebootOnUpdate(true); httpUpdate.update(sc, updateUrl);
        } else { M5.Display.println("Fail."); delay(2000); enterState(STATE_RUNNING); }
    }
}

void enterState(State ns) {
    currentState = ns; M5.Display.fillScreen(BLACK); stateTimer = millis();
    switch (ns) {
        case STATE_BOOT:
            M5.Display.setTextSize(2); M5.Display.drawCenterString("M5Stack CoreS3 SE", 160, 40);
            M5.Display.drawCenterString("Version: " FIRMWARE_VERSION, 160, 70);
            drawButton(20, 120, 130, 60, "UPDATE", BLUE); drawButton(170, 120, 130, 60, "RUN", GREEN);
            break;
        case STATE_SELECT_NET_TYPE:
            M5.Display.drawCenterString("Select Network", 160, 40);
            drawButton(20, 120, 130, 60, "WIFI", BLUE); drawButton(170, 120, 130, 60, "LAN", ORANGE);
            break;
        case STATE_SCAN_WIFI:
            M5.Display.drawCenterString("Scanning...", 160, 110); WiFi.mode(WIFI_STA); scanCount = WiFi.scanNetworks();
            selectedSsidIdx = 0; enterState(STATE_SELECT_SSID);
            break;
        case STATE_SELECT_SSID: drawSsidList(); break;
        case STATE_INPUT_PASSWORD:
            if (selectedSSID == storedOtaSsid || selectedSSID == storedRunSsid) wifiPassword = (isOtaMode ? storedOtaPass : storedRunPass);
            kbdPage = 0; updatePasswordDisplay(); drawKeyboard();
            break;
        case STATE_LAN_CONFIG:
            M5.Display.drawCenterString("LAN Config", 160, 40);
            drawButton(20, 120, 130, 60, "DHCP", lanUseDhcp ? GREEN : BLUE); drawButton(170, 120, 130, 60, "STATIC", !lanUseDhcp ? GREEN : BLUE);
            drawButton(110, 190, 100, 40, "NEXT", ORANGE);
            break;
        case STATE_LAN_STATIC_INPUT: kbdPage = 1; lanInputIdx = 0; updateLanStaticDisplay(); drawKeyboard(); break;
        case STATE_CONNECTING:
            M5.Display.drawCenterString("Connecting...", 160, 110);
            if (activeNet == NET_WIFI) { mqttClient.setClient(wifiClient); WiFi.begin(selectedSSID.c_str(), wifiPassword.c_str()); }
            else {
                mqttClient.setClient(ethClient); SPI.begin(LAN_SPI_SCK_PIN, LAN_SPI_MISO_PIN, LAN_SPI_MOSI_PIN, -1);
                LAN.setResetPin(LAN_RST_PIN); LAN.reset(); LAN.init(LAN_CS_PIN);
                if (lanUseDhcp) Ethernet.begin(mac);
                else { IPAddress ip, gw, msk, dns; ip.fromString(lanIP); gw.fromString(lanGW); msk.fromString(lanMask); dns.fromString(lanDNS); Ethernet.begin(mac, ip, dns, gw, msk); }
            }
            break;
        case STATE_OTA: updateFirmware(); break;
        case STATE_RUNNING:
            M5.Display.fillScreen(DARKGREY); M5.Display.drawCenterString("MQTT MONITOR", 160, 10);
            if ((activeNet == NET_WIFI && WiFi.status() != WL_CONNECTED) || (activeNet == NET_LAN && Ethernet.localIP()[0] == 0)) {
                M5.Display.drawCenterString("Disconnected!", 160, 100);
            } else updateRunningUI();
            break;
    }
}

void handleTouch() {
    auto detail = M5.Touch.getDetail(); if (!detail.isPressed()) return;
    int x = detail.x, y = detail.y;
    switch (currentState) {
        case STATE_BOOT:
            if (y > 120 && y < 180) { if (x < 150) { isOtaMode = true; enterState(STATE_SELECT_NET_TYPE); } else { isOtaMode = false; enterState(STATE_SELECT_NET_TYPE); } }
            break;
        case STATE_SELECT_NET_TYPE:
            if (y > 120 && y < 180) { if (x < 150) { activeNet = NET_WIFI; enterState(STATE_SCAN_WIFI); } else { activeNet = NET_LAN; enterState(STATE_LAN_CONFIG); } }
            break;
        case STATE_LAN_CONFIG:
            if (y > 120 && y < 180) { lanUseDhcp = (x < 150); enterState(STATE_LAN_CONFIG); }
            else if (y > 190 && x > 110 && x < 210) { if (lanUseDhcp) enterState(STATE_CONNECTING); else enterState(STATE_LAN_STATIC_INPUT); }
            break;
        case STATE_LAN_STATIC_INPUT:
            if (x >= KBD_X && y >= KBD_Y) {
                int c = (x-KBD_X)/(BTN_W+2), r = (y-KBD_Y)/(BTN_H+2), idx = r*KBD_COLS+c; const char* map = (kbdPage==0?kbdMap0:kbdMap1);
                if (idx < strlen(map)) {
                    char k = map[idx]; String* t = (lanInputIdx==0?&lanIP : lanInputIdx==1?&lanGW : lanInputIdx==2?&lanMask : &lanDNS);
                    if (k == '<') { if (t->length()>0) t->remove(t->length()-1); }
                    else if (k == '>') lanInputIdx = (lanInputIdx+1)%4;
                    else if (k == '\x01') { kbdPage = 1-kbdPage; drawKeyboard(); }
                    else if (k == '\n') enterState(STATE_CONNECTING);
                    else if ((k >= '0' && k <= '9') || k == '.') *t += k;
                    updateLanStaticDisplay(); delay(200);
                }
            }
            break;
        case STATE_SELECT_SSID:
            if (y > 180) {
                if (x < 100) { if (selectedSsidIdx > 0) selectedSsidIdx--; drawSsidList(); }
                else if (x > 220) { if (selectedSsidIdx < scanCount-1) selectedSsidIdx++; drawSsidList(); }
                else if (x > 110 && x < 210) { selectedSSID = WiFi.SSID(selectedSsidIdx); wifiPassword = ""; enterState(STATE_INPUT_PASSWORD); }
                delay(200);
            }
            break;
        case STATE_INPUT_PASSWORD:
            if (x >= KBD_X && y >= KBD_Y) {
                int c = (x-KBD_X)/(BTN_W+2), r = (y-KBD_Y)/(BTN_H+2), idx = r*KBD_COLS+c; const char* map = (kbdPage==0?kbdMap0:kbdMap1);
                if (idx < strlen(map)) {
                    char k = map[idx];
                    if (k == '<') { if (wifiPassword.length()>0) wifiPassword.remove(wifiPassword.length()-1); }
                    else if (k == '>') { kbdPage = 1; drawKeyboard(); }
                    else if (k == '\x01') { kbdPage = 0; drawKeyboard(); }
                    else if (k == '\n') enterState(STATE_CONNECTING);
                    else if (k == '_') wifiPassword += " "; else wifiPassword += k;
                    updatePasswordDisplay(); delay(200);
                }
            }
            break;
        case STATE_RUNNING:
            if ((activeNet == NET_WIFI && WiFi.status() != WL_CONNECTED) || (activeNet == NET_LAN && Ethernet.localIP()[0] == 0)) enterState(STATE_SELECT_NET_TYPE);
            else if (y > 200) enterState(STATE_BOOT);
            break;
    }
}

void loop() {
    M5.update(); if (M5.Touch.getCount() > 0) handleTouch();
    if (currentState == STATE_BOOT) {
        unsigned long e = (millis() - stateTimer) / 1000;
        if (e >= 30) { isOtaMode = false; enterState(STATE_RUNNING); }
        else { M5.Display.setCursor(100, 200); M5.Display.setTextSize(2); M5.Display.printf("Auto:%ds ", 30 - (int)e); }
    }
    if (currentState == STATE_CONNECTING) {
        bool c = (activeNet == NET_WIFI ? WiFi.status() == WL_CONNECTED : Ethernet.localIP()[0] != 0);
        if (c) {
            if (activeNet == NET_WIFI) saveWiFiConfig(); else saveLANConfig();
            configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
            struct tm ti; if (getLocalTime(&ti)) M5.Rtc.setDateTime(&ti);
            if (isOtaMode) enterState(STATE_OTA); else enterState(STATE_RUNNING);
        } else if (millis() - stateTimer > 15000) {
            M5.Display.fillScreen(RED); M5.Display.drawCenterString("Fail", 160, 110); delay(2000);
            enterState(activeNet == NET_WIFI ? STATE_INPUT_PASSWORD : STATE_LAN_CONFIG);
        }
    }
    if (currentState == STATE_RUNNING) {
        updateClock(); if (!mqttClient.connected()) reconnectMqtt(); mqttClient.loop();
        if (millis() - lastPublishTime > 10000) {
            lastPublishTime = millis();
            if (mqttClient.connected()) { char ts[32]; sprintf(ts, "Uptime: %lu", millis()/1000); mqttClient.publish(mqttTopicPub, ts); }
        }
    }
    delay(10);
}
