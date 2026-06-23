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
#include <ArduinoJson.h>

#include "version.h"
#include "icons_2026.h"
#include "state.h"
#include "config_mgr.h"
#include "network_mgr.h"
#include "mqtt_mgr.h"
#include "diag_mgr.h"
#include "gui.h"
#include "ota_mgr.h"

// State implementation
State currentState = STATE_BOOT;
State previousState = STATE_BOOT;
unsigned long stateTimer = 0;
bool isOtaMode = false;

void enterState(State ns) {
    currentState = ns; 
    M5.Display.fillScreen(BLACK); 
    stateTimer = millis();
    
    switch (ns) {
        case STATE_BOOT:
            M5.Display.setTextSize(2); 
            M5.Display.drawCenterString("PROWAVE Diag MON", 160, 30);
            M5.Display.setTextColor(YELLOW); 
            M5.Display.drawCenterString("Version: " FIRMWARE_VERSION, 160, 55);
            M5.Display.setTextColor(CYAN);
            { 
                auto dt = M5.Rtc.getDateTime(); 
                M5.Display.drawCenterString(String(dt.date.year)+"/"+String(dt.date.month)+"/"+String(dt.date.date)+" "+String(dt.time.hours)+":"+String(dt.time.minutes), 160, 85); 
            }
            M5.Display.setTextColor(WHITE);
            drawButton(10, 125, 145, 50, "UPDATE", BLUE); 
            drawButton(165, 125, 145, 50, "RUN", GREEN);
            drawButton(10, 185, 145, 45, "SET RTC", ORANGE); 
            drawButton(165, 185, 145, 45, "SET MQTT", PURPLE);
            break;
            
        case STATE_SELECT_NET_TYPE:
            M5.Display.drawCenterString("Select Connection", 160, 20);
            drawButton(20, 60, 280, 50, "WIFI", BLUE); 
            drawButton(20, 120, 280, 50, "LAN (DHCP)", GREEN); 
            drawButton(20, 180, 280, 50, "LAN (STATIC)", ORANGE);
            break;
            
        case STATE_SCAN_WIFI: 
            M5.Display.drawCenterString("Scanning...", 160, 110); 
            WiFi.mode(WIFI_STA); 
            scanCount = WiFi.scanNetworks(); 
            selectedSsidIdx = 0; 
            enterState(STATE_SELECT_SSID); 
            break;
            
        case STATE_SELECT_SSID: 
            drawSsidList(); 
            break;
            
        case STATE_INPUT_PASSWORD: 
            if (selectedSSID == storedOtaSsid || selectedSSID == storedRunSsid) {
                wifiPassword = (isOtaMode ? storedOtaPass : storedRunPass); 
            }
            kbdPage = 0; 
            updatePasswordDisplay(); 
            drawKeyboard(); 
            break;
            
        case STATE_LAN_STATIC_INPUT: 
            kbdPage = 1; 
            lanInputIdx = 0; 
            updateLanStaticDisplay(); 
            drawKeyboard(); 
            break;
            
        case STATE_SET_MQTT: 
            kbdPage = 0; 
            mqttSetStep = 0; 
            updateMqttStepDisplay(); 
            drawKeyboard(); 
            break;
            
        case STATE_CONNECTING:
            M5.Display.drawCenterString("Connecting...", 160, 60);
            M5.Display.setTextSize(1); 
            M5.Display.setTextColor(YELLOW);
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
            M5.Display.setTextSize(2); 
            M5.Display.setTextColor(WHITE);

            // Re-bind network client for MQTT
            mqttClient.setServer(mqttServer.c_str(), mqttPort);
            mqttClient.setBufferSize(2048);
            if (activeNet == NET_WIFI) {
                mqttClient.setClient(wifiClient);
            } else {
                mqttClient.setClient(ethClient);
            }

            startNetworkConnection(selectedSSID, wifiPassword);
            break;
            
        case STATE_OTA: 
            updateFirmware(); 
            break;
            
        case STATE_RUNNING:
            historyViewIdx = -1; 
            scrollOffset = 0; 
            M5.Display.fillScreen(DARKGREY); 
            M5.Display.setTextColor(WHITE); 
            M5.Display.setTextSize(2);
            M5.Display.setCursor(10, 10); 
            M5.Display.print("MQTT MONITOR"); 
            M5.Display.drawLine(0, 35, 320, 35, WHITE);
            if (!isNetworkConnected()) { 
                M5.Display.drawCenterString("Disconnected!", 160, 100); 
                Serial.println("Network Disconnected!");
            } else {
                updateRunningUI();
                Serial.printf("Running... Network OK. IP: %s\n", getLocalIPString().c_str());
            }
            break;
            
        case STATE_DIAG:
            drawDiagDashboard();
            break;
            
        case STATE_VALUE_DISPLAY:
            drawValueDashboard();
            break;
            
        case STATE_MODE_SELECT:
            drawModeSelection();
            break;
            
        case STATE_SET_RTC:
            { 
                auto dt = M5.Rtc.getDateTime(); 
                rtcY = dt.date.year; 
                rtcM = dt.date.month; 
                rtcD = dt.date.date; 
                rtcH = dt.time.hours; 
                rtcMin = dt.time.minutes; 
                rtcSetIdx = 0; 
            }
            drawRtcSetting(); 
            break;
    }
    delay(300); 
}

void handleTouch() {
    auto detail = M5.Touch.getDetail(); 
    if (!detail.isPressed()) return;
    int x = detail.x, y = detail.y;
    
    switch (currentState) {
        case STATE_BOOT:
            if (y > 125 && y < 175) { 
                if (x < 160) { 
                    isOtaMode = true; 
                    activeNet = NET_WIFI; 
                    enterState(STATE_SCAN_WIFI); 
                } else { 
                    isOtaMode = false; 
                    enterState(STATE_SELECT_NET_TYPE); 
                } 
            } else if (y > 185) { 
                if (x < 160) enterState(STATE_SET_RTC); 
                else enterState(STATE_SET_MQTT); 
            }
            break;
            
        case STATE_SELECT_NET_TYPE:
            if (x > 20 && x < 300) {
                if (y > 60 && y < 110) { 
                    activeNet = NET_WIFI; 
                    enterState(STATE_SCAN_WIFI); 
                } else if (y > 120 && y < 170) { 
                    activeNet = NET_LAN_DHCP; 
                    enterState(STATE_CONNECTING); 
                } else if (y > 180 && y < 230) { 
                    activeNet = NET_LAN_STATIC; 
                    enterState(STATE_LAN_STATIC_INPUT); 
                }
            }
            break;
            
        case STATE_SET_RTC:
            if (y > 140 && y < 185) {
                if (x < 105) { // +
                    if (rtcSetIdx == 0) rtcY++; 
                    else if (rtcSetIdx == 1) rtcM = (rtcM % 12) + 1;
                    else if (rtcSetIdx == 2) rtcD = (rtcD % 31) + 1; 
                    else if (rtcSetIdx == 3) rtcH = (rtcH + 1) % 24;
                    else rtcMin = (rtcMin + 1) % 60;
                    drawRtcSetting(); 
                    delay(150);
                } else if (x > 105 && x < 210) { // -
                    if (rtcSetIdx == 0) rtcY--; 
                    else if (rtcSetIdx == 1) rtcM = (rtcM > 1) ? rtcM - 1 : 12;
                    else if (rtcSetIdx == 2) rtcD = (rtcD > 1) ? rtcD - 1 : 31; 
                    else if (rtcSetIdx == 3) rtcH = (rtcH > 0) ? rtcH - 1 : 23;
                    else rtcMin = (rtcMin > 0) ? rtcMin - 1 : 59;
                    drawRtcSetting(); 
                    delay(150);
                } else { 
                    rtcSetIdx = (rtcSetIdx + 1) % 5; 
                    drawRtcSetting(); 
                    delay(500); 
                }
            } else if (y > 190 && x > 110 && x < 210) { // SAVE
                m5::rtc_datetime_t dt; 
                dt.date.year = rtcY; 
                dt.date.month = rtcM; 
                dt.date.date = rtcD; 
                dt.time.hours = rtcH; 
                dt.time.minutes = rtcMin; 
                dt.time.seconds = 0;
                M5.Rtc.setDateTime(dt); 
                enterState(STATE_BOOT);
            }
            break;
            
        case STATE_SET_MQTT:
            if (x >= KBD_X && y >= KBD_Y) {
                int c = (x - KBD_X) / (BTN_W + 2);
                int r = (y - KBD_Y) / (BTN_H + 2);
                int idx = r * KBD_COLS + c; 
                const char* map = (kbdPage == 0 ? kbdMap0 : kbdMap1);
                if (idx < (int)strlen(map)) {
                    char k = map[idx]; 
                    String* t;
                    if (mqttSetStep == 0) t = &mqttServer; 
                    else if (mqttSetStep == 1) { 
                        static String pS; 
                        pS = String(mqttPort); 
                        t = &pS; 
                    } else if (mqttSetStep == 2) {
                        t = &mqttTopicSub; 
                    } else {
                        t = &mqttTopicPub;
                    }

                    if (k == '<') { 
                        if (t->length() > 0) t->remove(t->length() - 1); 
                        else if (mqttSetStep == 0) enterState(STATE_BOOT); 
                    } else if (k == '\x01') { 
                        kbdPage = 1 - kbdPage; 
                        drawKeyboard(); 
                    } else if (k == '\x02') { 
                        enterState(STATE_BOOT); 
                    } else if (k == '\n') { // OK NEXT STEP
                        if (mqttSetStep == 1) mqttPort = t->toInt();
                        if (mqttSetStep < 3) { 
                            mqttSetStep++; 
                            updateMqttStepDisplay(); 
                            drawKeyboard(); 
                            delay(500); 
                        } else { 
                            saveMQTTConfig(); 
                            enterState(STATE_BOOT); 
                        }
                    } else { 
                        if (k == '_') *t += " "; else *t += k; 
                        if (mqttSetStep == 1) mqttPort = t->toInt();
                        updateMqttStepDisplay(); 
                        delay(150);
                    }
                }
            }
            break;
            
        case STATE_LAN_STATIC_INPUT:
            if (x >= KBD_X && y >= KBD_Y) {
                int c = (x - KBD_X) / (BTN_W + 2);
                int r = (y - KBD_Y) / (BTN_H + 2);
                int idx = r * KBD_COLS + c; 
                const char* map = (kbdPage == 0 ? kbdMap0 : kbdMap1);
                if (idx < (int)strlen(map)) {
                    char k = map[idx]; 
                    String* t = (lanInputIdx == 0 ? &lanIP : lanInputIdx == 1 ? &lanGW : lanInputIdx == 2 ? &lanMask : &lanDNS);
                    if (k == '<') { 
                        if (t->length() > 0) t->remove(t->length() - 1); 
                    } else if (k == '\x01') { 
                        kbdPage = 1 - kbdPage; 
                        drawKeyboard(); 
                    } else if (k == '\n') {
                        enterState(STATE_CONNECTING);
                    } else if ((k >= '0' && k <= '9') || k == '.') {
                        *t += k;
                    }
                    updateLanStaticDisplay(); 
                    delay(150);
                }
            }
            break;
            
        case STATE_SELECT_SSID:
            if (y > 180) {
                if (x < 100) { 
                    if (selectedSsidIdx > 0) selectedSsidIdx--; 
                    drawSsidList(); 
                } else if (x > 220) { 
                    if (selectedSsidIdx < scanCount - 1) selectedSsidIdx++; 
                    drawSsidList(); 
                } else if (x > 110 && x < 210) { 
                    selectedSSID = WiFi.SSID(selectedSsidIdx); 
                    wifiPassword = ""; 
                    enterState(STATE_INPUT_PASSWORD); 
                }
                delay(150);
            }
            break;
            
        case STATE_INPUT_PASSWORD:
            if (x >= KBD_X && y >= KBD_Y) {
                int c = (x - KBD_X) / (BTN_W + 2);
                int r = (y - KBD_Y) / (BTN_H + 2);
                int idx = r * KBD_COLS + c; 
                const char* map = (kbdPage == 0 ? kbdMap0 : kbdMap1);
                if (idx < (int)strlen(map)) {
                    char k = map[idx]; 
                    if (k == '<') { 
                        if (wifiPassword.length() > 0) wifiPassword.remove(wifiPassword.length() - 1); 
                    } else if (k == '\x01') { 
                        kbdPage = 1 - kbdPage; 
                        drawKeyboard(); 
                    } else if (k == '\n') {
                        enterState(STATE_CONNECTING); 
                    } else if (k == '_') {
                        wifiPassword += " "; 
                    } else {
                        wifiPassword += k;
                    }
                    updatePasswordDisplay(); 
                    delay(150);
                }
            }
            break;
            
        case STATE_RUNNING:
            if (y < 40) { 
                previousState = currentState; 
                enterState(STATE_MODE_SELECT); 
                return; 
            } 
            if (y > 100 && y < 200) { 
                if (x > 280) { // Scroll Buttons
                    if (y < 150) scrollOffset -= 80; else scrollOffset += 80;
                    updateRunningUI(); 
                    delay(150); 
                    return;
                }
                if (detail.isDragging()) {
                    scrollOffset -= (detail.y - detail.prev_y);
                    updateRunningUI(); 
                    return;
                }
            }
            if (y > 200) { 
                if (x > 215 && x < 265) { 
                    if (historyCount > 0) { 
                        if (historyViewIdx == -1) historyViewIdx = (historyWriteIdx + 9) % 10; 
                        else historyViewIdx = (historyViewIdx + 9) % 10; 
                        scrollOffset = 0; 
                        updateRunningUI(); 
                        delay(150); 
                    } 
                } else if (x > 265) { 
                    if (historyViewIdx != -1) { 
                        historyViewIdx = (historyViewIdx + 1) % 10; 
                        if (historyViewIdx == (historyWriteIdx + 9) % 10) historyViewIdx = -1; 
                        scrollOffset = 0; 
                        updateRunningUI(); 
                        delay(150); 
                    } 
                } else if (x > 150 && x < 210) { 
                    enterState(STATE_BOOT); 
                } 
            } else {
                previousState = currentState; 
                enterState(STATE_MODE_SELECT);
            }
            break;
            
        case STATE_DIAG:
            previousState = currentState; 
            enterState(STATE_MODE_SELECT);
            break;
            
        case STATE_VALUE_DISPLAY:
            if (x < 280 && y > 140 && y < 170) {
                showSpeed = !showSpeed;
                saveGuiConfig();
                drawValueDashboard();
                delay(150);
                return;
            }
            if (x > 280) { // Value Scroll Buttons
                if (y > 180) {
                    if (y < 210) valueScrollOffset -= 20; else valueScrollOffset += 20;
                    drawValueDashboard(); 
                    delay(100); 
                    return;
                }
            }
            previousState = currentState; 
            enterState(STATE_MODE_SELECT);
            break;
            
        case STATE_MODE_SELECT:
            if (x > 20 && x < 300) {
                if (y > 60 && y < 105) enterState(STATE_RUNNING);
                else if (y > 115 && y < 160) enterState(STATE_DIAG);
                else if (y > 170 && y < 215) enterState(STATE_VALUE_DISPLAY);
            }
            break;
    }
}

void setup() {
    auto cfg = M5.config(); 
    M5.begin(cfg);
    M5.Display.setRotation(1);
    delay(200); 
    
    initSD(); 
    loadWiFiConfig(); 
    loadLANConfig(); 
    loadMQTTConfig(); 
    loadNetPref();
    loadGuiConfig();
    
    initMqtt();
    
    enterState(STATE_BOOT);
}

void loop() {
    M5.update(); 
    if (M5.Touch.getCount() > 0) handleTouch();
    
    if (currentState == STATE_BOOT && !isOtaMode) {
        static unsigned long lastUpdate = 0;
        if (millis() - lastUpdate > 1000) {
            auto dt = M5.Rtc.getDateTime(); 
            M5.Display.setTextColor(CYAN, BLACK);
            M5.Display.setCursor(60, 85); 
            M5.Display.printf("%04d/%02d/%02d %02d:%02d:%02d", dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes, dt.time.seconds);
            lastUpdate = millis();
        }
        unsigned long e = (millis() - stateTimer) / 1000;
        if (e >= 30) { 
            if (activeNet == NET_WIFI) {
                selectedSSID = storedRunSsid;
                wifiPassword = storedRunPass;
            }
            enterState(STATE_CONNECTING); 
        }
    }
    
    if (currentState == STATE_CONNECTING) {
        bool c = isNetworkConnected();
        if (c) { 
            saveNetPref(); 
            if (activeNet == NET_WIFI) {
                saveWiFiConfig(isOtaMode, selectedSSID, wifiPassword); 
            } else {
                saveLANConfig();
            }
            configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov"); 
            struct tm ti; 
            if (getLocalTime(&ti)) M5.Rtc.setDateTime(&ti);
            if (isOtaMode) enterState(STATE_OTA); else enterState(STATE_RUNNING);
        } else if (millis() - stateTimer > 15000) { 
            M5.Display.fillScreen(RED); 
            M5.Display.drawCenterString("Fail", 160, 110); 
            delay(2000); 
            enterState(STATE_SELECT_NET_TYPE); 
        }
    }
    
    if (currentState == STATE_MODE_SELECT) {
        if (millis() - stateTimer > 5000) enterState(previousState);
    }
    
    if (currentState == STATE_RUNNING || currentState == STATE_DIAG || currentState == STATE_VALUE_DISPLAY) {
        if (currentState == STATE_RUNNING) updateClock(); 
        if (!mqttClient.connected()) reconnectMqtt(); 
        mqttLoop();
        publishUptime();
    }
    delay(10);
}
