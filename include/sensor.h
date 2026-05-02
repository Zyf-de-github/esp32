#pragma once
#include <Arduino.h>

/**
 * @brief Read temperature and humidity from a DHT sensor.
 *
 * Populates the provided float pointers on success.
 * Returns true if the reading is valid, false otherwise.
 */
bool readDHT(float *temperature, float *humidity);

/**
 * @brief Initialise the DHT sensor.
 */
void initDHT();
