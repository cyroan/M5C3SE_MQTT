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

// --- Configuration Files ---
const char* updateUrl = "https://raw.githubusercontent.com/cyroan/M5C3SE_MQTT/master/M5C3SE_MQTT.ino.m5stack_cores3.bin";
const char* versionUrl = "https://raw.githubusercontent.com/cyroan/M5C3SE_MQTT/master/version.txt";
const char* wifiConfigFile = "/wifi_config.txt";
const char* lanConfigFile = "/lan_config.txt";
const char* mqttConfigFile = "/mqtt_config.txt";
const char* netPrefFile = "/net_pref.txt";

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

// --- MQTT Dynamic Configuration ---
String mqttServer = "mqtt.m5stack.com";
int mqttPort = 1883;
String mqttTopicSub = "Advantech/#";
String mqttTopicPub = "Advantech/T";

struct MqttMsg { String topic; String payload; String timestamp; };
MqttMsg msgHistory[10];
int historyWriteIdx = 0, historyViewIdx = -1, historyCount = 0;
unsigned long lastPublishTime = 0; 
bool sdAvailable = false;

WiFiClient wifiClient;
EthernetClient ethClient;
PubSubClient mqttClient;

enum NetworkType { NET_WIFI, NET_LAN_DHCP, NET_LAN_STATIC };
NetworkType activeNet = NET_WIFI;

enum State {
    STATE_BOOT, STATE_SELECT_NET_TYPE, STATE_SCAN_WIFI, STATE_SELECT_SSID,
    STATE_INPUT_PASSWORD, STATE_LAN_STATIC_INPUT, STATE_CONNECTING, STATE_OTA, STATE_RUNNING,
    STATE_SET_RTC, STATE_SET_MQTT
};

State currentState = STATE_BOOT;
unsigned long stateTimer = 0;
String selectedSSID = "", wifiPassword = "";
String storedOtaSsid = "", storedOtaPass = "";
String storedRunSsid = "", storedRunPass = "";
String lanIP = "192.168.1.100", lanGW = "192.168.1.1", lanMask = "255.255.255.0", lanDNS = "1.1.1.1";
int lanInputIdx = 0; 
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x89};
M5Module_LAN LAN;
int scanCount = 0, selectedSsidIdx = 0, kbdPage = 0;
bool isOtaMode = false;
int scrollOffset = 0;
M5Canvas msgCanvas(&M5.Display);

// RTC/MQTT Setting variables
int rtcY, rtcM, rtcD, rtcH, rtcMin, rtcSetIdx = 0; 
int mqttSetStep = 0; 

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
        file.println(activeNet == NET_LAN_DHCP ? "1" : "0");
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
        file.println(mqttServer); file.println(String(mqttPort));
        file.println(mqttTopicSub); file.println(mqttTopicPub);
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

// --- Keyboard Logic ---
const char* kbdMap0 = "ABCDEFGHIJKLMNOPQRSTUVW<\x01"; // Slot 23=<, 24=\x01
const char* kbdMap1 = "XYZ0123456789.-_/@: \x01\x02\n"; // Slot 21= , 22=\x01, 23=\x02, 24=\n
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
    for (int i = 0; i < 25; i++) {
        int r = i / KBD_COLS, c = i % KBD_COLS;
        int bx = KBD_X + c * (BTN_W + 2), by = KBD_Y + r * (BTN_H + 2);
        if (i >= strlen(map)) break;
        char key = map[i];
        if (key == '<') drawButton(bx, by, BTN_W, BTN_H, "BS", RED, 2); // Text Size 2
        else if (key == '\x01') drawButton(bx, by, BTN_W, BTN_H, "KB", ORANGE, 2); // Text Size 2, Label KB
        else if (key == '\x02') drawButton(bx, by, BTN_W, BTN_H, "HOME", PURPLE, 2); // Text Size 2
        else if (key == '\n') drawButton(bx, by, BTN_W, BTN_H, "OK", GREEN, 2); // Text Size 2
        else { 
            char lbl[2] = {key, 0}; 
            drawButton(bx, by, BTN_W, BTN_H, key == ' ' ? "SPC" : lbl, BLUE, 2); 
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
    String dispTopic = "None", dispMsg = "Waiting...", dispTime = "";
    if (historyCount > 0) {
        int idx = (historyViewIdx == -1) ? (historyWriteIdx + 9) % 10 : historyViewIdx;
        dispTopic = msgHistory[idx].topic; dispMsg = msgHistory[idx].payload; dispTime = msgHistory[idx].timestamp;
    }
    M5.Display.setTextColor(YELLOW); M5.Display.setTextSize(2); M5.Display.setCursor(10, 45); M5.Display.print(dispTopic);
    if (historyViewIdx != -1) { M5.Display.setTextColor(ORANGE); M5.Display.printf(" [%d]", ((historyWriteIdx - 1 - historyViewIdx + 10) % 10) + 1); }
    M5.Display.drawLine(10, 70, 310, 70, DARKGREY);
    
    // Message Display Area (Scrolled)
    int msgAreaY = 105, msgAreaH = 95; 
    M5.Display.setTextColor(CYAN); M5.Display.setTextSize(2); M5.Display.setCursor(10, 80); M5.Display.print("Msg: ");
    if (dispTime != "") { M5.Display.setTextColor(LIGHTGREY); M5.Display.print("(" + dispTime + ")"); }
    
    // Draw Message with Scrolling using ClipRect
    msgCanvas.createSprite(270, 1500); 
    msgCanvas.fillSprite(BLACK);
    msgCanvas.setTextColor(WHITE);
    msgCanvas.setTextSize(2);
    msgCanvas.setCursor(0, 0);
    msgCanvas.println(dispMsg);
    
    int contentH = msgCanvas.getCursorY();
    if (scrollOffset < 0) scrollOffset = 0;
    if (contentH > msgAreaH && scrollOffset > contentH - msgAreaH) scrollOffset = contentH - msgAreaH;
    if (contentH <= msgAreaH) scrollOffset = 0;

    M5.Display.setClipRect(10, msgAreaY, 270, msgAreaH);
    msgCanvas.pushSprite(10, msgAreaY - scrollOffset);
    M5.Display.clearClipRect();
    msgCanvas.deleteSprite();

    // Scroll Buttons & Bar
    if (contentH > msgAreaH) {
        drawButton(285, 105, 30, 45, "^", BLUE, 1); 
        drawButton(285, 155, 30, 45, "v", BLUE, 1); 
        int barH = (msgAreaH * msgAreaH) / contentH;
        if (barH < 5) barH = 5;
        int barY = msgAreaY + (scrollOffset * (msgAreaH - barH)) / (contentH - msgAreaH);
        M5.Display.fillRect(280, msgAreaY, 3, msgAreaH, DARKGREY);
        M5.Display.fillRect(280, barY, 3, barH, WHITE);
    }

    M5.Display.fillRect(5, 205, 310, 32, DARKGREY); 
    M5.Display.setTextSize(1); M5.Display.setTextColor(WHITE); M5.Display.setCursor(10, 215); M5.Display.print("SD:");
    M5.Display.fillCircle(35, 220, 5, sdAvailable ? GREEN : RED);
    M5.Display.setCursor(50, 215);
    if (activeNet == NET_WIFI) M5.Display.printf("WiFi:%s", WiFi.SSID().c_str());
    else M5.Display.printf("LAN:%s", Ethernet.localIP().toString().c_str());
    M5.Display.setCursor(160, 215); M5.Display.setTextColor(LIGHTGREY); M5.Display.print("[HOME]");
    drawButton(220, 207, 45, 28, "UP", historyCount > 1 ? BLUE : LIGHTGREY, 1);
    drawButton(270, 207, 45, 28, "DN", historyCount > 1 ? BLUE : LIGHTGREY, 1);
    updateClock();
}

void mqttCallback(char* topic, byte* payload, unsigned long length) {
    Serial.printf("MQTT Received [%s] Length: %u\n", topic, length);
    String pl = ""; 
    for (int i = 0; i < length; i++) {
        pl += (char)payload[i];
        if (i > 2000) break; // Safety cap for UI display
    }
    auto dt = M5.Rtc.getDateTime();
    char ts[32]; sprintf(ts, "%02d:%02d:%02d", dt.time.hours, dt.time.minutes, dt.time.seconds);
    msgHistory[historyWriteIdx] = {String(topic), pl, String(ts)};
    historyWriteIdx = (historyWriteIdx + 1) % 10; if (historyCount < 10) historyCount++;
    if (sdAvailable) {
        File file = SD.open("/RECEIVR.TXT", FILE_APPEND);
        if (file) { file.printf("[%04d-%02d-%02d %s] T: %s | M: %s\n", dt.date.year, dt.date.month, dt.date.date, ts, topic, pl.c_str()); file.close(); }
    }
    if (currentState == STATE_RUNNING) updateRunningUI();
}

void reconnectMqtt() {
    bool netReady = (activeNet == NET_WIFI ? WiFi.status() == WL_CONNECTED : (Ethernet.localIP()[0] != 0));
    if (!netReady) return;
    if (!mqttClient.connected()) {
        M5.Display.setCursor(10, 195); M5.Display.setTextSize(1); M5.Display.setTextColor(WHITE);
        M5.Display.print("MQTT Reconnecting...");
        Serial.printf("Attempting MQTT connection to %s:%d...\n", mqttServer.c_str(), mqttPort);
        String clientId = "M5-C3SE-" + String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str())) { 
            Serial.println("Connected to MQTT Broker.");
            bool subOk = mqttClient.subscribe(mqttTopicSub.c_str()); 
            Serial.printf("Subscribe to [%s] %s\n", mqttTopicSub.c_str(), subOk ? "SUCCESS" : "FAILED");
            M5.Display.fillRect(0, 190, 320, 15, BLACK); 
        } else {
            Serial.printf("MQTT Connect Failed, rc=%d\n", mqttClient.state());
        }
    }
}

bool initSD() { SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN); sdAvailable = SD.begin(SD_SPI_CS_PIN, SPI, 25000000); return sdAvailable; }

void setup() {
    auto cfg = M5.config(); M5.begin(cfg);
    M5.Display.setRotation(1);
    delay(200); // Give SD card a moment
    initSD(); loadWiFiConfig(); loadLANConfig(); loadMQTTConfig(); loadNetPref();
    mqttClient.setServer(mqttServer.c_str(), mqttPort);
    mqttClient.setCallback(mqttCallback);
    enterState(STATE_BOOT);
}

void drawSsidList() {
    M5.Display.fillScreen(BLACK); M5.Display.setTextSize(2); M5.Display.setCursor(10, 5);
    M5.Display.setTextColor(ORANGE); M5.Display.println("Select SSID:");
    int startY = 35, itemsPerPage = 5; int startIndex = (selectedSsidIdx / itemsPerPage) * itemsPerPage;
    for (int i = 0; i < itemsPerPage; i++) {
        int currentIdx = startIndex + i; if (currentIdx >= scanCount) break;
        String ssid = WiFi.SSID(currentIdx); bool isStored = (isOtaMode && ssid == storedOtaSsid) || (!isOtaMode && ssid == storedRunSsid);
        if (currentIdx == selectedSsidIdx) { M5.Display.fillRect(0, startY + i * 30, 320, 28, BLUE); M5.Display.setTextColor(WHITE); }
        else M5.Display.setTextColor(isStored ? GREEN : LIGHTGREY);
        M5.Display.setCursor(15, startY + 5 + i * 30); M5.Display.printf("%d. %s%s", currentIdx + 1, ssid.c_str(), isStored ? " *" : "");
    }
    M5.Display.setTextSize(1); M5.Display.setTextColor(DARKGREY); M5.Display.setCursor(250, 10); M5.Display.printf("P.%d/%d", (startIndex/itemsPerPage)+1, (scanCount+itemsPerPage-1)/itemsPerPage);
    drawButton(5, 190, 95, 45, "UP", DARKGREY); drawButton(110, 190, 100, 45, "SELECT", GREEN); drawButton(220, 190, 95, 45, "DOWN", DARKGREY);
}

void updatePasswordDisplay() {
    M5.Display.fillRect(0, 0, 320, 40, DARKGREY); M5.Display.setCursor(5, 5);
    M5.Display.setTextSize(1); M5.Display.setTextColor(WHITE);
    M5.Display.printf("Mode:%s | SSID:%s", isOtaMode ? "OTA" : "RUN", selectedSSID.c_str()); M5.Display.setCursor(5, 20); M5.Display.setTextSize(2); M5.Display.printf("PW:%s", wifiPassword.c_str());
}

void updateLanStaticDisplay() {
    M5.Display.fillRect(0, 0, 320, 45, DARKGREY); M5.Display.setTextSize(1); M5.Display.setCursor(5, 5);
    String lbls[] = {"IP:", "GW:", "MSK:", "DNS:"}; String vls[] = {lanIP, lanGW, lanMask, lanDNS};
    for(int i=0; i<4; i++) { M5.Display.setTextColor(i == lanInputIdx ? YELLOW : WHITE); M5.Display.printf("%s%s ", lbls[i].c_str(), vls[i].c_str()); }
}

void drawRtcSetting() {
    M5.Display.fillScreen(BLACK); M5.Display.setTextSize(2); M5.Display.setTextColor(ORANGE); M5.Display.drawCenterString("Set RTC Time", 160, 20);
    String lbls[] = {"Y", "M", "D", "H", "Min"}; int vls[] = {rtcY, rtcM, rtcD, rtcH, rtcMin};
    for(int i=0; i<5; i++) {
        M5.Display.setTextColor(i == rtcSetIdx ? YELLOW : WHITE);
        M5.Display.drawCenterString(String(vls[i]).c_str(), 40 + i * 60, 75);
        M5.Display.setTextSize(1); M5.Display.drawCenterString(lbls[i].c_str(), 40 + i * 60, 105); M5.Display.setTextSize(2);
    }
    drawButton(10, 140, 95, 45, "+", BLUE, 2);  
    drawButton(112, 140, 95, 45, "-", RED, 2);   
    drawButton(215, 140, 95, 45, "Next", ORANGE, 2); 
    drawButton(110, 190, 100, 45, "SAVE", GREEN);
}

void updateMqttStepDisplay() {
    M5.Display.fillRect(0, 0, 320, 45, DARKGREY); M5.Display.setTextSize(1); M5.Display.setTextColor(WHITE);
    String title = ""; String val = "";
    if (mqttSetStep == 0) { title = "STEP 1: SERVER"; val = mqttServer; }
    else if (mqttSetStep == 1) { title = "STEP 2: PORT"; val = String(mqttPort); }
    else if (mqttSetStep == 2) { title = "STEP 3: SUB TOPIC"; val = mqttTopicSub; }
    else { title = "STEP 4: PUB TOPIC"; val = mqttTopicPub; }
    M5.Display.drawCenterString(title, 160, 5);
    M5.Display.setTextSize(2); M5.Display.setTextColor(YELLOW);
    M5.Display.drawCenterString(val, 160, 20);
}

void updateFirmware() {
    M5.Display.fillScreen(BLACK); M5.Display.setCursor(0, 0); M5.Display.println("Checking Updates (WiFi)...");
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
            M5.Display.setTextSize(2); M5.Display.drawCenterString("M5Stack CoreS3 SE", 160, 30);
            M5.Display.setTextColor(YELLOW); M5.Display.drawCenterString("Version: " FIRMWARE_VERSION, 160, 55);
            M5.Display.setTextColor(CYAN);
            { auto dt = M5.Rtc.getDateTime(); M5.Display.drawCenterString(String(dt.date.year)+"/"+String(dt.date.month)+"/"+String(dt.date.date)+" "+String(dt.time.hours)+":"+String(dt.time.minutes), 160, 85); }
            M5.Display.setTextColor(WHITE);
            drawButton(10, 125, 145, 50, "UPDATE", BLUE); drawButton(165, 125, 145, 50, "RUN", GREEN);
            drawButton(10, 185, 145, 45, "SET RTC", ORANGE); drawButton(165, 185, 145, 45, "SET MQTT", PURPLE);
            break;
        case STATE_SELECT_NET_TYPE:
            M5.Display.drawCenterString("Select Connection", 160, 20);
            drawButton(20, 60, 280, 50, "WIFI", BLUE); drawButton(20, 120, 280, 50, "LAN (DHCP)", GREEN); drawButton(20, 180, 280, 50, "LAN (STATIC)", ORANGE);
            break;
        case STATE_SCAN_WIFI: M5.Display.drawCenterString("Scanning...", 160, 110); WiFi.mode(WIFI_STA); scanCount = WiFi.scanNetworks(); selectedSsidIdx = 0; enterState(STATE_SELECT_SSID); break;
        case STATE_SELECT_SSID: drawSsidList(); break;
        case STATE_INPUT_PASSWORD: if (selectedSSID == storedOtaSsid || selectedSSID == storedRunSsid) wifiPassword = (isOtaMode ? storedOtaPass : storedRunPass); kbdPage = 0; updatePasswordDisplay(); drawKeyboard(); break;
        case STATE_LAN_STATIC_INPUT: kbdPage = 1; lanInputIdx = 0; updateLanStaticDisplay(); drawKeyboard(); break;
        case STATE_SET_MQTT: kbdPage = 0; mqttSetStep = 0; updateMqttStepDisplay(); drawKeyboard(); break;
        case STATE_CONNECTING:
            M5.Display.drawCenterString("Connecting...", 160, 60);
            M5.Display.setTextSize(1); M5.Display.setTextColor(YELLOW);
            {
                String netMode = (activeNet == NET_WIFI) ? "WIFI" : (activeNet == NET_LAN_DHCP ? "LAN (DHCP)" : "LAN (STATIC)");
                M5.Display.drawCenterString("Mode: " + netMode, 160, 90);
                if (activeNet == NET_WIFI) {
                    M5.Display.drawCenterString("SSID: " + selectedSSID, 160, 110);
                    M5.Display.drawCenterString("PASS: " + wifiPassword, 160, 130);
                } else if (activeNet == NET_LAN_STATIC) {
                    M5.Display.drawCenterString("IP: " + lanIP, 160, 110);
                }
            }
            M5.Display.setTextSize(2); M5.Display.setTextColor(WHITE);

            mqttClient.setServer(mqttServer.c_str(), mqttPort);
            mqttClient.setBufferSize(2048); // Increase buffer for 1KB+ payloads
            if (activeNet == NET_WIFI) { 
                mqttClient.setClient(wifiClient); 
                WiFi.mode(WIFI_STA);
                WiFi.begin(selectedSSID.c_str(), wifiPassword.c_str()); 
            }
            else {
                mqttClient.setClient(ethClient); 
                SPI.begin(LAN_SPI_SCK_PIN, LAN_SPI_MISO_PIN, LAN_SPI_MOSI_PIN, -1);
                LAN.setResetPin(LAN_RST_PIN); LAN.reset(); LAN.init(LAN_CS_PIN);
                if (activeNet == NET_LAN_DHCP) Ethernet.begin(mac);
                else { IPAddress ip, gw, msk, dns; ip.fromString(lanIP); gw.fromString(lanGW); msk.fromString(lanMask); dns.fromString(lanDNS); Ethernet.begin(mac, ip, dns, gw, msk); }
            }
            break;
        case STATE_OTA: updateFirmware(); break;
        case STATE_RUNNING:
            historyViewIdx = -1; scrollOffset = 0; M5.Display.fillScreen(DARKGREY); M5.Display.setTextColor(WHITE); M5.Display.setTextSize(2);
            M5.Display.setCursor(10, 10); M5.Display.print("MQTT MONITOR"); M5.Display.drawLine(0, 35, 320, 35, WHITE);
            if ((activeNet == NET_WIFI && WiFi.status() != WL_CONNECTED) || (activeNet != NET_WIFI && Ethernet.localIP()[0] == 0)) { 
                M5.Display.drawCenterString("Disconnected!", 160, 100); 
                Serial.println("Network Disconnected!");
            } else {
                updateRunningUI();
                Serial.printf("Running... Network OK. IP: %s\n", activeNet == NET_WIFI ? WiFi.localIP().toString().c_str() : Ethernet.localIP().toString().c_str());
            }
            break;
        case STATE_SET_RTC:
            { auto dt = M5.Rtc.getDateTime(); rtcY = dt.date.year; rtcM = dt.date.month; rtcD = dt.date.date; rtcH = dt.time.hours; rtcMin = dt.time.minutes; rtcSetIdx = 0; }
            drawRtcSetting(); break;
    }
    delay(300); 
}

void handleTouch() {
    auto detail = M5.Touch.getDetail(); if (!detail.isPressed()) return;
    int x = detail.x, y = detail.y;
    switch (currentState) {
        case STATE_BOOT:
            if (y > 125 && y < 175) { if (x < 160) { isOtaMode = true; activeNet = NET_WIFI; enterState(STATE_SCAN_WIFI); } else { isOtaMode = false; enterState(STATE_SELECT_NET_TYPE); } }
            else if (y > 185) { if (x < 160) enterState(STATE_SET_RTC); else enterState(STATE_SET_MQTT); }
            break;
        case STATE_SELECT_NET_TYPE:
            if (x > 20 && x < 300) {
                if (y > 60 && y < 110) { activeNet = NET_WIFI; enterState(STATE_SCAN_WIFI); }
                else if (y > 120 && y < 170) { activeNet = NET_LAN_DHCP; enterState(STATE_CONNECTING); }
                else if (y > 180 && y < 230) { activeNet = NET_LAN_STATIC; enterState(STATE_LAN_STATIC_INPUT); }
            }
            break;
        case STATE_SET_RTC:
            if (y > 140 && y < 185) {
                if (x < 105) { // +
                    if (rtcSetIdx == 0) rtcY++; else if (rtcSetIdx == 1) rtcM = (rtcM % 12) + 1;
                    else if (rtcSetIdx == 2) rtcD = (rtcD % 31) + 1; else if (rtcSetIdx == 3) rtcH = (rtcH + 1) % 24;
                    else rtcMin = (rtcMin + 1) % 60;
                    drawRtcSetting(); delay(150);
                } else if (x > 105 && x < 210) { // -
                    if (rtcSetIdx == 0) rtcY--; else if (rtcSetIdx == 1) rtcM = (rtcM > 1) ? rtcM - 1 : 12;
                    else if (rtcSetIdx == 2) rtcD = (rtcD > 1) ? rtcD - 1 : 31; else if (rtcSetIdx == 3) rtcH = (rtcH > 0) ? rtcH - 1 : 23;
                    else rtcMin = (rtcMin > 0) ? rtcMin - 1 : 59;
                    drawRtcSetting(); delay(150);
                } else { rtcSetIdx = (rtcSetIdx + 1) % 5; drawRtcSetting(); delay(500); }
            } else if (y > 190 && x > 110 && x < 210) { // SAVE
                m5::rtc_datetime_t dt; dt.date.year = rtcY; dt.date.month = rtcM; dt.date.date = rtcD; dt.time.hours = rtcH; dt.time.minutes = rtcMin; dt.time.seconds = 0;
                M5.Rtc.setDateTime(dt); enterState(STATE_BOOT);
            }
            break;
        case STATE_SET_MQTT:
            if (x >= KBD_X && y >= KBD_Y) {
                int c = (x-KBD_X)/(BTN_W+2), r = (y-KBD_Y)/(BTN_H+2), idx = r*KBD_COLS+c; const char* map = (kbdPage==0?kbdMap0:kbdMap1);
                if (idx < strlen(map)) {
                    char k = map[idx]; String* t;
                    if (mqttSetStep == 0) t = &mqttServer; 
                    else if (mqttSetStep == 1) { static String pS; pS = String(mqttPort); t = &pS; }
                    else if (mqttSetStep == 2) t = &mqttTopicSub; else t = &mqttTopicPub;

                    if (k == '<') { if (t->length() > 0) t->remove(t->length() - 1); else if (mqttSetStep == 0) enterState(STATE_BOOT); }
                    else if (k == '\x01') { kbdPage = 1 - kbdPage; drawKeyboard(); } // KB PAGE TOGGLE
                    else if (k == '\x02') { enterState(STATE_BOOT); } // HOME EXIT
                    else if (k == '\n') { // OK NEXT STEP
                        if (mqttSetStep == 1) mqttPort = t->toInt();
                        if (mqttSetStep < 3) { mqttSetStep++; updateMqttStepDisplay(); drawKeyboard(); delay(500); }
                        else { saveMQTTConfig(); enterState(STATE_BOOT); }
                    }
                    else { 
                        if (k == '_') *t += " "; else *t += k; 
                        if (mqttSetStep == 1) mqttPort = t->toInt();
                        updateMqttStepDisplay(); delay(150);
                    }
                }
            }
            break;
        case STATE_LAN_STATIC_INPUT:
            if (x >= KBD_X && y >= KBD_Y) {
                int c = (x-KBD_X)/(BTN_W+2), r = (y-KBD_Y)/(BTN_H+2), idx = r*KBD_COLS+c; const char* map = (kbdPage==0?kbdMap0:kbdMap1);
                if (idx < strlen(map)) {
                    char k = map[idx]; String* t = (lanInputIdx==0?&lanIP : lanInputIdx==1?&lanGW : lanInputIdx==2?&lanMask : &lanDNS);
                    if (k == '<') { if (t->length()>0) t->remove(t->length()-1); }
                    else if (k == '\x01') { kbdPage = 1-kbdPage; drawKeyboard(); }
                    else if (k == '\n') enterState(STATE_CONNECTING);
                    else if ((k >= '0' && k <= '9') || k == '.') *t += k;
                    updateLanStaticDisplay(); delay(150);
                }
            }
            break;
        case STATE_SELECT_SSID:
            if (y > 180) {
                if (x < 100) { if (selectedSsidIdx > 0) selectedSsidIdx--; drawSsidList(); }
                else if (x > 220) { if (selectedSsidIdx < scanCount-1) selectedSsidIdx++; drawSsidList(); }
                else if (x > 110 && x < 210) { selectedSSID = WiFi.SSID(selectedSsidIdx); wifiPassword = ""; enterState(STATE_INPUT_PASSWORD); }
                delay(150);
            }
            break;
        case STATE_INPUT_PASSWORD:
            if (x >= KBD_X && y >= KBD_Y) {
                int c = (x-KBD_X)/(BTN_W+2), r = (y-KBD_Y)/(BTN_H+2), idx = r*KBD_COLS+c; const char* map = (kbdPage==0?kbdMap0:kbdMap1);
                if (idx < strlen(map)) {
                    char k = map[idx]; if (k == '<') { if (wifiPassword.length()>0) wifiPassword.remove(wifiPassword.length()-1); }
                    else if (k == '\x01') { kbdPage = 1-kbdPage; drawKeyboard(); }
                    else if (k == '\n') enterState(STATE_CONNECTING); else if (k == '_') wifiPassword += " "; else wifiPassword += k;
                    updatePasswordDisplay(); delay(150);
                }
            }
            break;
        case STATE_RUNNING:
            if (y > 100 && y < 200) { 
                if (x > 280) { // Scroll Buttons
                    if (y < 150) scrollOffset -= 80; else scrollOffset += 80;
                    updateRunningUI(); delay(150); return;
                }
                if (detail.isDragging()) {
                    scrollOffset -= (detail.y - detail.prev_y);
                    updateRunningUI(); return;
                }
            }
            if (y > 200) { 
                if (x > 215 && x < 265) { 
                    if (historyCount > 0) { 
                        if (historyViewIdx == -1) historyViewIdx = (historyWriteIdx + 9) % 10; 
                        else historyViewIdx = (historyViewIdx + 9) % 10; 
                        scrollOffset = 0; updateRunningUI(); delay(150); 
                    } 
                }
                else if (x > 265) { 
                    if (historyViewIdx != -1) { 
                        historyViewIdx = (historyViewIdx + 1) % 10; 
                        if (historyViewIdx == (historyWriteIdx + 9) % 10) historyViewIdx = -1; 
                        scrollOffset = 0; updateRunningUI(); delay(150); 
                    } 
                }
                else if (x > 150 && x < 210) { // Narrowed HOME touch area
                    enterState(STATE_BOOT); 
                } 
            }
            break;
    }
}

void loop() {
    M5.update(); if (M5.Touch.getCount() > 0) handleTouch();
    if (currentState == STATE_BOOT && !isOtaMode) {
        static unsigned long lastUpdate = 0;
        if (millis() - lastUpdate > 1000) {
            auto dt = M5.Rtc.getDateTime(); M5.Display.setTextColor(CYAN, BLACK);
            M5.Display.setCursor(60, 85); M5.Display.printf("%04d/%02d/%02d %02d:%02d:%02d", dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes, dt.time.seconds);
            lastUpdate = millis();
        }
        unsigned long e = (millis() - stateTimer) / 1000;
        if (e >= 30) { 
            // Auto-run: Load stored credentials and attempt connection
            if (activeNet == NET_WIFI) {
                selectedSSID = storedRunSsid;
                wifiPassword = storedRunPass;
            }
            enterState(STATE_CONNECTING); 
        }
    }
    if (currentState == STATE_CONNECTING) {
        bool c = (activeNet == NET_WIFI ? WiFi.status() == WL_CONNECTED : Ethernet.localIP()[0] != 0);
        if (c) { 
            saveNetPref(); // Save last successful network type
            if (activeNet == NET_WIFI) saveWiFiConfig(); else saveLANConfig();
            configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov"); struct tm ti; if (getLocalTime(&ti)) M5.Rtc.setDateTime(&ti);
            if (isOtaMode) enterState(STATE_OTA); else enterState(STATE_RUNNING);
        } else if (millis() - stateTimer > 15000) { M5.Display.fillScreen(RED); M5.Display.drawCenterString("Fail", 160, 110); delay(2000); enterState(STATE_SELECT_NET_TYPE); }
    }
    if (currentState == STATE_RUNNING) {
        updateClock(); if (!mqttClient.connected()) reconnectMqtt(); mqttClient.loop();
        if (millis() - lastPublishTime > 10000) { lastPublishTime = millis(); if (mqttClient.connected()) { char ts[32]; sprintf(ts, "Uptime:%lu", millis()/1000); mqttClient.publish(mqttTopicPub.c_str(), ts); } }
    }
    delay(10);
}
