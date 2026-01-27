#include "WebServer.h"

// HTML page stored in PROGMEM - Enhanced with hold buttons, target display, level mode
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Impala Air Ride</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui;background:#1a1a1a;color:#fff;padding:10px;user-select:none;-webkit-user-select:none}
h1{font-size:18px;text-align:center;margin-bottom:15px;color:#ffd700}
.tank{text-align:center;padding:10px;background:#333;border-radius:8px;margin-bottom:10px}
.tank span{font-size:24px;font-weight:bold}
.tank .lockout{color:#f44336;font-size:12px;display:none}
.tank.low .lockout{display:block}
.presets{display:flex;gap:6px;margin-bottom:10px}
.presets button{flex:1;padding:12px 5px;font-size:14px;border:none;border-radius:8px;cursor:pointer;color:#fff}
.presets button:nth-child(1){background:#2196F3}
.presets button:nth-child(2){background:#4CAF50}
.presets button:nth-child(3){background:#ff9800}
.presets button:active{opacity:0.7}
.memory{display:flex;gap:6px;margin-bottom:10px}
.memory button{flex:1;padding:10px;font-size:12px;border:none;border-radius:8px;cursor:pointer;background:#555;color:#fff}
.memory button:active{opacity:0.7}
.memory button.has-data{background:#9c27b0}
.level{display:flex;gap:6px;margin-bottom:10px}
.level button{flex:1;padding:8px;font-size:11px;border:2px solid #444;border-radius:8px;cursor:pointer;background:#333;color:#888}
.level button.active{border-color:#4CAF50;color:#4CAF50}
.level button:active{opacity:0.7}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.bag{background:#333;padding:12px;border-radius:8px;text-align:center}
.bag h3{font-size:12px;margin-bottom:3px;color:#aaa}
.bag .psi{font-size:32px;font-weight:bold;margin:5px 0}
.bag .target{font-size:11px;color:#888;margin-bottom:8px}
.bag .target span{color:#4CAF50}
.bag .btns{display:flex;gap:5px;justify-content:center}
.bag button{width:60px;height:60px;font-size:28px;border:none;border-radius:8px;cursor:pointer;color:#fff;transition:opacity 0.1s}
.bag button:active,.bag button.held{opacity:0.7;transform:scale(0.95)}
.bag .up{background:#4CAF50}
.bag .dn{background:#f44336}
.bag.timeout button{opacity:0.3;pointer-events:none}
.pump{margin-top:10px;padding:8px;background:#333;border-radius:8px;text-align:center;font-size:12px}
.pump .runtime{color:#666;font-size:10px;margin-top:4px}
.status{font-size:10px;color:#666;text-align:center;margin-top:8px}
</style>
</head>
<body>
<h1>1964 IMPALA AIR RIDE</h1>
<div class="tank" id="tankDiv">Tank: <span id="tk">--</span> PSI<div class="lockout">TANK LOW - INFLATE DISABLED</div></div>
<div class="presets">
<button onclick="pr(0)">LAY</button>
<button onclick="pr(1)">CRUISE</button>
<button onclick="pr(2)">MAX</button>
</div>
<div class="memory">
<button id="saveBtn" onclick="saveH()">SAVE HEIGHT</button>
<button id="restoreBtn" onclick="restoreH()">RESTORE</button>
</div>
<div class="level">
<button id="lvlOff" onclick="lvl(0)">LEVEL OFF</button>
<button id="lvlFront" onclick="lvl(1)">FRONT</button>
<button id="lvlRear" onclick="lvl(2)">REAR</button>
<button id="lvlAll" onclick="lvl(3)">ALL</button>
</div>
<div class="grid">
<div class="bag" id="bag0"><h3>FRONT LEFT</h3><div class="psi" id="b0">--</div><div class="target">Target: <span id="t0">--</span></div><div class="btns"><button class="up" data-b="0" data-d="1">+</button><button class="dn" data-b="0" data-d="-1">-</button></div></div>
<div class="bag" id="bag1"><h3>FRONT RIGHT</h3><div class="psi" id="b1">--</div><div class="target">Target: <span id="t1">--</span></div><div class="btns"><button class="up" data-b="1" data-d="1">+</button><button class="dn" data-b="1" data-d="-1">-</button></div></div>
<div class="bag" id="bag2"><h3>REAR LEFT</h3><div class="psi" id="b2">--</div><div class="target">Target: <span id="t2">--</span></div><div class="btns"><button class="up" data-b="2" data-d="1">+</button><button class="dn" data-b="2" data-d="-1">-</button></div></div>
<div class="bag" id="bag3"><h3>REAR RIGHT</h3><div class="psi" id="b3">--</div><div class="target">Target: <span id="t3">--</span></div><div class="btns"><button class="up" data-b="3" data-d="1">+</button><button class="dn" data-b="3" data-d="-1">-</button></div></div>
</div>
<div class="pump">Pumps: <span id="pm">--</span><div class="runtime" id="rt"></div></div>
<div class="status">ESP32 Advanced Air Ride Controller</div>
<script>
var holdInt=null,holdBag=-1,holdDir=0;

// Hold button handlers
document.querySelectorAll('.bag button').forEach(function(btn){
  btn.addEventListener('touchstart',function(e){
    e.preventDefault();
    startHold(this);
  });
  btn.addEventListener('mousedown',function(e){
    startHold(this);
  });
  btn.addEventListener('touchend',stopHold);
  btn.addEventListener('touchcancel',stopHold);
  btn.addEventListener('mouseup',stopHold);
  btn.addEventListener('mouseleave',stopHold);
});

function startHold(btn){
  var b=parseInt(btn.dataset.b);
  var d=parseInt(btn.dataset.d);
  btn.classList.add('held');
  holdBag=b;holdDir=d;
  // Immediate action
  fetch('/b?n='+b+'&d='+d+'&h=1');
  // Continuous while held
  holdInt=setInterval(function(){
    fetch('/b?n='+b+'&d='+d+'&h=1');
  },100);
}

function stopHold(){
  if(holdInt){
    clearInterval(holdInt);
    holdInt=null;
  }
  document.querySelectorAll('.bag button').forEach(function(b){b.classList.remove('held')});
  if(holdBag>=0){
    fetch('/bh?n='+holdBag);  // Signal hold release
    holdBag=-1;holdDir=0;
  }
}

function pr(p){fetch('/p?n='+p).then(upd)}
function lvl(m){fetch('/l?m='+m).then(upd)}
function saveH(){fetch('/sh').then(upd)}
function restoreH(){fetch('/rh').then(upd)}

function upd(){fetch('/s').then(function(r){return r.json()}).then(function(d){
  document.getElementById('tk').textContent=d.tank.toFixed(0);
  var tankDiv=document.getElementById('tankDiv');
  if(d.lockout){tankDiv.classList.add('low')}else{tankDiv.classList.remove('low')}
  for(var i=0;i<4;i++){
    document.getElementById('b'+i).textContent=d.bags[i].toFixed(0);
    document.getElementById('t'+i).textContent=d.targets[i].toFixed(0);
    var bagDiv=document.getElementById('bag'+i);
    if(d.timeouts&&d.timeouts[i]){bagDiv.classList.add('timeout')}else{bagDiv.classList.remove('timeout')}
  }
  document.getElementById('pm').textContent=d.pump;
  if(d.runtime){document.getElementById('rt').textContent='Runtime: '+d.runtime}
  // Update level buttons
  document.querySelectorAll('.level button').forEach(function(b,idx){
    if(idx==d.level){b.classList.add('active')}else{b.classList.remove('active')}
  });
  // Update restore button
  var restoreBtn=document.getElementById('restoreBtn');
  if(d.hasHeight){restoreBtn.classList.add('has-data')}else{restoreBtn.classList.remove('has-data')}
})}
setInterval(upd,400);upd();
</script>
</body>
</html>
)rawliteral";

AirRideWebServer::AirRideWebServer(AirBag* b, Compressor* c, float* tp)
    : bags(b),
      compressor(c),
      tankPressure(tp),
      server(80),
      wifiReady(false),
      levelMode(LEVEL_OFF),
      lastLevelAdjust(0),
      tankLockout(false),
      hasStoredHeight(false) {
    for (int i = 0; i < NUM_BAGS; i++) {
        lastHeight[i] = 0.0;
    }
}

void AirRideWebServer::begin() {
    Serial.print("Starting WiFi AP...");

    // Configure ESP32 as Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL, 0, MAX_WIFI_CLIENTS);

    delay(100);

    wifiReady = true;

    // Load saved ride height from EEPROM
    loadRideHeight();

    // Setup routes
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/s", HTTP_GET, [this]() { handleStatus(); });
    server.on("/b", HTTP_GET, [this]() { handleBag(); });
    server.on("/bh", HTTP_GET, [this]() { handleBagHold(); });
    server.on("/p", HTTP_GET, [this]() { handlePreset(); });
    server.on("/l", HTTP_GET, [this]() { handleLevel(); });
    server.on("/sh", HTTP_GET, [this]() { handleSaveHeight(); });
    server.on("/rh", HTTP_GET, [this]() { handleRestoreHeight(); });
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
    server.send(200, "text/html", HTML_PAGE);
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
    json += "}";

    server.send(200, "application/json", json);
}

void AirRideWebServer::handleBag() {
    if (server.hasArg("n") && server.hasArg("d")) {
        int bagNum = server.arg("n").toInt();
        int dir = server.arg("d").toInt();

        if (bagNum >= 0 && bagNum < NUM_BAGS) {
            if (dir > 0) {
                // Check tank lockout before inflating
                if (!tankLockout) {
                    bags[bagNum].inflate();
                }
            } else {
                bags[bagNum].deflate();
            }
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
            // Set target to current pressure (stop auto-tracking)
            bags[bagNum].setTargetPressure(bags[bagNum].getPressure());
        }
    }
    handleStatus();
}

void AirRideWebServer::handlePreset() {
    if (server.hasArg("n")) {
        int presetNum = server.arg("n").toInt();

        if (presetNum >= 0 && presetNum < NUM_PRESETS) {
            bags[FRONT_LEFT].setTargetPressure(DEFAULT_PRESETS[presetNum].frontLeft);
            bags[FRONT_RIGHT].setTargetPressure(DEFAULT_PRESETS[presetNum].frontRight);
            bags[REAR_LEFT].setTargetPressure(DEFAULT_PRESETS[presetNum].rearLeft);
            bags[REAR_RIGHT].setTargetPressure(DEFAULT_PRESETS[presetNum].rearRight);

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

void AirRideWebServer::handleLevel() {
    if (server.hasArg("m")) {
        int mode = server.arg("m").toInt();
        if (mode >= 0 && mode <= 3) {
            levelMode = (LevelMode)mode;
        }
    }
    handleStatus();
}

void AirRideWebServer::handleSaveHeight() {
    // Save current pressures to EEPROM
    for (int i = 0; i < NUM_BAGS; i++) {
        lastHeight[i] = bags[i].getPressure();
    }
    saveRideHeight();
    handleStatus();
}

void AirRideWebServer::handleRestoreHeight() {
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

void AirRideWebServer::handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}

void AirRideWebServer::saveRideHeight() {
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.write(EEPROM_ADDR_VERSION, EEPROM_VERSION);
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

String AirRideWebServer::getHtmlPage() {
    return String(HTML_PAGE);
}
