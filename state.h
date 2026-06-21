#ifndef STATE_H
#define STATE_H

enum State {
    STATE_BOOT, STATE_SELECT_NET_TYPE, STATE_SCAN_WIFI, STATE_SELECT_SSID,
    STATE_INPUT_PASSWORD, STATE_LAN_STATIC_INPUT, STATE_CONNECTING, STATE_OTA, STATE_RUNNING,
    STATE_SET_RTC, STATE_SET_MQTT, STATE_DIAG, STATE_VALUE_DISPLAY, STATE_MODE_SELECT
};

extern State currentState;
extern State previousState;
extern unsigned long stateTimer;

void enterState(State ns);

#endif // STATE_H
