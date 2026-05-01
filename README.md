# 🌦️ Weather & Water Monitoring Station

> Cloud-connected industrial automation — ESP32 + Siemens LOGO! 8.4 + MQTT + Modbus TCP

A full-stack IoT/industrial automation project that bridges a Siemens LOGO! 8.4 PLC with the Adafruit IO cloud platform through an ESP32 microcontroller. Environmental sensors feed live data to a remote dashboard, while the dashboard sends control commands back to the physical pump and valve — all over standard industrial and IoT protocols.

---

## System Architecture

```
┌─────────────────┐        MQTT         ┌─────────────────┐
│   Adafruit IO   │◄────────────────────│                 │
│   Dashboard     │   port 1883         │   ESP32         │
│   (Cloud)       │────────────────────►│   DevKit V1     │
└─────────────────┘   HTTP REST poll    │   (Bridge)      │
                       every 2 s        │                 │
                                        └────────┬────────┘
                                                 │
                                                 │ Modbus TCP
                                                 │ port 503
                                                 │ (ESP32 = server)
                                                 │ (LOGO! = client)
                                        ┌────────▼────────┐
                                        │  Siemens        │        DB25
                                        │  LOGO! 8.4      │◄──────────────► I/O Simulator
                                        │  (PLC)          │                  9AK1014
                                        └─────────────────┘
```

**Key design principle:** The ESP32 runs a passive Modbus TCP **server** on port 503. The LOGO! is always the **active client** — it polls NI blocks (reads ESP32 coils 0–3 for commands) and pushes NQ blocks (writes ESP32 coils 4–8 for real output status). The ESP32 never initiates a connection to the LOGO!.

---

## Hardware

| Component | Model | Role |
|-----------|-------|------|
| Microcontroller | ESP32 DevKit V1 | Bridge: cloud ↔ industrial network |
| PLC | Siemens LOGO! 8.4 (0BA8) | Industrial control — pump & valve |
| Temp/Humidity sensor | DHT11 | GPIO 15, 1-wire digital |
| Air quality sensor | MQ-135 | GPIO 34 (analog), GPIO 4 (digital alert) |
| I/O Simulator | Siemens 9AK1014-1AA00 | Local manual control panel via DB25 |
| Router | Any | Shared LAN — static IPs |

**IP addressing:**
- LOGO! 8.4 → `192.168.0.8`
- ESP32 → `192.168.0.118` (DHCP or static)

---

## Protocols

### MQTT — Sensor Telemetry to Cloud
- Broker: **Adafruit IO** (port 1883)
- Direction: ESP32 **publishes only** (7 feeds)
- The dashboard holds the only MQTT connection on the free tier

| Feed | Type | Description |
|------|------|-------------|
| `temperature` | float | DHT11 — °C |
| `humidity` | float | DHT11 — %RH |
| `air-quality-percent` | int | MQ-135 mapped 0–100% |
| `air-alert` | string | `"ALERT"` on threshold breach |
| `flow-direction` | bool | I3 sensor via LOGO! NQ |
| `device-value` | int | AI1 analog knob via LOGO! |
| `pump` / `valve` | bool | Real Q1/Q2 output state from LOGO! NQ |

### HTTP REST — Control Commands from Dashboard
- The ESP32 polls Adafruit IO REST API every **2 seconds** via `fetchFeedValue()`
- On state change → writes to ESP32 Modbus server coils 0–1
- Used instead of MQTT subscribe to avoid connection conflicts on the free tier

### Modbus TCP — Industrial Control Layer
- ESP32 server on **port 503** — passive, never initiates
- LOGO! is always the client

**ESP32 Coil Map:**

| Coil | Direction | LOGO! block | Purpose |
|------|-----------|-------------|---------|
| 0 | LOGO! reads | NI1 | Valve SET command |
| 1 | LOGO! reads | NI2 | Pump SET command |
| 2 | LOGO! reads | NI3 | Valve RESET |
| 3 | LOGO! reads | NI4 | Pump RESET |
| 4 | LOGO! writes | NQ3 | I3 flow direction feedback |
| 5 | LOGO! writes | NQ1 | I1 switch state |
| 6 | LOGO! writes | NQ2 | I2 switch state |
| 7 | LOGO! writes | NQ4 | Q1 real pump output |
| 8 | LOGO! writes | NQ5 | Q2 real valve output |

---

## LOGO! Soft Comfort — Ladder Logic

The LOGO! owns the physical I/O. The ESP32 never drives the pump or valve directly.

```
I1 (local switch) ──┐
                    ├── OR ──► Q1  (Pump)
NI2 (ESP32 coil 1)─┘

I2 (local switch) ──┐
                    ├── OR ──► Q2  (Valve)
NI1 (ESP32 coil 0)─┘

I3 ──────────────────────────► NQ3 → ESP32 coil 4  (flow feedback)
AI1 ─────────────────────────► NQ  → ESP32        (analog value)
```

Either the **local physical switch** OR the **remote cloud command** activates each output. If the internet drops, the LOGO! and simulator keep working normally.

**Network configuration in LOGO!Soft Comfort:**
`Tools → Ethernet Connections → Static IP 192.168.0.8 → Enable Modbus server port 502`

---

## Firmware Structure (PlatformIO)

```
Weather_Water_Station/
├── platformio.ini          # Board, framework, libs, build flags
├── src/
│   ├── main.cpp            # setup() + loop() — orchestrates all subsystems
│   ├── config.h            # Pins, IPs, feed names, timing constants
│   ├── wifi_mqtt.cpp/.h    # WiFi, MQTT publish, HTTP REST polling
│   ├── modbus_logo.cpp/.h  # Modbus server (ESP32) + client probing
│   └── sensors.cpp/.h      # DHT11 + MQ-135 read and publish
├── include/
└── lib/
```

**Dependencies (`platformio.ini`):**

```ini
[env:esp32dev]
platform        = espressif32
board           = esp32dev
framework       = arduino
monitor_speed   = 115200
upload_speed    = 921600
board_build.flash_mode = dio
lib_ignore      = WiFiNINA_-_Adafruit_Fork
lib_deps =
    adafruit/DHT sensor library@^1.4.4
    adafruit/Adafruit Unified Sensor@^1.1.4
    adafruit/Adafruit MQTT Library@^2.5.8
    emelianov/modbus-esp8266@^4.1.0
```

> `flash_mode = dio` fixes the `invalid header: 0xffffffff` boot loop on this ESP32 chip revision.
> `lib_ignore` excludes `WiFiNINA_Adafruit_Fork` which overrides ESP32's native `WiFi.h` and causes `WIFI_STA` errors.

---

## Wiring — ESP32 Pin Map

| Component | Pin on device | ESP32 GPIO | Notes |
|-----------|--------------|------------|-------|
| DHT11 | DATA | GPIO 15 | 1-wire, internal pull-up on module |
| DHT11 | VCC | 3.3V | |
| MQ-135 | AOUT | GPIO 34 | Voltage divider 10kΩ + 20kΩ (5V → 3.3V) |
| MQ-135 | DOUT | GPIO 4 | Digital alert, active LOW |
| MQ-135 | VCC | 5V (VIN) | Heater draws ~150mA |
| LOGO! 8.4 | RJ-45 | via router | Static IP 192.168.0.8, Modbus port 502 |
| Onboard LED | — | GPIO 2 | Air quality alert blink |

⚠️ **Critical:** MQ-135 AOUT outputs up to 5V. Without the voltage divider, GPIO 34 would be over-voltaged and the ADC channel could be permanently damaged.

---

## Cloud Dashboard

Platform: **Adafruit IO** (free tier)
Dashboard name: `Weather Station Parameters`

Feeds: temperature · humidity · air-quality-percent · air-alert · flow-direction · device-value · pump · valve

Publish rate: every 20 s (~6 msg/min, within the 30 msg/min free tier limit)

---

## Quick Start

1. Clone the repo and open in VS Code with PlatformIO installed
2. Copy `src/config.h` and fill in your WiFi credentials and Adafruit IO key
3. Set LOGO! static IP to `192.168.0.8`, enable Modbus server on port 502
4. Flash the ESP32 (`Upload` in PlatformIO)
5. Open Serial Monitor at 115200 baud — confirm WiFi, MQTT, and Modbus connections
6. Open your Adafruit IO dashboard — live data should appear within 20 seconds

---

## Technical Report

Full project documentation (architecture, schematics, protocol deep-dive, firmware walkthrough):
📄 [`Weather_Water_Station_Report.pptx`](./Weather_Water_Station_Report.pptx)

---

## Author

**Rawad Fakhreddine**
Electrical Engineering · 2026

---

*Built with ESP32 (Arduino/PlatformIO) · Siemens LOGO!Soft Comfort · Adafruit IO · EPLAN Electric P8*
