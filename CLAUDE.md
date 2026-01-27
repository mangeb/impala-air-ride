# Project Overview

Air ride suspension controller for a 1964 Chevrolet Impala with WiFi mobile control.

### Features
- **Pressure Monitoring**: VDO 10-180 ohm sensors for tank and 4 air bags
- **Solenoid Control**: RideTech Big Red 4-way manifold (8 solenoids - inflate/deflate per corner)
- **Dual Compressors**: Smart pump control - both on below 70 PSI, alternating above
- **WiFi Control**: Mobile web interface with 3 presets (Lay, Cruise, Max)
- **Safety**: Tank cutoff, max/min pressure limits

### Hardware Components
- Arduino Mega 2560 + WiFi Shield **OR** ESP32
- 5x VDO 0-150 PSI pressure sensors (10-180 ohm resistance)
- 1x RideTech Big Red 4-Way Manifold (8 solenoids)
- 2x 12V air compressor pumps
- 10-channel relay module
- Air tank and plumbing

## Tech Stack

- **Platforms**: Arduino Mega 2560 / ESP32
- **Language**: C/C++ (Arduino)
- **IDE**: Arduino IDE / PlatformIO
- **WiFi**: Built-in AP mode (SSID: Impala64)

## Project Structure

```
├── arduino-mega/           # Arduino Mega 2560 + WiFi Shield version
│   ├── src/main.ino
│   ├── lib/
│   ├── include/
│   └── docs/SCHEMATIC.txt
│
├── esp32/                  # ESP32 basic version (for testing)
│   ├── src/main.ino
│   ├── lib/
│   ├── include/
│   └── docs/
│       ├── SCHEMATIC.txt
│       └── preview.html    # UI preview for browser
│
├── esp32-advanced/         # ESP32 advanced version (full features)
│   ├── src/main.ino
│   ├── lib/
│   ├── include/
│   └── docs/
│       ├── SCHEMATIC.txt
│       └── preview.html
│
└── CLAUDE.md
```

## ESP32 Versions

### Basic (`esp32/`)
Simple version for initial testing and hardware verification.

### Advanced (`esp32-advanced/`)
Full-featured version with:
- **Hold Buttons**: Press and hold +/- for continuous inflate/deflate
- **Target PSI Display**: Shows current and target pressure for each bag
- **Ride Height Memory**: Save/restore last position (EEPROM)
- **Level Mode**: Auto-match left/right pressures (Front, Rear, or All)
- **Watchdog Timer**: Auto-reset if system hangs
- **Solenoid Timeout**: 30-second max to prevent overheating
- **Pressure Smoothing**: ADC averaging for stable readings
- **OTA Updates**: Wireless firmware updates
- **Pump Runtime Tracking**: Maintenance scheduling
- **Tank Lockout**: Hysteresis prevents pump cycling

## Development Commands

```bash
# Arduino Mega with Arduino CLI
cd arduino-mega
arduino-cli compile --fqbn arduino:avr:mega .
arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:mega .

# ESP32 with Arduino CLI
cd esp32
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 .

# PlatformIO (either version)
pio run              # Build
pio run -t upload    # Upload to board
pio device monitor   # Serial monitor
```

## WiFi Control

- **SSID**: Impala64
- **Password**: airride1964
- **IP**: 192.168.4.1 (default AP IP)

### Presets
| Preset | Front PSI | Rear PSI |
|--------|-----------|----------|
| Lay    | 0         | 0        |
| Cruise | 80        | 50       |
| Max    | 100       | 80       |

## Code Style & Conventions

- Use `UPPER_CASE` for pin definitions and constants
- Use `camelCase` for functions and variables
- Group related pins together
- Comment hardware-specific values (PSI ranges, calibration values)

## Important Notes

- VDO sensors use voltage divider: 100Ω reference resistor to 5V (Mega) or 3.3V (ESP32)
- Relays are active-LOW (configurable in config.h)
- ESP32: Use only ADC1 pins (GPIO 32-36, 39) - ADC2 conflicts with WiFi
- RideTech Big Red solenoids use Weatherpak connectors
- Always test relay polarity before final installation
