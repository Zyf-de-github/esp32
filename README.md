# ESP32 IoT Node — Temperature & Humidity Monitor

A minimal, production-ready ESP32 firmware that:

- Reads **temperature** and **humidity** from a **DHT22** sensor every 10 seconds
- Connects to a **WiFi** network
- Publishes sensor readings as **JSON** to an **MQTT** broker
- Uses the built-in LED as a visual status indicator

---

## Hardware

| Component | Connection |
|-----------|-----------|
| ESP32 devkit (any variant) | — |
| DHT22 DATA pin | GPIO 4 (configurable) |
| 10 kΩ pull-up resistor | between DHT22 DATA and 3.3 V |

Wiring diagram:

```
ESP32 3.3V ──┬─── DHT22 VCC
             │
            10kΩ
             │
ESP32 GPIO4 ─┴─── DHT22 DATA
ESP32 GND  ────── DHT22 GND
```

---

## Software Setup

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- A WiFi access point
- An MQTT broker (default: `broker.hivemq.com` — public, no auth)

### 1. Clone the repository

```bash
git clone https://github.com/Zyf-de-github/esp32.git
cd esp32
```

### 2. Configure credentials

Edit `include/config.h` and update:

```c
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
```

Optionally change:

| Constant | Default | Description |
|----------|---------|-------------|
| `MQTT_BROKER_HOST` | `broker.hivemq.com` | MQTT broker hostname |
| `MQTT_BROKER_PORT` | `1883` | MQTT broker port |
| `MQTT_CLIENT_ID` | `esp32-iot-node` | Unique client identifier |
| `MQTT_TOPIC_SENSOR` | `iot/esp32/sensor` | Topic for sensor data |
| `DHT_PIN` | `4` | GPIO pin connected to DHT DATA |
| `DHT_TYPE` | `DHT22` | Sensor model (`DHT11` or `DHT22`) |
| `SENSOR_SAMPLE_INTERVAL_MS` | `10000` | Sampling interval (ms) |

### 3. Build and flash

```bash
# Compile
pio run

# Flash to connected ESP32
pio run --target upload

# Open serial monitor
pio device monitor
```

---

## MQTT Message Format

Sensor readings are published to `iot/esp32/sensor` as a JSON object:

```json
{"temperature":23.5,"humidity":61.2,"unit":"C"}
```

An online/offline status message is published (retained) to `iot/esp32/status`:

```
online
```

You can subscribe to sensor data from any MQTT client:

```bash
# Using mosquitto_sub
mosquitto_sub -h broker.hivemq.com -t "iot/esp32/sensor"
```

---

## Project Structure

```
esp32/
├── include/
│   ├── config.h               # All user-configurable constants
│   ├── sensor.h               # DHT sensor interface
│   ├── wifi_manager.h         # WiFi connection interface
│   └── mqtt_client_wrapper.h  # MQTT publish interface
├── src/
│   ├── main.cpp               # Application entry point
│   ├── sensor.cpp             # DHT22 driver
│   ├── wifi_manager.cpp       # WiFi driver
│   └── mqtt_client_wrapper.cpp# MQTT driver
├── platformio.ini             # PlatformIO build configuration
└── README.md
```

---

## Dependencies

Managed automatically by PlatformIO:

| Library | Version | Purpose |
|---------|---------|---------|
| `adafruit/DHT sensor library` | ^1.4.6 | DHT11/DHT22 driver |
| `adafruit/Adafruit Unified Sensor` | ^1.1.14 | Sensor abstraction layer |
| `knolleary/PubSubClient` | ^2.8.0 | MQTT client |

---

## License

MIT