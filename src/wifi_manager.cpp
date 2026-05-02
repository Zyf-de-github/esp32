#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>

bool connectWiFi() {
    Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= WIFI_TIMEOUT_MS) {
            Serial.println("\n[WiFi] Connection timed out");
            return false;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.printf("\n[WiFi] Connected — IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}
