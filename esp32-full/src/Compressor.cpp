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
      filling(false),
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

    // Hysteresis: start filling when below TANK_MIN_PSI, stop at targetPressure
    // This prevents rapid on/off cycling when pressure hovers near the target
    if (tankPressure >= targetPressure) {
        if (filling) {
            Serial.print("[PUMP] Tank full (");
            Serial.print(tankPressure, 1);
            Serial.println(" PSI) - pumps OFF");
            filling = false;
        }
        setPump1(false);
        setPump2(false);
        return;
    }

    // Start a new fill cycle only when pressure drops below TANK_MIN_PSI
    if (!filling) {
        if (tankPressure < TANK_MIN_PSI) {
            filling = true;
            Serial.print("[PUMP] Tank below ");
            Serial.print(TANK_MIN_PSI, 0);
            Serial.print(" PSI (");
            Serial.print(tankPressure, 1);
            Serial.println(" PSI) - starting fill cycle");
        } else {
            // Between TANK_MIN_PSI and targetPressure, but not in a fill cycle
            // Don't start pumps — wait for pressure to drop below TANK_MIN_PSI
            setPump1(false);
            setPump2(false);
            return;
        }
    }

    // Active fill cycle: choose pump strategy based on pressure
    if (tankPressure <= PUMP_BOTH_ON_THRESHOLD) {
        // Very low - run both pumps for maximum fill rate
        if (!pump1On || !pump2On) {
            Serial.print("[PUMP] Tank low (");
            Serial.print(tankPressure, 1);
            Serial.println(" PSI) - BOTH pumps ON");
        }
        setPump1(true);
        setPump2(true);
    } else {
        // Above threshold - alternate single pump to reduce wear
        if (currentTime - lastSwitchTime >= PUMP_SWITCH_INTERVAL) {
            alternatePump = !alternatePump;
            lastSwitchTime = currentTime;
            Serial.print("[PUMP] Alternating to P");
            Serial.print(alternatePump ? "2" : "1");
            Serial.print(" (tank=");
            Serial.print(tankPressure, 1);
            Serial.println(" PSI)");
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
    if (mode != currentMode) {
        Serial.print("[PUMP] Mode changed to ");
        const char* names[] = {"AUTO", "OFF", "BOTH", "P1 ONLY", "P2 ONLY"};
        Serial.println(names[mode]);
        // Reset fill cycle when switching modes
        if (mode != PUMP_AUTO) {
            filling = false;
        }
    }
    currentMode = mode;
}

void Compressor::setTargetPressure(float psi) {
    if (psi > TANK_MAX_PSI) psi = TANK_MAX_PSI;
    if (psi < TANK_MIN_PSI) psi = TANK_MIN_PSI;
    targetPressure = psi;
}

void Compressor::setPump1(bool on) {
    if (on != pump1On) {
        Serial.print("[PUMP] P1 ");
        Serial.println(on ? "ON" : "OFF");
    }
    pump1On = on;
    digitalWrite(pump1Pin, on ? RELAY_ON : RELAY_OFF);
}

void Compressor::setPump2(bool on) {
    if (on != pump2On) {
        Serial.print("[PUMP] P2 ");
        Serial.println(on ? "ON" : "OFF");
    }
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
