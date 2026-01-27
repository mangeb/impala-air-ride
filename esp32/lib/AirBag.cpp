#include "AirBag.h"

AirBag::AirBag(uint8_t pressurePin, uint8_t inflatePin, uint8_t deflatePin, const char* name)
    : pressureSensorPin(pressurePin),
      inflateSolenoidPin(inflatePin),
      deflateSolenoidPin(deflatePin),
      bagName(name),
      currentPressure(0.0),
      targetPressure(0.0),
      state(VALVE_HOLD) {
}

void AirBag::begin() {
    pinMode(inflateSolenoidPin, OUTPUT);
    pinMode(deflateSolenoidPin, OUTPUT);

    // Ensure both solenoids are off at startup (valves closed, bag holds)
    digitalWrite(inflateSolenoidPin, RELAY_OFF);
    digitalWrite(deflateSolenoidPin, RELAY_OFF);
    state = VALVE_HOLD;

    // Initial pressure reading
    currentPressure = readPressure();
    targetPressure = currentPressure;
}

void AirBag::update() {
    currentPressure = readPressure();

    // Safety checks - stop if at limits
    if (state == VALVE_INFLATE && isAtMaxPressure()) {
        hold();
    }
    if (state == VALVE_DEFLATE && isAtMinPressure()) {
        hold();
    }
}

float AirBag::readPressure() {
    // ESP32: 12-bit ADC (0-4095), 3.3V reference
    int rawValue = analogRead(pressureSensorPin);
    float voltage = (rawValue / ADC_RESOLUTION) * ADC_REFERENCE_VOLTAGE;

    // Convert voltage to resistance using voltage divider formula
    float resistance = resistanceFromVoltage(voltage);

    // Convert resistance to PSI
    return resistanceToPsi(resistance);
}

float AirBag::resistanceFromVoltage(float voltage) {
    // Prevent division by zero
    if (voltage >= ADC_REFERENCE_VOLTAGE - 0.01) {
        return SENSOR_MAX_OHMS;
    }
    if (voltage <= 0.01) {
        return SENSOR_MIN_OHMS;
    }

    // Voltage divider formula solved for R_sensor
    // R_sensor = R_ref * V_out / (V_in - V_out)
    float resistance = REFERENCE_RESISTOR * voltage / (ADC_REFERENCE_VOLTAGE - voltage);

    return resistance;
}

float AirBag::resistanceToPsi(float resistance) {
    // Clamp resistance to sensor range
    if (resistance < SENSOR_MIN_OHMS) resistance = SENSOR_MIN_OHMS;
    if (resistance > SENSOR_MAX_OHMS) resistance = SENSOR_MAX_OHMS;

    // Linear interpolation from resistance to PSI
    // VDO 10-180 ohm: 10 ohm = 0 PSI, 180 ohm = 150 PSI
    float psi = ((resistance - SENSOR_MIN_OHMS) / (SENSOR_MAX_OHMS - SENSOR_MIN_OHMS)) * SENSOR_MAX_PSI;

    return psi;
}

void AirBag::inflate() {
    if (isAtMaxPressure()) {
        return; // Safety: don't exceed max pressure
    }

    // RideTech Big Red: Open inflate solenoid, close deflate
    digitalWrite(deflateSolenoidPin, RELAY_OFF);  // Close deflate first
    digitalWrite(inflateSolenoidPin, RELAY_ON);   // Open inflate
    state = VALVE_INFLATE;
}

void AirBag::deflate() {
    // No minimum check for Big Red - allow full dump to 0 PSI for "Lay" mode

    // RideTech Big Red: Close inflate solenoid, open deflate
    digitalWrite(inflateSolenoidPin, RELAY_OFF);  // Close inflate first
    digitalWrite(deflateSolenoidPin, RELAY_ON);   // Open deflate (dump)
    state = VALVE_DEFLATE;
}

void AirBag::hold() {
    // RideTech Big Red: Close both solenoids - bag holds pressure
    digitalWrite(inflateSolenoidPin, RELAY_OFF);
    digitalWrite(deflateSolenoidPin, RELAY_OFF);
    state = VALVE_HOLD;
}

void AirBag::setTargetPressure(float psi) {
    // Clamp to safe range
    if (psi < MIN_BAG_PSI) psi = MIN_BAG_PSI;
    if (psi > MAX_BAG_PSI) psi = MAX_BAG_PSI;
    targetPressure = psi;
}

bool AirBag::isAtTarget(float tolerance) const {
    return abs(currentPressure - targetPressure) <= tolerance;
}
