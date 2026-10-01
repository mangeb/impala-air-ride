# Impala Air Ride

Firmware and web controls for an air-ride suspension controller, with variants for Arduino Mega 2560, ESP32, and ESP32-S3.

## Repository layout

- `arduino-mega/` — Mega 2560 with a Wi-Fi shield
- `esp32/` — basic ESP32 controller
- `esp32-advanced/` — ESP32-S3 controller with OTA and advanced controls
- `esp32-full/` — ESP32-S3 full UI/feature variant

## Configure credentials locally

Every variant requires an ignored local credential header:

```bash
cp esp32/include/secrets.example.h esp32/include/secrets.h
# Edit esp32/include/secrets.h and replace every placeholder.
```

Use the corresponding path for the other variants. Do not commit any `secrets.h` file. Advanced/full builds require both Wi-Fi and OTA credentials. See `SECURITY.md` for required rotation work caused by historical exposure.

## Verify and build

```bash
./scripts/verify.sh --scan-only  # configuration and disclosure checks only
./scripts/verify.sh              # checks plus all four PlatformIO builds
```

The build script may create temporary `secrets.h` files from examples when local files are absent; it removes only the files it created. It never uploads firmware, connects to a controller, or changes device/network settings.

To build one variant after creating its local `secrets.h`:

```bash
pio run --project-dir esp32
```

Uploads and OTA deployment are intentionally manual and outside the verification contract.
