// ============================================================
//  modbus_logo.cpp — Modbus TCP and LOGO! implementations
// ============================================================

#include "modbus_logo.h"
#include "wifi_mqtt.h"
#include <Arduino.h>

// ------------------------------------------------------------
//  Object definitions
// ------------------------------------------------------------
ModbusTCP modbus;
IPAddress logoIP;

// ------------------------------------------------------------
//  Private state variables
// ------------------------------------------------------------
static bool          logoReachable  = false;
static bool          modbusBusy     = false;
static bool          lastValveState = false;
static bool          lastPumpState  = false;
static unsigned long lastProbeTime  = 0;
static bool          coilData[10]   = {false};
// coilData index map:
//  [0] = NQ1 (I1 feedback)
//  [1] = NQ2 (I2 feedback)
//  [2] = NQ3 (Flow / I3)
//  [3] = NQ4 (Q1 confirmed — Valve)
//  [4] = NQ5 (Q2 confirmed — Pump)

// ------------------------------------------------------------
//  State getters/setters
// ------------------------------------------------------------
bool getLastValveState()           { return lastValveState; }
bool getLastPumpState()            { return lastPumpState;  }
void setLastValveState(bool state) { lastValveState = state; }
void setLastPumpState(bool state)  { lastPumpState  = state; }
bool getNQ1State()                 { return coilData[0]; }
bool getNQ2State()                 { return coilData[1]; }
bool getNQ3State()                 { return coilData[2]; }
bool getQ1State()                  { return coilData[3]; }  // NQ4 → Q1 confirmed
bool getQ2State()                  { return coilData[4]; }  // NQ5 → Q2 confirmed
bool getNQ4State()                 { return coilData[3]; }
bool getNQ5State()                 { return coilData[4]; }

// ------------------------------------------------------------
//  waitModbus — waits for Modbus response
// ------------------------------------------------------------
void waitModbus() {
    unsigned long start = millis();
    while (millis() - start < MODBUS_TIMEOUT_MS) {
        modbus.task();
        yield();
        delay(1);
    }
}

// ------------------------------------------------------------
//  writeLOGOCoil — kept for potential future use
// ------------------------------------------------------------
bool writeLOGOCoil(uint16_t addr, bool state, const char* label) {
    if (modbusBusy) {
        Serial.printf("[Modbus] BUSY, skipping write %s\n", label);
        return false;
    }
    modbusBusy = true;
    if (!modbus.isConnected(logoIP)) {
        if (!modbus.connect(logoIP, LOGO_PORT)) {
            Serial.printf("[Modbus] FAILED to connect for %s\n", label);
            modbusBusy = false;
            return false;
        }
    }
    modbus.writeCoil(logoIP, addr, state, nullptr, 1);
    waitModbus();
    Serial.printf("[Modbus] WRITE %s addr=%u val=%s\n",
                  label, addr, state ? "ON" : "OFF");
    bool readback = false;
    modbus.readCoil(logoIP, addr, &readback, 1, nullptr, 1);
    waitModbus();
    Serial.printf("[Modbus] READBACK %s addr=%u → %d (expected %d) %s\n",
                  label, addr, (int)readback, (int)state,
                  (readback == state) ? "OK" : "MISMATCH");
    modbusBusy = false;
    return (readback == state);
}

// ------------------------------------------------------------
//  probeLOGO — check reachability, reassert coils every cycle
// ------------------------------------------------------------
bool probeLOGO() {
    if (millis() - lastProbeTime < LOGO_PROBE_INTERVAL_MS)
        return logoReachable;
    lastProbeTime = millis();

    WiFiClient probe;
    bool ok = probe.connect(logoIP, LOGO_PORT, 1000);
    if (ok) probe.stop();

    if (ok && !logoReachable) {
        Serial.println("[Modbus] LOGO! reachable — resending states");
        logoReachable = true;
    } else if (!ok && logoReachable) {
        Serial.println("[Modbus] LOGO! offline");
        logoReachable = false;
    }

    // Always reassert coil values every probe cycle when reachable
    if (logoReachable) {
        modbus.Coil(COIL_VALVE,     lastValveState);
        modbus.Coil(COIL_PUMP,      lastPumpState);
        modbus.Coil(COIL_VALVE_OFF, false);
        modbus.Coil(COIL_PUMP_OFF,  false);
    }

    return logoReachable;
}

// ------------------------------------------------------------
//  readAndPublishLOGO — publish valve/pump/flow status
//  AI1 removed — NAQ1 not working reliably
// ------------------------------------------------------------
void readAndPublishLOGO() {
    if (!logoReachable) {
        Serial.println("[LOGO!] offline, skipping read");
        return;
    }

    bool valveSt = getQ1State();   // NQ4 → coilData[3]
    bool pumpSt  = getQ2State();   // NQ5 → coilData[4]
    bool flow    = getNQ3State();  // NQ3 → coilData[2]

    Serial.println("------------------------------------");
    Serial.printf("[LOGO!] Q1(Valve)=%d Q2(Pump)=%d Flow=%d\n",
                  valveSt, pumpSt, flow);
    Serial.println("------------------------------------");

    if (mqtt.connected()) {
        pubFlow.publish((int32_t)(flow ? 1 : 0));
        // Only publish status when no pending command
        if (!anyPendingCommand) {
            pubValveStatus.publish((int32_t)(valveSt ? 1 : 0));
            pubPumpStatus.publish((int32_t)(pumpSt  ? 1 : 0));
        }
    }
}

// ------------------------------------------------------------
//  Modbus server callbacks
// ------------------------------------------------------------
uint16_t coilRead(TRegister* reg, uint16_t val) {
    return val;
}

uint16_t coilWrite(TRegister* reg, uint16_t val) {
    uint16_t addr  = reg->address.address;
    bool     state = (val != 0);

    // Log only on state changes; always update the shadow array.
    if (addr == COIL_NQ1_VALVE) {
        if (state != coilData[0]) Serial.printf("[Server] NQ1 (I1 feedback) → %s\n", state ? "ON" : "OFF");
        coilData[0] = state;
    } else if (addr == COIL_NQ2_PUMP) {
        if (state != coilData[1]) Serial.printf("[Server] NQ2 (I2 feedback) → %s\n", state ? "ON" : "OFF");
        coilData[1] = state;
    } else if (addr == COIL_NQ3_FLOW) {
        if (state != coilData[2]) Serial.printf("[Server] NQ3 (Flow) → %s\n", state ? "ON" : "OFF");
        coilData[2] = state;
    } else if (addr == COIL_NQ4_Q1) {
        if (state != coilData[3]) Serial.printf("[Server] NQ4 (Q1 Valve confirm) → %s\n", state ? "ON" : "OFF");
        coilData[3] = state;
    } else if (addr == COIL_NQ5_Q2) {
        if (state != coilData[4]) Serial.printf("[Server] NQ5 (Q2 Pump confirm) → %s\n", state ? "ON" : "OFF");
        coilData[4] = state;
    }
    return val;
}

// ------------------------------------------------------------
//  initModbusServer
// ------------------------------------------------------------
void initModbusServer() {
    modbus.server(503);
    modbus.addCoil(COIL_VALVE,     false);  // 0 → NI1  (Valve SET)
    modbus.addCoil(COIL_PUMP,      false);  // 1 → NI2  (Pump SET)
    modbus.addCoil(COIL_VALVE_OFF, false);  // 2 → NI3  (Valve RESET)
    modbus.addCoil(COIL_PUMP_OFF,  false);  // 3 → NI4  (Pump RESET)
    modbus.addCoil(COIL_NQ3_FLOW,  false);  // 4 → NQ3  (Flow / I3)
    modbus.addCoil(COIL_NQ1_VALVE, false);  // 5 → NQ1  (I1 feedback)
    modbus.addCoil(COIL_NQ2_PUMP,  false);  // 6 → NQ2  (I2 feedback)
    modbus.addCoil(COIL_NQ4_Q1,    false);  // 7 → NQ4  (Q1 confirmed)
    modbus.addCoil(COIL_NQ5_Q2,    false);  // 8 → NQ5  (Q2 confirmed)
    modbus.onSetCoil(COIL_NQ3_FLOW,  coilWrite);
    modbus.onSetCoil(COIL_NQ1_VALVE, coilWrite);
    modbus.onSetCoil(COIL_NQ2_PUMP,  coilWrite);
    modbus.onSetCoil(COIL_NQ4_Q1,    coilWrite);
    modbus.onSetCoil(COIL_NQ5_Q2,    coilWrite);
    // NAQ1 (AI1) removed — not working reliably
    Serial.println("[Modbus] Server started on port 503");
}