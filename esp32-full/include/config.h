#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// ESP32 ADVANCED AIR RIDE CONTROLLER
// ============================================
// Enhanced version with:
// - Hold buttons for continuous inflate/deflate
// - Target PSI display
// - Saveable presets (EEPROM)
// - Level mode (auto-match left/right)
// - Watchdog timer
// - Solenoid timeout protection
// - Pressure smoothing
// - OTA updates
// - Diagnostics & logging

// ============================================
// ESP32-S3 PIN DEFINITIONS
// ============================================
// ESP32-S3 has built-in WiFi + native USB
// ESP32-S3 ADC: 12-bit (0-4095)
// ADC1: GPIO 1-10  (safe with WiFi)
// ADC2: GPIO 11-20 (conflicts with WiFi - avoid!)
// GPIO 0: boot button, avoid
// GPIO 19/20: USB D-/D+, reserved for native USB

// Pressure Sensor Analog Pins (VDO 0-150 PSI resistance sensors)
// Using ADC1 pins only - these work with WiFi enabled
#define TANK_PRESSURE_PIN        1   // ADC1_CH0
#define FRONT_LEFT_PRESSURE_PIN  2   // ADC1_CH1
#define FRONT_RIGHT_PRESSURE_PIN 3   // ADC1_CH2
#define REAR_LEFT_PRESSURE_PIN   4   // ADC1_CH3
#define REAR_RIGHT_PRESSURE_PIN  5   // ADC1_CH4

// RideTech Big Red Valve Solenoid Pins (8 solenoids - 2 per corner)
// Each corner has separate INFLATE and DEFLATE solenoids
// Both are normally closed - energize to open
#define FRONT_LEFT_INFLATE_PIN   6
#define FRONT_LEFT_DEFLATE_PIN   7
#define FRONT_RIGHT_INFLATE_PIN  15
#define FRONT_RIGHT_DEFLATE_PIN  16
#define REAR_LEFT_INFLATE_PIN    17
#define REAR_LEFT_DEFLATE_PIN    18
#define REAR_RIGHT_INFLATE_PIN   8
#define REAR_RIGHT_DEFLATE_PIN   21

// Compressor Pump Relay Pins
#define PUMP_1_PIN               47
#define PUMP_2_PIN               48

// ============================================
// WIFI CONFIGURATION
// ============================================
#define WIFI_SSID               "Impala64"
#define WIFI_PASS               "airride1964"
#define WIFI_CHANNEL            1
#define MAX_WIFI_CLIENTS        4

// ============================================
// OTA UPDATE CONFIGURATION
// ============================================
#define OTA_HOSTNAME            "impala-airride"
#define OTA_PASSWORD            "ota1964"

// ============================================
// VDO PRESSURE SENSOR CALIBRATION
// ============================================
// VDO resistance-based sensor: 10-180 ohm (European standard)
// Uses voltage divider with known resistor to convert resistance to voltage
//
// Circuit: 3.3V ----[R_REF]----+----[VDO_SENSOR]---- GND
//                             |
//                          Analog Pin
//
// NOTE: ESP32 uses 3.3V reference, not 5V!

// Reference resistor value (use 100 ohm for good range with 10-180 ohm sensor)
#define REFERENCE_RESISTOR      100.0  // ohms

// VDO sensor resistance range
#define SENSOR_MIN_OHMS         10.0   // Resistance at 0 PSI
#define SENSOR_MAX_OHMS         180.0  // Resistance at 150 PSI
#define SENSOR_MAX_PSI          150.0  // Maximum PSI reading

// ESP32 ADC (12-bit, 3.3V reference)
#define ADC_REFERENCE_VOLTAGE   3.3
#define ADC_RESOLUTION          4095.0

// Pressure smoothing (averaging)
#define PRESSURE_SAMPLES        5      // Number of samples to average
#define PRESSURE_SAMPLE_DELAY   2      // ms between samples

// ============================================
// TANK PRESSURE & COMPRESSOR SETTINGS
// ============================================

// Tank pressure range - pumps maintain this automatically
#define TANK_MIN_PSI            100.0  // Pumps turn ON below this
#define TANK_MAX_PSI            150.0  // Pumps turn OFF at this

// Pump operation thresholds
#define PUMP_BOTH_ON_THRESHOLD  70.0   // Both pumps run below this PSI
#define PUMP_ALTERNATE_ABOVE    70.0   // Single pump alternates above this PSI

// Tank safety with hysteresis
#define TANK_CUTOFF_PSI         60.0   // Stop bag inflation if tank below this
#define TANK_RESUME_PSI         80.0   // Resume inflation when tank above this

// ============================================
// BAG SAFETY LIMITS
// ============================================

#define MIN_BAG_PSI             0.0    // Minimum safe bag pressure (0 for "Lay" mode)
#define MAX_BAG_PSI             120.0  // Maximum safe bag pressure

// ============================================
// SOLENOID PROTECTION
// ============================================

#define SOLENOID_TIMEOUT_MS     30000  // Max continuous solenoid on time (30 sec)
#define SOLENOID_COOLDOWN_MS    5000   // Cooldown after timeout (5 sec)

// ============================================
// LEVEL MODE SETTINGS
// ============================================

#define LEVEL_TOLERANCE_PSI     2.0    // Acceptable difference for "level"
#define LEVEL_ADJUST_STEP_MS    200    // Time between level adjustments

// ============================================
// TIMING CONSTANTS
// ============================================

#define PRESSURE_READ_INTERVAL  100    // Read pressure every 100ms
#define PUMP_SWITCH_INTERVAL    30000  // Alternate pumps every 30 seconds when topping off
#define SERIAL_BAUD_RATE        115200 // ESP32 typically uses higher baud
#define WATCHDOG_TIMEOUT_S      10     // Watchdog timer in seconds

// Pump maintenance thresholds (hours)
#define PUMP_MAINTENANCE_HOURS  50.0   // Warn when pump exceeds this runtime
#define PUMP_OVERDUE_HOURS      100.0  // Critical warning at this runtime

// ============================================
// EEPROM CONFIGURATION
// ============================================

#define EEPROM_SIZE             512    // Bytes to allocate
#define EEPROM_MAGIC            0x64   // Magic byte to verify valid data
#define EEPROM_VERSION          1      // Data structure version

// EEPROM addresses
#define EEPROM_ADDR_MAGIC       0
#define EEPROM_ADDR_VERSION     1
#define EEPROM_ADDR_PRESET_FLAG 2      // Per-preset saved flags (1 byte, bit 0=P1, bit 1=P2, bit 2=P3)
#define EEPROM_ADDR_PRESET1     20     // Custom preset 1 (16 bytes: 4x float)
#define EEPROM_ADDR_PRESET2     36     // Custom preset 2 (16 bytes: 4x float)
#define EEPROM_ADDR_PRESET3     52     // Custom preset 3 (16 bytes: 4x float)
#define EEPROM_ADDR_PUMP_HOURS  68     // Pump runtime hours (float)

// Leak monitor EEPROM (25 bytes: flag + timestamp + 5 pressures)
#define EEPROM_ADDR_LEAK_FLAG       72  // Valid flag (1 byte, 0xAA)
#define EEPROM_ADDR_LEAK_TIME       73  // Snapshot epoch (uint32_t, 4 bytes)
#define EEPROM_ADDR_LEAK_PRESSURES  77  // FL,FR,RL,RR,Tank (5 floats, 20 bytes)

// Tank maintenance timer EEPROM (5 bytes: flag + last service epoch)
#define EEPROM_ADDR_TANK_MAINT_FLAG  97  // Valid flag (1 byte, 0xBB)
#define EEPROM_ADDR_TANK_MAINT_EPOCH 98  // Last service epoch (uint32_t, 4 bytes)

// Sensor calibration EEPROM (1 flag + 5 sensors × 12 bytes = 61 bytes)
// Per sensor: offset (float 4B) + gain (float 4B) + refResistor (float 4B)
#define EEPROM_ADDR_CAL_FLAG         104 // Valid flag (1 byte, 0xCC)
#define EEPROM_ADDR_CAL_DATA         105 // Start of calibration data
// Sensor order: 0=Tank, 1=FL, 2=FR, 3=RL, 4=RR
// Each sensor: 12 bytes (offset + gain + refResistor)
// Total: 105 + 60 = 165 bytes (within 512 EEPROM)

// ============================================
// SENSOR CALIBRATION SETTINGS
// ============================================
// Two-point calibration: correctedPsi = (rawPsi * gain) + offset
// Sanity bounds prevent bad calibration from bricking readings

#define CAL_VALID_FLAG          0xCC
#define CAL_NUM_SENSORS         5       // Tank + 4 bags
#define CAL_GAIN_MIN            0.8     // Reject gain outside this range
#define CAL_GAIN_MAX            1.2
#define CAL_OFFSET_MIN          -10.0   // Reject offset outside this range (PSI)
#define CAL_OFFSET_MAX          10.0
#define CAL_REF_RESISTOR_MIN    80.0    // Reject ref resistor outside this range (ohms)
#define CAL_REF_RESISTOR_MAX    120.0

struct SensorCalibration {
    float offset;       // PSI offset correction (added after gain)
    float gain;         // PSI gain multiplier (applied first)
    float refResistor;  // Actual reference resistor value (ohms)
};

// ============================================
// LEAK MONITOR SETTINGS
// ============================================
// Detects slow leaks by comparing a saved pressure snapshot
// against current readings after the car has been sitting.
// Thresholds use both total drop AND rate to distinguish
// real leaks from temperature-related pressure changes (~1-2 PSI).

#define LEAK_SNAPSHOT_VALID     0xAA        // EEPROM flag value
#define LEAK_SNAPSHOT_INTERVAL  600000      // Save snapshot every 10 min (ms)
#define LEAK_MIN_SNAPSHOT_PSI   5.0         // Ignore sensors below this PSI
#define LEAK_WARN_DROP_PSI      2.0         // Yellow: total PSI drop
#define LEAK_WARN_RATE_PSI_HR   0.1         // Yellow: AND rate exceeds (PSI/hr)
#define LEAK_ALERT_DROP_PSI     5.0         // Red: total PSI drop
#define LEAK_ALERT_RATE_PSI_HR  0.25        // Red: AND rate exceeds (PSI/hr)

// ============================================
// TANK MAINTENANCE TIMER SETTINGS
// ============================================
// 3-month (90-day) service interval for tank inspection/drain.
// Persisted in EEPROM; controllable via /tank endpoint.

#define TANK_MAINT_VALID        0xBB        // EEPROM flag value
#define TANK_MAINT_INTERVAL_SEC 7776000UL   // 90 days in seconds (90 * 86400)

// ============================================
// RELAY CONFIGURATION
// ============================================

// Set to true if your relay module is active-LOW (most common)
#define RELAY_ACTIVE_LOW        true

#if RELAY_ACTIVE_LOW
  #define RELAY_ON  LOW
  #define RELAY_OFF HIGH
#else
  #define RELAY_ON  HIGH
  #define RELAY_OFF LOW
#endif

// ============================================
// BAG POSITION INDICES
// ============================================

#define FRONT_LEFT  0
#define FRONT_RIGHT 1
#define REAR_LEFT   2
#define REAR_RIGHT  3
#define NUM_BAGS    4

// ============================================
// DEMO / BENCH TEST MODE
// ============================================
// Runtime-toggled via /demo endpoint (no recompile needed)
// Default initial values when simulation is active
#define DEMO_BAG_PSI            66.0   // Default bag pressure in demo mode
#define DEMO_TANK_PSI           150.0  // Default tank pressure in demo mode

// Simulation physics rates (tuned for PRESSURE_READ_INTERVAL = 100ms)
#define SIM_TANK_DECAY_RATE     0.06   // PSI lost per tick from natural leakage
#define SIM_PUMP_FILL_RATE      0.38   // Base pump fill rate per tick
#define SIM_BAG_INFLATE_RATE    0.30   // Bag fill rate multiplier * sqrt(deltaP)
#define SIM_BAG_DEFLATE_RATE    0.25   // Bag dump rate multiplier * sqrt(pressure)
#define SIM_BAG_TANK_DRAIN      0.12   // Tank drain per inflating bag * sqrt(deltaP)
#define SIM_JITTER_RANGE        50     // Random jitter ±0.005 PSI (value/10000)

// Simulated leak for testing leak detection (toggled via /simleak endpoint)
// simLeakTarget: -1=none, 0=FL, 1=FR, 2=RL, 3=RR, 4=tank, 5=random
#define SIM_LEAK_RATE_PSI_TICK  0.15   // Aggressive: ~1.5 PSI/sec (at 100ms ticks)

// Runtime demo mode globals (defined in main.ino)
extern bool demoMode;
extern float simTankPressure;
extern int simLeakTarget;           // Which sensor is leaking (-1=none)
extern float simLeakRate;           // PSI per tick to subtract
void setDemoMode(bool enabled);

// Tank sensor calibration (defined in main.ino)
extern SensorCalibration tankCalibration;
extern bool tankCalibrated;

#endif // CONFIG_H
