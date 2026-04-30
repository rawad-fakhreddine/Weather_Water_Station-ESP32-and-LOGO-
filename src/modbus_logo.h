#pragma once

// ============================================================
//  modbus_logo.h — Modbus TCP and LOGO! declarations
// ============================================================

#include <ModbusTCP.h>
#include <WiFi.h>
#include "config.h"

// ------------------------------------------------------------
//  Shared Modbus objects
// ------------------------------------------------------------
extern ModbusTCP modbus;
extern IPAddress logoIP;
extern bool anyPendingCommand;

// ------------------------------------------------------------
//  State getters/setters
//  Used by wifi_mqtt.cpp to access valve/pump state
// ------------------------------------------------------------
bool getLastValveState();
bool getLastPumpState();
void setLastValveState(bool state);
void setLastPumpState(bool state);

// ------------------------------------------------------------
//  Functions
// ------------------------------------------------------------
void     waitModbus();
bool     writeLOGOCoil(uint16_t addr, bool state, const char* label);  
bool     probeLOGO();
void     readAndPublishLOGO();
void     initModbusServer();
bool     getNQ1State();
bool     getNQ2State();
bool     getNQ3State();
bool     getQ1State();
bool     getQ2State();
bool     getNQ4State();
bool     getNQ5State();
