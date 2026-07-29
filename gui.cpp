#include "gui.h"
#include "mqtt_mgr.h"
#include "icons_2026.h"
#include <vector>

M5Canvas dimCanvas(&M5.Display);
M5Canvas msgCanvas(&M5.Display);

int scrollOffset = 0;
int valueScrollOffset = 0;
String selectedSSID = "";
String wifiPassword = "";
int scanCount = 0;
int selectedSsidIdx = 0;
int kbdPage = 0;
int lanInputIdx = 0;

int rtcY = 2026, rtcM = 1, rtcD = 1, rtcH = 0, rtcMin = 0, rtcSetIdx = 0;
int mqttSetStep = 0;
int selectedTopicIdx = 0;

const char* kbdMap0 = "ABCDEFGHIJKLMNOPQRSTUVW<\x01"; // Slot 23=<, 24=\x01
const char* kbdMap1 = "XYZ0123456789.-_/@: \x01\x02\n"; // Slot 21= , 22=\x01, 23=\x02, 24=\n
const int KBD_COLS = 5, KBD_ROWS = 5, BTN_W = 60, BTN_H = 38, KBD_X = 10, KBD_Y = 45;

void drawButton(int x, int y, int w, int h, const char* label, uint32_t color, int textSize) {
    M5.Display.fillRoundRect(x, y, w, h, 6, color);
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(textSize);
    M5.Display.drawCenterString(label, x + w / 2, y + h / 2 - (textSize * 4));
}

void drawDiagDashboard() {
    M5.Display.fillScreen(BLACK);
    int iconSize = 78;
    int startX = 4; // (320 - 4*78) / 2 = 4
    int startY = 3; // (240 - 3*78) / 2 = 3
    
    if (dimCanvas.width() == 0) {
        dimCanvas.createSprite(iconSize, iconSize);
        dimCanvas.fillSprite(0x000F); // Navy Blue (Dark Blue)
    }
    
    M5.Display.setSwapBytes(true); 
    bool anyActive = false;
    // Map screen slots (0..11) to condition icon indices (0..11)
    // R1: 6, 10, 7, 9  | R2: 8, 5, 2, 1  | R3: 4, 11, 3, 12
    const int displayOrder[12] = {5, 9, 6, 8, 7, 4, 1, 0, 3, 10, 2, 11};

    for (int slot = 0; slot < 12; slot++) {
        int idx = displayOrder[slot];
        int col = slot % 4;
        int row = slot / 4;
        int x = startX + col * iconSize;
        int y = startY + row * iconSize;
        M5.Display.pushImage(x, y, iconSize, iconSize, icons_2026[idx]);
        
        if (conditionActive[idx]) {
            anyActive = true;
            // Triggered: Normal brightness + Red Frame
            M5.Display.drawRect(x, y, iconSize, iconSize, RED);
            M5.Display.drawRect(x + 1, y + 1, iconSize - 2, iconSize - 2, RED);
        } else {
            // Untriggered: Dimmed display
            dimCanvas.pushSprite(&M5.Display, x, y, 160); // 160 is alpha (~63% dim)
        }
    }

    if (!anyActive) {
        M5.Display.setTextSize(4);
        M5.Display.setTextColor(GREEN);
        M5.Display.drawCenterString("ALL PASS", 160, 100);
    }
}

void drawValueDashboard() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE);
    M5.Display.setFont(&fonts::Font2); 
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 10); M5.Display.print("VALUE DISPLAY");
    M5.Display.drawLine(0, 35, 320, 35, WHITE);

    // Top Section: Address 0-4
    int yStart = 45, yStep = 25;
    M5.Display.setCursor(15, yStart);
    M5.Display.setTextColor(WHITE); M5.Display.print("X Velocity: ");
    M5.Display.setTextColor(alarmX ? RED : WHITE); M5.Display.printf("%.2f mm/s", addr0);

    M5.Display.setCursor(15, yStart + yStep);
    M5.Display.setTextColor(WHITE); M5.Display.print("Y Velocity: ");
    M5.Display.setTextColor(alarmY ? RED : WHITE); M5.Display.printf("%.2f mm/s", addr1);

    M5.Display.setCursor(15, yStart + yStep*2);
    M5.Display.setTextColor(WHITE); M5.Display.print("Z Velocity: ");
    M5.Display.setTextColor(alarmZ ? RED : WHITE); M5.Display.printf("%.2f mm/s", addr2);

    M5.Display.setCursor(15, yStart + yStep*3);
    M5.Display.setTextColor(WHITE); M5.Display.printf("Temp:       %.3f C", addr3);

    M5.Display.setCursor(15, yStart + yStep*4);
    if (showSpeed) {
        M5.Display.setTextColor(WHITE); M5.Display.printf("Speed:      %.0f RPM", addr4);
    } else {
        M5.Display.setTextColor(DARKGREY); M5.Display.print("Speed:      [Hidden]");
    }

    M5.Display.drawLine(0, 175, 320, 175, DARKGREY);

    // Bottom Section: Diagnostic Text with Scrolling
    int diagAreaY = 180, diagAreaH = 55;
    bool anyActive = false;
    std::vector<String> activeList;
    for (int i = 0; i < 12; i++) {
        if (conditionActive[i]) {
            activeList.push_back(conditionNames[i]);
            anyActive = true;
        }
    }

    if (!anyActive) {
        M5.Display.setTextColor(GREEN);
        M5.Display.setTextSize(2);
        M5.Display.drawCenterString("Status Normal", 160, 200);
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.setTextSize(2);
        
        // Use a canvas to calculate height and handle clipping
        static M5Canvas diagCanvas(&M5.Display);
        diagCanvas.createSprite(280, 500); // Temporary large height
        diagCanvas.fillSprite(BLACK);
        diagCanvas.setCursor(0, 0);
        diagCanvas.setTextColor(RED);
        diagCanvas.setTextSize(2);
        for (const auto& issue : activeList) {
            diagCanvas.println(issue);
        }
        
        int contentH = diagCanvas.getCursorY();
        if (valueScrollOffset < 0) valueScrollOffset = 0;
        if (contentH > diagAreaH && valueScrollOffset > contentH - diagAreaH) valueScrollOffset = contentH - diagAreaH;
        if (contentH <= diagAreaH) valueScrollOffset = 0;

        M5.Display.setClipRect(10, diagAreaY, 280, diagAreaH);
        diagCanvas.pushSprite(10, diagAreaY - valueScrollOffset);
        M5.Display.clearClipRect();
        diagCanvas.deleteSprite();

        // Scroll indicators
        if (contentH > diagAreaH) {
            M5.Display.setTextColor(BLUE);
            if (valueScrollOffset > 0) M5.Display.drawCenterString("^", 305, diagAreaY + 5);
            if (valueScrollOffset < contentH - diagAreaH) M5.Display.drawCenterString("v", 305, diagAreaY + 35);
        }
    }
    M5.Display.setFont(&fonts::Font0); 
}

void drawModeSelection() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(ORANGE);
    M5.Display.setTextSize(2);
    M5.Display.drawCenterString("SELECT DISPLAY MODE", 160, 20);

    drawButton(20, 60, 280, 45, "MONITOR (MQTT)", BLUE);
    drawButton(20, 115, 280, 45, "DIAGNOSTIC (ICONS)", GREEN);
    drawButton(20, 170, 280, 45, "VALUES (DATA)", PURPLE);
    
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(LIGHTGREY);
    M5.Display.drawCenterString("Auto-close in 5s", 160, 225);
}

void drawKeyboard() {
    M5.Display.fillRect(0, KBD_Y - 5, 320, 240 - (KBD_Y - 5), BLACK);
    const char* map = (kbdPage == 0) ? kbdMap0 : kbdMap1;
    for (int i = 0; i < 25; i++) {
        int r = i / KBD_COLS, c = i % KBD_COLS;
        int bx = KBD_X + c * (BTN_W + 2), by = KBD_Y + r * (BTN_H + 2);
        if (i >= (int)strlen(map)) break;
        char key = map[i];
        if (key == '<') drawButton(bx, by, BTN_W, BTN_H, "BS", RED, 2);
        else if (key == '\x01') drawButton(bx, by, BTN_W, BTN_H, "KB", ORANGE, 2);
        else if (key == '\x02') drawButton(bx, by, BTN_W, BTN_H, "HOME", PURPLE, 2);
        else if (key == '\n') drawButton(bx, by, BTN_W, BTN_H, "OK", GREEN, 2);
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
    
    // Check if Topic matches PW/#, Advantech/# or contains RSSI to increase font size by 1 (Size 3)
    String dispTopicUpper = dispTopic; dispTopicUpper.toUpperCase();
    String subTopicUpper = mqttTopicSub; subTopicUpper.toUpperCase();
    bool isLargeFont = (dispTopic.startsWith("PW/") || dispTopic == "PW/#" ||
                        dispTopic.startsWith("Advantech/") || dispTopic == "Advantech/#" ||
                        dispTopic.startsWith("ADVANTECH/") || dispTopic == "ADVANTECH/#" ||
                        mqttTopicSub == "PW/#" || mqttTopicSub == "Advantech/#" || mqttTopicSub == "ADVANTECH/#" ||
                        dispTopicUpper.indexOf("RSSI") != -1 || subTopicUpper.indexOf("RSSI") != -1);
    int msgTextSize = isLargeFont ? 3 : 2;

    // Draw Message with Scrolling using ClipRect
    msgCanvas.createSprite(270, 1500); 
    msgCanvas.fillSprite(BLACK);
    msgCanvas.setTextColor(WHITE);
    msgCanvas.setTextSize(msgTextSize);
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

void drawSsidList() {
    M5.Display.fillScreen(BLACK); M5.Display.setTextSize(2); M5.Display.setCursor(10, 5);
    M5.Display.setTextColor(ORANGE); M5.Display.println("Select SSID:");
    int startY = 35, itemsPerPage = 5; int startIndex = (selectedSsidIdx / itemsPerPage) * itemsPerPage;
    for (int i = 0; i < itemsPerPage; i++) {
        int currentIdx = startIndex + i; if (currentIdx >= scanCount) break;
        String ssid = WiFi.SSID(currentIdx); bool isStored = (storedOtaSsid != "" && ssid == storedOtaSsid) || (storedRunSsid != "" && ssid == storedRunSsid);
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
    // Check main configuration state
    extern bool isOtaMode;
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
    else if (mqttSetStep == 2) { title = "STEP 3: SUB TOPIC (LIST)"; val = mqttTopicSub; }
    else { title = "STEP 4: PUB TOPIC"; val = mqttTopicPub; }
    M5.Display.drawCenterString(title, 160, 5);
    M5.Display.setTextSize(2); M5.Display.setTextColor(YELLOW);
    M5.Display.drawCenterString(val, 160, 20);
}

void drawMqttTopicSelectUI() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 5);
    M5.Display.setTextColor(ORANGE);
    M5.Display.println("Select SUB Topic:");

    int startY = 35;
    int itemsPerPage = 5;
    int totalItems = (int)mqttTopicList.size();
    if (totalItems == 0) return;

    if (selectedTopicIdx < 0) selectedTopicIdx = 0;
    if (selectedTopicIdx >= totalItems) selectedTopicIdx = totalItems - 1;

    int startIndex = (selectedTopicIdx / itemsPerPage) * itemsPerPage;

    for (int i = 0; i < itemsPerPage; i++) {
        int currentIdx = startIndex + i;
        if (currentIdx >= totalItems) break;

        String topic = mqttTopicList[currentIdx];
        bool isCurrent = (topic == mqttTopicSub);

        if (currentIdx == selectedTopicIdx) {
            M5.Display.fillRect(0, startY + i * 30, 320, 28, BLUE);
            M5.Display.setTextColor(WHITE);
        } else {
            M5.Display.setTextColor(isCurrent ? GREEN : LIGHTGREY);
        }

        M5.Display.setCursor(15, startY + 5 + i * 30);
        M5.Display.printf("%d. %s%s", currentIdx + 1, topic.c_str(), isCurrent ? " *" : "");
    }

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(DARKGREY);
    M5.Display.setCursor(250, 10);
    int totalPages = (totalItems + itemsPerPage - 1) / itemsPerPage;
    int currentPage = (startIndex / itemsPerPage) + 1;
    M5.Display.printf("P.%d/%d", currentPage, totalPages);

    drawButton(5, 190, 95, 45, "UP", DARKGREY);
    drawButton(110, 190, 100, 45, "SELECT", GREEN);
    drawButton(220, 190, 95, 45, "DOWN", DARKGREY);
}

