#pragma once

// ─── WiFi Settings ────────────────────────────────────────────────────────────
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define WIFI_TIMEOUT_MS 20000

// ─── MQTT Broker Settings ─────────────────────────────────────────────────────
#define MQTT_BROKER_HOST    "broker.hivemq.com"
#define MQTT_BROKER_PORT    1883
#define MQTT_CLIENT_ID      "esp32-iot-node"
#define MQTT_TOPIC_SENSOR   "iot/esp32/sensor"
#define MQTT_TOPIC_STATUS   "iot/esp32/status"
#define MQTT_KEEPALIVE_SEC  60
#define MQTT_RECONNECT_MS   5000

// ─── DHT Sensor Settings ──────────────────────────────────────────────────────
#define DHT_PIN         4
#define DHT_TYPE        DHT22    // DHT11 or DHT22

// ─── Sampling & Timing ────────────────────────────────────────────────────────
#define SENSOR_SAMPLE_INTERVAL_MS  10000   // 10 seconds between readings
#define SERIAL_BAUD_RATE           115200

// ─── LED Status Indicator ─────────────────────────────────────────────────────
#define STATUS_LED_PIN   2      // Built-in LED on most ESP32 devkits
