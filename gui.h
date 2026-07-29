#ifndef GUI_H
#define GUI_H

#include <M5Unified.h>
#include "state.h"
#include "config_mgr.h"
#include "diag_mgr.h"

extern M5Canvas dimCanvas;
extern M5Canvas msgCanvas;

extern int scrollOffset;
extern int valueScrollOffset;
extern String selectedSSID;
extern String wifiPassword;
extern int scanCount;
extern int selectedSsidIdx;
extern int kbdPage;
extern int lanInputIdx;

extern int rtcY, rtcM, rtcD, rtcH, rtcMin, rtcSetIdx;
extern int mqttSetStep;
extern int selectedTopicIdx;

// Constants for Keyboard
extern const char* kbdMap0;
extern const char* kbdMap1;
extern const int KBD_COLS;
extern const int KBD_ROWS;
extern const int BTN_W;
extern const int BTN_H;
extern const int KBD_X;
extern const int KBD_Y;

void drawButton(int x, int y, int w, int h, const char* label, uint32_t color, int textSize = 2);
void drawDiagDashboard();
void drawValueDashboard();
void drawModeSelection();
void drawKeyboard();
void updateClock();
void updateRunningUI();
void drawSsidList();
void updatePasswordDisplay();
void updateLanStaticDisplay();
void drawRtcSetting();
void updateMqttStepDisplay();
void drawMqttTopicSelectUI();

#endif // GUI_H
