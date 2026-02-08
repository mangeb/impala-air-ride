#include "AirRideWebServer.h"
#include "html_content.h"  // Auto-generated gzipped React UI
#include "debug_html_content.h"  // Auto-generated gzipped debug console
#include <sys/time.h>

AirRideWebServer::AirRideWebServer(AirBag* b, Compressor* c, float* tp)
    : bags(b),
      compressor(c),
      tankPressure(tp),
      server(80),
      wifiReady(false),
      levelMode(LEVEL_OFF),
      lastLevelAdjust(0),
      tankLockout(false),
      pumpEnabled(true),
      timeSynced(false),
      leakSnapshotValid(false),
      leakSnapshotEpoch(0),
      lastLeakSnapshotSave(0),
      tankMaintLastService(0),
      tankMaintValid(false) {
    // Initialize presets from defaults
    for (int p = 0; p < NUM_PRESETS; p++) {
        currentPresets[p][0] = DEFAULT_PRESETS[p].frontLeft;
        currentPresets[p][1] = DEFAULT_PRESETS[p].frontRight;
        currentPresets[p][2] = DEFAULT_PRESETS[p].rearLeft;
        currentPresets[p][3] = DEFAULT_PRESETS[p].rearRight;
    }
}

void AirRideWebServer::begin() {
    Serial.print("Starting WiFi AP...");

    // Configure ESP32 as Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL, 0, MAX_WIFI_CLIENTS);

    delay(100);

    wifiReady = true;

    // Load custom presets from EEPROM
    loadPresetsFromEEPROM();

    // Load leak snapshot from EEPROM
    loadLeakSnapshot();

    // Load tank maintenance timer from EEPROM
    loadTankMaintFromEEPROM();

    // Load sensor calibration from EEPROM
    loadCalibrationFromEEPROM();

    // Setup routes
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/debug", HTTP_GET, [this]() { handleDebug(); });
    server.on("/s", HTTP_GET, [this]() { handleStatus(); });
    server.on("/b", HTTP_GET, [this]() { handleBag(); });
    server.on("/bh", HTTP_GET, [this]() { handleBagHold(); });
    server.on("/bt", HTTP_GET, [this]() { handleBagTarget(); });
    server.on("/p", HTTP_GET, [this]() { handlePreset(); });
    server.on("/sp", HTTP_GET, [this]() { handleSavePreset(); });
    server.on("/l", HTTP_GET, [this]() { handleLevel(); });
    server.on("/po", HTTP_GET, [this]() { handlePumpOverride(); });
    server.on("/time", HTTP_GET, [this]() { handleTimeSync(); });
    server.on("/demo", HTTP_GET, [this]() { handleDemoToggle(); });
    server.on("/leak", HTTP_GET, [this]() { handleLeakStatus(); });
    server.on("/tank", HTTP_GET, [this]() { handleTankMaint(); });
    server.on("/simleak", HTTP_GET, [this]() { handleSimLeak(); });
    server.on("/cal", HTTP_GET, [this]() { handleCalibration(); });
    server.on("/calreset", HTTP_GET, [this]() { handleCalibrationReset(); });
    server.onNotFound([this]() { handleNotFound(); });

    server.begin();

    Serial.println(" OK");
    Serial.print("SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("Password: ");
    Serial.println(WIFI_PASS);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
}

void AirRideWebServer::update() {
    if (!wifiReady) return;
    server.handleClient();

    // Update tank lockout state
    updateTankLockout(*tankPressure);

    // Update level mode
    updateLevelMode();

    // Periodic leak snapshot save
    updateLeakSnapshot();
}

void AirRideWebServer::handleRoot() {
    Serial.println("[WEB] GET / - Serving React UI (gzip, " + String(HTML_CONTENT_SIZE) + " bytes)");
    // Serve gzipped React UI from PROGMEM
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html", (const char*)HTML_CONTENT, HTML_CONTENT_SIZE);
}

void AirRideWebServer::handleDebug() {
    Serial.println("[WEB] GET /debug - Serving debug console (gzip, " + String(DEBUG_HTML_CONTENT_SIZE) + " bytes)");
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html", (const char*)DEBUG_HTML_CONTENT, DEBUG_HTML_CONTENT_SIZE);
}

void AirRideWebServer::handleStatus() {
    String json = "{\"tank\":";
    json += String(*tankPressure, 1);
    json += ",\"bags\":[";
    for (int i = 0; i < NUM_BAGS; i++) {
        if (i > 0) json += ",";
        json += String(bags[i].getPressure(), 1);
    }
    json += "],\"targets\":[";
    for (int i = 0; i < NUM_BAGS; i++) {
        if (i > 0) json += ",";
        json += String(bags[i].getTargetPressure(), 1);
    }
    json += "],\"timeouts\":[";
    for (int i = 0; i < NUM_BAGS; i++) {
        if (i > 0) json += ",";
        json += bags[i].isSolenoidTimedOut() ? "true" : "false";
    }
    json += "],\"pump\":\"";
    json += compressor->getModeString();
    json += " P1:";
    json += compressor->isPump1Running() ? "ON" : "off";
    json += " P2:";
    json += compressor->isPump2Running() ? "ON" : "off";
    json += "\",\"runtime\":\"P1:";
    json += String(compressor->getPump1RuntimeHours(), 1);
    json += "h P2:";
    json += String(compressor->getPump2RuntimeHours(), 1);
    json += "h\",\"level\":";
    json += String((int)levelMode);
    json += ",\"lockout\":";
    json += tankLockout ? "true" : "false";
    json += ",\"pumpEnabled\":";
    json += pumpEnabled ? "true" : "false";
    json += ",\"demo\":";
    json += demoMode ? "true" : "false";

    // Current preset values (may be customized)
    json += ",\"presets\":[";
    for (int p = 0; p < NUM_PRESETS; p++) {
        if (p > 0) json += ",";
        json += "[";
        for (int i = 0; i < 4; i++) {
            if (i > 0) json += ",";
            float v = currentPresets[p][i];
            json += (isnan(v) || isinf(v)) ? "0" : String(v, 0);
        }
        json += "]";
    }
    json += "]";

    // Maintenance status
    bool p1Due = compressor->isPump1MaintenanceDue();
    bool p2Due = compressor->isPump2MaintenanceDue();
    bool p1Overdue = compressor->isPump1Overdue();
    bool p2Overdue = compressor->isPump2Overdue();
    if (p1Due || p2Due) {
        json += ",\"maint\":\"";
        if (p1Overdue || p2Overdue) {
            json += "MAINTENANCE OVERDUE: ";
        } else {
            json += "Maintenance due: ";
        }
        if (p1Due && p2Due) {
            json += "P1 & P2";
        } else if (p1Due) {
            json += "Pump 1";
        } else {
            json += "Pump 2";
        }
        json += "\",\"maintOverdue\":";
        json += (p1Overdue || p2Overdue) ? "true" : "false";
    }

    // Tank maintenance timer
    // Simulated leak status
    json += ",\"simLeak\":{\"active\":";
    json += (simLeakTarget >= 0) ? "true" : "false";
    json += ",\"target\":";
    json += String(simLeakTarget);
    if (simLeakTarget >= 0 && simLeakTarget <= 4) {
        const char* names[] = {"FL", "FR", "RL", "RR", "TANK"};
        json += ",\"targetName\":\"";
        json += names[simLeakTarget];
        json += "\"";
    }
    json += "}";

    json += ",\"tankMaint\":{\"valid\":";
    json += tankMaintValid ? "true" : "false";
    if (tankMaintValid) {
        json += ",\"lastService\":";
        json += String(tankMaintLastService);
        if (timeSynced) {
            json += ",\"due\":";
            json += isTankMaintDue() ? "true" : "false";
            json += ",\"daysRemaining\":";
            json += String(getTankMaintDaysRemaining());
        }
    }
    json += ",\"timeSynced\":";
    json += timeSynced ? "true" : "false";
    json += "}";

    json += "}";

    server.send(200, "application/json", json);
}

void AirRideWebServer::handleBag() {
    if (server.hasArg("n") && server.hasArg("d")) {
        int bagNum = server.arg("n").toInt();
        int dir = server.arg("d").toInt();

        Serial.print("[WEB] /b bag=");
        Serial.print(bagNum);
        Serial.print(" dir=");
        Serial.print(dir > 0 ? "INFLATE" : "DEFLATE");

        if (bagNum >= 0 && bagNum < NUM_BAGS) {
            if (dir > 0) {
                // Check tank lockout before inflating
                if (!tankLockout) {
                    bags[bagNum].inflate();
                    // Move target ahead so updateTargetTracking doesn't fight manual control
                    float current = bags[bagNum].getPressure();
                    if (bags[bagNum].getTargetPressure() <= current) {
                        bags[bagNum].setTargetPressure(MAX_BAG_PSI);
                    }
                    Serial.print(" cur=");
                    Serial.print(current, 1);
                    Serial.println(" OK");
                } else {
                    Serial.println(" BLOCKED (tank lockout)");
                }
            } else {
                bags[bagNum].deflate();
                // Move target down so updateTargetTracking doesn't fight manual control
                float current = bags[bagNum].getPressure();
                if (bags[bagNum].getTargetPressure() >= current) {
                    bags[bagNum].setTargetPressure(MIN_BAG_PSI);
                }
                Serial.print(" cur=");
                Serial.print(current, 1);
                Serial.println(" OK");
            }
        } else {
            Serial.println(" INVALID bag number");
        }
    }
    handleStatus();
}

void AirRideWebServer::handleBagHold() {
    // Called when button is released - stop the bag
    if (server.hasArg("n")) {
        int bagNum = server.arg("n").toInt();
        if (bagNum >= 0 && bagNum < NUM_BAGS) {
            bags[bagNum].hold();
            float lockedPsi = bags[bagNum].getPressure();
            bags[bagNum].setTargetPressure(lockedPsi);
            Serial.print("[WEB] /bh RELEASE bag=");
            Serial.print(bagNum);
            Serial.print(" locked at ");
            Serial.print(lockedPsi, 1);
            Serial.println(" PSI");
        }
    }
    handleStatus();
}

void AirRideWebServer::handleBagTarget() {
    // Set target pressure for a specific bag: /bt?n=<bag>&t=<psi>
    if (server.hasArg("n") && server.hasArg("t")) {
        int bagNum = server.arg("n").toInt();
        float targetPsi = server.arg("t").toFloat();

        Serial.print("[WEB] /bt TARGET bag=");
        Serial.print(bagNum);
        Serial.print(" target=");
        Serial.print(targetPsi, 1);
        Serial.println(" PSI");

        if (bagNum >= 0 && bagNum < NUM_BAGS) {
            // Clamp to safe range
            if (targetPsi < MIN_BAG_PSI) targetPsi = MIN_BAG_PSI;
            if (targetPsi > MAX_BAG_PSI) targetPsi = MAX_BAG_PSI;

            bags[bagNum].setTargetPressure(targetPsi);

            // Start moving to target
            float current = bags[bagNum].getPressure();
            if (current < targetPsi - 2.0) {
                if (!tankLockout) {
                    bags[bagNum].inflate();
                }
            } else if (current > targetPsi + 2.0) {
                bags[bagNum].deflate();
            } else {
                bags[bagNum].hold();
            }
        }
    }
    handleStatus();
}

void AirRideWebServer::handlePreset() {
    if (server.hasArg("n")) {
        int presetNum = server.arg("n").toInt();

        if (presetNum >= 0 && presetNum < NUM_PRESETS) {
            Serial.print("[WEB] /p PRESET ");
            Serial.print(DEFAULT_PRESETS[presetNum].name);
            Serial.print(" (");
            Serial.print(presetNum);
            Serial.print(") FL=");
            Serial.print(currentPresets[presetNum][0], 0);
            Serial.print(" FR=");
            Serial.print(currentPresets[presetNum][1], 0);
            Serial.print(" RL=");
            Serial.print(currentPresets[presetNum][2], 0);
            Serial.print(" RR=");
            Serial.println(currentPresets[presetNum][3], 0);

            applyPreset(presetNum);
        }
    }
    handleStatus();
}

void AirRideWebServer::handleSavePreset() {
    if (server.hasArg("n") && server.hasArg("fl") && server.hasArg("fr") && server.hasArg("rl") && server.hasArg("rr")) {
        int presetNum = server.arg("n").toInt();
        float fl = server.arg("fl").toFloat();
        float fr = server.arg("fr").toFloat();
        float rl = server.arg("rl").toFloat();
        float rr = server.arg("rr").toFloat();

        if (presetNum >= 0 && presetNum < NUM_PRESETS) {
            // Clamp values to safe range
            fl = constrain(fl, MIN_BAG_PSI, MAX_BAG_PSI);
            fr = constrain(fr, MIN_BAG_PSI, MAX_BAG_PSI);
            rl = constrain(rl, MIN_BAG_PSI, MAX_BAG_PSI);
            rr = constrain(rr, MIN_BAG_PSI, MAX_BAG_PSI);

            currentPresets[presetNum][0] = fl;
            currentPresets[presetNum][1] = fr;
            currentPresets[presetNum][2] = rl;
            currentPresets[presetNum][3] = rr;

            savePresetToEEPROM(presetNum);

            Serial.print("[WEB] /sp SAVE PRESET ");
            Serial.print(DEFAULT_PRESETS[presetNum].name);
            Serial.print(" FL=");
            Serial.print(fl, 0);
            Serial.print(" FR=");
            Serial.print(fr, 0);
            Serial.print(" RL=");
            Serial.print(rl, 0);
            Serial.print(" RR=");
            Serial.println(rr, 0);
        }
    }
    handleStatus();
}

void AirRideWebServer::savePresetToEEPROM(int presetNum) {
    if (presetNum < 0 || presetNum >= NUM_PRESETS) return;

    // Initialize EEPROM header on first write
    if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) {
        EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
        EEPROM.write(EEPROM_ADDR_VERSION, EEPROM_VERSION);
        EEPROM.write(EEPROM_ADDR_PRESET_FLAG, 0);
    }

    // Each preset is 16 bytes (4 floats)
    int baseAddr;
    switch (presetNum) {
        case 0: baseAddr = EEPROM_ADDR_PRESET1; break;
        case 1: baseAddr = EEPROM_ADDR_PRESET2; break;
        case 2: baseAddr = EEPROM_ADDR_PRESET3; break;
        default: return;
    }

    EEPROM.put(baseAddr,      currentPresets[presetNum][0]); // FL
    EEPROM.put(baseAddr + 4,  currentPresets[presetNum][1]); // FR
    EEPROM.put(baseAddr + 8,  currentPresets[presetNum][2]); // RL
    EEPROM.put(baseAddr + 12, currentPresets[presetNum][3]); // RR

    // Set the flag bit for this preset
    uint8_t flags = EEPROM.read(EEPROM_ADDR_PRESET_FLAG);
    flags |= (1 << presetNum);
    EEPROM.write(EEPROM_ADDR_PRESET_FLAG, flags);

    EEPROM.commit();
    Serial.print("Preset ");
    Serial.print(presetNum);
    Serial.println(" saved to EEPROM");
}

void AirRideWebServer::loadPresetsFromEEPROM() {
    if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) return;

    uint8_t flags = EEPROM.read(EEPROM_ADDR_PRESET_FLAG);

    for (int p = 0; p < NUM_PRESETS; p++) {
        if (!(flags & (1 << p))) continue; // Not saved, keep default

        int baseAddr;
        switch (p) {
            case 0: baseAddr = EEPROM_ADDR_PRESET1; break;
            case 1: baseAddr = EEPROM_ADDR_PRESET2; break;
            case 2: baseAddr = EEPROM_ADDR_PRESET3; break;
            default: continue;
        }

        EEPROM.get(baseAddr,      currentPresets[p][0]);
        EEPROM.get(baseAddr + 4,  currentPresets[p][1]);
        EEPROM.get(baseAddr + 8,  currentPresets[p][2]);
        EEPROM.get(baseAddr + 12, currentPresets[p][3]);

        // Validate — discard if any value is NaN/Inf/out of range
        bool valid = true;
        for (int i = 0; i < 4; i++) {
            if (isnan(currentPresets[p][i]) || isinf(currentPresets[p][i])
                || currentPresets[p][i] < MIN_BAG_PSI || currentPresets[p][i] > MAX_BAG_PSI) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            currentPresets[p][0] = DEFAULT_PRESETS[p].frontLeft;
            currentPresets[p][1] = DEFAULT_PRESETS[p].frontRight;
            currentPresets[p][2] = DEFAULT_PRESETS[p].rearLeft;
            currentPresets[p][3] = DEFAULT_PRESETS[p].rearRight;
            // Clear flag so we don't re-read garbage next boot
            flags &= ~(1 << p);
            EEPROM.write(EEPROM_ADDR_PRESET_FLAG, flags);
            EEPROM.commit();
            Serial.print("Invalid EEPROM data for preset ");
            Serial.print(p);
            Serial.println(" — using defaults");
            continue;
        }

        Serial.print("Loaded custom preset ");
        Serial.print(DEFAULT_PRESETS[p].name);
        Serial.print(": FL=");
        Serial.print(currentPresets[p][0], 0);
        Serial.print(" FR=");
        Serial.print(currentPresets[p][1], 0);
        Serial.print(" RL=");
        Serial.print(currentPresets[p][2], 0);
        Serial.print(" RR=");
        Serial.println(currentPresets[p][3], 0);
    }
}

void AirRideWebServer::handleLevel() {
    if (server.hasArg("m")) {
        int mode = server.arg("m").toInt();
        const char* modeNames[] = {"OFF", "FRONT", "REAR", "ALL"};
        if (mode >= 0 && mode <= 3) {
            levelMode = (LevelMode)mode;
            Serial.print("[WEB] /l LEVEL mode=");
            Serial.println(modeNames[mode]);
        }
    }
    handleStatus();
}

void AirRideWebServer::handlePumpOverride() {
    pumpEnabled = !pumpEnabled;
    Serial.print("[WEB] /po PUMP OVERRIDE ");
    Serial.println(pumpEnabled ? "ENABLED" : "DISABLED");
    if (!pumpEnabled) {
        compressor->setMode(PUMP_OFF);
    } else {
        compressor->setMode(PUMP_AUTO);
    }
    handleStatus();
}

void AirRideWebServer::handleDemoToggle() {
    setDemoMode(!demoMode);
    handleStatus();
}

void AirRideWebServer::applyPreset(int presetNum) {
    if (presetNum < 0 || presetNum >= NUM_PRESETS) return;

    bags[FRONT_LEFT].setTargetPressure(currentPresets[presetNum][0]);
    bags[FRONT_RIGHT].setTargetPressure(currentPresets[presetNum][1]);
    bags[REAR_LEFT].setTargetPressure(currentPresets[presetNum][2]);
    bags[REAR_RIGHT].setTargetPressure(currentPresets[presetNum][3]);

    // Start moving to targets
    for (int i = 0; i < NUM_BAGS; i++) {
        float current = bags[i].getPressure();
        float target = bags[i].getTargetPressure();
        if (current < target - 2.0) {
            if (!tankLockout) {
                bags[i].inflate();
            }
        } else if (current > target + 2.0) {
            bags[i].deflate();
        } else {
            bags[i].hold();
        }
    }
}

const char* AirRideWebServer::getPresetName(int presetNum) const {
    if (presetNum >= 0 && presetNum < NUM_PRESETS) {
        return DEFAULT_PRESETS[presetNum].name;
    }
    return "Unknown";
}

void AirRideWebServer::handleTimeSync() {
    if (server.hasArg("t")) {
        long epoch = server.arg("t").toInt();
        if (epoch > 1600000000L) { // Sanity check: after ~Sep 2020
            struct timeval tv;
            tv.tv_sec = epoch;
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);
            timeSynced = true;

            struct tm timeinfo;
            localtime_r(&tv.tv_sec, &timeinfo);
            char buf[32];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
            Serial.print("[WEB] /time synced from browser: ");
            Serial.println(buf);
        }
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

// ============================================
// LEAK MONITOR
// ============================================

void AirRideWebServer::loadLeakSnapshot() {
    if (EEPROM.read(EEPROM_ADDR_LEAK_FLAG) != LEAK_SNAPSHOT_VALID) return;

    EEPROM.get(EEPROM_ADDR_LEAK_TIME, leakSnapshotEpoch);
    if (leakSnapshotEpoch < 1600000000UL) return; // Invalid timestamp

    for (int i = 0; i < NUM_BAGS + 1; i++) {
        EEPROM.get(EEPROM_ADDR_LEAK_PRESSURES + i * 4, leakSnapshotPressures[i]);
        if (isnan(leakSnapshotPressures[i]) || isinf(leakSnapshotPressures[i])) {
            Serial.println("Leak snapshot has corrupt data — discarding");
            return;
        }
    }

    leakSnapshotValid = true;
    Serial.print("Leak snapshot loaded (");
    struct tm timeinfo;
    time_t t = (time_t)leakSnapshotEpoch;
    localtime_r(&t, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.print(buf);
    Serial.print("): FL=");
    Serial.print(leakSnapshotPressures[0], 1);
    Serial.print(" FR=");
    Serial.print(leakSnapshotPressures[1], 1);
    Serial.print(" RL=");
    Serial.print(leakSnapshotPressures[2], 1);
    Serial.print(" RR=");
    Serial.print(leakSnapshotPressures[3], 1);
    Serial.print(" Tank=");
    Serial.println(leakSnapshotPressures[4], 1);
}

void AirRideWebServer::saveLeakSnapshot() {
    time_t now = time(NULL);
    leakSnapshotEpoch = (uint32_t)now;

    leakSnapshotPressures[0] = bags[FRONT_LEFT].getPressure();
    leakSnapshotPressures[1] = bags[FRONT_RIGHT].getPressure();
    leakSnapshotPressures[2] = bags[REAR_LEFT].getPressure();
    leakSnapshotPressures[3] = bags[REAR_RIGHT].getPressure();
    leakSnapshotPressures[4] = *tankPressure;

    EEPROM.write(EEPROM_ADDR_LEAK_FLAG, LEAK_SNAPSHOT_VALID);
    EEPROM.put(EEPROM_ADDR_LEAK_TIME, leakSnapshotEpoch);
    for (int i = 0; i < NUM_BAGS + 1; i++) {
        EEPROM.put(EEPROM_ADDR_LEAK_PRESSURES + i * 4, leakSnapshotPressures[i]);
    }
    EEPROM.commit();

    leakSnapshotValid = true;
    lastLeakSnapshotSave = millis();

    Serial.print("Leak snapshot saved: FL=");
    Serial.print(leakSnapshotPressures[0], 1);
    Serial.print(" FR=");
    Serial.print(leakSnapshotPressures[1], 1);
    Serial.print(" RL=");
    Serial.print(leakSnapshotPressures[2], 1);
    Serial.print(" RR=");
    Serial.print(leakSnapshotPressures[3], 1);
    Serial.print(" Tank=");
    Serial.println(leakSnapshotPressures[4], 1);
}

void AirRideWebServer::updateLeakSnapshot() {
    if (!timeSynced) return;

    unsigned long now = millis();
    if (now - lastLeakSnapshotSave < LEAK_SNAPSHOT_INTERVAL) return;

    // Only save when all bags are holding (not actively inflating/deflating)
    for (int i = 0; i < NUM_BAGS; i++) {
        if (!bags[i].isHolding()) return;
    }

    // Need at least one sensor with meaningful pressure
    bool hasPressure = (*tankPressure > LEAK_MIN_SNAPSHOT_PSI);
    if (!hasPressure) {
        for (int i = 0; i < NUM_BAGS; i++) {
            if (bags[i].getPressure() > LEAK_MIN_SNAPSHOT_PSI) {
                hasPressure = true;
                break;
            }
        }
    }
    if (!hasPressure) return;

    saveLeakSnapshot();
}

void AirRideWebServer::handleLeakStatus() {
    // Handle reset
    if (server.hasArg("reset") && server.arg("reset") == "1") {
        EEPROM.write(EEPROM_ADDR_LEAK_FLAG, 0);
        EEPROM.commit();
        leakSnapshotValid = false;
        leakSnapshotEpoch = 0;
        Serial.println("[WEB] /leak RESET — snapshot cleared");
        server.send(200, "application/json", "{\"valid\":false}");
        return;
    }

    if (!leakSnapshotValid || !timeSynced) {
        server.send(200, "application/json", "{\"valid\":false}");
        return;
    }

    time_t now = time(NULL);
    long elapsed = (long)now - (long)leakSnapshotEpoch;
    if (elapsed < 0) elapsed = 0;
    float elapsedHours = elapsed / 3600.0;

    float current[5];
    current[0] = bags[FRONT_LEFT].getPressure();
    current[1] = bags[FRONT_RIGHT].getPressure();
    current[2] = bags[REAR_LEFT].getPressure();
    current[3] = bags[REAR_RIGHT].getPressure();
    current[4] = *tankPressure;

    String json = "{\"valid\":true,\"elapsed\":";
    json += String(elapsed);

    json += ",\"snapshot\":[";
    for (int i = 0; i < 5; i++) {
        if (i > 0) json += ",";
        json += String(leakSnapshotPressures[i], 1);
    }

    json += "],\"current\":[";
    for (int i = 0; i < 5; i++) {
        if (i > 0) json += ",";
        json += String(current[i], 1);
    }

    json += "],\"rates\":[";
    for (int i = 0; i < 5; i++) {
        if (i > 0) json += ",";
        float drop = leakSnapshotPressures[i] - current[i];
        float rate = (elapsedHours > 0.01) ? (drop / elapsedHours) : 0.0;
        json += String(rate, 2);
    }

    json += "],\"status\":[";
    for (int i = 0; i < 5; i++) {
        if (i > 0) json += ",";
        float drop = leakSnapshotPressures[i] - current[i];
        float rate = (elapsedHours > 0.01) ? (drop / elapsedHours) : 0.0;
        // Sensors that weren't pressurized are always "ok"
        if (leakSnapshotPressures[i] < LEAK_MIN_SNAPSHOT_PSI) {
            json += "0";
        } else if (drop >= LEAK_ALERT_DROP_PSI && rate >= LEAK_ALERT_RATE_PSI_HR) {
            json += "2"; // leak
        } else if (drop >= LEAK_WARN_DROP_PSI && rate >= LEAK_WARN_RATE_PSI_HR) {
            json += "1"; // warn
        } else {
            json += "0"; // ok
        }
    }
    json += "]}";

    server.send(200, "application/json", json);
}

// ============================================
// TANK MAINTENANCE TIMER
// ============================================

void AirRideWebServer::loadTankMaintFromEEPROM() {
    if (EEPROM.read(EEPROM_ADDR_TANK_MAINT_FLAG) != TANK_MAINT_VALID) return;

    EEPROM.get(EEPROM_ADDR_TANK_MAINT_EPOCH, tankMaintLastService);
    if (tankMaintLastService < 1600000000UL) return; // Invalid timestamp

    tankMaintValid = true;

    struct tm timeinfo;
    time_t t = (time_t)tankMaintLastService;
    localtime_r(&t, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.print("Tank maintenance last service: ");
    Serial.println(buf);
}

void AirRideWebServer::saveTankMaintToEEPROM(uint32_t epoch) {
    tankMaintLastService = epoch;
    tankMaintValid = true;

    EEPROM.write(EEPROM_ADDR_TANK_MAINT_FLAG, TANK_MAINT_VALID);
    EEPROM.put(EEPROM_ADDR_TANK_MAINT_EPOCH, tankMaintLastService);
    EEPROM.commit();

    Serial.print("Tank maintenance saved: epoch=");
    Serial.println(tankMaintLastService);
}

bool AirRideWebServer::isTankMaintDue() const {
    if (!tankMaintValid || !timeSynced) return false;

    time_t now = time(NULL);
    uint32_t elapsed = (uint32_t)now - tankMaintLastService;
    return elapsed >= TANK_MAINT_INTERVAL_SEC;
}

int AirRideWebServer::getTankMaintDaysRemaining() const {
    if (!tankMaintValid || !timeSynced) return -1;

    time_t now = time(NULL);
    uint32_t elapsed = (uint32_t)now - tankMaintLastService;
    if (elapsed >= TANK_MAINT_INTERVAL_SEC) {
        // Overdue — return negative days
        return -((int)(elapsed - TANK_MAINT_INTERVAL_SEC) / 86400);
    }
    return (int)(TANK_MAINT_INTERVAL_SEC - elapsed) / 86400;
}

void AirRideWebServer::handleTankMaint() {
    // Reset: mark current time as last service
    if (server.hasArg("reset") && server.arg("reset") == "1") {
        if (!timeSynced) {
            server.send(200, "application/json", "{\"error\":\"Time not synced\"}");
            return;
        }
        time_t now = time(NULL);
        saveTankMaintToEEPROM((uint32_t)now);
        Serial.println("[WEB] /tank RESET — service complete");
    }

    // Set specific epoch (debug): /tank?set=<epoch>
    if (server.hasArg("set")) {
        uint32_t epoch = (uint32_t)server.arg("set").toInt();
        if (epoch > 1600000000UL) {
            saveTankMaintToEEPROM(epoch);
            Serial.print("[WEB] /tank SET epoch=");
            Serial.println(epoch);
        }
    }

    // Return status JSON
    String json = "{\"valid\":";
    json += tankMaintValid ? "true" : "false";
    if (tankMaintValid) {
        json += ",\"lastService\":";
        json += String(tankMaintLastService);
        if (timeSynced) {
            json += ",\"due\":";
            json += isTankMaintDue() ? "true" : "false";
            json += ",\"daysRemaining\":";
            json += String(getTankMaintDaysRemaining());
        }
    }
    json += ",\"timeSynced\":";
    json += timeSynced ? "true" : "false";
    json += "}";

    server.send(200, "application/json", json);
}

void AirRideWebServer::handleSimLeak() {
    // Start simulated leak: /simleak?target=<0-4|random>  (0=FL,1=FR,2=RL,3=RR,4=tank)
    // Stop simulated leak:  /simleak?stop=1
    // Optional rate:        /simleak?target=2&rate=0.3

    if (server.hasArg("stop") && server.arg("stop") == "1") {
        simLeakTarget = -1;
        Serial.println("[SIM] Leak simulation STOPPED");
    } else if (server.hasArg("target")) {
        String targetStr = server.arg("target");
        if (targetStr == "random") {
            // Pick a random bag or tank (0-4)
            simLeakTarget = random(0, 5);
        } else {
            simLeakTarget = targetStr.toInt();
            if (simLeakTarget < 0 || simLeakTarget > 4) simLeakTarget = -1;
        }

        // Optional custom rate
        if (server.hasArg("rate")) {
            simLeakRate = server.arg("rate").toFloat();
            if (simLeakRate <= 0) simLeakRate = SIM_LEAK_RATE_PSI_TICK;
        } else {
            simLeakRate = SIM_LEAK_RATE_PSI_TICK;
        }

        const char* names[] = {"FL", "FR", "RL", "RR", "TANK"};
        if (simLeakTarget >= 0 && simLeakTarget <= 4) {
            Serial.print("[SIM] Leak simulation STARTED on ");
            Serial.print(names[simLeakTarget]);
            Serial.print(" at ");
            Serial.print(simLeakRate, 3);
            Serial.println(" PSI/tick");
        }
    }

    // Return current leak sim status
    String json = "{\"active\":";
    json += (simLeakTarget >= 0) ? "true" : "false";
    json += ",\"target\":";
    json += String(simLeakTarget);
    if (simLeakTarget >= 0 && simLeakTarget <= 4) {
        const char* names[] = {"FL", "FR", "RL", "RR", "TANK"};
        json += ",\"targetName\":\"";
        json += names[simLeakTarget];
        json += "\"";
    }
    json += ",\"rate\":";
    json += String(simLeakRate, 3);
    json += "}";

    server.send(200, "application/json", json);
}

// ============================================
// SENSOR CALIBRATION
// ============================================

bool AirRideWebServer::validateCalibration(const SensorCalibration& cal) {
    if (isnan(cal.offset) || isinf(cal.offset)) return false;
    if (isnan(cal.gain) || isinf(cal.gain)) return false;
    if (isnan(cal.refResistor) || isinf(cal.refResistor)) return false;
    if (cal.offset < CAL_OFFSET_MIN || cal.offset > CAL_OFFSET_MAX) return false;
    if (cal.gain < CAL_GAIN_MIN || cal.gain > CAL_GAIN_MAX) return false;
    if (cal.refResistor < CAL_REF_RESISTOR_MIN || cal.refResistor > CAL_REF_RESISTOR_MAX) return false;
    return true;
}

void AirRideWebServer::loadCalibrationFromEEPROM() {
    if (EEPROM.read(EEPROM_ADDR_CAL_FLAG) != CAL_VALID_FLAG) {
        Serial.println("No calibration data in EEPROM — using defaults");
        return;
    }

    Serial.print("Loading calibration from EEPROM: ");
    const char* sensorNames[] = {"Tank", "FL", "FR", "RL", "RR"};

    for (int i = 0; i < CAL_NUM_SENSORS; i++) {
        int addr = EEPROM_ADDR_CAL_DATA + (i * 12);
        SensorCalibration cal;
        EEPROM.get(addr,     cal.offset);
        EEPROM.get(addr + 4, cal.gain);
        EEPROM.get(addr + 8, cal.refResistor);

        if (!validateCalibration(cal)) {
            Serial.print(sensorNames[i]);
            Serial.print("=INVALID ");
            continue;
        }

        if (i == 0) {
            // Tank sensor
            tankCalibration = cal;
            tankCalibrated = (cal.offset != 0.0 || cal.gain != 1.0 || cal.refResistor != REFERENCE_RESISTOR);
        } else {
            // Bag sensor (index 1-4 maps to bags[0-3])
            bags[i - 1].setCalibration(cal);
        }

        Serial.print(sensorNames[i]);
        Serial.print("(o=");
        Serial.print(cal.offset, 2);
        Serial.print(" g=");
        Serial.print(cal.gain, 3);
        Serial.print(" r=");
        Serial.print(cal.refResistor, 1);
        Serial.print(") ");
    }
    Serial.println();
}

void AirRideWebServer::saveCalibrationToEEPROM() {
    // Initialize EEPROM header if needed
    if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) {
        EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
        EEPROM.write(EEPROM_ADDR_VERSION, EEPROM_VERSION);
    }

    EEPROM.write(EEPROM_ADDR_CAL_FLAG, CAL_VALID_FLAG);

    // Save tank calibration (sensor 0)
    int addr = EEPROM_ADDR_CAL_DATA;
    EEPROM.put(addr,     tankCalibration.offset);
    EEPROM.put(addr + 4, tankCalibration.gain);
    EEPROM.put(addr + 8, tankCalibration.refResistor);

    // Save bag calibrations (sensors 1-4)
    for (int i = 0; i < NUM_BAGS; i++) {
        addr = EEPROM_ADDR_CAL_DATA + ((i + 1) * 12);
        const SensorCalibration& cal = bags[i].getCalibration();
        EEPROM.put(addr,     cal.offset);
        EEPROM.put(addr + 4, cal.gain);
        EEPROM.put(addr + 8, cal.refResistor);
    }

    EEPROM.commit();
    Serial.println("Calibration saved to EEPROM");
}

void AirRideWebServer::handleCalibration() {
    // SET calibration: /cal?s=<sensor>&o=<offset>&g=<gain>&r=<refResistor>
    // sensor: 0=tank, 1=FL, 2=FR, 3=RL, 4=RR
    // All params optional except s (reads current if no set params)
    // ZERO: /cal?s=<sensor>&zero=<rawPsi>  — sets offset so this rawPsi reads as 0
    // SPAN: /cal?s=<sensor>&span_raw=<rawPsi>&span_ref=<actualPsi> — sets gain

    if (server.hasArg("s")) {
        int sensor = server.arg("s").toInt();
        if (sensor < 0 || sensor >= CAL_NUM_SENSORS) {
            server.send(400, "application/json", "{\"error\":\"Invalid sensor (0-4)\"}");
            return;
        }

        // Get current calibration for this sensor
        SensorCalibration cal;
        if (sensor == 0) {
            cal = tankCalibration;
        } else {
            cal = bags[sensor - 1].getCalibration();
        }

        bool changed = false;

        // Zero calibration: offset = -rawPsi (so rawPsi reads as 0)
        if (server.hasArg("zero")) {
            float rawPsi = server.arg("zero").toFloat();
            cal.offset = -rawPsi * cal.gain;
            changed = true;
            Serial.print("[CAL] Zero sensor ");
            Serial.print(sensor);
            Serial.print(" rawPsi=");
            Serial.print(rawPsi, 2);
            Serial.print(" -> offset=");
            Serial.println(cal.offset, 2);
        }

        // Span calibration: given rawPsi and actual reference PSI, compute gain
        if (server.hasArg("span_raw") && server.hasArg("span_ref")) {
            float spanRaw = server.arg("span_raw").toFloat();
            float spanRef = server.arg("span_ref").toFloat();
            if (spanRaw > 0.1) {
                cal.gain = spanRef / spanRaw;
                // Recalculate offset if zero was set before
                // offset stays as-is since it's applied after gain
                changed = true;
                Serial.print("[CAL] Span sensor ");
                Serial.print(sensor);
                Serial.print(" raw=");
                Serial.print(spanRaw, 1);
                Serial.print(" ref=");
                Serial.print(spanRef, 1);
                Serial.print(" -> gain=");
                Serial.println(cal.gain, 4);
            }
        }

        // Direct set: offset, gain, refResistor
        if (server.hasArg("o")) {
            cal.offset = server.arg("o").toFloat();
            changed = true;
        }
        if (server.hasArg("g")) {
            cal.gain = server.arg("g").toFloat();
            changed = true;
        }
        if (server.hasArg("r")) {
            cal.refResistor = server.arg("r").toFloat();
            changed = true;
        }

        if (changed) {
            if (!validateCalibration(cal)) {
                server.send(400, "application/json", "{\"error\":\"Calibration out of bounds\"}");
                return;
            }

            // Apply the calibration
            if (sensor == 0) {
                tankCalibration = cal;
                tankCalibrated = (cal.offset != 0.0 || cal.gain != 1.0 || cal.refResistor != REFERENCE_RESISTOR);
            } else {
                bags[sensor - 1].setCalibration(cal);
            }

            // Save to EEPROM
            saveCalibrationToEEPROM();
        }
    }

    // Return current calibration state for all sensors
    String json = "{\"sensors\":[";
    const char* sensorNames[] = {"Tank", "FL", "FR", "RL", "RR"};

    for (int i = 0; i < CAL_NUM_SENSORS; i++) {
        if (i > 0) json += ",";

        SensorCalibration cal;
        bool isCal;
        float currentPsi;
        if (i == 0) {
            cal = tankCalibration;
            isCal = tankCalibrated;
            currentPsi = *tankPressure;
        } else {
            cal = bags[i - 1].getCalibration();
            isCal = bags[i - 1].isCalibrated();
            currentPsi = bags[i - 1].getPressure();
        }

        json += "{\"name\":\"";
        json += sensorNames[i];
        json += "\",\"calibrated\":";
        json += isCal ? "true" : "false";
        json += ",\"offset\":";
        json += String(cal.offset, 3);
        json += ",\"gain\":";
        json += String(cal.gain, 4);
        json += ",\"refResistor\":";
        json += String(cal.refResistor, 1);
        json += ",\"currentPsi\":";
        json += String(currentPsi, 1);
        json += "}";
    }

    json += "]}";
    server.send(200, "application/json", json);
}

void AirRideWebServer::handleCalibrationReset() {
    // Reset all sensors to factory defaults
    // Optional: /calreset?s=<sensor> to reset single sensor

    if (server.hasArg("s")) {
        int sensor = server.arg("s").toInt();
        if (sensor < 0 || sensor >= CAL_NUM_SENSORS) {
            server.send(400, "application/json", "{\"error\":\"Invalid sensor (0-4)\"}");
            return;
        }

        SensorCalibration defaults = { 0.0, 1.0, REFERENCE_RESISTOR };
        if (sensor == 0) {
            tankCalibration = defaults;
            tankCalibrated = false;
        } else {
            bags[sensor - 1].setCalibration(defaults);
        }

        Serial.print("[CAL] Reset sensor ");
        Serial.println(sensor);
    } else {
        // Reset all
        SensorCalibration defaults = { 0.0, 1.0, REFERENCE_RESISTOR };
        tankCalibration = defaults;
        tankCalibrated = false;
        for (int i = 0; i < NUM_BAGS; i++) {
            bags[i].setCalibration(defaults);
        }
        Serial.println("[CAL] All sensors reset to factory defaults");
    }

    saveCalibrationToEEPROM();
    handleCalibration(); // Return updated state
}

void AirRideWebServer::handleNotFound() {
    Serial.print("[WEB] 404 Not Found: ");
    Serial.println(server.uri());
    server.send(404, "text/plain", "Not Found");
}

void AirRideWebServer::updateTankLockout(float tankPressure) {
    // Hysteresis: lock out at TANK_CUTOFF_PSI, resume at TANK_RESUME_PSI
    if (tankLockout) {
        if (tankPressure >= TANK_RESUME_PSI) {
            tankLockout = false;
            Serial.println("Tank pressure restored - inflation enabled");
        }
    } else {
        if (tankPressure < TANK_CUTOFF_PSI) {
            tankLockout = true;
            // Stop all inflation
            for (int i = 0; i < NUM_BAGS; i++) {
                if (bags[i].isInflating()) {
                    bags[i].hold();
                }
            }
            Serial.println("Tank pressure low - inflation disabled");
        }
    }
}

void AirRideWebServer::updateLevelMode() {
    if (levelMode == LEVEL_OFF) return;

    unsigned long currentTime = millis();
    if (currentTime - lastLevelAdjust < LEVEL_ADJUST_STEP_MS) return;
    lastLevelAdjust = currentTime;

    float fl = bags[FRONT_LEFT].getPressure();
    float fr = bags[FRONT_RIGHT].getPressure();
    float rl = bags[REAR_LEFT].getPressure();
    float rr = bags[REAR_RIGHT].getPressure();

    switch (levelMode) {
        case LEVEL_FRONT: {
            // Match front left and right
            float frontAvg = (fl + fr) / 2.0;
            if (abs(fl - fr) > LEVEL_TOLERANCE_PSI) {
                bags[FRONT_LEFT].setTargetPressure(frontAvg);
                bags[FRONT_RIGHT].setTargetPressure(frontAvg);
            }
            break;
        }
        case LEVEL_REAR: {
            // Match rear left and right
            float rearAvg = (rl + rr) / 2.0;
            if (abs(rl - rr) > LEVEL_TOLERANCE_PSI) {
                bags[REAR_LEFT].setTargetPressure(rearAvg);
                bags[REAR_RIGHT].setTargetPressure(rearAvg);
            }
            break;
        }
        case LEVEL_ALL: {
            // Match front pair and rear pair
            float frontAvg = (fl + fr) / 2.0;
            float rearAvg = (rl + rr) / 2.0;
            if (abs(fl - fr) > LEVEL_TOLERANCE_PSI) {
                bags[FRONT_LEFT].setTargetPressure(frontAvg);
                bags[FRONT_RIGHT].setTargetPressure(frontAvg);
            }
            if (abs(rl - rr) > LEVEL_TOLERANCE_PSI) {
                bags[REAR_LEFT].setTargetPressure(rearAvg);
                bags[REAR_RIGHT].setTargetPressure(rearAvg);
            }
            break;
        }
        default:
            break;
    }
}

