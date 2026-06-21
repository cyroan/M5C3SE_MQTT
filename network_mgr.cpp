#include "network_mgr.h"

WiFiClient wifiClient;
EthernetClient ethClient;
M5Module_LAN LAN;

void startNetworkConnection(const String& selectedSSID, const String& wifiPassword) {
    if (activeNet == NET_WIFI) { 
        WiFi.mode(WIFI_STA);
        WiFi.begin(selectedSSID.c_str(), wifiPassword.c_str()); 
    }
    else {
        SPI.begin(LAN_SPI_SCK_PIN, LAN_SPI_MISO_PIN, LAN_SPI_MOSI_PIN, -1);
        LAN.setResetPin(LAN_RST_PIN); 
        LAN.reset(); 
        LAN.init(LAN_CS_PIN);
        if (activeNet == NET_LAN_DHCP) {
            Ethernet.begin(mac);
        } else {
            IPAddress ip, gw, msk, dns; 
            ip.fromString(lanIP); 
            gw.fromString(lanGW); 
            msk.fromString(lanGW); // Fallback if DNS not specified
            dns.fromString(lanDNS); 
            // The original logic was: Ethernet.begin(mac, ip, dns, gw, msk)
            Ethernet.begin(mac, ip, dns, gw, msk); 
        }
    }
}

bool isNetworkConnected() {
    if (activeNet == NET_WIFI) {
        return (WiFi.status() == WL_CONNECTED);
    } else {
        return (Ethernet.localIP()[0] != 0);
    }
}

String getLocalIPString() {
    if (activeNet == NET_WIFI) {
        return WiFi.localIP().toString();
    } else {
        return Ethernet.localIP().toString();
    }
}
