// ============================================================
//  sensors.cpp — DHT11 and MQ135 sensor implementations
// ============================================================

#include "sensors.h"
#include "wifi_mqtt.h"
#include <Arduino.h>

// ------------------------------------------------------------
//  Object definition
// ------------------------------------------------------------
DHT dht(DHT_PIN, DHT_TYPE);

// ------------------------------------------------------------
//  Private state
// ------------------------------------------------------------
static bool alertActive = false;

// ------------------------------------------------------------
//  initSensors — called once in setup()
// ------------------------------------------------------------
void initSensors() {
    pinMode(MQ135_DOUT, INPUT);
    pinMode(LED_PIN,    OUTPUT);
    digitalWrite(LED_PIN, LOW);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    dht.begin();
    Serial.println("[DHT11] Initialized on GPIO 15");
    Serial.println("[MQ135] Initialized on GPIO 34");
}

// ------------------------------------------------------------
//  readAndPublishSensors — called every SENSOR_INTERVAL_MS
// ------------------------------------------------------------
void readAndPublishSensors() {

    // --- DHT11 ---
    float temperature = dht.readTemperature();
    float humidity    = dht.readHumidity();

    if (!isnan(temperature) && !isnan(humidity)) {
        Serial.printf("[DHT11] T=%.1fC H=%.1f%%\n",
                      temperature, humidity);
        if (mqtt.connected()) {
            pubTemp.publish(temperature);
            pubHumidity.publish(humidity);
        }
    } else {
        Serial.println("[DHT11] ERROR: Failed to read!");
    }

    // --- MQ135 (no warmup) ---
    long sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += analogRead(MQ135_AOUT);
        delay(5);
    }
    int raw = (int)(sum / 10);
    int pct = constrain(map(raw, 0, 4095, 0, 100), 0, 100);
    Serial.printf("[MQ135] raw=%d pct=%d%%\n", raw, pct);

    if (mqtt.connected()) {
        pubAirPct.publish(pct);

        // Air alert — publish only on state change
        if (raw > AIR_ALERT_THRESHOLD && !alertActive) {
            alertActive = true;
            pubAirAlert.publish("ALERT");
            Serial.println("[ALERT] Poor air quality!");
            // Blink LED 3 times
            for (int i = 0; i < 3; i++) {
                digitalWrite(LED_PIN, HIGH); delay(100);
                digitalWrite(LED_PIN, LOW);  delay(100);
            }
        } else if (raw <= AIR_ALERT_THRESHOLD && alertActive) {
            alertActive = false;
            pubAirAlert.publish("OK");
            Serial.println("[ALERT] Air quality OK.");
        }
    }
}
