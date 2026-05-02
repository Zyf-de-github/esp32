/**
 * ESP32 IoT Node — Temperature & Humidity Monitor
 *
 * Reads data from a DHT22 sensor and publishes the results over MQTT
 * every SENSOR_SAMPLE_INTERVAL_MS milliseconds.
 *
 * Hardware:
 *   - ESP32 development board (any variant)
 *   - DHT22 sensor connected to DHT_PIN (default: GPIO 4)
 *   - 10 kΩ pull-up resistor between DHT DATA and 3.3 V
 *
 * Dependencies (install via PlatformIO lib_deps or Arduino Library Manager):
 *   - DHT sensor library by Adafruit
 *   - Adafruit Unified Sensor
 *   - PubSubClient by Nick O'Leary
 */

#include <Arduino.h>
#include "config.h"
#include "sensor.h"
#include "wifi_manager.h"
#include "mqtt_client_wrapper.h"

static unsigned long lastSampleMs = 0;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static void blinkLED(int times, int delayMs = 150) {
    for (int i = 0; i < times; i++) {
        digitalWrite(STATUS_LED_PIN, HIGH);
        delay(delayMs);
        digitalWrite(STATUS_LED_PIN, LOW);
        delay(delayMs);
    }
}

static void buildJSON(char *buf, size_t bufLen, float temperature, float humidity) {
    snprintf(buf, bufLen,
             "{\"temperature\":%.1f,\"humidity\":%.1f,\"unit\":\"C\"}",
             temperature, humidity);
}

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    pinMode(STATUS_LED_PIN, OUTPUT);

    Serial.println("\n=== ESP32 IoT Node booting ===");

    initDHT();

    if (!connectWiFi()) {
        Serial.println("[Main] WiFi failed — rebooting in 5 s");
        blinkLED(10, 100);
        delay(5000);
        ESP.restart();
    }

    if (!connectMQTT()) {
        Serial.println("[Main] MQTT failed — continuing without broker");
    }

    blinkLED(3);
    Serial.println("[Main] Setup complete");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
    loopMQTT();

    unsigned long now = millis();
    if (now - lastSampleMs >= SENSOR_SAMPLE_INTERVAL_MS) {
        lastSampleMs = now;

        float temperature, humidity;
        if (readDHT(&temperature, &humidity)) {
            Serial.printf("[Main] Temp: %.1f °C  Humidity: %.1f %%\n",
                          temperature, humidity);

            char json[128];
            buildJSON(json, sizeof(json), temperature, humidity);
            publishMQTT(MQTT_TOPIC_SENSOR, json);
            blinkLED(1);
        }
    }
}
