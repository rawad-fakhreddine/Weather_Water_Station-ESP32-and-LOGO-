#include <Arduino.h>
#include "esp_system.h"
#include "config.h"
#include "wifi_mqtt.h"
#include "modbus_logo.h"
#include "sensors.h"

// ------------------------------------------------------------
//  Timers
// ------------------------------------------------------------
static unsigned long lastSensorTime = 0;
static unsigned long lastLogoTime   = 0;
static unsigned long lastHeartbeat  = 0;
static unsigned long loopCount      = 0;

// ------------------------------------------------------------
//  Reset reason printer
// ------------------------------------------------------------
static void printResetReason() {
    esp_reset_reason_t r = esp_reset_reason();
    Serial.print("[RESET] ");
    switch (r) {
        case ESP_RST_POWERON:  Serial.println("Power-on");           break;
        case ESP_RST_SW:       Serial.println("Software restart");   break;
        case ESP_RST_PANIC:    Serial.println("Panic/exception");    break;
        case ESP_RST_INT_WDT:  Serial.println("Interrupt watchdog"); break;
        case ESP_RST_TASK_WDT: Serial.println("Task watchdog");      break;
        case ESP_RST_BROWNOUT: Serial.println("Brownout");           break;
        default:               Serial.printf("Other (%d)\n", r);     break;
    }
}

// ============================================================
//  setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.println("   Weather & Water Station — ESP32      ");
    Serial.println("========================================");
    printResetReason();

    // Initialize sensors
    initSensors();

    // Set LOGO! IP address
    logoIP.fromString(LOGO_IP_STR);

    // Connect WiFi
    WiFi.disconnect(true);
    delay(200);
    WiFi.mode(WIFI_STA);
    delay(200);
    connectWiFi();

    // Connect MQTT (blocking in setup is fine)
    Serial.print("[MQTT] Connecting...");
    int8_t ret;
    int attempts = 0;
    while ((ret = mqtt.connect()) != 0) {
        Serial.printf(" failed (%s), retry in 5s\n",
                      mqtt.connectErrorString(ret));
        mqtt.disconnect();
        delay(5000);
        if (++attempts > 5) ESP.restart();
    }
    Serial.println(" connected!");

    initModbusServer();

    Serial.println("[Setup] Complete.");
    Serial.println("  Valve/Pump controlled via HTTP polling");
    Serial.println("  I3  (addr=2) → flow-direction feed");
}

// ============================================================
//  loop
// ============================================================
void loop() {
    loopCount++;

    connectWiFi();
    checkMQTT();
    readMQTTSubscriptions();

    modbus.task();

    if (millis() - lastHeartbeat > HEARTBEAT_MS) {
        lastHeartbeat = millis();
        Serial.printf("[hb] loop=%lu uptime=%lus\n",
                      loopCount, millis() / 1000);
    }

    probeLOGO();

    unsigned long now = millis();

    if (now - lastLogoTime > LOGO_INTERVAL_MS) {
        lastLogoTime = now;
        readAndPublishLOGO();
    }

    if (now - lastSensorTime > SENSOR_INTERVAL_MS) {
        lastSensorTime = now;
        readAndPublishSensors();
    }
}
