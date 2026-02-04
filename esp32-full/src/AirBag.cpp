#include "AirBag.h"

AirBag::AirBag(uint8_t pressurePin, uint8_t inflatePin, uint8_t deflatePin, const char* name)
    : pressureSensorPin(pressurePin),
      inflateSolenoidPin(inflatePin),
      deflateSolenoidPin(deflatePin),
      bagName(name),
      currentPressure(0.0),
      targetPressure(0.0),
      state(VALVE_HOLD),
      solenoidOnStartTime(0),
      solenoidTimedOut(false),
      timeoutCooldownStart(0),
      bufferIndex(0),
      bufferFilled(false),
      calibrated(false) {
    // Initialize pressure buffer
    for (int i = 0; i < PRESSURE_SAMPLES; i++) {
        pressureBuffer[i] = 0.0;
    }
    // Default calibration (no correction)
    calibration.offset = 0.0;
    calibration.gain = 1.0;
    calibration.refResistor = REFERENCE_RESISTOR;
}

void AirBag::begin() {
    pinMode(inflateSolenoidPin, OUTPUT);
    pinMode(deflateSolenoidPin, OUTPUT);

    // Ensure both solenoids are off at startup (valves closed, bag holds)
    digitalWrite(inflateSolenoidPin, RELAY_OFF);
    digitalWrite(deflateSolenoidPin, RELAY_OFF);
    state = VALVE_HOLD;

    // Fill pressure buffer with initial readings
    for (int i = 0; i < PRESSURE_SAMPLES; i++) {
        pressureBuffer[i] = readPressure();
        delay(PRESSURE_SAMPLE_DELAY);
    }
    bufferFilled = true;
    bufferIndex = 0;

    currentPressure = readPressureSmoothed();
    targetPressure = currentPressure;
}

void AirBag::update() {
    // Add new reading to buffer
    pressureBuffer[bufferIndex] = readPressure();
    bufferIndex = (bufferIndex + 1) % PRESSURE_SAMPLES;

    // Use smoothed reading
    currentPressure = readPressureSmoothed();

    // Check solenoid timeout
    checkSolenoidTimeout();

    // Safety checks - stop if at limits
    if (state == VALVE_INFLATE && isAtMaxPressure()) {
        hold();
    }
    if (state == VALVE_DEFLATE && isAtMinPressure()) {
        hold();
    }
}

float AirBag::readPressure() {
    if (demoMode) {
        // Simulate pressure changes based on valve state
        // Physics ported from frontend simulation, scaled for 100ms interval
        float simPressure = currentPressure > 0 ? currentPressure : DEMO_BAG_PSI;

        if (state == VALVE_INFLATE) {
            // Differential pressure based fill (faster when tank >> bag)
            float deltaP = max(0.0f, simTankPressure - simPressure);
            if (deltaP > 1.0f) {
                float fillSpeed = SIM_BAG_INFLATE_RATE * sqrt(deltaP);
                simPressure += fillSpeed;
            }
            if (simPressure > MAX_BAG_PSI) simPressure = MAX_BAG_PSI;
        } else if (state == VALVE_DEFLATE) {
            // Dump to atmosphere - faster at higher pressure
            float dumpSpeed = SIM_BAG_DEFLATE_RATE * sqrt(max(0.0f, simPressure));
            simPressure -= dumpSpeed;
            if (simPressure < MIN_BAG_PSI) simPressure = MIN_BAG_PSI;
        }

        // Apply simulated leak on this bag
        if (simLeakTarget >= 0 && simLeakTarget <= 3) {
            // Map bag name to index for comparison
            int bagIdx = -1;
            if (bagName[0] == 'F' && bagName[1] == 'L') bagIdx = 0;
            else if (bagName[0] == 'F' && bagName[1] == 'R') bagIdx = 1;
            else if (bagName[0] == 'R' && bagName[1] == 'L') bagIdx = 2;
            else if (bagName[0] == 'R' && bagName[1] == 'R') bagIdx = 3;
            if (bagIdx == simLeakTarget) {
                simPressure -= simLeakRate;
                if (simPressure < 0) simPressure = 0;
            }
        }

        // Add realistic jitter
        simPressure += (random(-SIM_JITTER_RANGE, SIM_JITTER_RANGE) / 10000.0f);

        return simPressure;
    }

    // ESP32: 12-bit ADC (0-4095), 3.3V reference
    int rawValue = analogRead(pressureSensorPin);
    float voltage = (rawValue / ADC_RESOLUTION) * ADC_REFERENCE_VOLTAGE;

    // Convert voltage to resistance using voltage divider formula
    float resistance = resistanceFromVoltage(voltage);

    // Convert resistance to PSI, then apply calibration
    float rawPsi = resistanceToPsi(resistance);
    return applyCalibration(rawPsi);
}

float AirBag::readRawPressure() {
    if (demoMode) {
        return currentPressure; // In demo mode, raw = current
    }
    int rawValue = analogRead(pressureSensorPin);
    float voltage = (rawValue / ADC_RESOLUTION) * ADC_REFERENCE_VOLTAGE;
    float resistance = resistanceFromVoltage(voltage);
    return resistanceToPsi(resistance);
}

float AirBag::applyCalibration(float rawPsi) {
    return (rawPsi * calibration.gain) + calibration.offset;
}

void AirBag::setCalibration(const SensorCalibration& cal) {
    calibration = cal;
    calibrated = (cal.offset != 0.0 || cal.gain != 1.0 || cal.refResistor != REFERENCE_RESISTOR);
}

float AirBag::readPressureSmoothed() {
    // Average all samples in buffer
    float sum = 0.0;
    int count = bufferFilled ? PRESSURE_SAMPLES : bufferIndex;

    if (count == 0) {
        return readPressure();
    }

    for (int i = 0; i < count; i++) {
        sum += pressureBuffer[i];
    }
    return sum / count;
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
    // Uses per-sensor calibrated reference resistor value
    float resistance = calibration.refResistor * voltage / (ADC_REFERENCE_VOLTAGE - voltage);

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

    // Check if in cooldown period
    if (solenoidTimedOut) {
        if (millis() - timeoutCooldownStart < SOLENOID_COOLDOWN_MS) {
            return; // Still in cooldown
        }
        solenoidTimedOut = false; // Cooldown complete
    }

    // RideTech Big Red: Open inflate solenoid, close deflate
    digitalWrite(deflateSolenoidPin, RELAY_OFF);  // Close deflate first
    digitalWrite(inflateSolenoidPin, RELAY_ON);   // Open inflate

    if (state != VALVE_INFLATE) {
        solenoidOnStartTime = millis();
    }
    state = VALVE_INFLATE;
}

void AirBag::deflate() {
    // Check if in cooldown period
    if (solenoidTimedOut) {
        if (millis() - timeoutCooldownStart < SOLENOID_COOLDOWN_MS) {
            return; // Still in cooldown
        }
        solenoidTimedOut = false; // Cooldown complete
    }

    // RideTech Big Red: Close inflate solenoid, open deflate
    digitalWrite(inflateSolenoidPin, RELAY_OFF);  // Close inflate first
    digitalWrite(deflateSolenoidPin, RELAY_ON);   // Open deflate (dump)

    if (state != VALVE_DEFLATE) {
        solenoidOnStartTime = millis();
    }
    state = VALVE_DEFLATE;
}

void AirBag::hold() {
    // RideTech Big Red: Close both solenoids - bag holds pressure
    digitalWrite(inflateSolenoidPin, RELAY_OFF);
    digitalWrite(deflateSolenoidPin, RELAY_OFF);
    state = VALVE_HOLD;
    solenoidOnStartTime = 0;
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

void AirBag::checkSolenoidTimeout() {
    if (state == VALVE_HOLD) {
        return;
    }

    if (solenoidOnStartTime > 0 &&
        millis() - solenoidOnStartTime > SOLENOID_TIMEOUT_MS) {
        // Solenoid has been on too long - force hold
        hold();
        solenoidTimedOut = true;
        timeoutCooldownStart = millis();
        Serial.print("WARNING: ");
        Serial.print(bagName);
        Serial.println(" solenoid timeout - cooling down");
    }
}

void AirBag::resetSolenoidTimeout() {
    solenoidTimedOut = false;
    timeoutCooldownStart = 0;
}

unsigned long AirBag::getSolenoidOnTime() const {
    if (state == VALVE_HOLD || solenoidOnStartTime == 0) {
        return 0;
    }
    return millis() - solenoidOnStartTime;
}
