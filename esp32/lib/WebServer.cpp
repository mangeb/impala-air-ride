#include "WebServer.h"

// HTML page stored in PROGMEM
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Impala Air Ride</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui;background:#1a1a1a;color:#fff;padding:10px}
h1{font-size:18px;text-align:center;margin-bottom:15px;color:#ffd700}
.tank{text-align:center;padding:10px;background:#333;border-radius:8px;margin-bottom:15px}
.tank span{font-size:24px;font-weight:bold}
.presets{display:flex;gap:8px;margin-bottom:15px}
.presets button{flex:1;padding:15px;font-size:16px;border:none;border-radius:8px;cursor:pointer;color:#fff}
.presets button:nth-child(1){background:#2196F3}
.presets button:nth-child(2){background:#4CAF50}
.presets button:nth-child(3){background:#ff9800}
.presets button:active{opacity:0.7}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.bag{background:#333;padding:15px;border-radius:8px;text-align:center}
.bag h3{font-size:14px;margin-bottom:5px}
.bag .psi{font-size:28px;font-weight:bold;margin:10px 0}
.bag .btns{display:flex;gap:5px;justify-content:center}
.bag button{width:50px;height:50px;font-size:24px;border:none;border-radius:8px;cursor:pointer;color:#fff}
.bag button:active{opacity:0.7}
.bag .up{background:#4CAF50}
.bag .dn{background:#f44336}
.pump{margin-top:15px;padding:10px;background:#333;border-radius:8px;text-align:center}
.pump span{font-size:12px;color:#888}
.status{font-size:10px;color:#666;text-align:center;margin-top:10px}
</style>
</head>
<body>
<h1>🚗 1964 IMPALA AIR RIDE</h1>
<div class="tank">Tank: <span id="tk">--</span> PSI</div>
<div class="presets">
<button onclick="pr(0)">LAY</button>
<button onclick="pr(1)">CRUISE</button>
<button onclick="pr(2)">MAX</button>
</div>
<div class="grid">
<div class="bag"><h3>FRONT LEFT</h3><div class="psi" id="b0">--</div><div class="btns"><button class="up" onclick="bg(0,1)">+</button><button class="dn" onclick="bg(0,-1)">−</button></div></div>
<div class="bag"><h3>FRONT RIGHT</h3><div class="psi" id="b1">--</div><div class="btns"><button class="up" onclick="bg(1,1)">+</button><button class="dn" onclick="bg(1,-1)">−</button></div></div>
<div class="bag"><h3>REAR LEFT</h3><div class="psi" id="b2">--</div><div class="btns"><button class="up" onclick="bg(2,1)">+</button><button class="dn" onclick="bg(2,-1)">−</button></div></div>
<div class="bag"><h3>REAR RIGHT</h3><div class="psi" id="b3">--</div><div class="btns"><button class="up" onclick="bg(3,1)">+</button><button class="dn" onclick="bg(3,-1)">−</button></div></div>
</div>
<div class="pump">Pumps: <span id="pm">--</span></div>
<div class="status">ESP32 Air Ride Controller</div>
<script>
function bg(b,d){fetch('/b?n='+b+'&d='+d).then(upd)}
function pr(p){fetch('/p?n='+p).then(upd)}
function upd(){fetch('/s').then(r=>r.json()).then(d=>{
document.getElementById('tk').textContent=d.tank.toFixed(0);
for(var i=0;i<4;i++)document.getElementById('b'+i).textContent=d.bags[i].toFixed(0);
document.getElementById('pm').textContent=d.pump;
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
      wifiReady(false) {
}

void AirRideWebServer::begin() {
    Serial.print("Starting WiFi AP...");

    // Configure ESP32 as Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL, 0, MAX_WIFI_CLIENTS);

    delay(100);

    wifiReady = true;

    // Setup routes
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/s", HTTP_GET, [this]() { handleStatus(); });
    server.on("/b", HTTP_GET, [this]() { handleBag(); });
    server.on("/p", HTTP_GET, [this]() { handlePreset(); });
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
    json += "],\"pump\":\"";
    json += compressor->getModeString();
    json += " P1:";
    json += compressor->isPump1Running() ? "ON" : "off";
    json += " P2:";
    json += compressor->isPump2Running() ? "ON" : "off";
    json += "\"}";

    server.send(200, "application/json", json);
}

void AirRideWebServer::handleBag() {
    if (server.hasArg("n") && server.hasArg("d")) {
        int bagNum = server.arg("n").toInt();
        int dir = server.arg("d").toInt();

        if (bagNum >= 0 && bagNum < NUM_BAGS) {
            if (dir > 0) {
                bags[bagNum].inflate();
            } else {
                bags[bagNum].deflate();
            }
        }
    }
    handleStatus();
}

void AirRideWebServer::handlePreset() {
    if (server.hasArg("n")) {
        int presetNum = server.arg("n").toInt();

        if (presetNum >= 0 && presetNum < NUM_PRESETS) {
            bags[FRONT_LEFT].setTargetPressure(PRESETS[presetNum].frontLeft);
            bags[FRONT_RIGHT].setTargetPressure(PRESETS[presetNum].frontRight);
            bags[REAR_LEFT].setTargetPressure(PRESETS[presetNum].rearLeft);
            bags[REAR_RIGHT].setTargetPressure(PRESETS[presetNum].rearRight);

            // Start moving to targets
            for (int i = 0; i < NUM_BAGS; i++) {
                float current = bags[i].getPressure();
                float target = bags[i].getTargetPressure();
                if (current < target - 2.0) {
                    bags[i].inflate();
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

void AirRideWebServer::handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}

String AirRideWebServer::getHtmlPage() {
    return String(HTML_PAGE);
}
