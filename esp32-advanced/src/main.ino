/*
 * Air Ride Suspension Controller - ESP32 ADVANCED Version
 * 1964 Chevrolet Impala
 *
 * Enhanced features:
 * - Hold buttons for continuous inflate/deflate
 * - Target PSI display
 * - Ride height memory (EEPROM)
 * - Level mode (auto-match left/right)
 * - Watchdog timer
 * - Solenoid timeout protection
 * - Pressure smoothing
 * - OTA firmware updates
 * - Pump runtime tracking
 * - Tank lockout with hysteresis
 *
 * ESP32 with built-in WiFi - no shield required!
 */

#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
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

float tankPressure = 0.0;
AirRideWebServer webServer(bags, &compressor, &tankPressure);

unsigned long lastPressureRead = 0;

// ============================================
// FUNCTION PROTOTYPES
// ============================================

float readTankPressure();
float readTankPressureSmoothed();
void updateTargetTracking();
void printStatus();
void printHelp();
void processSerialCommand();
void stopAllBags();
void setupOTA();
void setupWatchdog();

// Tank pressure smoothing
float tankPressureBuffer[PRESSURE_SAMPLES];
int tankBufferIndex = 0;
bool tankBufferFilled = false;

// ============================================
// SETUP
// ============================================

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000); // Give serial time to connect

    Serial.println("====================================");
    Serial.println("Air Ride Controller v3.0 (ESP32 ADV)");
    Serial.println("1964 Chevrolet Impala");
    Serial.println("====================================");

    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    Serial.println("EEPROM initialized");

    // Initialize all air bags
    for (int i = 0; i < NUM_BAGS; i++) {
        bags[i].begin();
    }
    Serial.println("Air bags initialized");

    // Initialize compressor
    compressor.begin();
    Serial.println("Compressor initialized");

    // Initialize WiFi web server
    webServer.begin();

    // Setup OTA updates
    setupOTA();

    // Setup watchdog timer
    setupWatchdog();

    // Fill tank pressure buffer
    for (int i = 0; i < PRESSURE_SAMPLES; i++) {
        tankPressureBuffer[i] = readTankPressure();
        delay(PRESSURE_SAMPLE_DELAY);
    }
    tankBufferFilled = true;

    // Initial tank reading
    tankPressure = readTankPressureSmoothed();

    // Check for maintenance warnings
    if (compressor.isMaintenanceDue()) {
        Serial.println("************************************");
        if (compressor.isPump1Overdue()) {
            Serial.println("WARNING: Pump 1 maintenance OVERDUE!");
        } else if (compressor.isPump1MaintenanceDue()) {
            Serial.println("NOTICE: Pump 1 maintenance due");
        }
        if (compressor.isPump2Overdue()) {
            Serial.println("WARNING: Pump 2 maintenance OVERDUE!");
        } else if (compressor.isPump2MaintenanceDue()) {
            Serial.println("NOTICE: Pump 2 maintenance due");
        }
        Serial.println("Use MR1 or MR2 to reset after service");
        Serial.println("************************************");
    }

    Serial.println("====================================");
    Serial.println("System Ready");
    printHelp();
}

// ============================================
// MAIN LOOP
// ============================================

void loop() {
    unsigned long currentTime = millis();

    // Reset watchdog
    esp_task_wdt_reset();

    // Handle OTA updates
    ArduinoOTA.handle();

    // Handle WiFi clients
    webServer.update();

    // Read pressures at defined interval
    if (currentTime - lastPressureRead >= PRESSURE_READ_INTERVAL) {
        lastPressureRead = currentTime;

        // Update tank pressure (smoothed)
        tankPressureBuffer[tankBufferIndex] = readTankPressure();
        tankBufferIndex = (tankBufferIndex + 1) % PRESSURE_SAMPLES;
        tankPressure = readTankPressureSmoothed();

        // Update compressor (handles pump logic automatically)
        compressor.update(tankPressure);

        // Update all bags (reads pressure, enforces safety limits, checks timeouts)
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

void setupOTA() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        // Stop all solenoids before OTA update
        for (int i = 0; i < NUM_BAGS; i++) {
            bags[i].hold();
        }
        compressor.setMode(PUMP_OFF);
        Serial.println("OTA Update starting...");
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA Update complete!");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();
    Serial.println("OTA updates enabled");
}

void setupWatchdog() {
    // Initialize watchdog with timeout
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);
    Serial.print("Watchdog enabled (");
    Serial.print(WATCHDOG_TIMEOUT_S);
    Serial.println("s timeout)");
}

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

float readTankPressureSmoothed() {
    float sum = 0.0;
    int count = tankBufferFilled ? PRESSURE_SAMPLES : tankBufferIndex;

    if (count == 0) {
        return readTankPressure();
    }

    for (int i = 0; i < count; i++) {
        sum += tankPressureBuffer[i];
    }
    return sum / count;
}

void updateTargetTracking() {
    // Auto-adjust bags toward their target pressure
    // This enables preset functionality
    const float tolerance = 2.0;

    for (int i = 0; i < NUM_BAGS; i++) {
        float current = bags[i].getPressure();
        float target = bags[i].getTargetPressure();

        // Skip if solenoid is timed out
        if (bags[i].isSolenoidTimedOut()) {
            continue;
        }

        // Only auto-adjust if we have a meaningful target set
        if (target > 0 || bags[i].isInflating() || bags[i].isDeflating()) {
            if (current < target - tolerance) {
                if (!bags[i].isInflating() && !webServer.isTankLockout()) {
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
    Serial.print(" PSI");
    if (webServer.isTankLockout()) {
        Serial.print(" [LOCKOUT]");
    }
    Serial.print(" | Pumps: ");
    Serial.print(compressor.getModeString());
    Serial.print(" [");
    Serial.print(compressor.isPump1Running() ? "P1:ON" : "P1:off");
    Serial.print(" ");
    Serial.print(compressor.isPump2Running() ? "P2:ON" : "P2:off");
    Serial.print("] Target: ");
    Serial.print(compressor.getTargetPressure(), 0);
    Serial.println(" PSI");

    // Pump runtime
    Serial.print("Pump Runtime: P1=");
    Serial.print(compressor.getPump1RuntimeHours(), 1);
    Serial.print("h");
    if (compressor.isPump1Overdue()) {
        Serial.print(" [OVERDUE!]");
    } else if (compressor.isPump1MaintenanceDue()) {
        Serial.print(" [MAINT DUE]");
    }
    Serial.print(" P2=");
    Serial.print(compressor.getPump2RuntimeHours(), 1);
    Serial.print("h");
    if (compressor.isPump2Overdue()) {
        Serial.println(" [OVERDUE!]");
    } else if (compressor.isPump2MaintenanceDue()) {
        Serial.println(" [MAINT DUE]");
    } else {
        Serial.println();
    }

    // WiFi status
    Serial.print("WiFi: ");
    if (webServer.isConnected()) {
        Serial.print("AP Active - IP: ");
        Serial.println(webServer.getIP());
    } else {
        Serial.println("Not Ready");
    }

    // Level mode
    Serial.print("Level Mode: ");
    switch (webServer.getLevelMode()) {
        case LEVEL_OFF:   Serial.println("OFF"); break;
        case LEVEL_FRONT: Serial.println("FRONT"); break;
        case LEVEL_REAR:  Serial.println("REAR"); break;
        case LEVEL_ALL:   Serial.println("ALL"); break;
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

        if (bags[i].isSolenoidTimedOut()) {
            Serial.print(" [TIMEOUT]");
        } else if (bags[i].isInflating()) {
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
    Serial.println("Level: L0=off, L1=front, L2=rear, L3=all");
    Serial.println("Memory: MS=save height, MH=restore height");
    Serial.println("Maint:  MR1=reset pump1, MR2=reset pump2 (after service)");
    Serial.println("Status: ?=help, P=print status");
    Serial.print("WiFi: Connect to '");
    Serial.print(WIFI_SSID);
    Serial.print("' password '");
    Serial.print(WIFI_PASS);
    Serial.println("'");
    Serial.print("OTA: ");
    Serial.print(OTA_HOSTNAME);
    Serial.println(".local");
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
                if (!webServer.isTankLockout()) {
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

        case 'L':
        case 'l': {
            while (!Serial.available()) { delay(1); }
            int mode = Serial.read() - '0';
            if (mode >= 0 && mode <= 3) {
                webServer.setLevelMode((LevelMode)mode);
                Serial.print("Level mode: ");
                switch (mode) {
                    case 0: Serial.println("OFF"); break;
                    case 1: Serial.println("FRONT"); break;
                    case 2: Serial.println("REAR"); break;
                    case 3: Serial.println("ALL"); break;
                }
            }
            break;
        }

        case 'M':
        case 'm': {
            while (!Serial.available()) { delay(1); }
            char subCmd = Serial.read();
            if (subCmd == 'S' || subCmd == 's') {
                webServer.saveRideHeight();
                Serial.println("Ride height saved");
            } else if (subCmd == 'H' || subCmd == 'h') {
                if (webServer.hasLastRideHeight()) {
                    Serial.println("Restoring ride height...");
                } else {
                    Serial.println("No saved ride height");
                }
            } else if (subCmd == 'R' || subCmd == 'r') {
                // Maintenance reset: MR1 or MR2
                while (!Serial.available()) { delay(1); }
                char pumpNum = Serial.read();
                if (pumpNum == '1') {
                    compressor.resetPump1Runtime();
                } else if (pumpNum == '2') {
                    compressor.resetPump2Runtime();
                } else {
                    Serial.println("Use MR1 or MR2 to reset pump maintenance");
                }
            }
            break;
        }

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
