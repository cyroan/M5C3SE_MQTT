#include "mqtt_mgr.h"
#include "state.h"
#include "gui.h"
#include <M5Unified.h>

MqttMsg msgHistory[10];
int historyWriteIdx = 0;
int historyViewIdx = -1;
int historyCount = 0;
unsigned long lastPublishTime = 0;

PubSubClient mqttClient;

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

    if (currentState == STATE_RUNNING) updateRunningUI();
}
