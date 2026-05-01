#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <PubSubClient.h> // Nick O'Leary's Library
#include <time.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <LittleFS.h>
#include "version.h"

// --- Configuration ---
const char* updateUrl = "https://raw.githubusercontent.com/cyroan/M5C3SE_MQTT/master/M5C3SE_MQTT.ino.m5stack_cores3.bin";
const char* versionUrl = "https://raw.githubusercontent.com/cyroan/M5C3SE_MQTT/master/version.txt";
const char* wifiConfigFile = "/wifi_config.txt";

// --- SD Card Pins for CoreS3 ---
#define SD_SPI_SCK_PIN  36
#define SD_SPI_MISO_PIN 35
#define SD_SPI_MOSI_PIN 37
#define SD_SPI_CS_PIN   4

// --- MQTT Configuration ---
const char* mqttServer = "mqtt.m5stack.com";
const int mqttPort = 1883;
const char* mqttTopicSub = "Advantech/#";
const char* mqttTopicPub = "Advantech/T";
String lastMqttMsg = "Waiting for data...";
String lastMqttTopic = "None";

unsigned long lastPublishTime = 0; 
bool sdAvailable = false;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

enum State {
    STATE_BOOT,
    STATE_SCAN_WIFI,
    STATE_SELECT_SSID,
    STATE_INPUT_PASSWORD,
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

int scanCount = 0;
int selectedSsidIdx = 0;
int kbdPage = 0;
bool isOtaMode = false;

// --- LittleFS WiFi Config Helpers ---
void saveWiFiConfig() {
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
        Serial.println("WiFi Config saved to SD");
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
        Serial.println("WiFi Config loaded from SD");
    }
}

// Keyboard Layout (5 columns x 5 rows)
const char* kbdMap0 = "ABCDEFGHIJKLMNOPQRSTUVW<>"; 
const char* kbdMap1 = "XYZ0123456789.-_ \x01\n";

const int KBD_COLS = 5;
const int KBD_ROWS = 5;
const int BTN_W = 60;
const int BTN_H = 38;
const int KBD_X = 10;
const int KBD_Y = 45;

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
        int r = i / KBD_COLS; int c = i % KBD_COLS;
        int bx = KBD_X + c * (BTN_W + 2); int by = KBD_Y + r * (BTN_H + 2);
        char key = map[i];
        if (key == '<') drawButton(bx, by, BTN_W, BTN_H, "BS", RED);
        else if (key == '>') drawButton(bx, by, BTN_W, BTN_H, "NEXT", ORANGE);
        else if (key == '\x01') drawButton(bx, by, BTN_W, BTN_H, "BACK", ORANGE);
        else if (key == '\n') drawButton(bx, by, BTN_W, BTN_H, "OK", GREEN);
        else { char label[2] = {key, 0}; drawButton(bx, by, BTN_W, BTN_H, label, BLUE); }
    }
}

// --- MQTT Callback ---
void mqttCallback(char* topic, byte* payload, unsigned long length) {
    lastMqttTopic = String(topic);
    lastMqttMsg = "";
    for (int i = 0; i < length; i++) {
        lastMqttMsg += (char)payload[i];
    }

    // 取得當前 RTC 時間
    auto dt = M5.Rtc.getDateTime();
    char timeStr[32];
    sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d", 
            dt.date.year, dt.date.month, dt.date.date,
            dt.time.hours, dt.time.minutes, dt.time.seconds);

    // 寫入 SD 卡
    if (sdAvailable) {
        File file = SD.open("/RECEIVR.TXT", FILE_APPEND);
        if (file) {
            file.printf("[%s] Topic: %s | Msg: %s\n", timeStr, topic, lastMqttMsg.c_str());
            file.close();
        }
    }

    if (currentState == STATE_RUNNING) {
        updateRunningUI();
    }
}

void reconnectMqtt() {
    if (!mqttClient.connected()) {
        M5.Display.setCursor(10, 215);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(WHITE);
        M5.Display.print("MQTT Connecting...");
        String clientId = "M5Stack-Client-" + String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str())) {
            mqttClient.subscribe(mqttTopicSub);
            M5.Display.fillRect(0, 210, 320, 30, DARKGREY); 
        }
    }
}

void updateClock() {
    static int lastSec = -1;
    auto dt = M5.Rtc.getDateTime();
    if (dt.time.seconds == lastSec) return;
    lastSec = dt.time.seconds;

    // 在標題列右側顯示時間
    M5.Display.fillRect(200, 5, 115, 25, DARKGREY); 
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(210, 10);
    M5.Display.printf("%02d:%02d:%02d", dt.time.hours, dt.time.minutes, dt.time.seconds);
}

void updateRunningUI() {
    M5.Display.fillRect(0, 40, 320, 160, BLACK);
    M5.Display.setTextColor(YELLOW);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 45);
    M5.Display.println("Last Topic:");
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 68);
    M5.Display.setTextColor(WHITE);
    M5.Display.println(lastMqttTopic);
    M5.Display.drawLine(10, 95, 310, 95, DARKGREY);
    M5.Display.setCursor(10, 105);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(CYAN);
    M5.Display.println("Message:");
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 130);
    M5.Display.println(lastMqttMsg);
    
    // 顯示 SD 卡狀態 (移至底部的灰色區域)
    M5.Display.fillRect(5, 215, 150, 20, DARKGREY); 
    M5.Display.setTextSize(1);
    M5.Display.setCursor(10, 220);
    M5.Display.setTextColor(sdAvailable ? GREEN : RED);
    M5.Display.printf("SD: %s", sdAvailable ? "READY (Logging...)" : "NOT FOUND");
    
    updateClock();
}

bool initSD() {
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    sdAvailable = SD.begin(SD_SPI_CS_PIN, SPI, 25000000);
    if (sdAvailable) {
        Serial.println("SD Card Initialized successfully.");
        uint8_t cardType = SD.cardType();
        Serial.print("SD Card Type: ");
        if (cardType == CARD_MMC) Serial.println("MMC");
        else if (cardType == CARD_SD) Serial.println("SDSC");
        else if (cardType == CARD_SDHC) Serial.println("SDHC");
        else Serial.println("UNKNOWN");
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        Serial.printf("SD Card Size: %lluMB\n", cardSize);
    } else {
        Serial.println("SD Card Initialization failed.");
    }
    return sdAvailable;
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    
    // 初始化 SD 卡 (必須先初始化 SD 才能讀取 WiFi 設定)
    initSD();
    
    // 從 SD 卡載入 WiFi 設定
    loadWiFiConfig();
    
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    enterState(STATE_BOOT);
}

void enterState(State newState) {
    currentState = newState;
    M5.Display.fillScreen(BLACK);
    stateTimer = millis();

    switch (currentState) {
        case STATE_BOOT:
            M5.Display.setTextSize(2);
            M5.Display.drawCenterString("M5Stack CoreS3 SE", 160, 40);
            M5.Display.drawCenterString("Version: " FIRMWARE_VERSION, 160, 70);
            drawButton(20, 120, 130, 60, "CHECK UPDATE", BLUE);
            drawButton(170, 120, 130, 60, "RUN", GREEN);
            break;

        case STATE_SCAN_WIFI:
            M5.Display.drawCenterString("Scanning WiFi...", 160, 110);
            WiFi.mode(WIFI_STA); WiFi.disconnect();
            scanCount = WiFi.scanNetworks();
            selectedSsidIdx = 0;
            enterState(STATE_SELECT_SSID);
            break;

        case STATE_SELECT_SSID: drawSsidList(); break;

        case STATE_INPUT_PASSWORD:
            // 自動帶入已儲存的密碼
            if (isOtaMode && selectedSSID == storedOtaSsid) wifiPassword = storedOtaPass;
            else if (!isOtaMode && selectedSSID == storedRunSsid) wifiPassword = storedRunPass;
            
            kbdPage = 0; updatePasswordDisplay(); drawKeyboard();
            break;

        case STATE_CONNECTING:
            M5.Display.drawCenterString("Connecting...", 160, 110);
            WiFi.begin(selectedSSID.c_str(), wifiPassword.c_str());
            break;

        case STATE_OTA: updateFirmware(); break;

        case STATE_RUNNING:
            // 進入 RUN 時再次檢查 SD
            if (!sdAvailable) initSD();
            
            M5.Display.fillScreen(DARKGREY);
            M5.Display.setTextColor(WHITE);
            M5.Display.setTextSize(2);
            M5.Display.drawCenterString("MQTT MONITOR", 100, 10);
            M5.Display.drawLine(0, 35, 320, 35, WHITE);
            if (WiFi.status() != WL_CONNECTED) {
                M5.Display.drawCenterString("WiFi Not Connected!", 160, 100);
                M5.Display.setTextSize(1);
                M5.Display.drawCenterString("Tap screen to scan WiFi", 160, 140);
            } else {
                updateRunningUI();
            }
            break;
    }
}

void drawSsidList() {
    M5.Display.fillScreen(BLACK); M5.Display.setTextSize(2); M5.Display.setCursor(10, 5);
    M5.Display.setTextColor(ORANGE); M5.Display.println("Select SSID:");
    int startY = 35;
    for (int i = 0; i < min(scanCount, 5); i++) {
        String ssid = WiFi.SSID(i);
        bool isStored = (isOtaMode && ssid == storedOtaSsid) || (!isOtaMode && ssid == storedRunSsid);
        
        if (i == selectedSsidIdx) { M5.Display.fillRect(0, startY + i * 30, 320, 28, BLUE); M5.Display.setTextColor(WHITE); }
        else { M5.Display.setTextColor(isStored ? GREEN : LIGHTGREY); }
        
        M5.Display.setCursor(15, startY + 5 + i * 30); 
        M5.Display.printf("%s%s", ssid.c_str(), isStored ? " *" : "");
    }
    drawButton(5, 190, 95, 45, "UP", DARKGREY);
    drawButton(110, 190, 100, 45, "SELECT", GREEN);
    drawButton(220, 190, 95, 45, "DOWN", DARKGREY);
}

void updatePasswordDisplay() {
    M5.Display.fillRect(0, 0, 320, 40, DARKGREY); M5.Display.setCursor(5, 5);
    M5.Display.setTextSize(1); M5.Display.setTextColor(WHITE);
    M5.Display.printf("Mode: %s | SSID: %s", isOtaMode ? "OTA" : "RUN", selectedSSID.c_str());
    M5.Display.setCursor(5, 20); M5.Display.setTextSize(2);
    M5.Display.printf("PW: %s", wifiPassword.c_str());
}

void updateFirmware() {
    M5.Display.fillScreen(BLACK); M5.Display.setCursor(0, 0); M5.Display.println("Checking for updates...");
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.begin(client, versionUrl);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String newVersion = http.getString(); newVersion.trim(); 
        if (newVersion.equals(FIRMWARE_VERSION)) {
            M5.Display.println("Latest version."); delay(2000); enterState(STATE_RUNNING); return;
        }
    } else {
        M5.Display.println("Update Check Fail"); delay(2000); enterState(STATE_RUNNING); return;
    }
    http.end();
    M5.Display.println("Updating...");
    httpUpdate.onProgress([](int cur, int total) {
        M5.Display.fillRect(0, 180, 320, 30, BLACK); M5.Display.setCursor(0, 180);
        M5.Display.printf("Progress: %d%%", (cur * 100) / total);
    });
    httpUpdate.rebootOnUpdate(true);
    httpUpdate.update(client, updateUrl);
}

void handleTouch() {
    auto detail = M5.Touch.getDetail();
    if (!detail.isPressed()) return;
    int x = detail.x; int y = detail.y;

    switch (currentState) {
        case STATE_BOOT:
            if (y > 120 && y < 180) {
                if (x > 20 && x < 150) { isOtaMode = true; enterState(STATE_SCAN_WIFI); }
                else if (x > 170 && x < 300) { isOtaMode = false; enterState(STATE_RUNNING); }
            }
            break;

        case STATE_SELECT_SSID:
            if (y > 180) {
                if (x < 100) { selectedSsidIdx = (selectedSsidIdx > 0) ? selectedSsidIdx - 1 : 0; drawSsidList(); }
                else if (x > 220) { selectedSsidIdx = (selectedSsidIdx < scanCount - 1) ? selectedSsidIdx + 1 : scanCount - 1; drawSsidList(); }
                else if (x >= 110 && x <= 210) { selectedSSID = WiFi.SSID(selectedSsidIdx); wifiPassword = ""; enterState(STATE_INPUT_PASSWORD); }
                delay(200);
            }
            break;

        case STATE_INPUT_PASSWORD:
            if (x >= KBD_X && x < KBD_X + KBD_COLS * (BTN_W + 2) && y >= KBD_Y && y < KBD_Y + KBD_ROWS * (BTN_H + 2)) {
                int c = (x - KBD_X) / (BTN_W + 2); int r = (y - KBD_Y) / (BTN_H + 2);
                int idx = r * KBD_COLS + c;
                const char* map = (kbdPage == 0) ? kbdMap0 : kbdMap1;
                if (idx < strlen(map)) {
                    char key = map[idx];
                    if (key == '<') { if (wifiPassword.length() > 0) wifiPassword.remove(wifiPassword.length() - 1); }
                    else if (key == '>') { kbdPage = 1; drawKeyboard(); }
                    else if (key == '\x01') { kbdPage = 0; drawKeyboard(); }
                    else if (key == '\n') { enterState(STATE_CONNECTING); return; }
                    else if (key == '_') { wifiPassword += " "; }
                    else { wifiPassword += key; }
                    updatePasswordDisplay(); delay(200);
                }
            }
            break;

        case STATE_RUNNING:
            if (WiFi.status() != WL_CONNECTED) { isOtaMode = false; enterState(STATE_SCAN_WIFI); }
            else if (y > 200) enterState(STATE_BOOT);
            break;
    }
}

void loop() {
    M5.update();
    if (M5.Touch.getCount() > 0) handleTouch();

    if (currentState == STATE_BOOT) {
        unsigned long elapsed = (millis() - stateTimer) / 1000;
        if (elapsed >= 30) { isOtaMode = false; enterState(STATE_RUNNING); }
        else { M5.Display.setCursor(100, 200); M5.Display.setTextSize(2); M5.Display.printf("Auto-run: %ds ", 30 - (int)elapsed); }
    }
    
    if (currentState == STATE_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            // 連線成功，記錄密碼
            saveWiFiConfig();
            
            // 同步時間 (GMT+8)
            configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                M5.Rtc.setDateTime(&timeinfo); // 同步 NTP 到硬體 RTC
            }
            
            if (isOtaMode) enterState(STATE_OTA);
            else enterState(STATE_RUNNING);
        } else if (millis() - stateTimer > 15000) { 
            M5.Display.fillScreen(RED); M5.Display.drawCenterString("Fail", 160, 110); delay(2000); enterState(STATE_INPUT_PASSWORD); 
        }
    }

    if (currentState == STATE_RUNNING && WiFi.status() == WL_CONNECTED) {
        updateClock(); // 每一秒更新時鐘顯示
        
        if (!mqttClient.connected()) reconnectMqtt();
        mqttClient.loop();

        if (millis() - lastPublishTime > 10000) {
            lastPublishTime = millis();
            if (mqttClient.connected()) {
                char timeStr[32];
                sprintf(timeStr, "Uptime: %lu s", millis() / 1000);
                mqttClient.publish(mqttTopicPub, timeStr);
            }
        }
    }
    delay(10);
}
