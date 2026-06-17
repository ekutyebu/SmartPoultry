#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "PreferencesManager.h"
#include "SensorManager.h"
#include "ActuatorManager.h"

class NetworkManager {
public:
  NetworkManager(PreferencesManager& prefs, SensorManager& sensors, ActuatorManager& actuators);
  void begin();
  void update(); // Handles web server clients and connection monitoring
  
  bool isConnected() const;
  String getWifiStatusString() const;
  String getLocalIP() const;
  
  // Cloud sync
  bool syncWithCloud(const SensorData& sData, const ActuatorState& aState);
  
  // Direct Telegram alert sender
  void sendTelegramAlert(const String& message);

private:
  PreferencesManager& _prefs;
  SensorManager& _sensors;
  ActuatorManager& _actuators;
  
  WebServer _server;
  bool _apMode;
  unsigned long _lastWifiCheckTime;
  unsigned long _lastTelegramTime;
  
  void setupAP();
  void setupClientWiFi();
  void checkWifiConnection();
  
  // Local Web Server Routes
  void handleRoot();
  void handleConfig();
  void handleSaveSettings();
  void handleToggleActuator();
  
  // JSON payload parsers
  void processCloudResponse(const String& jsonResponse);
};

#endif // NETWORK_MANAGER_H
