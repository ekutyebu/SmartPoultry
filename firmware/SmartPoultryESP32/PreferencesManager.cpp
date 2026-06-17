#include "PreferencesManager.h"
#include <string.h>

PreferencesManager::PreferencesManager() {}

void PreferencesManager::begin() {
  _prefs.begin("poultry", false); // Open namespace "poultry" in read-write mode
  
  // Check if system has been initialized before
  if (!_prefs.getBool("init_done", false)) {
    DEBUG_PRINTLN("NVS not initialized. Writing default configurations...");
    loadDefaults();
    _prefs.putBool("init_done", true);
  } else {
    // Load persisted settings
    _settings.tempHighLimit = _prefs.getFloat("t_high", DEFAULT_TEMP_HIGH);
    _settings.tempLowLimit = _prefs.getFloat("t_low", DEFAULT_TEMP_LOW);
    _settings.gasHighLimit = _prefs.getFloat("gas_limit", DEFAULT_GAS_LIMIT);
    _settings.waterLowLimit = _prefs.getFloat("water_low", DEFAULT_WATER_LOW);
    _settings.autoMode = _prefs.getBool("auto_mode", true);
    
    // Check if the old key is present and migrate it to wifi_ssid_0
    String oldSsid = _prefs.getString("wifi_ssid", "");
    if (oldSsid.length() > 0) {
      String oldPass = _prefs.getString("wifi_pass", "");
      _prefs.putString("wifi_ssid_0", oldSsid);
      _prefs.putString("wifi_pass_0", oldPass);
      _prefs.remove("wifi_ssid");
      _prefs.remove("wifi_pass");
    }

    // Load persisted settings
    for (int i = 0; i < 3; i++) {
      String ssidKey = "wifi_ssid_" + String(i);
      String passKey = "wifi_pass_" + String(i);
      _prefs.getString(ssidKey.c_str(), "").toCharArray(_settings.wifiSSID[i], 33);
      _prefs.getString(passKey.c_str(), "").toCharArray(_settings.wifiPass[i], 65);
    }
    _prefs.getString("tg_token", DEFAULT_TELEGRAM_BOT_TOKEN).toCharArray(_settings.telegramToken, 80);
    _prefs.getString("tg_chat", DEFAULT_TELEGRAM_CHAT_ID).toCharArray(_settings.telegramChatId, 20);
    
    DEBUG_PRINTLN("NVS Configurations loaded successfully.");
  }
}

void PreferencesManager::loadDefaults() {
  _settings.tempHighLimit = DEFAULT_TEMP_HIGH;
  _settings.tempLowLimit = DEFAULT_TEMP_LOW;
  _settings.gasHighLimit = DEFAULT_GAS_LIMIT;
  _settings.waterLowLimit = DEFAULT_WATER_LOW;
  _settings.autoMode = true;
  
  for (int i = 0; i < 3; i++) {
    strcpy(_settings.wifiSSID[i], ""); // Empty initially to prompt AP setup
    strcpy(_settings.wifiPass[i], "");
  }
  strcpy(_settings.telegramToken, DEFAULT_TELEGRAM_BOT_TOKEN);
  strcpy(_settings.telegramChatId, DEFAULT_TELEGRAM_CHAT_ID);
  
  // Write to flash
  _prefs.putFloat("t_high", _settings.tempHighLimit);
  _prefs.putFloat("t_low", _settings.tempLowLimit);
  _prefs.putFloat("gas_limit", _settings.gasHighLimit);
  _prefs.putFloat("water_low", _settings.waterLowLimit);
  _prefs.putBool("auto_mode", _settings.autoMode);
  for (int i = 0; i < 3; i++) {
    String ssidKey = "wifi_ssid_" + String(i);
    String passKey = "wifi_pass_" + String(i);
    _prefs.putString(ssidKey.c_str(), String(_settings.wifiSSID[i]));
    _prefs.putString(passKey.c_str(), String(_settings.wifiPass[i]));
  }
  _prefs.putString("tg_token", String(_settings.telegramToken));
  _prefs.putString("tg_chat", String(_settings.telegramChatId));
}

SystemSettings PreferencesManager::getSettings() const {
  return _settings;
}

void PreferencesManager::saveSettings(const SystemSettings& settings) {
  _settings = settings;
  
  _prefs.putFloat("t_high", _settings.tempHighLimit);
  _prefs.putFloat("t_low", _settings.tempLowLimit);
  _prefs.putFloat("gas_limit", _settings.gasHighLimit);
  _prefs.putFloat("water_low", _settings.waterLowLimit);
  _prefs.putBool("auto_mode", _settings.autoMode);
  for (int i = 0; i < 3; i++) {
    String ssidKey = "wifi_ssid_" + String(i);
    String passKey = "wifi_pass_" + String(i);
    _prefs.putString(ssidKey.c_str(), String(_settings.wifiSSID[i]));
    _prefs.putString(passKey.c_str(), String(_settings.wifiPass[i]));
  }
  _prefs.putString("tg_token", String(_settings.telegramToken));
  _prefs.putString("tg_chat", String(_settings.telegramChatId));
  
  DEBUG_PRINTLN("All settings saved and synchronized to NVS.");
}

void PreferencesManager::setThresholds(float tempHigh, float tempLow, float gasHigh, float waterLow) {
  _settings.tempHighLimit = tempHigh;
  _settings.tempLowLimit = tempLow;
  _settings.gasHighLimit = gasHigh;
  _settings.waterLowLimit = waterLow;
  
  _prefs.putFloat("t_high", tempHigh);
  _prefs.putFloat("t_low", tempLow);
  _prefs.putFloat("gas_limit", gasHigh);
  _prefs.putFloat("water_low", waterLow);
  
  DEBUG_PRINTLN("Threshold parameters updated in NVS.");
}

void PreferencesManager::setWifiCredentials(const String& ssid, const String& pass) {
  ssid.toCharArray(_settings.wifiSSID[0], 33);
  pass.toCharArray(_settings.wifiPass[0], 65);
  
  _prefs.putString("wifi_ssid_0", ssid);
  _prefs.putString("wifi_pass_0", pass);
  
  DEBUG_PRINTLN("WiFi configuration credentials updated in NVS (Slot 0).");
}

void PreferencesManager::setTelegramSettings(const String& token, const String& chatId) {
  token.toCharArray(_settings.telegramToken, 80);
  chatId.toCharArray(_settings.telegramChatId, 20);
  
  _prefs.putString("tg_token", token);
  _prefs.putString("tg_chat", chatId);
  
  DEBUG_PRINTLN("Telegram bot token and chat parameters updated in NVS.");
}

void PreferencesManager::setAutoMode(bool autoMode) {
  _settings.autoMode = autoMode;
  _prefs.putBool("auto_mode", autoMode);
  
  DEBUG_PRINTF("Automation Mode updated to: %s in NVS.\n", autoMode ? "AUTO" : "MANUAL");
}
