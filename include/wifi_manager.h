#pragma once
#include <Arduino.h>

/**
 * @brief Connect to WiFi using credentials in config.h.
 *
 * Blocks until the connection is established or WIFI_TIMEOUT_MS elapses.
 * Returns true on success.
 */
bool connectWiFi();

/**
 * @brief Returns true if the ESP32 currently has a WiFi connection.
 */
bool isWiFiConnected();
