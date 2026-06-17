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
    
    _prefs.getString("wifi_ssid", "").toCharArray(_settings.wifiSSID, 33);
    _prefs.getString("wifi_pass", "").toCharArray(_settings.wifiPass, 65);
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
  
  strcpy(_settings.wifiSSID, ""); // Empty initially to prompt AP setup
  strcpy(_settings.wifiPass, "");
  strcpy(_settings.telegramToken, DEFAULT_TELEGRAM_BOT_TOKEN);
  strcpy(_settings.telegramChatId, DEFAULT_TELEGRAM_CHAT_ID);
  
  // Write to flash
  _prefs.putFloat("t_high", _settings.tempHighLimit);
  _prefs.putFloat("t_low", _settings.tempLowLimit);
  _prefs.putFloat("gas_limit", _settings.gasHighLimit);
  _prefs.putFloat("water_low", _settings.waterLowLimit);
  _prefs.putBool("auto_mode", _settings.autoMode);
  _prefs.putString("wifi_ssid", String(_settings.wifiSSID));
  _prefs.putString("wifi_pass", String(_settings.wifiPass));
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
  _prefs.putString("wifi_ssid", String(_settings.wifiSSID));
  _prefs.putString("wifi_pass", String(_settings.wifiPass));
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
  ssid.toCharArray(_settings.wifiSSID, 33);
  pass.toCharArray(_settings.wifiPass, 65);
  
  _prefs.putString("wifi_ssid", ssid);
  _prefs.putString("wifi_pass", pass);
  
  DEBUG_PRINTLN("WiFi configuration credentials updated in NVS.");
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
