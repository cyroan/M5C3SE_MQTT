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
            bool subUplink = mqttClient.subscribe("uplink/#");
            Serial.printf("Subscribe to [uplink/#] %s\n", subUplink ? "SUCCESS" : "FAILED");
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

    String tStr = String(topic);
    bool isUplink = tStr.startsWith("uplink/") || tStr == "uplink";

    // uplink 的內容不用 SHOW 在 MQTT MONITOR 中
    if (!isUplink) {
        msgHistory[historyWriteIdx] = {tStr, pl, String(ts)};
        historyWriteIdx = (historyWriteIdx + 1) % 10; 
        if (historyCount < 10) historyCount++;
    }
    
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

    if (currentState == STATE_RUNNING && !isUplink) updateRunningUI();
}

bool parseAdvantechPayload(const char* topic, byte* payload, unsigned int length) {
    String tStr = String(topic);
    String tUpper = tStr;
    tUpper.toUpperCase();

    // 支援 uplink/#, Advantech/+/data 以及主題含 RSSI, LSNR, DB
    bool matchTopic = tStr.startsWith("uplink/") || tStr == "uplink" ||
                      tStr.startsWith("Advantech/") || tStr.startsWith("ADVANTECH/") ||
                      tUpper.indexOf("UPLINK") != -1 ||
                      tUpper.indexOf("RSSI") != -1 ||
                      tUpper.indexOf("LSNR") != -1 ||
                      tUpper.indexOf("DB") != -1;

    if (!matchTopic) {
        return false;
    }
    
    // Extract SN from topic if possible: uplink/{devEUI}/... or Advantech/{SN}/...
    if (tStr.startsWith("uplink/")) {
        int s1 = tStr.indexOf('/');
        int s2 = tStr.indexOf('/', s1 + 1);
        if (s2 != -1) {
            latestSN = tStr.substring(s1 + 1, s2);
        } else {
            latestSN = tStr.substring(s1 + 1);
        }
    } else if (tStr.startsWith("Advantech/") || tStr.startsWith("ADVANTECH/")) {
        int firstSlash = tStr.indexOf('/');
        int secondSlash = tStr.indexOf('/', firstSlash + 1);
        if (firstSlash != -1 && secondSlash != -1 && secondSlash > firstSlash + 1) {
            latestSN = tStr.substring(firstSlash + 1, secondSlash);
        }
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        auto dt = M5.Rtc.getDateTime();
        char buf[16];
        sprintf(buf, "%02d:%02d:%02d", dt.time.hours, dt.time.minutes, dt.time.seconds);
        latestDataTime = String(buf);
        return true;
    }

    // SN in JSON (devEUI, deveui, dev_eui, device, sn, mac, devId)
    if (doc["devEUI"].is<const char*>()) latestSN = doc["devEUI"].as<const char*>();
    else if (doc["deveui"].is<const char*>()) latestSN = doc["deveui"].as<const char*>();
    else if (doc["dev_eui"].is<const char*>()) latestSN = doc["dev_eui"].as<const char*>();
    else if (doc["device"].is<const char*>()) latestSN = doc["device"].as<const char*>();
    else if (doc["sn"].is<const char*>()) latestSN = doc["sn"].as<const char*>();
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

    // RSSI (單位 dB)
    if (doc["rssi"].is<float>()) { latestRSSI = doc["rssi"].as<float>(); hasDbRssiData = true; }
    else if (doc["RSSI"].is<float>()) { latestRSSI = doc["RSSI"].as<float>(); hasDbRssiData = true; }
    else if (doc["gw_rssi"].is<float>()) { latestRSSI = doc["gw_rssi"].as<float>(); hasDbRssiData = true; }

    // LSNR / DB (單位 dB)
    if (doc["lsnr"].is<float>()) { latestDB = doc["lsnr"].as<float>(); hasDbRssiData = true; }
    else if (doc["LSNR"].is<float>()) { latestDB = doc["LSNR"].as<float>(); hasDbRssiData = true; }
    else if (doc["snr"].is<float>()) { latestDB = doc["snr"].as<float>(); hasDbRssiData = true; }
    else if (doc["SNR"].is<float>()) { latestDB = doc["SNR"].as<float>(); hasDbRssiData = true; }
    else if (doc["db"].is<float>()) { latestDB = doc["db"].as<float>(); hasDbRssiData = true; }
    else if (doc["dB"].is<float>()) { latestDB = doc["dB"].as<float>(); hasDbRssiData = true; }
    else if (doc["DB"].is<float>()) { latestDB = doc["DB"].as<float>(); hasDbRssiData = true; }
    else if (doc["sound"].is<float>()) { latestDB = doc["sound"].as<float>(); hasDbRssiData = true; }
    else if (doc["vib"].is<float>()) { latestDB = doc["vib"].as<float>(); hasDbRssiData = true; }
    else if (doc["value"].is<float>()) { latestDB = doc["value"].as<float>(); hasDbRssiData = true; }

    // Nested rxInfo (LoRaWAN metadata array)
    if (doc["rxInfo"].is<JsonArray>() && doc["rxInfo"].size() > 0) {
        auto rx = doc["rxInfo"][0];
        if (rx["rssi"].is<float>()) { latestRSSI = rx["rssi"].as<float>(); hasDbRssiData = true; }
        if (rx["lsnr"].is<float>()) { latestDB = rx["lsnr"].as<float>(); hasDbRssiData = true; }
        else if (rx["snr"].is<float>()) { latestDB = rx["snr"].as<float>(); hasDbRssiData = true; }
    }

    return true;
}
