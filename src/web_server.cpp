#include "web_server.h"
#include "config_store.h"
#include "crsf.h"
#include "sensors.h"
#include "motors.h"

RoverWebServer webServer;

// Embedded emergency fallback HTML if LittleFS files have not been uploaded with 'pio run -t uploadfs'
static const char FALLBACK_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Rover HUD - Filesystem Setup Required</title>
    <style>
        body { background: #080c14; color: #e2e8f0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; padding: 20px; text-align: center; }
        .box { background: #0f182e; border: 2px solid #00f0ff; border-radius: 12px; padding: 24px; max-width: 550px; margin: 30px auto; box-shadow: 0 0 30px rgba(0,240,255,0.2); }
        h1 { color: #00f0ff; font-size: 1.4rem; margin-bottom: 10px; }
        .warn { background: rgba(255,183,3,0.15); border: 1px solid #ffb703; color: #ffb703; padding: 12px; border-radius: 6px; margin: 15px 0; font-size: 0.9rem; text-align: left; }
        code { background: #162444; color: #00f5a0; padding: 3px 8px; border-radius: 4px; font-family: monospace; }
        .live { margin-top: 20px; padding: 14px; background: #090e1a; border-radius: 8px; text-align: left; font-family: monospace; font-size: 0.85rem; }
        .btn { background: #ff2a55; color: #fff; border: none; padding: 10px 20px; border-radius: 6px; font-weight: bold; cursor: pointer; margin-top: 15px; }
    </style>
</head>
<body>
    <div class="box">
        <h1>🛰️ MINING ROVER - CORE FIRMWARE ONLINE</h1>
        <p>The ESP32 firmware is running, but the <strong>LittleFS Web Dashboard assets</strong> have not been flashed to ESP32 flash memory yet.</p>
        
        <div class="warn">
            <strong>📋 To upload the complete Tactical Web HUD:</strong><br>
            In your VS Code terminal / PlatformIO prompt, run:<br><br>
            <code>pio run -t uploadfs</code><br><br>
            <em>Or click: PlatformIO Icon &rarr; esp32dev &rarr; Platform &rarr; Upload Filesystem Image</em>
        </div>

        <div class="live" id="live-data">
            <div>CRSF Link: <span id="crsf-stat">Checking...</span></div>
            <div>Distance: <span id="dist-stat">--</span> cm</div>
            <div>Attitude Tilt: <span id="tilt-stat">--</span>&deg;</div>
            <div>Flame: <span id="flame-stat">--</span> | Gas: <span id="gas-stat">--</span></div>
        </div>

        <button class="btn" onclick="fetch('/api/emergency_stop',{method:'POST'}).then(()=>alert('E-Stop Toggled!'))">🛑 EMERGENCY STOP</button>
    </div>

    <script>
        setInterval(() => {
            fetch('/api/sensors').then(r=>r.json()).then(d => {
                document.getElementById('dist-stat').textContent = d.distance_cm.toFixed(1);
                document.getElementById('tilt-stat').textContent = d.total_tilt_deg.toFixed(1);
                document.getElementById('flame-stat').textContent = d.flame_detected ? '🔥 DETECTED' : 'CLEAR';
                document.getElementById('gas-stat').textContent = d.gas_detected ? '☣️ HAZARD' : 'NOMINAL';
            }).catch(()=>{});
            fetch('/api/status').then(r=>r.json()).then(d => {
                document.getElementById('crsf-stat').textContent = d.link_connected ? '✅ ACTIVE (420K)' : '❌ FAILSAFE';
            }).catch(()=>{});
        }, 1000);
    </script>
</body>
</html>
)rawliteral";

RoverWebServer::RoverWebServer()
    : _server(80),
      _ws("/ws"),
      _lastBroadcastMs(0),
      _learnActive(false) {
    for (int i = 0; i < CRSF_NUM_CHANNELS; i++) {
        _learnBaseline[i] = CRSF_CHANNEL_MID;
    }
}

void RoverWebServer::begin() {
    // 1. Initialize LittleFS first
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] Error mounting LittleFS file system!");
    } else {
        Serial.println("[FS] LittleFS mounted successfully.");
        if (LittleFS.exists("/index.html")) {
            Serial.println("[FS] /index.html found in LittleFS.");
        } else {
            Serial.println("[FS] WARNING: /index.html NOT found in LittleFS! Run 'pio run -t uploadfs'.");
        }
    }

    // 2. Setup WiFi AP and optional STA
    setupWiFi();

    // 3. Setup WebSocket & HTTP Routes
    setupWebSocket();
    setupRoutes();

    _server.begin();
    Serial.println("[HTTP] Web server listening on port 80 (http://192.168.4.1).");
}

void RoverWebServer::setupWiFi() {
    const RoverConfig &cfg = configStore.get();

    WiFi.persistent(false);
    WiFi.disconnect(true);
    delay(50);

    // Static IP configuration for SoftAP
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);

    if (strlen(cfg.staSSID) > 0) {
        WiFi.mode(WIFI_AP_STA);
        Serial.printf("[WIFI] Connecting to STA: %s...\n", cfg.staSSID);
        WiFi.begin(cfg.staSSID, cfg.staPassword);
    } else {
        WiFi.mode(WIFI_AP);
    }

    WiFi.softAPConfig(local_IP, gateway, subnet);
    bool apOk = WiFi.softAP(cfg.apSSID, cfg.apPassword, DEFAULT_AP_CHANNEL);
    
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[WIFI] SoftAP '%s' started (%s). IP: %s\n", 
                  cfg.apSSID, apOk ? "OK" : "FAILED", apIP.toString().c_str());
}

void RoverWebServer::setupWebSocket() {
    _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onWsEvent(server, client, type, arg, data, len);
    });
    _server.addHandler(&_ws);
}

void RoverWebServer::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        // Handle incoming WebSocket messages if needed
    }
}

void RoverWebServer::setupRoutes() {
    // Default CORS & caching headers
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    // REST: Get Status
    _server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<512> doc;
        doc["uptime_s"] = millis() / 1000;
        doc["link_connected"] = crsf.isConnected();
        doc["failsafe"] = crsf.isFailsafe();
        doc["armed"] = motors.isArmed();
        doc["emergency_stop"] = motors.isEmergencyStopped();
        doc["hazard_active"] = sensors.isAnyHazardActive();
        doc["threat_score"] = sensors.getData().threatScore;
        doc["threat_level"] = sensors.getData().threatLevel;
        doc["ap_ip"] = WiFi.softAPIP().toString();
        doc["sta_ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Disconnected";

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // REST: Get Raw & Normalized Channels
    _server.on("/api/channels", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<1024> doc;
        doc["connected"] = crsf.isConnected();
        doc["failsafe"] = crsf.isFailsafe();
        doc["frame_age_ms"] = crsf.getFrameAgeMs();

        JsonArray rawArr = doc.createNestedArray("raw");
        JsonArray pctArr = doc.createNestedArray("percent");
        const uint16_t *channels = crsf.getAllChannels();
        for (int i = 0; i < CRSF_NUM_CHANNELS; i++) {
            rawArr.add(channels[i]);
            pctArr.add(crsf.getPercent(i));
        }

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // REST: Get Sensor Snapshot
    _server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<1024> doc;
        const SensorData &sd = sensors.getData();

        doc["temperature"] = sd.temperature;
        doc["humidity"] = sd.humidity;
        doc["dew_point"] = sd.dewPoint;
        doc["heat_index"] = sd.heatIndex;
        doc["fog_risk"] = sd.fogRisk;
        doc["dht_valid"] = sd.dhtValid;

        doc["flame_detected"] = sd.flameDetected;
        doc["gas_detected"] = sd.gasDetected;
        doc["distance_cm"] = sd.distanceCm;
        doc["obstacle_warning"] = sd.obstacleWarning;
        doc["proximity_zone"] = sd.proximityZone;

        doc["pitch_deg"] = sd.pitchDeg;
        doc["roll_deg"] = sd.rollDeg;
        doc["total_tilt_deg"] = sd.totalTiltDeg;
        doc["g_force"] = sd.gForce;
        doc["tilt_hazard"] = sd.tiltHazard;
        doc["mpu_available"] = sd.mpuAvailable;

        doc["threat_score"] = sd.threatScore;
        doc["threat_level"] = sd.threatLevel;

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // REST: Get Config
    _server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<768> doc;
        JsonObject root = doc.to<JsonObject>();
        configStore.serializeFull(root);

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // REST: Save Channel Mapping
    _server.on("/api/config/mapping", HTTP_POST, 
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<512> doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) {
                request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                return;
            }

            JsonObjectConst obj = doc.as<JsonObjectConst>();
            if (configStore.deserializeMapping(obj)) {
                request->send(200, "application/json", "{\"success\":true,\"message\":\"Mapping saved to flash\"}");
            } else {
                request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save mapping\"}");
            }
        }
    );

    // REST: Save WiFi Settings
    _server.on("/api/config/wifi", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<512> doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) {
                request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                return;
            }

            JsonObjectConst obj = doc.as<JsonObjectConst>();
            if (configStore.deserializeWiFi(obj)) {
                request->send(200, "application/json", "{\"success\":true,\"message\":\"WiFi settings saved\"}");
            } else {
                request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save WiFi config\"}");
            }
        }
    );

    // REST: Learn baseline capture
    _server.on("/api/learn/baseline", HTTP_POST, [this](AsyncWebServerRequest *request) {
        const uint16_t *current = crsf.getAllChannels();
        for (int i = 0; i < CRSF_NUM_CHANNELS; i++) {
            _learnBaseline[i] = current[i];
        }
        _learnActive = true;
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Baseline captured\"}");
    });

    // REST: Learn detect
    _server.on("/api/learn/detect", HTTP_POST, [this](AsyncWebServerRequest *request) {
        const uint16_t *current = crsf.getAllChannels();
        int maxChan = -1;
        int maxDelta = 0;

        for (int i = 0; i < CRSF_NUM_CHANNELS; i++) {
            int delta = abs((int)current[i] - (int)_learnBaseline[i]);
            if (delta > maxDelta) {
                maxDelta = delta;
                maxChan = i;
            }
        }

        StaticJsonDocument<256> doc;
        if (maxDelta >= 150 && maxChan >= 0) {
            doc["detected"] = true;
            doc["channel"] = maxChan;
            doc["delta"] = maxDelta;
            doc["current_value"] = current[maxChan];
        } else {
            doc["detected"] = false;
            doc["max_delta"] = maxDelta;
        }

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // REST: Emergency Stop toggle
    _server.on("/api/emergency_stop", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool current = motors.isEmergencyStopped();
        motors.setEmergencyStop(!current);

        StaticJsonDocument<128> doc;
        doc["emergency_stop"] = motors.isEmergencyStopped();
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // REST: Test Motor Hardware (Left, Right, or Both)
    _server.on("/api/test_motor", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<256> doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) {
                request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                return;
            }

            const char *motor = doc["motor"] | "both";
            int speed = doc["speed"] | 200;
            uint32_t dur = doc["duration_ms"] | 2000;

            int leftSpeed = 0;
            int rightSpeed = 0;

            if (strcmp(motor, "left") == 0) {
                leftSpeed = speed;
                rightSpeed = 0;
            } else if (strcmp(motor, "right") == 0) {
                leftSpeed = 0;
                rightSpeed = speed;
            } else {
                leftSpeed = speed;
                rightSpeed = speed;
            }

            motors.testMotor(leftSpeed, rightSpeed, dur);
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Motor test started\"}");
        }
    );

    // Explicit Root Handlers with Fallback
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            request->send(200, "text/html", FALLBACK_HTML);
        }
    });

    _server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            request->send(200, "text/html", FALLBACK_HTML);
        }
    });

    _server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/style.css")) {
            request->send(LittleFS, "/style.css", "text/css");
        } else {
            request->send(404, "text/plain", "CSS not found. Please upload filesystem.");
        }
    });

    _server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/app.js")) {
            request->send(LittleFS, "/app.js", "application/javascript");
        } else {
            request->send(404, "text/plain", "JS not found. Please upload filesystem.");
        }
    });

    // Captive Portal Detection Redirection (Android / Apple / Windows)
    _server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    _server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    _server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    _server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });

    // Serve all static files from LittleFS root
    _server.serveStatic("/", LittleFS, "/");

    // 404 Fallback Handler
    _server.onNotFound([](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            request->send(200, "text/html", FALLBACK_HTML);
        }
    });
}

void RoverWebServer::broadcastTelemetry() {
    if (_ws.count() == 0) return;

    StaticJsonDocument<1792> doc;
    doc["type"] = "telemetry";

    // Link Status
    JsonObject link = doc.createNestedObject("link");
    link["connected"] = crsf.isConnected();
    link["failsafe"] = crsf.isFailsafe();
    link["age_ms"] = crsf.getFrameAgeMs();
    link["valid_frames"] = crsf.getValidFrameCount();
    link["crc_errors"] = crsf.getCrcErrorCount();

    // Sensor Readings & Fusion
    const SensorData &sd = sensors.getData();
    JsonObject sens = doc.createNestedObject("sensors");
    sens["temp"] = sd.temperature;
    sens["humidity"] = sd.humidity;
    sens["dew_point"] = sd.dewPoint;
    sens["heat_index"] = sd.heatIndex;
    sens["fog_risk"] = sd.fogRisk;
    sens["dht_ok"] = sd.dhtValid;

    sens["flame"] = sd.flameDetected;
    sens["gas"] = sd.gasDetected;
    sens["distance"] = sd.distanceCm;
    sens["obstacle"] = sd.obstacleWarning;
    sens["prox_zone"] = sd.proximityZone;

    sens["pitch"] = sd.pitchDeg;
    sens["roll"] = sd.rollDeg;
    sens["tilt"] = sd.totalTiltDeg;
    sens["g_force"] = sd.gForce;
    sens["tilt_hazard"] = sd.tiltHazard;
    sens["mpu_ok"] = sd.mpuAvailable;

    sens["threat_score"] = sd.threatScore;
    sens["threat_level"] = sd.threatLevel;

    // Rover & Actuator State
    JsonObject rov = doc.createNestedObject("rover");
    rov["armed"] = motors.isArmed();
    rov["e_stop"] = motors.isEmergencyStopped();
    rov["left_pwm"] = motors.getLeftPWM();
    rov["right_pwm"] = motors.getRightPWM();
    rov["mast_angle"] = motors.getMastAngle();

    // Raw Channels array (all 16)
    JsonArray chs = doc.createNestedArray("channels");
    const uint16_t *channels = crsf.getAllChannels();
    for (int i = 0; i < CRSF_NUM_CHANNELS; i++) {
        chs.add(channels[i]);
    }

    String json;
    serializeJson(doc, json);
    _ws.textAll(json);
}

void RoverWebServer::update() {
    uint32_t now = millis();

    // Clean dead WebSocket clients
    _ws.cleanupClients();

    // Broadcast live telemetry packet at ~2Hz (every 500ms)
    if (now - _lastBroadcastMs >= TELEMETRY_INTERVAL_MS) {
        _lastBroadcastMs = now;
        broadcastTelemetry();
    }
}
