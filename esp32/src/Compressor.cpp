#include "Compressor.h"

Compressor::Compressor(uint8_t p1Pin, uint8_t p2Pin)
    : pump1Pin(p1Pin),
      pump2Pin(p2Pin),
      currentMode(PUMP_AUTO),
      targetPressure(TANK_MAX_PSI),
      pump1On(false),
      pump2On(false),
      alternatePump(false),
      lastSwitchTime(0),
      bothPumpsActive(false) {
}

void Compressor::begin() {
    pinMode(pump1Pin, OUTPUT);
    pinMode(pump2Pin, OUTPUT);

    // Ensure pumps are off at startup
    setPump1(false);
    setPump2(false);
}

void Compressor::update(float tankPressure) {
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
}

void Compressor::runAutoMode(float tankPressure) {
    unsigned long currentTime = millis();

    // Tank is full - turn off both pumps
    if (tankPressure >= targetPressure) {
        setPump1(false);
        setPump2(false);
        bothPumpsActive = false;
        return;
    }

    // Hysteresis for dual/single pump threshold:
    // Turn both on when dropping below PUMP_BOTH_ON_THRESHOLD (70 PSI)
    // Switch to single when rising above PUMP_BOTH_OFF_THRESHOLD (75 PSI)
    if (tankPressure <= PUMP_BOTH_ON_THRESHOLD) {
        bothPumpsActive = true;
    } else if (tankPressure >= PUMP_BOTH_OFF_THRESHOLD) {
        bothPumpsActive = false;
    }

    // Tank needs air
    if (tankPressure < TANK_MIN_PSI) {
        if (bothPumpsActive) {
            setPump1(true);
            setPump2(true);
        } else {
            // Alternate single pump to reduce wear
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
