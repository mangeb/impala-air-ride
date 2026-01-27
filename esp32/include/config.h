#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// ESP32 PIN DEFINITIONS
// ============================================
// ESP32 has built-in WiFi - no shield needed!
// ESP32 ADC: 12-bit (0-4095), but ADC2 conflicts with WiFi
// Use ADC1 pins only: GPIO 32-39 for analog

// Pressure Sensor Analog Pins (VDO 0-150 PSI resistance sensors)
// Using ADC1 pins (GPIO 32-36) - these work with WiFi enabled
#define TANK_PRESSURE_PIN        36  // ADC1_CH0 (VP)
#define FRONT_LEFT_PRESSURE_PIN  39  // ADC1_CH3 (VN)
#define FRONT_RIGHT_PRESSURE_PIN 34  // ADC1_CH6
#define REAR_LEFT_PRESSURE_PIN   35  // ADC1_CH7
#define REAR_RIGHT_PRESSURE_PIN  32  // ADC1_CH4

// RideTech Big Red Valve Solenoid Pins (8 solenoids - 2 per corner)
// Each corner has separate INFLATE and DEFLATE solenoids
// Both are normally closed - energize to open
#define FRONT_LEFT_INFLATE_PIN   13
#define FRONT_LEFT_DEFLATE_PIN   12
#define FRONT_RIGHT_INFLATE_PIN  14
#define FRONT_RIGHT_DEFLATE_PIN  27
#define REAR_LEFT_INFLATE_PIN    26
#define REAR_LEFT_DEFLATE_PIN    25
#define REAR_RIGHT_INFLATE_PIN   33
#define REAR_RIGHT_DEFLATE_PIN   23

// Compressor Pump Relay Pins
#define PUMP_1_PIN               19
#define PUMP_2_PIN               18

// ============================================
// WIFI CONFIGURATION
// ============================================
#define WIFI_SSID               "Impala64"
#define WIFI_PASS               "airride1964"
#define WIFI_CHANNEL            1
#define MAX_WIFI_CLIENTS        4

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

// ============================================
// TANK PRESSURE & COMPRESSOR SETTINGS
// ============================================

// Tank pressure range - pumps maintain this automatically
#define TANK_MIN_PSI            100.0  // Pumps turn ON below this
#define TANK_MAX_PSI            150.0  // Pumps turn OFF at this

// Pump operation thresholds
#define PUMP_BOTH_ON_THRESHOLD  70.0   // Both pumps run below this PSI
#define PUMP_ALTERNATE_ABOVE    70.0   // Single pump alternates above this PSI

// Tank safety
#define TANK_CUTOFF_PSI         60.0   // Stop bag inflation if tank below this

// ============================================
// BAG SAFETY LIMITS
// ============================================

#define MIN_BAG_PSI             0.0    // Minimum safe bag pressure (0 for "Lay" mode)
#define MAX_BAG_PSI             120.0  // Maximum safe bag pressure

// ============================================
// TIMING CONSTANTS
// ============================================

#define PRESSURE_READ_INTERVAL  100    // Read pressure every 100ms
#define PUMP_SWITCH_INTERVAL    30000  // Alternate pumps every 30 seconds when topping off
#define SERIAL_BAUD_RATE        115200 // ESP32 typically uses higher baud

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

#endif // CONFIG_H
