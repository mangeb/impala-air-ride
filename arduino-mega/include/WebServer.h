#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "AirBag.h"
#include "Compressor.h"

// WiFi AP credentials
#define WIFI_SSID "Impala64"
#define WIFI_PASS "airride1964"

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

class WebServer {
  public:
    WebServer(AirBag* bags, Compressor* comp, float* tankPressure);

    void begin();
    void update();

    bool isConnected() const { return wifiReady; }

  private:
    AirBag* bags;
    Compressor* compressor;
    float* tankPressure;

    WiFiServer server;
    bool wifiReady;

    void handleClient(WiFiClient& client);
    void sendHomePage(WiFiClient& client);
    void sendJsonStatus(WiFiClient& client);
    void parseRequest(String& request);

    // HTML content helpers
    void sendHeader(WiFiClient& client, const char* contentType, int contentLength = -1);
    void sendHtmlPage(WiFiClient& client);
};

#endif // WEBSERVER_H
