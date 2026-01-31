#include "WebServer.h"

// Minimal HTML page stored in PROGMEM to save RAM
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
.presets button{flex:1;padding:15px;font-size:16px;border:none;border-radius:8px;cursor:pointer}
.presets button:nth-child(1){background:#2196F3}
.presets button:nth-child(2){background:#4CAF50}
.presets button:nth-child(3){background:#ff9800}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.bag{background:#333;padding:15px;border-radius:8px;text-align:center}
.bag h3{font-size:14px;margin-bottom:5px}
.bag .psi{font-size:28px;font-weight:bold;margin:10px 0}
.bag .btns{display:flex;gap:5px;justify-content:center}
.bag button{width:50px;height:50px;font-size:24px;border:none;border-radius:8px;cursor:pointer}
.bag .up{background:#4CAF50}
.bag .dn{background:#f44336}
.pump{margin-top:15px;padding:10px;background:#333;border-radius:8px;text-align:center}
.pump span{font-size:12px;color:#888}
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
<script>
function bg(b,d){fetch('/b?n='+b+'&d='+d).then(upd)}
function pr(p){fetch('/p?n='+p).then(upd)}
function upd(){fetch('/s').then(r=>r.json()).then(d=>{
document.getElementById('tk').textContent=d.tank.toFixed(0);
for(var i=0;i<4;i++)document.getElementById('b'+i).textContent=d.bags[i].toFixed(0);
document.getElementById('pm').textContent=d.pump;
})}
setInterval(upd,1000);upd();
</script>
</body>
</html>
)rawliteral";

WebServer::WebServer(AirBag* b, Compressor* c, float* tp)
    : bags(b),
      compressor(c),
      tankPressure(tp),
      server(80),
      wifiReady(false) {
}

void WebServer::begin() {
    Serial.print(F("Starting WiFi AP..."));

    // Start Access Point
    WiFi.beginAP(WIFI_SSID, WIFI_PASS);

    // Wait for AP to start
    delay(1000);

    if (WiFi.status() == WL_AP_LISTENING) {
        wifiReady = true;
        server.begin();

        Serial.println(F(" OK"));
        Serial.print(F("SSID: "));
        Serial.println(WIFI_SSID);
        Serial.print(F("IP: "));
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(F(" FAILED"));
    }
}

void WebServer::update() {
    if (!wifiReady) return;

    WiFiClient client = server.available();
    if (client) {
        handleClient(client);
    }
}

void WebServer::handleClient(WiFiClient& client) {
    String request = "";
    unsigned long timeout = millis() + 1000;

    while (client.connected() && millis() < timeout) {
        if (client.available()) {
            char c = client.read();
            request += c;

            // End of HTTP headers
            if (request.endsWith("\r\n\r\n")) {
                break;
            }
        }
    }

    if (request.length() > 0) {
        parseRequest(request);

        // Determine which page to serve
        if (request.indexOf("GET /s") >= 0) {
            // JSON status endpoint
            sendJsonStatus(client);
        } else if (request.indexOf("GET /b?") >= 0) {
            // Bag control: /b?n=0&d=1 (bag 0, direction +1 or -1)
            int nPos = request.indexOf("n=");
            int dPos = request.indexOf("d=");
            if (nPos >= 0 && dPos >= 0) {
                int bagNum = request.charAt(nPos + 2) - '0';
                int dir = request.charAt(dPos + 2) == '1' ? 1 : -1;

                if (bagNum >= 0 && bagNum < NUM_BAGS) {
                    if (dir > 0) {
                        bags[bagNum].inflate();
                    } else {
                        bags[bagNum].deflate();
                    }
                }
            }
            sendJsonStatus(client);
        } else if (request.indexOf("GET /p?") >= 0) {
            // Preset: /p?n=0
            int nPos = request.indexOf("n=");
            if (nPos >= 0) {
                int presetNum = request.charAt(nPos + 2) - '0';
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
            sendJsonStatus(client);
        } else {
            // Serve main page
            sendHtmlPage(client);
        }
    }

    delay(1);
    client.stop();
}

void WebServer::sendHtmlPage(WiFiClient& client) {
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: text/html"));
    client.println(F("Connection: close"));
    client.println();

    // Send HTML from PROGMEM
    int len = strlen_P(HTML_PAGE);
    for (int i = 0; i < len; i++) {
        client.write(pgm_read_byte(HTML_PAGE + i));
    }
}

void WebServer::sendJsonStatus(WiFiClient& client) {
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println();

    // Build JSON response
    client.print(F("{\"tank\":"));
    client.print(*tankPressure, 1);
    client.print(F(",\"bags\":["));
    for (int i = 0; i < NUM_BAGS; i++) {
        if (i > 0) client.print(',');
        client.print(bags[i].getPressure(), 1);
    }
    client.print(F("],\"pump\":\""));
    client.print(compressor->getModeString());
    client.print(F(" P1:"));
    client.print(compressor->isPump1Running() ? F("ON") : F("off"));
    client.print(F(" P2:"));
    client.print(compressor->isPump2Running() ? F("ON") : F("off"));
    client.print(F("\"}"));
}

void WebServer::parseRequest(String& request) {
    // Just extract the first line for logging
    int endLine = request.indexOf('\r');
    if (endLine > 0) {
        String firstLine = request.substring(0, endLine);
        Serial.print(F("HTTP: "));
        Serial.println(firstLine);
    }
}
