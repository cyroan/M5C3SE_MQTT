#include <M5Unified.h>
#include "icons_2026.h"

uint32_t lastAlarmUpdate = 0;
uint32_t lastFlashUpdate = 0;
int alarmIndex = -1; 
bool flashState = false;
uint16_t nonDisplayColor = 0;

void drawDashboard() {
    int iconSize = 78;
    int startX = 4; // (320 - 4*78) / 2 = 4
    int startY = 3; // (240 - 3*78) / 2 = 3

    for (int i = 0; i < 12; i++) {
        int col = i % 4;
        int row = i / 4;
        int x = startX + col * iconSize;
        int y = startY + row * iconSize;

        // Draw the captured icon from icons_2026.h
        M5.Display.pushImage(x, y, iconSize, iconSize, icons_2026[i]);

        // Draw a red triangle indicator if in alarm
        if (i == alarmIndex && flashState) {
            // Draw a filled red triangle in the top-right corner
            M5.Display.fillTriangle(x + iconSize - 1, y, 
                                    x + iconSize - 20, y, 
                                    x + iconSize - 1, y + 20, 
                                    RED);
        }
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    M5.Display.setSwapBytes(true); // Fixes endianness for pushImage
    
    nonDisplayColor = BLACK; 
    M5.Display.fillScreen(nonDisplayColor);

    drawDashboard();
}

void loop() {
    M5.update();

    // Check if 10 seconds have passed to move the alarm
    if (millis() - lastAlarmUpdate > 10000) {
        lastAlarmUpdate = millis();
        alarmIndex = (alarmIndex + 1) % 12;
        drawDashboard();
    }

    // Check if 500ms have passed to flash the alarm
    if (alarmIndex != -1 && millis() - lastFlashUpdate > 500) {
        lastFlashUpdate = millis();
        flashState = !flashState;
        drawDashboard();
    }
}
