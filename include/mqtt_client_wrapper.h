#pragma once
#include <Arduino.h>

/**
 * @brief Connect to the MQTT broker defined in config.h.
 *
 * Must be called after a WiFi connection is available.
 * Returns true on success.
 */
bool connectMQTT();

/**
 * @brief Keep the MQTT connection alive and process incoming messages.
 *
 * Call this regularly inside loop().
 */
void loopMQTT();

/**
 * @brief Publish a JSON payload to the given topic.
 *
 * Returns true if the message was delivered successfully.
 */
bool publishMQTT(const char *topic, const char *payload);

/**
 * @brief Returns true if the MQTT client is currently connected.
 */
bool isMQTTConnected();
