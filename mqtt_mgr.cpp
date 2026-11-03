#include "mqtt_mgr.h"
#include "state.h"
#include "gui.h"
#include <M5Unified.h>
#include <ArduinoJson.h>

MqttMsg msgHistory[10];
int historyWriteIdx = 0;
int historyViewIdx = -1;
int historyCount = 0;
unsigned long lastPublishTime = 0;

PubSubClient mqttClient;

String latestSN = "";
String latestDataTime = "";
float latestRSSI = 0;
float latestDB = 0;
bool hasDbRssiData = false;

void initMqtt() {
    mqttClient.setServer(mqttServer.c_str(), mqttPort);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(2048); // For larger payloads
}

void reconnectMqtt() {
    if (!isNetworkConnected()) return;
    if (!mqttClient.connected()) {
        M5.Display.setCursor(10, 195); 
        M5.Display.setTextSize(1); 
        M5.Display.setTextColor(WHITE);
        M5.Display.print("MQTT Reconnecting...");
        Serial.printf("Attempting MQTT connection to %s:%d...\n", mqttServer.c_str(), mqttPort);
        String clientId = "M5-C3SE-" + String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str())) { 
            Serial.println("Connected to MQTT Broker.");
            bool subOk = mqttClient.subscribe(mqttTopicSub.c_str()); 
            Serial.printf("Subscribe to [%s] %s\n", mqttTopicSub.c_str(), subOk ? "SUCCESS" : "FAILED");
            bool subAdv = mqttClient.subscribe("Advantech/+/data");
            Serial.printf("Subscribe to [Advantech/+/data] %s\n", subAdv ? "SUCCESS" : "FAILED");
            M5.Display.fillRect(0, 190, 320, 15, BLACK); 
        } else {
            Serial.printf("MQTT Connect Failed, rc=%d\n", mqttClient.state());
        }
    }
}

void mqttLoop() {
    mqttClient.loop();
}

void publishUptime() {
    if (millis() - lastPublishTime > 10000) { 
        lastPublishTime = millis(); 
        if (mqttClient.connected()) { 
            char ts[32]; 
            sprintf(ts, "Uptime:%lu", millis() / 1000); 
            mqttClient.publish(mqttTopicPub.c_str(), ts); 
        } 
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("MQTT Received [%s] Length: %u\n", topic, length);
    String pl = ""; 
    for (unsigned int i = 0; i < length; i++) {
        pl += (char)payload[i];
        if (i > 2000) break; // Safety cap for UI display
    }
    auto dt = M5.Rtc.getDateTime();
    char ts[32]; 
    sprintf(ts, "%02d:%02d:%02d", dt.time.hours, dt.time.minutes, dt.time.seconds);
    msgHistory[historyWriteIdx] = {String(topic), pl, String(ts)};
    historyWriteIdx = (historyWriteIdx + 1) % 10; 
    if (historyCount < 10) historyCount++;
    
    if (sdAvailable) {
        File file = SD.open("/RECEIVR.TXT", FILE_APPEND);
        if (file) { 
            file.printf("[%04d-%02d-%02d %s] T: %s | M: %s\n", dt.date.year, dt.date.month, dt.date.date, ts, topic, pl.c_str()); 
            file.close(); 
        }
    }

    // --- Diagnostic JSON Logic ---
    bool isDiagPayload = parseMqttJsonPayload(topic, payload, length);
    if (isDiagPayload) {
        if (currentState == STATE_DIAG) drawDiagDashboard();
        if (currentState == STATE_VALUE_DISPLAY) drawValueDashboard();
    }

    // --- Advantech / DB / RSSI Logic ---
    bool isAdvPayload = parseAdvantechPayload(topic, payload, length);
    if (isAdvPayload) {
        if (currentState == STATE_DB_RSSI) drawDbRssiDashboard();
    }

    if (currentState == STATE_RUNNING) updateRunningUI();
}

bool parseAdvantechPayload(const char* topic, byte* payload, unsigned int length) {
    String tStr = String(topic);
    bool matchTopic = (tStr.startsWith("Advantech/") && tStr.endsWith("/data")) || 
                      (tStr.startsWith("ADVANTECH/") && tStr.endsWith("/data")) ||
                      (tStr.indexOf("Advantech/") != -1) ||
                      (tStr.indexOf("RSSI") != -1) ||
                      (tStr.indexOf("rssi") != -1);
    
    // Extract SN from topic if possible: Advantech/{SN}/data
    if (tStr.startsWith("Advantech/") || tStr.startsWith("ADVANTECH/")) {
        int firstSlash = tStr.indexOf('/');
        int secondSlash = tStr.indexOf('/', firstSlash + 1);
        if (firstSlash != -1 && secondSlash != -1 && secondSlash > firstSlash + 1) {
            latestSN = tStr.substring(firstSlash + 1, secondSlash);
        }
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        if (!matchTopic) return false;
        auto dt = M5.Rtc.getDateTime();
        char buf[16];
        sprintf(buf, "%02d:%02d:%02d", dt.time.hours, dt.time.minutes, dt.time.seconds);
        latestDataTime = String(buf);
        return true;
    }

    // SN in JSON
    if (doc["sn"].is<const char*>()) latestSN = doc["sn"].as<const char*>();
    else if (doc["SN"].is<const char*>()) latestSN = doc["SN"].as<const char*>();
    else if (doc["mac"].is<const char*>()) latestSN = doc["mac"].as<const char*>();
    else if (doc["devId"].is<const char*>()) latestSN = doc["devId"].as<const char*>();

    // Timestamp
    if (doc["time"].is<const char*>()) latestDataTime = doc["time"].as<const char*>();
    else if (doc["timestamp"].is<const char*>()) latestDataTime = doc["timestamp"].as<const char*>();
    else if (doc["datetime"].is<const char*>()) latestDataTime = doc["datetime"].as<const char*>();
    else {
        auto dt = M5.Rtc.getDateTime();
        char buf[16];
        sprintf(buf, "%02d:%02d:%02d", dt.time.hours, dt.time.minutes, dt.time.seconds);
        latestDataTime = String(buf);
    }

    // RSSI
    if (doc["rssi"].is<float>()) { latestRSSI = doc["rssi"].as<float>(); hasDbRssiData = true; }
    else if (doc["RSSI"].is<float>()) { latestRSSI = doc["RSSI"].as<float>(); hasDbRssiData = true; }
    else if (doc["gw_rssi"].is<float>()) { latestRSSI = doc["gw_rssi"].as<float>(); hasDbRssiData = true; }

    // DB
    if (doc["db"].is<float>()) { latestDB = doc["db"].as<float>(); hasDbRssiData = true; }
    else if (doc["dB"].is<float>()) { latestDB = doc["dB"].as<float>(); hasDbRssiData = true; }
    else if (doc["DB"].is<float>()) { latestDB = doc["DB"].as<float>(); hasDbRssiData = true; }
    else if (doc["sound"].is<float>()) { latestDB = doc["sound"].as<float>(); hasDbRssiData = true; }
    else if (doc["vib"].is<float>()) { latestDB = doc["vib"].as<float>(); hasDbRssiData = true; }
    else if (doc["value"].is<float>()) { latestDB = doc["value"].as<float>(); hasDbRssiData = true; }

    return matchTopic || hasDbRssiData;
}
