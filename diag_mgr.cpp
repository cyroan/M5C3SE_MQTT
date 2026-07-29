#include "diag_mgr.h"

bool conditionActive[12] = {false};
bool alarmX = false, alarmY = false, alarmZ = false;
float addr0 = 0, addr1 = 0, addr2 = 0, addr3 = 0, addr4 = 0;

const char* conditionNames[12] = {
    "Unbalance", "Misalignment", "Vortex problem", "Oil Whirl", "Structural looseness", 
    "Bearing housing bolts looseness", "Bearing housing looseness", "Bearing damage", "Bearing sleeve", "Bearing looseness", 
    "Gearbox damage", "rotor eccentricity"
};

void resetDiagnosis() {
    for (int i = 0; i < 12; i++) {
        conditionActive[i] = false;
    }
    alarmX = false;
    alarmY = false;
    alarmZ = false;
    addr0 = 0;
    addr1 = 0;
    addr2 = 0;
    addr3 = 0;
    addr4 = 0;
}

bool parseMqttJsonPayload(const char* topic, byte* payload, unsigned int length) {
    if (String(topic) != "Prowave/IVM") {
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
        return false;
    }

    for (int i = 0; i < 12; i++) {
        conditionActive[i] = false;
    }

    // Robust address search: supports root-level keys or nested MODBUS object keys (UPPER/lower case)
    int a0 = doc["MODBUS"]["ADDRESS0"] | doc["MODBUS"]["Address0"] | doc["Address0"] | doc["ADDRESS0"] | 0;
    int a1 = doc["MODBUS"]["ADDRESS1"] | doc["MODBUS"]["Address1"] | doc["Address1"] | doc["ADDRESS1"] | 0;
    int a2 = doc["MODBUS"]["ADDRESS2"] | doc["MODBUS"]["Address2"] | doc["Address2"] | doc["ADDRESS2"] | 0;
    int a3 = doc["MODBUS"]["ADDRESS3"] | doc["MODBUS"]["Address3"] | doc["Address3"] | doc["ADDRESS3"] | 0;
    int a4 = doc["MODBUS"]["ADDRESS4"] | doc["MODBUS"]["Address4"] | doc["Address4"] | doc["ADDRESS4"] | 0;
    int a5 = doc["MODBUS"]["ADDRESS5"] | doc["MODBUS"]["Address5"] | doc["Address5"] | doc["ADDRESS5"] | 0;
    int a6 = doc["MODBUS"]["ADDRESS6"] | doc["MODBUS"]["Address6"] | doc["Address6"] | doc["ADDRESS6"] | 0;
    int a7 = doc["MODBUS"]["ADDRESS7"] | doc["MODBUS"]["Address7"] | doc["Address7"] | doc["ADDRESS7"] | 0;
    int a8 = doc["MODBUS"]["ADDRESS8"] | doc["MODBUS"]["Address8"] | doc["Address8"] | doc["ADDRESS8"] | 0;
    int a9 = doc["MODBUS"]["ADDRESS9"] | doc["MODBUS"]["Address9"] | doc["Address9"] | doc["ADDRESS9"] | 0;

    addr0 = a0 * 0.01f;
    addr1 = a1 * 0.01f;
    addr2 = a2 * 0.01f;
    addr3 = a3 * 0.001f;
    addr4 = a4 * 6.0f; // Scale to RPM

    alarmX = (doc["RawData"]["AlarmX"] | doc["MODBUS"]["RawData"]["AlarmX"] | doc["AlarmX"] | 0) == 1;
    alarmY = (doc["RawData"]["AlarmY"] | doc["MODBUS"]["RawData"]["AlarmY"] | doc["AlarmY"] | 0) == 1;
    alarmZ = (doc["RawData"]["AlarmZ"] | doc["MODBUS"]["RawData"]["AlarmZ"] | doc["AlarmZ"] | 0) == 1;

    if (a5 == 1) conditionActive[0] = true; 
    else if (a5 == 2) conditionActive[4] = true; // Structural looseness
    else if (a5 == 3) conditionActive[9] = true;
    else if (a5 == 4) conditionActive[8] = true;
    else if (a5 == 5) conditionActive[6] = true;
    else if (a5 == 6) conditionActive[5] = true;
    else if (a5 == 7) conditionActive[7] = true;
    else if (a5 == 8) { conditionActive[0] = true; conditionActive[9] = true; }
    else if (a5 == 9) { conditionActive[0] = true; conditionActive[8] = true; }
    else if (a5 == 10) { conditionActive[0] = true; conditionActive[6] = true; }
    else if (a5 == 11) { conditionActive[0] = true; conditionActive[5] = true; }
    else if (a5 == 12) { conditionActive[0] = true; conditionActive[7] = true; }
    else if (a5 == 13) { conditionActive[4] = true; conditionActive[9] = true; }
    else if (a5 == 14) { conditionActive[4] = true; conditionActive[8] = true; }
    else if (a5 == 15) { conditionActive[4] = true; conditionActive[6] = true; }
    else if (a5 == 16) { conditionActive[4] = true; conditionActive[5] = true; }
    else if (a5 == 17) { conditionActive[4] = true; conditionActive[7] = true; }
    else if (a5 == 18) { conditionActive[9] = true; conditionActive[8] = true; }
    else if (a5 == 19) { conditionActive[9] = true; conditionActive[6] = true; }
    else if (a5 == 20) { conditionActive[9] = true; conditionActive[5] = true; }
    else if (a5 == 21) { conditionActive[9] = true; conditionActive[7] = true; }
    else if (a5 == 22) { conditionActive[8] = true; conditionActive[6] = true; }
    else if (a5 == 23) { conditionActive[8] = true; conditionActive[5] = true; }
    else if (a5 == 24) { conditionActive[8] = true; conditionActive[7] = true; }
    else if (a5 == 25) { conditionActive[6] = true; conditionActive[5] = true; }
    else if (a5 == 26) { conditionActive[6] = true; conditionActive[7] = true; }
    else if (a5 == 27) { conditionActive[5] = true; conditionActive[7] = true; }

    if (a6 == 1) conditionActive[1] = true;
    if (a7 == 1) { conditionActive[2] = true; conditionActive[3] = true; }
    if (a8 == 1) conditionActive[11] = true;
    if (a9 == 1) conditionActive[10] = true;

    return true;
}
