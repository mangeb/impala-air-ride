#ifndef AIRRIDE_WEBSERVER_H
#define AIRRIDE_WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>  // ESP32 built-in WebServer (from WebServer library)
#include <EEPROM.h>
#include "config.h"
#include "AirBag.h"
#include "Compressor.h"

// Preset definitions (PSI values)
struct Preset {
    const char* name;
    float frontLeft;
    float frontRight;
    float rearLeft;
    float rearRight;
};

// Default presets: Lay, Cruise, Max
const Preset DEFAULT_PRESETS[] = {
    {"Lay",    0.0,   0.0,  0.0,  0.0},   // All the way down
    {"Cruise", 80.0, 80.0, 50.0, 50.0},   // Front 80, Rear 50
    {"Max",   100.0, 100.0, 80.0, 80.0}   // Front 100, Rear 80
};
const int NUM_PRESETS = 3;

// Level mode options
enum LevelMode {
    LEVEL_OFF,
    LEVEL_FRONT,    // Match front left and right
    LEVEL_REAR,     // Match rear left and right
    LEVEL_ALL       // Match all four (front avg = rear avg)
};

class AirRideWebServer {
  public:
    AirRideWebServer(AirBag* bags, Compressor* comp, float* tankPressure);

    void begin();
    void update();

    bool isConnected() const { return wifiReady; }
    IPAddress getIP() const { return WiFi.softAPIP(); }

    // Level mode
    void setLevelMode(LevelMode mode) { levelMode = mode; }
    LevelMode getLevelMode() const { return levelMode; }
    void updateLevelMode();

    // Tank lockout with hysteresis
    bool isTankLockout() const { return tankLockout; }
    void updateTankLockout(float tankPressure);

    // Pump enable/disable override
    bool isPumpEnabled() const { return pumpEnabled; }
    void setPumpEnabled(bool enabled) { pumpEnabled = enabled; }

    // Actions callable from both web and serial
    void applyPreset(int presetNum);
    const char* getPresetName(int presetNum) const;

  private:
    AirBag* bags;
    Compressor* compressor;
    float* tankPressure;

    WebServer server;
    bool wifiReady;

    // Level mode
    LevelMode levelMode;
    unsigned long lastLevelAdjust;

    // Tank lockout hysteresis
    bool tankLockout;

    // Pump enable/disable
    bool pumpEnabled;

    // Mutable presets (loaded from EEPROM, fall back to DEFAULT_PRESETS)
    float currentPresets[NUM_PRESETS][4]; // [preset][FL, FR, RL, RR]
    void loadPresetsFromEEPROM();
    void savePresetToEEPROM(int presetNum);

    void handleRoot();
    void handleStatus();
    void handleBag();
    void handleBagHold();    // Hold button release
    void handleBagTarget();  // Set target for single bag: /bt?n=<bag>&t=<psi>
    void handlePreset();
    void handleSavePreset(); // Save current pressures to preset: /sp?n=<preset>&fl=&fr=&rl=&rr=
    void handleLevel();
    void handlePumpOverride();
    void handleNotFound();
};

#endif // AIRRIDE_WEBSERVER_H
