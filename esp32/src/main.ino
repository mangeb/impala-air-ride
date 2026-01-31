/*
 * Air Ride Suspension Controller - ESP32 Version
 * 1964 Chevrolet Impala
 *
 * Controls 4 air bags with independent pressure monitoring (VDO 10-180 ohm sensors)
 * and RideTech Big Red 4-way manifold (8 solenoids - 2 per corner).
 * Dual compressor pumps with smart control: both on below 70 PSI, alternating above.
 *
 * ESP32 with built-in WiFi - no shield required!
 */

#include <WiFi.h>
#include <WebServer.h>  // ESP32 built-in WebServer library
#include "config.h"
#include "AirBag.h"
#include "Compressor.h"
#include "AirRideWebServer.h"

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

float tankPressure = 0.0;
AirRideWebServer webServer(bags, &compressor, &tankPressure);

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
    delay(1000); // Give serial time to connect

    Serial.println("================================");
    Serial.println("Air Ride Controller v2.0 (ESP32)");
    Serial.println("1964 Chevrolet Impala");
    Serial.println("================================");

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

    Serial.println("System Ready");
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
                    Serial.println("SAFETY: Inflation stopped - tank pressure critical");
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
    // ESP32: 12-bit ADC, 3.3V reference
    int rawValue = analogRead(TANK_PRESSURE_PIN);
    float voltage = (rawValue / ADC_RESOLUTION) * ADC_REFERENCE_VOLTAGE;

    // Convert voltage to resistance (VDO resistance-based sensor)
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
    Serial.println("========== STATUS ==========");

    // Tank and compressor status
    Serial.print("Tank: ");
    Serial.print(tankPressure, 1);
    Serial.print(" PSI | Pumps: ");
    Serial.print(compressor.getModeString());
    Serial.print(" [");
    Serial.print(compressor.isPump1Running() ? "P1:ON" : "P1:off");
    Serial.print(" ");
    Serial.print(compressor.isPump2Running() ? "P2:ON" : "P2:off");
    Serial.print("] Target: ");
    Serial.print(compressor.getTargetPressure(), 0);
    Serial.println(" PSI");

    // WiFi status
    Serial.print("WiFi: ");
    if (webServer.isConnected()) {
        Serial.print("AP Active - IP: ");
        Serial.println(webServer.getIP());
    } else {
        Serial.println("Not Ready");
    }

    Serial.println("----------------------------");

    // Bag status
    for (int i = 0; i < NUM_BAGS; i++) {
        Serial.print(bags[i].getName());
        Serial.print(": ");
        Serial.print(bags[i].getPressure(), 1);
        Serial.print("/");
        Serial.print(bags[i].getTargetPressure(), 0);
        Serial.print(" PSI");

        if (bags[i].isInflating()) {
            Serial.print(" [INFLATE]");
        } else if (bags[i].isDeflating()) {
            Serial.print(" [DEFLATE]");
        } else {
            Serial.print(" [hold]");
        }
        Serial.println();
    }
    Serial.println("============================");
}

void printHelp() {
    Serial.println("--- Commands ---");
    Serial.println("Bags: I0-I3=inflate, D0-D3=deflate, H0-H3=hold, S=stop all");
    Serial.println("      (0=FL, 1=FR, 2=RL, 3=RR)");
    Serial.println("Pumps: PA=auto, PO=off, PB=both, P1=pump1, P2=pump2");
    Serial.println("       PT###=set target PSI (e.g. PT120)");
    Serial.println("Status: ?=help, P=print status");
    Serial.print("WiFi: Connect to '");
    Serial.print(WIFI_SSID);
    Serial.print("' password '");
    Serial.print(WIFI_PASS);
    Serial.println("'");
    Serial.println("----------------");
}

void processSerialCommand() {
    char cmd = Serial.read();

    switch (cmd) {
        case 'I':
        case 'i': {
            while (!Serial.available()) { delay(1); }
            int bagNum = Serial.read() - '0';
            if (bagNum >= 0 && bagNum < NUM_BAGS) {
                if (tankPressure >= TANK_CUTOFF_PSI) {
                    bags[bagNum].inflate();
                    Serial.print("Inflating ");
                    Serial.println(bags[bagNum].getName());
                } else {
                    Serial.println("Cannot inflate - tank pressure too low");
                }
            }
            break;
        }

        case 'D':
        case 'd': {
            while (!Serial.available()) { delay(1); }
            int bagNum = Serial.read() - '0';
            if (bagNum >= 0 && bagNum < NUM_BAGS) {
                bags[bagNum].deflate();
                Serial.print("Deflating ");
                Serial.println(bags[bagNum].getName());
            }
            break;
        }

        case 'H':
        case 'h': {
            while (!Serial.available()) { delay(1); }
            int bagNum = Serial.read() - '0';
            if (bagNum >= 0 && bagNum < NUM_BAGS) {
                bags[bagNum].hold();
                Serial.print("Holding ");
                Serial.println(bags[bagNum].getName());
            }
            break;
        }

        case 'S':
        case 's':
            stopAllBags();
            Serial.println("All bags stopped");
            break;

        case 'P':
        case 'p': {
            delay(10);
            if (Serial.available()) {
                char subCmd = Serial.read();
                switch (subCmd) {
                    case 'A':
                    case 'a':
                        compressor.setMode(PUMP_AUTO);
                        Serial.println("Pumps: AUTO mode");
                        break;
                    case 'O':
                    case 'o':
                        compressor.setMode(PUMP_OFF);
                        Serial.println("Pumps: OFF (manual override)");
                        break;
                    case 'B':
                    case 'b':
                        compressor.setMode(PUMP_BOTH_ON);
                        Serial.println("Pumps: BOTH ON (manual override)");
                        break;
                    case '1':
                        compressor.setMode(PUMP_1_ONLY);
                        Serial.println("Pumps: PUMP 1 only (manual override)");
                        break;
                    case '2':
                        compressor.setMode(PUMP_2_ONLY);
                        Serial.println("Pumps: PUMP 2 only (manual override)");
                        break;
                    case 'T':
                    case 't': {
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
                            Serial.print("Tank target set to ");
                            Serial.print(psi);
                            Serial.println(" PSI");
                        }
                        break;
                    }
                    default:
                        printStatus();
                        break;
                }
            } else {
                printStatus();
            }
            break;
        }

        case '?':
            printHelp();
            break;

        case '\n':
        case '\r':
            break;

        default:
            break;
    }
}

void stopAllBags() {
    for (int i = 0; i < NUM_BAGS; i++) {
        bags[i].hold();
    }
}
