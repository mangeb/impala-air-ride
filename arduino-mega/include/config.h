#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// ARDUINO MEGA 2560 PIN DEFINITIONS
// ============================================
// Mega has 54 digital pins and 16 analog pins
// WiFi Shield uses: D4 (SD), D10 (SS), D11 (MOSI), D12 (MISO), D13 (SCK)
// Also uses D50-D53 for SPI on Mega
// We use pins 22-31 to avoid conflicts

// Pressure Sensor Analog Pins (VDO 0-150 PSI resistance sensors)
#define TANK_PRESSURE_PIN        A0
#define FRONT_LEFT_PRESSURE_PIN  A1
#define FRONT_RIGHT_PRESSURE_PIN A2
#define REAR_LEFT_PRESSURE_PIN   A3
#define REAR_RIGHT_PRESSURE_PIN  A4

// RideTech Big Red Valve Solenoid Pins (8 solenoids - 2 per corner)
// Each corner has separate INFLATE and DEFLATE solenoids
// Both are normally closed - energize to open
// Using pins 22-29 on Mega (clear of WiFi Shield)
#define FRONT_LEFT_INFLATE_PIN   22
#define FRONT_LEFT_DEFLATE_PIN   23
#define FRONT_RIGHT_INFLATE_PIN  24
#define FRONT_RIGHT_DEFLATE_PIN  25
#define REAR_LEFT_INFLATE_PIN    26
#define REAR_LEFT_DEFLATE_PIN    27
#define REAR_RIGHT_INFLATE_PIN   28
#define REAR_RIGHT_DEFLATE_PIN   29

// Compressor Pump Relay Pins (using pins 30-31)
#define PUMP_1_PIN               30
#define PUMP_2_PIN               31

// ============================================
// VDO PRESSURE SENSOR CALIBRATION
// ============================================
// VDO resistance-based sensor: 10-180 ohm (European standard)
// Uses voltage divider with known resistor to convert resistance to voltage
//
// Circuit: 5V ----[R_REF]----+----[VDO_SENSOR]---- GND
//                           |
//                        Analog Pin
//
// At 0 PSI:   R_sensor = 10 ohm   -> V_out = 5 * 10 / (R_REF + 10)
// At 150 PSI: R_sensor = 180 ohm  -> V_out = 5 * 180 / (R_REF + 180)

// Reference resistor value (use 100 ohm for good range with 10-180 ohm sensor)
#define REFERENCE_RESISTOR      100.0  // ohms

// VDO sensor resistance range
#define SENSOR_MIN_OHMS         10.0   // Resistance at 0 PSI
#define SENSOR_MAX_OHMS         180.0  // Resistance at 150 PSI
#define SENSOR_MAX_PSI          150.0  // Maximum PSI reading

// Arduino ADC
#define ADC_REFERENCE_VOLTAGE   5.0
#define ADC_RESOLUTION          1023.0

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
#define SERIAL_BAUD_RATE        9600

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
