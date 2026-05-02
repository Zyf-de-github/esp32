#include "mqtt_client_wrapper.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient    wifiClient;
static PubSubClient  mqttClient(wifiClient);
static unsigned long lastReconnectAttemptMs = 0;

bool connectMQTT() {
    mqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    mqttClient.setKeepAlive(MQTT_KEEPALIVE_SEC);

    Serial.printf("[MQTT] Connecting to %s:%d ...\n", MQTT_BROKER_HOST, MQTT_BROKER_PORT);

    if (!mqttClient.connect(MQTT_CLIENT_ID)) {
        Serial.printf("[MQTT] Connection failed — state: %d\n", mqttClient.state());
        return false;
    }

    Serial.println("[MQTT] Connected");
    mqttClient.publish(MQTT_TOPIC_STATUS, "online", true /* retained */);
    return true;
}

void loopMQTT() {
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttemptMs >= MQTT_RECONNECT_MS) {
            lastReconnectAttemptMs = now;
            Serial.println("[MQTT] Disconnected — attempting reconnect ...");
            connectMQTT();
        }
    }
    mqttClient.loop();
}

bool publishMQTT(const char *topic, const char *payload) {
    if (!mqttClient.connected()) {
        Serial.println("[MQTT] Cannot publish — not connected");
        return false;
    }
    bool ok = mqttClient.publish(topic, payload);
    if (!ok) {
        Serial.printf("[MQTT] Publish to %s failed\n", topic);
    }
    return ok;
}

bool isMQTTConnected() {
    return mqttClient.connected();
}
