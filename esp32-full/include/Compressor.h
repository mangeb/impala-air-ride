#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <Arduino.h>
#include "config.h"

// Pump operation modes
enum PumpMode {
    PUMP_AUTO,      // Automatic based on tank pressure
    PUMP_OFF,       // Manual override - both pumps off
    PUMP_BOTH_ON,   // Manual override - both pumps on
    PUMP_1_ONLY,    // Manual override - pump 1 only
    PUMP_2_ONLY     // Manual override - pump 2 only
};

class Compressor {
  public:
    Compressor(uint8_t pump1Pin, uint8_t pump2Pin);

    void begin();
    void update(float tankPressure);

    // Manual overrides
    void setMode(PumpMode mode);
    PumpMode getMode() const { return currentMode; }

    // Set target pressure (for auto mode)
    void setTargetPressure(float psi);
    float getTargetPressure() const { return targetPressure; }

    // Status
    bool isPump1Running() const { return pump1On; }
    bool isPump2Running() const { return pump2On; }
    bool isRunning() const { return pump1On || pump2On; }

    // Get string representation of mode
    const char* getModeString() const;

    // Runtime tracking (for maintenance)
    unsigned long getPump1RuntimeMs() const { return pump1RuntimeMs; }
    unsigned long getPump2RuntimeMs() const { return pump2RuntimeMs; }
    float getPump1RuntimeHours() const { return pump1RuntimeMs / 3600000.0; }
    float getPump2RuntimeHours() const { return pump2RuntimeMs / 3600000.0; }
    void loadRuntimeFromEEPROM();
    void saveRuntimeToEEPROM();

    // Maintenance status
    bool isPump1MaintenanceDue() const { return getPump1RuntimeHours() >= PUMP_MAINTENANCE_HOURS; }
    bool isPump2MaintenanceDue() const { return getPump2RuntimeHours() >= PUMP_MAINTENANCE_HOURS; }
    bool isPump1Overdue() const { return getPump1RuntimeHours() >= PUMP_OVERDUE_HOURS; }
    bool isPump2Overdue() const { return getPump2RuntimeHours() >= PUMP_OVERDUE_HOURS; }
    bool isMaintenanceDue() const { return isPump1MaintenanceDue() || isPump2MaintenanceDue(); }
    void resetPump1Runtime();
    void resetPump2Runtime();

  private:
    uint8_t pump1Pin;
    uint8_t pump2Pin;

    PumpMode currentMode;
    float targetPressure;

    bool pump1On;
    bool pump2On;

    // Alternating pump logic
    bool alternatePump;          // Which pump to use when alternating (false=pump1, true=pump2)
    unsigned long lastSwitchTime;

    // Runtime tracking
    unsigned long pump1RuntimeMs;
    unsigned long pump2RuntimeMs;
    unsigned long lastRuntimeUpdate;
    unsigned long lastEEPROMSave;

    void setPump1(bool on);
    void setPump2(bool on);
    void runAutoMode(float tankPressure);
    void updateRuntime();
};

#endif // COMPRESSOR_H
