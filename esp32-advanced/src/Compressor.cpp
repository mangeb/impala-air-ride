#include "Compressor.h"
#include <EEPROM.h>

Compressor::Compressor(uint8_t p1Pin, uint8_t p2Pin)
    : pump1Pin(p1Pin),
      pump2Pin(p2Pin),
      currentMode(PUMP_AUTO),
      targetPressure(TANK_MAX_PSI),
      pump1On(false),
      pump2On(false),
      alternatePump(false),
      lastSwitchTime(0),
      pump1RuntimeMs(0),
      pump2RuntimeMs(0),
      lastRuntimeUpdate(0),
      lastEEPROMSave(0) {
}

void Compressor::begin() {
    pinMode(pump1Pin, OUTPUT);
    pinMode(pump2Pin, OUTPUT);

    // Ensure pumps are off at startup
    setPump1(false);
    setPump2(false);

    lastRuntimeUpdate = millis();

    // Load saved runtime from EEPROM
    loadRuntimeFromEEPROM();
}

void Compressor::update(float tankPressure) {
    // Update runtime counters
    updateRuntime();

    switch (currentMode) {
        case PUMP_AUTO:
            runAutoMode(tankPressure);
            break;

        case PUMP_OFF:
            setPump1(false);
            setPump2(false);
            break;

        case PUMP_BOTH_ON:
            setPump1(true);
            setPump2(true);
            break;

        case PUMP_1_ONLY:
            setPump1(true);
            setPump2(false);
            break;

        case PUMP_2_ONLY:
            setPump1(false);
            setPump2(true);
            break;
    }

    // Periodically save runtime to EEPROM (every 5 minutes)
    if (millis() - lastEEPROMSave > 300000) {
        saveRuntimeToEEPROM();
        lastEEPROMSave = millis();
    }
}

void Compressor::runAutoMode(float tankPressure) {
    unsigned long currentTime = millis();

    // Tank is full - turn off both pumps
    if (tankPressure >= targetPressure) {
        setPump1(false);
        setPump2(false);
        return;
    }

    // Tank needs air
    if (tankPressure < TANK_MIN_PSI) {
        // Below threshold - run both pumps for maximum fill rate
        if (tankPressure <= PUMP_BOTH_ON_THRESHOLD) {
            setPump1(true);
            setPump2(true);
        }
        // Above 70 PSI but below min - alternate pumps to reduce wear
        else {
            // Switch pumps periodically
            if (currentTime - lastSwitchTime >= PUMP_SWITCH_INTERVAL) {
                alternatePump = !alternatePump;
                lastSwitchTime = currentTime;
            }

            if (alternatePump) {
                setPump1(false);
                setPump2(true);
            } else {
                setPump1(true);
                setPump2(false);
            }
        }
    }
    // Tank is in acceptable range (between MIN and target)
    // Keep topping off with alternating single pump
    else if (tankPressure < targetPressure) {
        // Switch pumps periodically
        if (currentTime - lastSwitchTime >= PUMP_SWITCH_INTERVAL) {
            alternatePump = !alternatePump;
            lastSwitchTime = currentTime;
        }

        if (alternatePump) {
            setPump1(false);
            setPump2(true);
        } else {
            setPump1(true);
            setPump2(false);
        }
    }
}

void Compressor::setMode(PumpMode mode) {
    currentMode = mode;
}

void Compressor::setTargetPressure(float psi) {
    if (psi > TANK_MAX_PSI) psi = TANK_MAX_PSI;
    if (psi < TANK_MIN_PSI) psi = TANK_MIN_PSI;
    targetPressure = psi;
}

void Compressor::setPump1(bool on) {
    pump1On = on;
    digitalWrite(pump1Pin, on ? RELAY_ON : RELAY_OFF);
}

void Compressor::setPump2(bool on) {
    pump2On = on;
    digitalWrite(pump2Pin, on ? RELAY_ON : RELAY_OFF);
}

const char* Compressor::getModeString() const {
    switch (currentMode) {
        case PUMP_AUTO:     return "AUTO";
        case PUMP_OFF:      return "OFF";
        case PUMP_BOTH_ON:  return "BOTH";
        case PUMP_1_ONLY:   return "P1";
        case PUMP_2_ONLY:   return "P2";
        default:            return "???";
    }
}

void Compressor::updateRuntime() {
    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - lastRuntimeUpdate;
    lastRuntimeUpdate = currentTime;

    if (pump1On) {
        pump1RuntimeMs += elapsed;
    }
    if (pump2On) {
        pump2RuntimeMs += elapsed;
    }
}

void Compressor::loadRuntimeFromEEPROM() {
    // Check if EEPROM has valid data
    if (EEPROM.read(EEPROM_ADDR_MAGIC) == EEPROM_MAGIC) {
        float hours;
        EEPROM.get(EEPROM_ADDR_PUMP_HOURS, hours);
        // Store total hours split evenly (simplified - could store both separately)
        pump1RuntimeMs = (unsigned long)(hours * 3600000.0 / 2.0);
        pump2RuntimeMs = (unsigned long)(hours * 3600000.0 / 2.0);
    }
}

void Compressor::saveRuntimeToEEPROM() {
    float totalHours = (pump1RuntimeMs + pump2RuntimeMs) / 3600000.0;
    EEPROM.put(EEPROM_ADDR_PUMP_HOURS, totalHours);
    EEPROM.commit();
}

void Compressor::resetPump1Runtime() {
    pump1RuntimeMs = 0;
    saveRuntimeToEEPROM();
    Serial.println("Pump 1 runtime reset - maintenance complete");
}

void Compressor::resetPump2Runtime() {
    pump2RuntimeMs = 0;
    saveRuntimeToEEPROM();
    Serial.println("Pump 2 runtime reset - maintenance complete");
}
