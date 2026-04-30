#pragma once

// ------------------------------------------------------------
//  WiFi
// ------------------------------------------------------------
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ------------------------------------------------------------
//  Adafruit IO
// ------------------------------------------------------------
#define AIO_SERVER   "io.adafruit.com"
#define AIO_PORT     1883
#define AIO_USERNAME "YOUR_AIO_USERNAME"
#define AIO_KEY      "YOUR_AIO_KEY"

// Feed name helper
#define FEED(name) AIO_USERNAME "/feeds/" name

// ------------------------------------------------------------
//  LOGO! PLC
// ------------------------------------------------------------
#define LOGO_IP_STR "192.168.0.8"
#define LOGO_PORT   502

// ESP32 server coil addresses (LOGO! reads these)
#define COIL_VALVE      0    // NI1 → Coil 1 in LOGO!Soft
#define COIL_PUMP       1    // NI2 → Coil 2
#define COIL_VALVE_OFF  2    // NI3 → Coil 3
#define COIL_PUMP_OFF   3    // NI4 → Coil 4
#define COIL_NQ3_FLOW   4    // NQ3 → Coil 5
#define COIL_NQ1_VALVE  5    // NQ1 → Coil 6
#define COIL_NQ2_PUMP   6    // NQ2 → Coil 7
#define COIL_NQ4_Q1     7    // NQ4 → Coil 8 
#define COIL_NQ5_Q2     8    // NQ5 → Coil 9
#define HREG_AI1        1    // NAQ1 → HR 1

// ------------------------------------------------------------
//  Hardware pins
// ------------------------------------------------------------
#define DHT_PIN    15
#define DHT_TYPE   DHT11
#define MQ135_AOUT 34
#define MQ135_DOUT 4
#define LED_PIN    2

// ------------------------------------------------------------
//  Timing (milliseconds)
// ------------------------------------------------------------
#define SENSOR_INTERVAL_MS     20000   // Read DHT11 + MQ135
#define LOGO_INTERVAL_MS       20000   // Read LOGO! data
#define LOGO_PROBE_INTERVAL_MS 10000   // Check LOGO! reachable
#define HEARTBEAT_MS           5000    // Serial heartbeat
#define MODBUS_TIMEOUT_MS      150     // Per Modbus operation
#define RECONNECT_INTERVAL_MS  5000    // WiFi/MQTT retry
#define CONTROL_POLL_MS        500     // Poll valve/pump state from Adafruit REST API

// ------------------------------------------------------------
//  Thresholds
// ------------------------------------------------------------
#define AIR_ALERT_THRESHOLD    2000    // MQ135 raw ADC value
