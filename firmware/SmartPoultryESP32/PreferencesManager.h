#ifndef PREFERENCES_MANAGER_H
#define PREFERENCES_MANAGER_H

#include <Preferences.h>
#include "Config.h"

struct SystemSettings {
  float tempHighLimit;
  float tempLowLimit;
  float gasHighLimit;
  float waterLowLimit;
  
  bool autoMode;
  
  char wifiSSID[3][33];
  char wifiPass[3][65];
  
  char telegramToken[80];
  char telegramChatId[20];
};

class PreferencesManager {
public:
  PreferencesManager();
  void begin();
  SystemSettings getSettings() const;
  void saveSettings(const SystemSettings& settings);
  
  // Specific quick updates
  void setThresholds(float tempHigh, float tempLow, float gasHigh, float waterLow);
  void setWifiCredentials(const String& ssid, const String& pass);
  void setTelegramSettings(const String& token, const String& chatId);
  void setAutoMode(bool autoMode);

private:
  Preferences _prefs;
  SystemSettings _settings;
  
  void loadDefaults();
};

#endif // PREFERENCES_MANAGER_H
