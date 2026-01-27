#ifndef AIRBAG_H
#define AIRBAG_H

#include <Arduino.h>
#include "config.h"

// RideTech Big Red valve states
enum ValveState {
    VALVE_HOLD,     // Both solenoids OFF - bag holds pressure
    VALVE_INFLATE,  // Inflate solenoid ON - air from tank to bag
    VALVE_DEFLATE   // Deflate solenoid ON - air dumps to atmosphere
};

class AirBag {
  public:
    AirBag(uint8_t pressurePin, uint8_t inflatePin, uint8_t deflatePin, const char* name);

    void begin();
    void update();

    // Pressure reading
    float readPressure();
    float getPressure() const { return currentPressure; }

    // Valve control (RideTech Big Red - 2 solenoids per corner)
    void inflate();   // Open inflate solenoid (tank to bag)
    void deflate();   // Open deflate solenoid (bag to atmosphere)
    void hold();      // Close both solenoids (maintain pressure)

    // State checks
    ValveState getState() const { return state; }
    bool isInflating() const { return state == VALVE_INFLATE; }
    bool isDeflating() const { return state == VALVE_DEFLATE; }
    bool isHolding() const { return state == VALVE_HOLD; }
    bool isAtMinPressure() const { return currentPressure <= MIN_BAG_PSI; }
    bool isAtMaxPressure() const { return currentPressure >= MAX_BAG_PSI; }

    // Target pressure control
    void setTargetPressure(float psi);
    float getTargetPressure() const { return targetPressure; }
    bool isAtTarget(float tolerance = 2.0) const;

    const char* getName() const { return bagName; }

  private:
    uint8_t pressureSensorPin;
    uint8_t inflateSolenoidPin;
    uint8_t deflateSolenoidPin;
    const char* bagName;

    float currentPressure;
    float targetPressure;
    ValveState state;

    float resistanceFromVoltage(float voltage);
    float resistanceToPsi(float resistance);
};

#endif // AIRBAG_H
