#include "sensor.h"
#include "config.h"
#include <DHT.h>

static DHT dht(DHT_PIN, DHT_TYPE);

void initDHT() {
    dht.begin();
}

bool readDHT(float *temperature, float *humidity) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
        Serial.println("[Sensor] Failed to read from DHT sensor");
        return false;
    }

    *temperature = t;
    *humidity    = h;
    return true;
}
