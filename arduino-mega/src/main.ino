/*
 * Air Ride Suspension Controller
 * 1964 Chevrolet Impala
 *
 * Controls 4 air bags with independent pressure monitoring (VDO 10-180 ohm sensors)
 * and RideTech Big Red 4-way manifold (8 solenoids - 2 per corner).
 * Dual compressor pumps with smart control: both on below 70 PSI, alternating above.
 *
 * WiFi AP mode for mobile phone control.
 */

#include "config.h"
#include "AirBag.h"
#include "Compressor.h"
#include "WebServer.h"

// ============================================
// GLOBAL OBJECTS
// ============================================

// RideTech Big Red: 2 solenoids per corner (inflate + deflate)
AirBag bags[NUM_BAGS] = {
    AirBag(FRONT_LEFT_PRESSURE_PIN,  FRONT_LEFT_INFLATE_PIN,  FRONT_LEFT_DEFLATE_PIN,  "FL"),
    AirBag(FRONT_RIGHT_PRESSURE_PIN, FRONT_RIGHT_INFLATE_PIN, FRONT_RIGHT_DEFLATE_PIN, "FR"),
    AirBag(REAR_LEFT_PRESSURE_PIN,   REAR_LEFT_INFLATE_PIN,   REAR_LEFT_DEFLATE_PIN,   "RL"),
    AirBag(REAR_RIGHT_PRESSURE_PIN,  REAR_RIGHT_INFLATE_PIN,  REAR_RIGHT_DEFLATE_PIN,  "RR")
};

Compressor compressor(PUMP_1_PIN, PUMP_2_PIN);
WebServer webServer(bags, &compressor, &tankPressure);

float tankPressure = 0.0;
unsigned long lastPressureRead = 0;

// ============================================
// FUNCTION PROTOTYPES
// ============================================

float readTankPressure();
void updateTargetTracking();
void printStatus();
void printHelp();
void processSerialCommand();
void stopAllBags();

// ============================================
// SETUP
// ============================================

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.println(F("================================"));
    Serial.println(F("Air Ride Controller v2.0"));
    Serial.println(F("1964 Chevrolet Impala"));
    Serial.println(F("================================"));

    // Initialize all air bags
    for (int i = 0; i < NUM_BAGS; i++) {
        bags[i].begin();
    }

    // Initialize compressor
    compressor.begin();

    // Initialize WiFi web server
    webServer.begin();

    // Initial tank reading
    tankPressure = readTankPressure();

    Serial.println(F("System Ready"));
    printHelp();
}

// ============================================
// MAIN LOOP
// ============================================

void loop() {
    unsigned long currentTime = millis();

    // Handle WiFi clients
    webServer.update();

    // Read pressures at defined interval
    if (currentTime - lastPressureRead >= PRESSURE_READ_INTERVAL) {
        lastPressureRead = currentTime;

        // Update tank pressure
        tankPressure = readTankPressure();

        // Update compressor (handles pump logic automatically)
        compressor.update(tankPressure);

        // Safety: stop all inflation if tank critically low
        if (tankPressure < TANK_CUTOFF_PSI) {
            for (int i = 0; i < NUM_BAGS; i++) {
                if (bags[i].isInflating()) {
                    bags[i].hold();
                    Serial.println(F("SAFETY: Inflation stopped - tank pressure critical"));
                }
            }
        }

        // Update all bags (reads pressure, enforces safety limits)
        for (int i = 0; i < NUM_BAGS; i++) {
            bags[i].update();
        }

        // Auto-adjust bags toward target pressure (for presets)
        updateTargetTracking();
    }

    // Process any serial commands
    if (Serial.available()) {
        processSerialCommand();
    }
}

// ============================================
// HELPER FUNCTIONS
// ============================================

float readTankPressure() {
    // Read ADC value and convert to voltage
    int rawValue = analogRead(TANK_PRESSURE_PIN);
    float voltage = (rawValue / ADC_RESOLUTION) * ADC_REFERENCE_VOLTAGE;

    // Convert voltage to resistance (VDO resistance-based sensor)
    // Voltage divider: R_sensor = R_ref * V_out / (V_in - V_out)
    float resistance;
    if (voltage >= ADC_REFERENCE_VOLTAGE - 0.01) {
        resistance = SENSOR_MAX_OHMS;
    } else if (voltage <= 0.01) {
        resistance = SENSOR_MIN_OHMS;
    } else {
        resistance = REFERENCE_RESISTOR * voltage / (ADC_REFERENCE_VOLTAGE - voltage);
    }

    // Clamp and convert resistance to PSI
    if (resistance < SENSOR_MIN_OHMS) resistance = SENSOR_MIN_OHMS;
    if (resistance > SENSOR_MAX_OHMS) resistance = SENSOR_MAX_OHMS;

    return ((resistance - SENSOR_MIN_OHMS) / (SENSOR_MAX_OHMS - SENSOR_MIN_OHMS)) * SENSOR_MAX_PSI;
}

void updateTargetTracking() {
    // Auto-adjust bags toward their target pressure
    // This enables preset functionality
    const float tolerance = 2.0;

    for (int i = 0; i < NUM_BAGS; i++) {
        float current = bags[i].getPressure();
        float target = bags[i].getTargetPressure();

        // Only auto-adjust if we have a meaningful target set
        if (target > 0 || bags[i].isInflating() || bags[i].isDeflating()) {
            if (current < target - tolerance) {
                if (!bags[i].isInflating() && tankPressure >= TANK_CUTOFF_PSI) {
                    bags[i].inflate();
                }
            } else if (current > target + tolerance) {
                if (!bags[i].isDeflating()) {
                    bags[i].deflate();
                }
            } else {
                // At target - hold
                if (!bags[i].isHolding()) {
                    bags[i].hold();
                }
            }
        }
    }
}

void printStatus() {
    Serial.println(F("========== STATUS =========="));

    // Tank and compressor status
    Serial.print(F("Tank: "));
    Serial.print(tankPressure, 1);
    Serial.print(F(" PSI | Pumps: "));
    Serial.print(compressor.getModeString());
    Serial.print(F(" ["));
    Serial.print(compressor.isPump1Running() ? F("P1:ON") : F("P1:off"));
    Serial.print(F(" "));
    Serial.print(compressor.isPump2Running() ? F("P2:ON") : F("P2:off"));
    Serial.print(F("] Target: "));
    Serial.print(compressor.getTargetPressure(), 0);
    Serial.println(F(" PSI"));

    // WiFi status
    Serial.print(F("WiFi: "));
    Serial.println(webServer.isConnected() ? F("AP Active") : F("Not Ready"));

    Serial.println(F("----------------------------"));

    // Bag status
    for (int i = 0; i < NUM_BAGS; i++) {
        Serial.print(bags[i].getName());
        Serial.print(F(": "));
        Serial.print(bags[i].getPressure(), 1);
        Serial.print(F("/"));
        Serial.print(bags[i].getTargetPressure(), 0);
        Serial.print(F(" PSI"));

        if (bags[i].isInflating()) {
            Serial.print(F(" [INFLATE]"));
        } else if (bags[i].isDeflating()) {
            Serial.print(F(" [DEFLATE]"));
        } else {
            Serial.print(F(" [hold]"));
        }
        Serial.println();
    }
    Serial.println(F("============================"));
}

void printHelp() {
    Serial.println(F("--- Commands ---"));
    Serial.println(F("Bags: I0-I3=inflate, D0-D3=deflate, H0-H3=hold, S=stop all"));
    Serial.println(F("      (0=FL, 1=FR, 2=RL, 3=RR)"));
    Serial.println(F("Pumps: PA=auto, PO=off, PB=both, P1=pump1, P2=pump2"));
    Serial.println(F("       PT###=set target PSI (e.g. PT120)"));
    Serial.println(F("Status: ?=help, P=print status"));
    Serial.println(F("WiFi: Connect to 'Impala64' password 'airride1964'"));
    Serial.println(F("----------------"));
}

void processSerialCommand() {
    char cmd = Serial.read();

    switch (cmd) {
        case 'I':
        case 'i': {
            // Inflate command: I0, I1, I2, I3
            while (!Serial.available());
            int bagNum = Serial.read() - '0';
            if (bagNum >= 0 && bagNum < NUM_BAGS) {
                if (tankPressure >= TANK_CUTOFF_PSI) {
                    bags[bagNum].inflate();
                    Serial.print(F("Inflating "));
                    Serial.println(bags[bagNum].getName());
                } else {
                    Serial.println(F("Cannot inflate - tank pressure too low"));
                }
            }
            break;
        }

        case 'D':
        case 'd': {
            // Deflate command: D0, D1, D2, D3
            while (!Serial.available());
            int bagNum = Serial.read() - '0';
            if (bagNum >= 0 && bagNum < NUM_BAGS) {
                bags[bagNum].deflate();
                Serial.print(F("Deflating "));
                Serial.println(bags[bagNum].getName());
            }
            break;
        }

        case 'H':
        case 'h': {
            // Hold command: H0, H1, H2, H3
            while (!Serial.available());
            int bagNum = Serial.read() - '0';
            if (bagNum >= 0 && bagNum < NUM_BAGS) {
                bags[bagNum].hold();
                Serial.print(F("Holding "));
                Serial.println(bags[bagNum].getName());
            }
            break;
        }

        case 'S':
        case 's':
            // Stop all bags
            stopAllBags();
            Serial.println(F("All bags stopped"));
            break;

        case 'P':
        case 'p': {
            // Pump commands or Print status
            if (Serial.available()) {
                char subCmd = Serial.read();
                switch (subCmd) {
                    case 'A':
                    case 'a':
                        compressor.setMode(PUMP_AUTO);
                        Serial.println(F("Pumps: AUTO mode"));
                        break;
                    case 'O':
                    case 'o':
                        compressor.setMode(PUMP_OFF);
                        Serial.println(F("Pumps: OFF (manual override)"));
                        break;
                    case 'B':
                    case 'b':
                        compressor.setMode(PUMP_BOTH_ON);
                        Serial.println(F("Pumps: BOTH ON (manual override)"));
                        break;
                    case '1':
                        compressor.setMode(PUMP_1_ONLY);
                        Serial.println(F("Pumps: PUMP 1 only (manual override)"));
                        break;
                    case '2':
                        compressor.setMode(PUMP_2_ONLY);
                        Serial.println(F("Pumps: PUMP 2 only (manual override)"));
                        break;
                    case 'T':
                    case 't': {
                        // Set target pressure: PT120
                        int psi = 0;
                        while (Serial.available()) {
                            char c = Serial.peek();
                            if (c >= '0' && c <= '9') {
                                psi = psi * 10 + (Serial.read() - '0');
                            } else {
                                break;
                            }
                        }
                        if (psi > 0) {
                            compressor.setTargetPressure((float)psi);
                            Serial.print(F("Tank target set to "));
                            Serial.print(psi);
                            Serial.println(F(" PSI"));
                        }
                        break;
                    }
                    default:
                        // Just 'P' alone - print status
                        printStatus();
                        break;
                }
            } else {
                // Just 'P' alone - print status
                delay(10); // Small delay to check if more chars coming
                if (!Serial.available()) {
                    printStatus();
                }
            }
            break;
        }

        case '?':
            printHelp();
            break;

        case '\n':
        case '\r':
            // Ignore newlines
            break;

        default:
            // Unknown command
            break;
    }
}

void stopAllBags() {
    for (int i = 0; i < NUM_BAGS; i++) {
        bags[i].hold();
    }
}
