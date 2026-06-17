#include "NetworkManager.h"
#include <WiFiClientSecure.h>

NetworkManager::NetworkManager(PreferencesManager& prefs, SensorManager& sensors, ActuatorManager& actuators)
  : _prefs(prefs), _sensors(sensors), _actuators(actuators), _server(80), _apMode(false), 
    _lastWifiCheckTime(0), _lastTelegramTime(0) {}

void NetworkManager::begin() {
  SystemSettings settings = _prefs.getSettings();

  if (strlen(settings.wifiSSID[0]) == 0) {
    DEBUG_PRINTLN("No WiFi credentials set. Starting Access Point mode...");
    setupAP();
  } else {
    setupClientWiFi();
  }

  // Define HTTP local server routes
  _server.on("/", std::bind(&NetworkManager::handleRoot, this));
  _server.on("/config", std::bind(&NetworkManager::handleConfig, this));
  _server.on("/save", HTTP_POST, std::bind(&NetworkManager::handleSaveSettings, this));
  _server.on("/toggle", std::bind(&NetworkManager::handleToggleActuator, this));
  
  _server.begin();
  DEBUG_PRINTLN("Local Web Server running.");
}

void NetworkManager::setupAP() {
  _apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(DEFAULT_AP_SSID, DEFAULT_AP_PASS);
  DEBUG_PRINTF("Access Point SSID: %s started. IP Address: %s\n", DEFAULT_AP_SSID, WiFi.softAPIP().toString().c_str());
}

void NetworkManager::setupClientWiFi() {
  _apMode = false;
  SystemSettings settings = _prefs.getSettings();
  
  WiFi.mode(WIFI_STA);

  // Register all configured networks from NVS preferences
  bool hasCredentials = false;
  for (int i = 0; i < 3; i++) {
    if (strlen(settings.wifiSSID[i]) > 0) {
      _wifiMulti.addAP(settings.wifiSSID[i], settings.wifiPass[i]);
      DEBUG_PRINTF("WiFi Profile %d (NVS) added: %s\n", i + 1, settings.wifiSSID[i]);
      hasCredentials = true;
    }
  }

  // Register hardcoded fallback networks from Config.h
  for (int i = 0; i < WIFI_NETWORK_COUNT; i++) {
    _wifiMulti.addAP(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].password);
    DEBUG_PRINTF("WiFi Profile %d (Config.h) added: %s\n", i + 1, WIFI_NETWORKS[i].ssid);
    hasCredentials = true;
  }

  if (!hasCredentials) {
    DEBUG_PRINTLN("No WiFi credentials configured. Reverting to Access Point Mode...");
    setupAP();
    return;
  }

  DEBUG_PRINT("Scanning and connecting to configured WiFi networks");
  
  int attempts = 0;
  while (_wifiMulti.run() != WL_CONNECTED && attempts < 30) {
    delay(500);
    DEBUG_PRINT(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINTLN("\nWiFi Connected successfully.");
    DEBUG_PRINTF("Connected to: %s\n", WiFi.SSID().c_str());
    DEBUG_PRINTF("Local IP Address: %s\n", WiFi.localIP().toString().c_str());
    sendTelegramAlert("Smart Poultry Farm System Online. Power restored / System rebooted. Connected to WiFi: " + WiFi.SSID());
  } else {
    DEBUG_PRINTLN("\nFailed to connect to any configured WiFi. Reverting to Access Point Mode...");
    setupAP();
  }
}

void NetworkManager::update() {
  _server.handleClient();
  
  // Periodically check client WiFi health and auto-reconnect if dropped
  if (!_apMode) {
    unsigned long now = millis();
    if (now - _lastWifiCheckTime >= 10000) { // check every 10s
      _lastWifiCheckTime = now;
      checkWifiConnection();
    }
  }
}

void NetworkManager::checkWifiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("WiFi connection lost. Reconnecting via WiFiMulti...");
    _wifiMulti.run();
  }
}

bool NetworkManager::isConnected() const {
  return !_apMode && (WiFi.status() == WL_CONNECTED);
}

String NetworkManager::getWifiStatusString() const {
  if (_apMode) return "AP MODE";
  switch (WiFi.status()) {
    case WL_CONNECTED: return "CONNECTED";
    case WL_DISCONNECTED: return "DISCONNECTED";
    case WL_CONNECTION_LOST: return "CONN LOST";
    case WL_IDLE_STATUS: return "IDLE";
    default: return "SEARCHING";
  }
}

String NetworkManager::getLocalIP() const {
  if (_apMode) {
    return WiFi.softAPIP().toString();
  }
  return WiFi.localIP().toString();
}

bool NetworkManager::syncWithCloud(const SensorData& sData, const ActuatorState& aState) {
  if (!isConnected()) return false;

  WiFiClient client;
  HTTPClient http;
  
  SystemSettings settings = _prefs.getSettings();
  String url = String(DEFAULT_CLOUD_URL) + "/api/telemetry";

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  // Create JSON telemetry payload
  StaticJsonDocument<512> doc;
  doc["houseId"] = DEVICE_KEY;
  doc["temperature"] = sData.dhtFault ? -99.0 : sData.temperature;
  doc["humidity"] = sData.dhtFault ? -99.0 : sData.humidity;
  doc["gasLevel"] = sData.gasLevel;
  doc["waterLevel"] = sData.waterLevel;
  doc["feedLevelLow"] = sData.feedLevelLow;
  doc["fanState"] = aState.fan;
  doc["heaterState"] = aState.heater;
  doc["pumpState"] = aState.pump;
  doc["feederActive"] = aState.feederActive;
  
  // Diagnostics
  doc["dhtFault"] = sData.dhtFault;
  doc["gasFault"] = sData.gasFault;
  doc["waterFault"] = sData.waterFault;
  doc["pumpSafetyTripped"] = aState.pumpSafetyTripped;

  String jsonString;
  serializeJson(doc, jsonString);

  int httpCode = http.POST(jsonString);
  bool success = false;

  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    processCloudResponse(response);
    success = true;
  } else {
    DEBUG_PRINTF("[Cloud Sync] Failed, HTTP error code: %d - URL: %s\n", httpCode, url.c_str());
  }

  http.end();
  return success;
}

void NetworkManager::processCloudResponse(const String& jsonResponse) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonResponse);

  if (error) {
    DEBUG_PRINTF("Failed to parse cloud sync JSON: %s\n", error.c_str());
    return;
  }

  SystemSettings settings = _prefs.getSettings();
  bool settingsChanged = false;

  // 1. Synchronize Threshold Rules
  if (doc.containsKey("tempHighLimit") && doc["tempHighLimit"] != settings.tempHighLimit) {
    settings.tempHighLimit = doc["tempHighLimit"];
    settingsChanged = true;
  }
  if (doc.containsKey("tempLowLimit") && doc["tempLowLimit"] != settings.tempLowLimit) {
    settings.tempLowLimit = doc["tempLowLimit"];
    settingsChanged = true;
  }
  if (doc.containsKey("gasHighLimit") && doc["gasHighLimit"] != settings.gasHighLimit) {
    settings.gasHighLimit = doc["gasHighLimit"];
    settingsChanged = true;
  }
  if (doc.containsKey("waterLowLimit") && doc["waterLowLimit"] != settings.waterLowLimit) {
    settings.waterLowLimit = doc["waterLowLimit"];
    settingsChanged = true;
  }
  if (doc.containsKey("autoMode") && doc["autoMode"] != settings.autoMode) {
    settings.autoMode = doc["autoMode"];
    settingsChanged = true;
  }

  if (settingsChanged) {
    _prefs.saveSettings(settings);
    DEBUG_PRINTLN("System settings updated from Cloud sync.");
  }

  // 2. Execute Manual Override Commands (if not in AUTO mode)
  if (!settings.autoMode) {
    if (doc.containsKey("fanCommand")) {
      String fanCmd = doc["fanCommand"];
      if (fanCmd == "ON") _actuators.setFan(true);
      else if (fanCmd == "OFF") _actuators.setFan(false);
    }
    if (doc.containsKey("heaterCommand")) {
      String heatCmd = doc["heaterCommand"];
      if (heatCmd == "ON") _actuators.setHeater(true);
      else if (heatCmd == "OFF") _actuators.setHeater(false);
    }
    if (doc.containsKey("pumpCommand")) {
      String pumpCmd = doc["pumpCommand"];
      if (pumpCmd == "ON") _actuators.setPump(true);
      else if (pumpCmd == "OFF") _actuators.setPump(false);
    }
  }

  // Feeder can be manually triggered regardless of mode
  if (doc.containsKey("dispenseFeed") && doc["dispenseFeed"] == true) {
    int duration = doc.containsKey("feedDuration") ? doc["feedDuration"].as<int>() : FEED_SERVO_PORTION_MS;
    _actuators.triggerFeeder(duration);
  }
  
  if (doc.containsKey("resetPumpSafety") && doc["resetPumpSafety"] == true) {
    _actuators.resetPumpSafety();
  }
}

// Direct Telegram alert sender using a secure/non-secure HTTP client
void NetworkManager::sendTelegramAlert(const String& message) {
  unsigned long now = millis();
  // Throttle Telegram alerts to maximum once per 10 seconds to prevent API limits
  if (now - _lastTelegramTime < 10000) return;
  _lastTelegramTime = now;

  SystemSettings settings = _prefs.getSettings();
  if (strlen(settings.telegramToken) == 0 || strcmp(settings.telegramToken, DEFAULT_TELEGRAM_BOT_TOKEN) == 0) {
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // Telegram API runs on HTTPS, bypass certificate verification for simplicity
  HTTPClient http;

  // URL encode message parameter
  String encodedMsg = "";
  for (int i = 0; i < message.length(); i++) {
    char c = message.charAt(i);
    if (isalnum(c)) {
      encodedMsg += c;
    } else {
      char hex[4];
      sprintf(hex, "%%%02X", c);
      encodedMsg += hex;
    }
  }

  String url = "https://api.telegram.org/bot" + String(settings.telegramToken) + "/sendMessage?chat_id=" + String(settings.telegramChatId) + "&text=" + encodedMsg;
  
  http.begin(client, url);
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    DEBUG_PRINTLN("Telegram notification dispatched successfully.");
  } else {
    DEBUG_PRINTF("Telegram dispatch failed. HTTP Code: %d\n", code);
  }
  http.end();
}

// Local Dashboard Web UI
void NetworkManager::handleRoot() {
  SensorData s = _sensors.getData();
  ActuatorState a = _actuators.getState();
  SystemSettings set = _prefs.getSettings();

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Poultry Farm Local Console</title>
  <style>
    :root {
      --bg-color: #0b0f19;
      --card-bg: #111827;
      --border-color: #1f2937;
      --accent: #3b82f6;
      --accent-hover: #2563eb;
      --text: #f3f4f6;
      --text-muted: #9ca3af;
      --green: #10b981;
      --red: #ef4444;
    }
    body {
      background-color: var(--bg-color);
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      margin: 0;
      padding: 1rem;
      display: flex;
      flex-direction: column;
      align-items: center;
    }
    .container {
      width: 100%;
      max-width: 600px;
    }
    header {
      text-align: center;
      margin-bottom: 1.5rem;
    }
    h1 {
      margin: 0;
      font-size: 1.8rem;
      background: linear-gradient(to right, #60a5fa, #3b82f6);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    p.subtitle {
      color: var(--text-muted);
      margin: 0.2rem 0 0;
      font-size: 0.9rem;
    }
    .card {
      background-color: var(--card-bg);
      border: 1px solid var(--border-color);
      border-radius: 12px;
      padding: 1.2rem;
      margin-bottom: 1rem;
      box-shadow: 0 4px 6px rgba(0,0,0,0.3);
    }
    h2 {
      margin-top: 0;
      font-size: 1.2rem;
      border-bottom: 1px solid var(--border-color);
      padding-bottom: 0.5rem;
      color: #60a5fa;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 1rem;
    }
    .metric {
      background: rgba(255,255,255,0.02);
      border: 1px solid var(--border-color);
      padding: 0.8rem;
      border-radius: 8px;
      text-align: center;
    }
    .metric-val {
      font-size: 1.5rem;
      font-weight: bold;
      margin-top: 0.2rem;
    }
    .btn {
      display: inline-block;
      background-color: var(--accent);
      color: white;
      text-decoration: none;
      padding: 0.6rem 1.2rem;
      border-radius: 6px;
      font-weight: 500;
      text-align: center;
      transition: background-color 0.2s;
      border: none;
      cursor: pointer;
    }
    .btn:hover { background-color: var(--accent-hover); }
    .btn-red { background-color: var(--red); }
    .btn-red:hover { background-color: #dc2626; }
    .btn-green { background-color: var(--green); }
    .btn-green:hover { background-color: #059669; }
    .actuator-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 0.5rem 0;
      border-bottom: 1px dashed rgba(255,255,255,0.05);
    }
    .actuator-row:last-child { border-bottom: none; }
    .status {
      padding: 0.2rem 0.5rem;
      border-radius: 4px;
      font-size: 0.8rem;
      font-weight: bold;
    }
    .status-on { background-color: rgba(16,185,129,0.2); color: var(--green); }
    .status-off { background-color: rgba(239,68,68,0.2); color: var(--red); }
    .footer-links {
      display: flex;
      justify-content: space-between;
      margin-top: 1rem;
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>Smart Poultry Edge</h1>
      <p class="subtitle">Local Monitoring & Automation Console</p>
    </header>

    <!-- Readings Card -->
    <div class="card">
      <h2>Environmental Metrics</h2>
      <div class="grid">
        <div class="metric">
          <div style="color: var(--text-muted);">Temperature</div>
          <div class="metric-val">)rawliteral" + (s.dhtFault ? "ERR" : String(s.temperature, 1) + "&deg;C") + R"rawliteral(</div>
        </div>
        <div class="metric">
          <div style="color: var(--text-muted);">Humidity</div>
          <div class="metric-val">)rawliteral" + (s.dhtFault ? "ERR" : String(s.humidity, 1) + "%") + R"rawliteral(</div>
        </div>
        <div class="metric">
          <div style="color: var(--text-muted);">Air Quality (Gas)</div>
          <div class="metric-val">)rawliteral" + (s.gasFault ? "ERR" : String((int)s.gasLevel) + " PPM") + R"rawliteral(</div>
        </div>
        <div class="metric">
          <div style="color: var(--text-muted);">Water Level</div>
          <div class="metric-val">)rawliteral" + (s.waterFault ? "ERR" : String((int)s.waterLevel) + "%") + R"rawliteral(</div>
        </div>
      </div>
      <div class="actuator-row" style="margin-top: 1rem;">
        <span>Feed Level Status:</span>
        <span class="status )rawliteral" + String(s.feedLevelLow ? "status-off" : "status-on") + R"rawliteral(">)rawliteral" + String(s.feedLevelLow ? "LOW (REFILL NEEDED)" : "NORMAL") + R"rawliteral(</span>
      </div>
    </div>

    <!-- Actuators Card -->
    <div class="card">
      <h2>Actuator Overrides (Manual Mode only)</h2>
      <div class="actuator-row">
        <span>Cooling Fan</span>
        <div>
          <span class="status )rawliteral" + String(a.fan ? "status-on" : "status-off") + R"rawliteral(" style="margin-right: 0.5rem;">)rawliteral" + String(a.fan ? "RUNNING" : "OFF") + R"rawliteral(</span>
          <a class="btn" href="/toggle?target=fan">Toggle</a>
        </div>
      </div>
      <div class="actuator-row">
        <span>Heating Lamp</span>
        <div>
          <span class="status )rawliteral" + String(a.heater ? "status-on" : "status-off") + R"rawliteral(" style="margin-right: 0.5rem;">)rawliteral" + String(a.heater ? "ON" : "OFF") + R"rawliteral(</span>
          <a class="btn" href="/toggle?target=heater">Toggle</a>
        </div>
      </div>
      <div class="actuator-row">
        <span>Water Pump</span>
        <div>
          <span class="status )rawliteral" + String(a.pump ? "status-on" : "status-off") + R"rawliteral(" style="margin-right: 0.5rem;">)rawliteral" + String(a.pump ? "REFILLING" : "OFF") + R"rawliteral(</span>
          <a class="btn" href="/toggle?target=pump">Toggle</a>
        </div>
      </div>
      <div class="actuator-row">
        <span>Feed Dispenser (Servo)</span>
        <div>
          <a class="btn btn-green" href="/toggle?target=feeder">Dispense Portions</a>
        </div>
      </div>
      <div class="actuator-row" style="margin-top:0.5rem;">
        <span>Automation Mode:</span>
        <a class="btn" href="/toggle?target=automode" style="background-color: #8b5cf6;">)rawliteral" + String(set.autoMode ? "SWITCH TO MANUAL" : "SWITCH TO AUTO") + R"rawliteral(</a>
      </div>
    </div>

    <div class="footer-links">
      <a class="btn" href="/config" style="width: 48%; background-color: #374151;">Network & Settings</a>
      <a class="btn btn-green" href="/" style="width: 48%;">Refresh Data</a>
    </div>
  </div>
</body>
</html>
  )rawliteral";
  _server.send(200, "text/html", html);
}

// Config page local view
void NetworkManager::handleConfig() {
  SystemSettings settings = _prefs.getSettings();
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Configure Smart Poultry Edge</title>
  <style>
    body { background-color: #0b0f19; color: #f3f4f6; font-family: sans-serif; padding: 1rem; display: flex; flex-direction: column; align-items: center; }
    .container { width: 100%; max-width: 500px; }
    .card { background-color: #111827; border: 1px solid #1f2937; border-radius: 12px; padding: 1.5rem; }
    h2 { color: #60a5fa; border-bottom: 1px solid #1f2937; padding-bottom: 0.5rem; margin-top: 0; }
    .form-group { margin-bottom: 1rem; }
    label { display: block; font-size: 0.9rem; color: #9ca3af; margin-bottom: 0.3rem; }
    input[type="text"], input[type="password"] {
      width: 100%; box-sizing: border-box; background: #1f2937; border: 1px solid #374151;
      padding: 0.6rem; border-radius: 6px; color: white; font-size: 1rem;
    }
    .btn-container { display: flex; justify-content: space-between; margin-top: 1.5rem; }
    .btn { text-decoration: none; padding: 0.6rem 1.2rem; border-radius: 6px; font-weight: bold; cursor: pointer; text-align: center; border: none; }
    .btn-blue { background-color: #3b82f6; color: white; }
    .btn-gray { background-color: #4b5563; color: white; }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <h2>System Configurations</h2>
      <form action="/save" method="POST">
        <h3 style="color: #60a5fa; font-size: 1rem; margin-top: 1rem; border-bottom: 1px dashed #1f2937; padding-bottom: 0.3rem;">WiFi Networks (WiFiMulti)</h3>
        
        <div class="wifi-group" style="border-left: 2px solid #3b82f6; padding-left: 0.8rem; margin-bottom: 1rem;">
          <label style="color: #60a5fa; font-weight: bold; font-size: 0.85rem;">WiFi Network 1 (Primary)</label>
          <div class="form-group" style="margin-top: 0.3rem;">
            <label>WiFi SSID 1</label>
            <input type="text" name="ssid0" value=")rawliteral" + String(settings.wifiSSID[0]) + R"rawliteral(" placeholder="Local Network Name">
          </div>
          <div class="form-group">
            <label>WiFi Password 1</label>
            <input type="password" name="pass0" value=")rawliteral" + String(settings.wifiPass[0]) + R"rawliteral(" placeholder="Network Password">
          </div>
        </div>

        <div class="wifi-group" style="border-left: 2px solid #10b981; padding-left: 0.8rem; margin-bottom: 1rem;">
          <label style="color: #34d399; font-weight: bold; font-size: 0.85rem;">WiFi Network 2 (Backup)</label>
          <div class="form-group" style="margin-top: 0.3rem;">
            <label>WiFi SSID 2</label>
            <input type="text" name="ssid1" value=")rawliteral" + String(settings.wifiSSID[1]) + R"rawliteral(" placeholder="Backup SSID">
          </div>
          <div class="form-group">
            <label>WiFi Password 2</label>
            <input type="password" name="pass1" value=")rawliteral" + String(settings.wifiPass[1]) + R"rawliteral(" placeholder="Backup Password">
          </div>
        </div>

        <div class="wifi-group" style="border-left: 2px solid #8b5cf6; padding-left: 0.8rem; margin-bottom: 1.5rem;">
          <label style="color: #a78bfa; font-weight: bold; font-size: 0.85rem;">WiFi Network 3 (Backup)</label>
          <div class="form-group" style="margin-top: 0.3rem;">
            <label>WiFi SSID 3</label>
            <input type="text" name="ssid2" value=")rawliteral" + String(settings.wifiSSID[2]) + R"rawliteral(" placeholder="Backup SSID">
          </div>
          <div class="form-group">
            <label>WiFi Password 3</label>
            <input type="password" name="pass2" value=")rawliteral" + String(settings.wifiPass[2]) + R"rawliteral(" placeholder="Backup Password">
          </div>
        </div>

        <h3 style="color: #f59e0b; font-size: 1rem; border-bottom: 1px dashed #1f2937; padding-bottom: 0.3rem; margin-top: 1.5rem;">Cloud Alert Settings</h3>
        <div class="form-group" style="margin-top: 0.5rem;">
          <label>Telegram Bot Token</label>
          <input type="text" name="tg_token" value=")rawliteral" + String(settings.telegramToken) + R"rawliteral(" placeholder="7382910398:AAH...">
        </div>
        <div class="form-group">
          <label>Telegram Chat ID</label>
          <input type="text" name="tg_chat" value=")rawliteral" + String(settings.telegramChatId) + R"rawliteral(" placeholder="123456789">
        </div>
        <div class="btn-container">
          <a class="btn btn-gray" href="/">Back to Dashboard</a>
          <button type="submit" class="btn btn-blue">Save & Reboot</button>
        </div>
      </form>
    </div>
  </div>
</body>
</html>
  )rawliteral";
  _server.send(200, "text/html", html);
}

void NetworkManager::handleSaveSettings() {
  SystemSettings settings = _prefs.getSettings();

  // Parse SSID 1 / Password 1 (supporting old input names 'ssid' / 'pass' for backward-compatible fallback)
  if (_server.hasArg("ssid0")) {
    _server.arg("ssid0").toCharArray(settings.wifiSSID[0], 33);
  } else if (_server.hasArg("ssid")) {
    _server.arg("ssid").toCharArray(settings.wifiSSID[0], 33);
  }
  if (_server.hasArg("pass0")) {
    _server.arg("pass0").toCharArray(settings.wifiPass[0], 65);
  } else if (_server.hasArg("pass")) {
    _server.arg("pass").toCharArray(settings.wifiPass[0], 65);
  }

  // Parse SSID 2 / Password 2
  if (_server.hasArg("ssid1")) {
    _server.arg("ssid1").toCharArray(settings.wifiSSID[1], 33);
  }
  if (_server.hasArg("pass1")) {
    _server.arg("pass1").toCharArray(settings.wifiPass[1], 65);
  }

  // Parse SSID 3 / Password 3
  if (_server.hasArg("ssid2")) {
    _server.arg("ssid2").toCharArray(settings.wifiSSID[2], 33);
  }
  if (_server.hasArg("pass2")) {
    _server.arg("pass2").toCharArray(settings.wifiPass[2], 65);
  }

  if (_server.hasArg("tg_token")) {
    _server.arg("tg_token").toCharArray(settings.telegramToken, 80);
  }
  if (_server.hasArg("tg_chat")) {
    _server.arg("tg_chat").toCharArray(settings.telegramChatId, 20);
  }

  _prefs.saveSettings(settings);

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Rebooting...</title><meta http-equiv="refresh" content="5;url=/"><style>body{background-color:#0b0f19;color:white;font-family:sans-serif;text-align:center;padding:3rem;}</style></head>
<body>
  <h2>Settings Saved Successfully</h2>
  <p>The ESP32 is now rebooting to establish connection. Redirecting in 5 seconds...</p>
</body>
</html>
  )rawliteral";
  _server.send(200, "text/html", html);
  delay(1000);
  ESP.restart(); // Software reboot
}

void NetworkManager::handleToggleActuator() {
  if (!_server.hasArg("target")) {
    _server.send(400, "text/plain", "Missing target argument.");
    return;
  }

  String target = _server.arg("target");
  SystemSettings settings = _prefs.getSettings();

  if (target == "automode") {
    _prefs.setAutoMode(!settings.autoMode);
  } else if (!settings.autoMode) {
    ActuatorState state = _actuators.getState();
    if (target == "fan") _actuators.setFan(!state.fan);
    else if (target == "heater") _actuators.setHeater(!state.heater);
    else if (target == "pump") _actuators.setPump(!state.pump);
  }
  
  if (target == "feeder") {
    _actuators.triggerFeeder();
  }

  // Redirect back to main dashboard
  _server.sendHeader("Location", "/");
  _server.send(302, "text/plain", "");
}
