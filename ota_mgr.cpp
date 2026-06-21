#include "ota_mgr.h"
#include "state.h"
#include "version.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <M5Unified.h>

const char* updateUrl = "https://raw.githubusercontent.com/cyroan/M5C3SE_MQTT/master/M5C3SE_MQTT.ino.m5stack_cores3.bin";
const char* versionUrl = "https://raw.githubusercontent.com/cyroan/M5C3SE_MQTT/master/version.txt";

void updateFirmware() {
    M5.Display.fillScreen(BLACK); 
    M5.Display.setCursor(0, 0); 
    M5.Display.println("Checking Updates (WiFi)...");
    
    WiFiClientSecure sc; 
    sc.setInsecure(); 
    HTTPClient h;
    
    if (h.begin(sc, versionUrl)) {
        if (h.GET() == 200) {
            String nv = h.getString(); 
            nv.trim();
            if (nv.equals(FIRMWARE_VERSION)) { 
                M5.Display.println("Latest."); 
                delay(2000); 
                enterState(STATE_RUNNING); 
                return; 
            }
            h.end(); 
            M5.Display.println("Updating...");
            
            httpUpdate.onProgress([](int c, int t){ 
                M5.Display.fillRect(0, 180, 320, 30, BLACK); 
                M5.Display.setCursor(0, 180); 
                M5.Display.printf("Progress: %d%%", (c * 100) / t); 
            });
            httpUpdate.rebootOnUpdate(true); 
            httpUpdate.update(sc, updateUrl);
        } else { 
            M5.Display.println("Fail."); 
            delay(2000); 
            enterState(STATE_RUNNING); 
        }
    }
}
