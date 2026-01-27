#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
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

    // Ride height memory
    void saveRideHeight();
    void loadRideHeight();
    bool hasLastRideHeight() const { return hasStoredHeight; }

    // Level mode
    void setLevelMode(LevelMode mode) { levelMode = mode; }
    LevelMode getLevelMode() const { return levelMode; }
    void updateLevelMode();

    // Tank lockout with hysteresis
    bool isTankLockout() const { return tankLockout; }
    void updateTankLockout(float tankPressure);

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

    // Ride height memory
    bool hasStoredHeight;
    float lastHeight[NUM_BAGS];

    void handleRoot();
    void handleStatus();
    void handleBag();
    void handleBagHold();  // New: hold button release
    void handlePreset();
    void handleLevel();
    void handleSaveHeight();
    void handleRestoreHeight();
    void handleNotFound();

    String getHtmlPage();
};

#endif // WEBSERVER_H
