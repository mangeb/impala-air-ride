#include "AirRideWebServer.h"
#include "html_content.h"  // Auto-generated gzipped React UI

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
      hasStoredHeight(false) {
    for (int i = 0; i < NUM_BAGS; i++) {
        lastHeight[i] = 0.0;
    }
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

    // Load saved ride height and custom presets from EEPROM
    loadRideHeight();
    loadPresetsFromEEPROM();

    // Setup routes
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/s", HTTP_GET, [this]() { handleStatus(); });
    server.on("/b", HTTP_GET, [this]() { handleBag(); });
    server.on("/bh", HTTP_GET, [this]() { handleBagHold(); });
    server.on("/bt", HTTP_GET, [this]() { handleBagTarget(); });
    server.on("/p", HTTP_GET, [this]() { handlePreset(); });
    server.on("/sp", HTTP_GET, [this]() { handleSavePreset(); });
    server.on("/l", HTTP_GET, [this]() { handleLevel(); });
    server.on("/sh", HTTP_GET, [this]() { handleSaveHeight(); });
    server.on("/rh", HTTP_GET, [this]() { handleRestoreHeight(); });
    server.on("/po", HTTP_GET, [this]() { handlePumpOverride(); });
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
}

void AirRideWebServer::handleRoot() {
    Serial.println("[WEB] GET / - Serving React UI (gzip, " + String(HTML_CONTENT_SIZE) + " bytes)");
    // Serve gzipped React UI from PROGMEM
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html", (const char*)HTML_CONTENT, HTML_CONTENT_SIZE);
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
    json += ",\"hasHeight\":";
    json += hasStoredHeight ? "true" : "false";
    json += ",\"pumpEnabled\":";
    json += pumpEnabled ? "true" : "false";

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

void AirRideWebServer::handleSaveHeight() {
    Serial.print("[WEB] /sh SAVE HEIGHT FL=");
    Serial.print(bags[FRONT_LEFT].getPressure(), 1);
    Serial.print(" FR=");
    Serial.print(bags[FRONT_RIGHT].getPressure(), 1);
    Serial.print(" RL=");
    Serial.print(bags[REAR_LEFT].getPressure(), 1);
    Serial.print(" RR=");
    Serial.println(bags[REAR_RIGHT].getPressure(), 1);
    // Save current pressures to EEPROM
    for (int i = 0; i < NUM_BAGS; i++) {
        lastHeight[i] = bags[i].getPressure();
    }
    saveRideHeight();
    handleStatus();
}

void AirRideWebServer::handleRestoreHeight() {
    Serial.print("[WEB] /rh RESTORE HEIGHT");
    if (hasStoredHeight) {
        Serial.print(" FL=");
        Serial.print(lastHeight[FRONT_LEFT], 1);
        Serial.print(" FR=");
        Serial.print(lastHeight[FRONT_RIGHT], 1);
        Serial.print(" RL=");
        Serial.print(lastHeight[REAR_LEFT], 1);
        Serial.print(" RR=");
        Serial.println(lastHeight[REAR_RIGHT], 1);
    } else {
        Serial.println(" (no saved data)");
    }
    if (hasStoredHeight) {
        // Set targets to stored heights
        bags[FRONT_LEFT].setTargetPressure(lastHeight[FRONT_LEFT]);
        bags[FRONT_RIGHT].setTargetPressure(lastHeight[FRONT_RIGHT]);
        bags[REAR_LEFT].setTargetPressure(lastHeight[REAR_LEFT]);
        bags[REAR_RIGHT].setTargetPressure(lastHeight[REAR_RIGHT]);

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
            }
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

void AirRideWebServer::handleNotFound() {
    Serial.print("[WEB] 404 Not Found: ");
    Serial.println(server.uri());
    server.send(404, "text/plain", "Not Found");
}

void AirRideWebServer::saveRideHeight() {
    bool firstWrite = (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC);
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.write(EEPROM_ADDR_VERSION, EEPROM_VERSION);
    if (firstWrite) {
        EEPROM.write(EEPROM_ADDR_PRESET_FLAG, 0); // No custom presets saved yet
    }
    EEPROM.put(EEPROM_ADDR_LAST_FL, lastHeight[FRONT_LEFT]);
    EEPROM.put(EEPROM_ADDR_LAST_FR, lastHeight[FRONT_RIGHT]);
    EEPROM.put(EEPROM_ADDR_LAST_RL, lastHeight[REAR_LEFT]);
    EEPROM.put(EEPROM_ADDR_LAST_RR, lastHeight[REAR_RIGHT]);
    EEPROM.commit();
    hasStoredHeight = true;
    Serial.println("Ride height saved to EEPROM");
}

void AirRideWebServer::loadRideHeight() {
    if (EEPROM.read(EEPROM_ADDR_MAGIC) == EEPROM_MAGIC) {
        EEPROM.get(EEPROM_ADDR_LAST_FL, lastHeight[FRONT_LEFT]);
        EEPROM.get(EEPROM_ADDR_LAST_FR, lastHeight[FRONT_RIGHT]);
        EEPROM.get(EEPROM_ADDR_LAST_RL, lastHeight[REAR_LEFT]);
        EEPROM.get(EEPROM_ADDR_LAST_RR, lastHeight[REAR_RIGHT]);
        hasStoredHeight = true;
        Serial.print("Loaded ride height: FL=");
        Serial.print(lastHeight[FRONT_LEFT]);
        Serial.print(" FR=");
        Serial.print(lastHeight[FRONT_RIGHT]);
        Serial.print(" RL=");
        Serial.print(lastHeight[REAR_LEFT]);
        Serial.print(" RR=");
        Serial.println(lastHeight[REAR_RIGHT]);
    } else {
        hasStoredHeight = false;
        Serial.println("No saved ride height found");
    }
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

uint32_t AirRideWebServer::getHtmlSize() {
    return HTML_CONTENT_SIZE;
}
