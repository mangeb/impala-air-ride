#ifndef AIRRIDE_WEBSERVER_H
#define AIRRIDE_WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
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

// Presets: Lay, Cruise, Max
const Preset PRESETS[] = {
    {"Lay",    0.0,   0.0,  0.0,  0.0},   // All the way down
    {"Cruise", 80.0, 80.0, 50.0, 50.0},   // Front 80, Rear 50
    {"Max",   100.0, 100.0, 80.0, 80.0}   // Front 100, Rear 80
};
const int NUM_PRESETS = 3;

class AirRideWebServer {
  public:
    AirRideWebServer(AirBag* bags, Compressor* comp, float* tankPressure);

    void begin();
    void update();

    bool isConnected() const { return wifiReady; }
    IPAddress getIP() const { return WiFi.softAPIP(); }

  private:
    AirBag* bags;
    Compressor* compressor;
    float* tankPressure;

    WebServer server;
    bool wifiReady;

    // Time sync from browser
    bool timeSynced;

    void handleRoot();
    void handleStatus();
    void handleBag();
    void handlePreset();
    void handleTimeSync();
    void handleNotFound();

    String getHtmlPage();
};

#endif // AIRRIDE_WEBSERVER_H
