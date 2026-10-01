# Security and credential handling

This repository previously tracked live Wi-Fi access-point credentials in firmware and documentation, plus an OTA password in the advanced ESP32 variants. Those historical values must be treated as exposed.

## Required operator action

1. Rotate the Wi-Fi AP password on every deployed controller built from this repository.
2. Rotate the OTA password on deployed `esp32-advanced` and `esp32-full` controllers.
3. Use different, unique Wi-Fi and OTA passwords.
4. Rebuild and flash firmware manually after review. This repository does not flash devices or change network settings automatically.

Removing values from the current branch does not remove them from existing clones, forks, caches, or Git history. A separate history rewrite may be considered only after coordinating with all collaborators; rotation is required regardless.

## Local configuration

Each firmware variant contains `include/secrets.example.h`. Copy it to `include/secrets.h`, replace every placeholder, and do not commit that local file. The root `.gitignore` excludes all variant `secrets.h` files.

The affected historical tracked files were:

- `CLAUDE.md`
- `arduino-mega/include/WebServer.h`
- `arduino-mega/src/main.ino`
- `esp32/include/config.h`
- `esp32/src/main.ino`
- `esp32-advanced/include/config.h`
- `esp32-advanced/src/main.ino`
- `esp32-full/include/config.h`
- `esp32-full/src/main.ino`

Firmware diagnostics also disclosed credentials through the following files and have been changed not to print passwords:

- `esp32/src/AirRideWebServer.cpp`
- `esp32-advanced/src/AirRideWebServer.cpp`
- `esp32-full/src/AirRideWebServer.cpp`

Never include credential values in bug reports, logs, screenshots, or serial-console transcripts.
