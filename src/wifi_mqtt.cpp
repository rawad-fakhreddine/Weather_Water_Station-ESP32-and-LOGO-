// ============================================================
//  wifi_mqtt.cpp — WiFi and MQTT implementations
// ============================================================

#include "wifi_mqtt.h"
#include "modbus_logo.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool anyPendingCommand = false;

// ------------------------------------------------------------
//  Object definitions
// ------------------------------------------------------------
WiFiClient mqttClient;
Adafruit_MQTT_Client mqtt(&mqttClient, AIO_SERVER, AIO_PORT,
                          AIO_USERNAME, AIO_KEY);

// Publish feeds
Adafruit_MQTT_Publish pubTemp        (&mqtt, FEED("temperature"));
Adafruit_MQTT_Publish pubHumidity    (&mqtt, FEED("humidity"));
Adafruit_MQTT_Publish pubAirPct      (&mqtt, FEED("air-quality-percent"));
Adafruit_MQTT_Publish pubAirAlert    (&mqtt, FEED("air-alert"));
Adafruit_MQTT_Publish pubFlow        (&mqtt, FEED("flow-direction"));
Adafruit_MQTT_Publish pubValveStatus (&mqtt, FEED("valve"));
Adafruit_MQTT_Publish pubPumpStatus  (&mqtt, FEED("pump"));

// ------------------------------------------------------------
//  connectWiFi
// ------------------------------------------------------------
void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;
    Serial.print("[WiFi] Connecting to ");
    Serial.print(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (++attempts > 40) {
            Serial.println("\n[WiFi] Timeout. Restarting...");
            ESP.restart();
        }
    }
    Serial.printf("\n[WiFi] Connected! IP: %s\n",
                  WiFi.localIP().toString().c_str());
    WiFi.setSleep(false);
}

// ------------------------------------------------------------
//  checkMQTT
// ------------------------------------------------------------
static unsigned long lastReconnectAttempt = 0;

void checkMQTT() {
    if (mqtt.connected()) return;
    unsigned long now = millis();
    if (now - lastReconnectAttempt < RECONNECT_INTERVAL_MS) return;
    lastReconnectAttempt = now;
    Serial.print("[MQTT] Reconnecting...");
    if (mqtt.connect() == 0) {
        Serial.println(" connected!");
    } else {
        Serial.println(" failed, retry in 5s");
        mqtt.disconnect();
    }
}

// ------------------------------------------------------------
//  fetchFeedValue — HTTP REST API poll
// ------------------------------------------------------------
static String fetchFeedValue(const char* feedKey) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    HTTPClient http;
    String url = String("https://io.adafruit.com/api/v2/")
                 + AIO_USERNAME + "/feeds/" + feedKey + "/data/last";
    http.begin(secureClient, url);
    http.addHeader("X-AIO-Key", AIO_KEY);
    String val = "";
    int httpCode = http.GET();
    if (httpCode == 200) {
        String body = http.getString();
        int idx = body.indexOf("\"value\":\"");
        if (idx >= 0) {
            int start = idx + 9;
            int end   = body.indexOf("\"", start);
            val = body.substring(start, end);
        }
    } else {
        Serial.printf("[HTTP] fetchFeed %s failed: %d\n", feedKey, httpCode);
    }
    http.end();
    return val;
}

// ------------------------------------------------------------
//  readMQTTSubscriptions — alternating poll + latching logic
// ------------------------------------------------------------
void readMQTTSubscriptions() {
    static unsigned long lastPoll      = 0;
    static bool pendingValveOn  = false;
    static bool pendingValveOff = false;
    static bool pendingPumpOn   = false;
    static bool pendingPumpOff  = false;
    static String lastValveOn  = "";
    static String lastValveOff = "";
    static String lastPumpOn   = "";
    static String lastPumpOff  = "";
    static bool pollOnFeeds = true;  // alternate ON/OFF feeds each cycle

    if (millis() - lastPoll < CONTROL_POLL_MS) return;
    lastPoll = millis();

    if (pollOnFeeds) {
        // --- Cycle A: poll ON feeds only (2 HTTP calls ~3s) ---
        String valveOn = fetchFeedValue("valve");
        if (valveOn.length() > 0 && valveOn != lastValveOn) {
            lastValveOn = valveOn;
            if (valveOn == "1" || valveOn == "ON" || valveOn == "on") {
                pendingValveOn  = true;
                pendingValveOff = false;
                Serial.println("[HTTP] Valve ON button detected");
            }
        }
        String pumpOn = fetchFeedValue("pump");
        if (pumpOn.length() > 0 && pumpOn != lastPumpOn) {
            lastPumpOn = pumpOn;
            if (pumpOn == "1" || pumpOn == "ON" || pumpOn == "on") {
                pendingPumpOn  = true;
                pendingPumpOff = false;
                Serial.println("[HTTP] Pump ON button detected");
            }
        }
    } else {
        // --- Cycle B: poll OFF feeds only (2 HTTP calls ~3s) ---
        String valveOff = fetchFeedValue("valve-off");
        if (valveOff.length() > 0 && valveOff != lastValveOff) {
            lastValveOff = valveOff;
            if (valveOff == "1" || valveOff == "ON" || valveOff == "on") {
                pendingValveOff = true;
                pendingValveOn  = false;
                Serial.println("[HTTP] Valve OFF button detected");
            }
        }
        String pumpOff = fetchFeedValue("pump-off");
        if (pumpOff.length() > 0 && pumpOff != lastPumpOff) {
            lastPumpOff = pumpOff;
            if (pumpOff == "1" || pumpOff == "ON" || pumpOff == "on") {
                pendingPumpOff = true;
                pendingPumpOn  = false;
                Serial.println("[HTTP] Pump OFF button detected");
            }
        }
    }

    pollOnFeeds = !pollOnFeeds;  // flip for next cycle

    // Update global flag
    anyPendingCommand = (pendingValveOn || pendingValveOff ||
                         pendingPumpOn  || pendingPumpOff);

    // --- Valve ON: assert coil every cycle until NQ4 confirms Q1 ON ---
    if (pendingValveOn) {
        modbus.Coil(COIL_VALVE,     true);
        modbus.Coil(COIL_VALVE_OFF, false);
        Serial.println("[HTTP] Valve SET pending → asserting");
        if (getQ1State()) {
            pendingValveOn = false;
            setLastValveState(true);
            Serial.println("[HTTP] Valve ON confirmed by NQ4");
        }
    }

    // --- Valve OFF: assert reset coil every cycle until NQ4 confirms Q1 OFF ---
    if (pendingValveOff) {
        modbus.Coil(COIL_VALVE_OFF, true);
        modbus.Coil(COIL_VALVE,     false);
        Serial.println("[HTTP] Valve RESET pending → asserting");
        if (getLastValveState() && !getQ1State()) {
            pendingValveOff = false;
            modbus.Coil(COIL_VALVE_OFF, false);
            setLastValveState(false);
            Serial.println("[HTTP] Valve OFF confirmed by NQ4");
        } else if (!getLastValveState()) {
            pendingValveOff = false;
            modbus.Coil(COIL_VALVE_OFF, false);
            Serial.println("[HTTP] Valve already OFF, cleared");
        }
    }

    // --- Pump ON: assert coil every cycle until NQ5 confirms Q2 ON ---
    if (pendingPumpOn) {
        modbus.Coil(COIL_PUMP,     true);
        modbus.Coil(COIL_PUMP_OFF, false);
        Serial.println("[HTTP] Pump SET pending → asserting");
        if (getQ2State()) {
            pendingPumpOn = false;
            setLastPumpState(true);
            Serial.println("[HTTP] Pump ON confirmed by NQ5");
        }
    }

    // --- Pump OFF: assert reset coil every cycle until NQ5 confirms Q2 OFF ---
    if (pendingPumpOff) {
        modbus.Coil(COIL_PUMP_OFF, true);
        modbus.Coil(COIL_PUMP,     false);
        Serial.println("[HTTP] Pump RESET pending → asserting");
        if (getLastPumpState() && !getQ2State()) {
            pendingPumpOff = false;
            modbus.Coil(COIL_PUMP_OFF, false);
            setLastPumpState(false);
            Serial.println("[HTTP] Pump OFF confirmed by NQ5");
        } else if (!getLastPumpState()) {
            pendingPumpOff = false;
            modbus.Coil(COIL_PUMP_OFF, false);
            Serial.println("[HTTP] Pump already OFF, cleared");
        }
    }

    // Update flag after applying
    anyPendingCommand = (pendingValveOn || pendingValveOff ||
                         pendingPumpOn  || pendingPumpOff);
}