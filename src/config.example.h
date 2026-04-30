#pragma once

// ============================================================
//  config.example.h — copy this file to config.h and fill in
//  your own credentials before building.
// ============================================================

// ------------------------------------------------------------
//  WiFi
// ------------------------------------------------------------
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ------------------------------------------------------------
//  Adafruit IO (https://io.adafruit.com → My Key)
// ------------------------------------------------------------
#define AIO_SERVER   "io.adafruit.com"
#define AIO_PORT     1883
#define AIO_USERNAME "YOUR_AIO_USERNAME"
#define AIO_KEY      "YOUR_AIO_KEY"

// Feed name helper — do not change
#define FEED(name) AIO_USERNAME "/feeds/" name

// ------------------------------------------------------------
//  LOGO! PLC  (Modbus TCP client target)
// ------------------------------------------------------------
#define LOGO_IP_STR "192.168.0.8"   // adjust to your network
#define LOGO_PORT   502

// ESP32 Modbus server coil map (ESP32 = server, LOGO! = client)
//  Coils 0-3 : commands ESP32 → LOGO! (via network outputs NI)
//  Coils 4-8 : feedback LOGO! → ESP32 (via network inputs NQ)
#define COIL_VALVE      0    // NI1 → SET  valve
#define COIL_PUMP       1    // NI2 → SET  pump
#define COIL_VALVE_OFF  2    // NI3 → RESET valve
#define COIL_PUMP_OFF   3    // NI4 → RESET pump
#define COIL_NQ3_FLOW   4    // NQ3 → flow direction feedback
#define COIL_NQ1_VALVE  5    // NQ1 → I1 feedback
#define COIL_NQ2_PUMP   6    // NQ2 → I2 feedback
#define COIL_NQ4_Q1     7    // NQ4 → Q1 valve confirmed
#define COIL_NQ5_Q2     8    // NQ5 → Q2 pump confirmed
#define HREG_AI1        1    // NAQ1 → HR1 (unused — not reliable)

// ------------------------------------------------------------
//  Hardware pins
// ------------------------------------------------------------
#define DHT_PIN    15   // DHT11 data
#define DHT_TYPE   DHT11
#define MQ135_AOUT 34   // MQ135 analog output (12-bit ADC)
#define MQ135_DOUT 4    // MQ135 digital threshold output
#define LED_PIN    2    // Onboard LED (air quality alert)

// ------------------------------------------------------------
//  Timing (milliseconds)
// ------------------------------------------------------------
#define SENSOR_INTERVAL_MS     20000   // DHT11 + MQ135 read period
#define LOGO_INTERVAL_MS       20000   // LOGO! status publish period
#define LOGO_PROBE_INTERVAL_MS 10000   // TCP reachability check period
#define HEARTBEAT_MS           5000    // Serial heartbeat period
#define MODBUS_TIMEOUT_MS      150     // Per-operation Modbus timeout
#define RECONNECT_INTERVAL_MS  5000    // WiFi / MQTT retry interval
#define CONTROL_POLL_MS        500     // Adafruit REST API poll interval

// ------------------------------------------------------------
//  Thresholds
// ------------------------------------------------------------
#define AIR_ALERT_THRESHOLD    2000    // MQ135 raw ADC — above = poor air
