#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "SensorManager.h"
#include "ActuatorManager.h"

enum LCDPage {
  PAGE_CLIMATE = 0,
  PAGE_RESOURCES,
  PAGE_ACTUATORS,
  PAGE_NETWORK,
  PAGE_COUNT // Helper for wrapping around
};

class DisplayManager {
public:
  DisplayManager();
  void begin();
  void update(const SensorData& sensorData, const ActuatorState& actuatorState, const String& wifiStatus, const String& ipAddr);
  void triggerAlertFlash(const String& message);

private:
  LiquidCrystal_I2C _lcd;
  LCDPage _currentPage;
  
  unsigned long _lastPageChangeTime;
  unsigned long _lastFlashTime;
  bool _flashToggle;

  bool _alertActive;
  String _alertMessage;

  void showClimatePage(const SensorData& data);
  void showResourcesPage(const SensorData& data);
  void showActuatorsPage(const ActuatorState& state, const SensorData& data);
  void showNetworkPage(const String& wifiStatus, const String& ipAddr);
  void showAlertPage();

  void createCustomChars();
};

#endif // DISPLAY_MANAGER_H
