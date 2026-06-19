# 1 "C:\\Users\\ekuty\\AppData\\Local\\Temp\\tmprcwts4qa"
#include <Arduino.h>
# 1 "C:/Users/ekuty/Desktop/SmartPoultry/firmware/SmartPoultryESP32/SmartPoultryESP32.ino"
#include <Arduino.h>
#include "Config.h"
#include "PreferencesManager.h"
#include "SensorManager.h"
#include "ActuatorManager.h"
#include "DisplayManager.h"
#include "NetworkManager.h"


PreferencesManager prefs;
SensorManager sensors;
ActuatorManager actuators;
DisplayManager display;
NetworkManager network(prefs, sensors, actuators);


const unsigned long SENSOR_INTERVAL = 1000;
const unsigned long DISPLAY_INTERVAL = 200;
unsigned long lastSensorReadTime = 0;
unsigned long lastDisplayUpdateTime = 0;
unsigned long lastCloudSyncTime = 0;


bool alertSentTempHigh = false;
bool alertSentTempLow = false;
bool alertSentGasHigh = false;
bool alertSentWaterLow = false;
bool alertSentFeedLow = false;
bool alertSentPumpSafety = false;
bool alertSentDhtFault = false;
void setup();
void executeEdgeAutomation(const SensorData& sData, const SystemSettings& settings);
void processAlertNotifications(const SensorData& sData, const ActuatorState& aState, const SystemSettings& settings);
void loop();
#line 33 "C:/Users/ekuty/Desktop/SmartPoultry/firmware/SmartPoultryESP32/SmartPoultryESP32.ino"
void setup() {
  #if DEBUG_SERIAL
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- Starting Smart Poultry Farm Controller ---");
  #endif


  prefs.begin();


  sensors.begin();
  actuators.begin();
  display.begin();


  network.begin();


  sensors.selfTest();


  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, HIGH);
}


void executeEdgeAutomation(const SensorData& sData, const SystemSettings& settings) {

  if (!settings.autoMode) return;

  ActuatorState aState = actuators.getState();


  if (!sData.dhtFault) {

    if (sData.temperature > settings.tempHighLimit) {
      if (!aState.fan) actuators.setFan(true);
      if (aState.heater) actuators.setHeater(false);
    }

    else if (sData.temperature < settings.tempLowLimit) {
      if (aState.fan) actuators.setFan(false);
      if (!aState.heater) actuators.setHeater(true);
    }

    else {
      if (aState.fan && sData.temperature <= (settings.tempHighLimit - HYSTERESIS_TEMP)) {

        if (sData.gasFault || sData.gasLevel <= settings.gasHighLimit) {
          actuators.setFan(false);
        }
      }
      if (aState.heater && sData.temperature >= (settings.tempLowLimit + HYSTERESIS_TEMP)) {
        actuators.setHeater(false);
      }
    }
  } else {

    if (aState.fan) actuators.setFan(false);
    if (aState.heater) actuators.setHeater(false);
  }


  if (!sData.gasFault) {
    if (sData.gasLevel > settings.gasHighLimit) {
      if (!aState.fan) {
        actuators.setFan(true);
        DEBUG_PRINTLN("Automation Override: High Gas levels. Activating Ventilator Fan!");
      }
    }
  }


  if (!sData.waterFault && !aState.pumpSafetyTripped) {
    if (sData.waterLevel < settings.waterLowLimit) {
      if (!aState.pump) {
        actuators.setPump(true);
        DEBUG_PRINTLN("Automation Action: Low Water level. Activating Pump!");
      }
    } else if (sData.waterLevel >= 95.0f) {
      if (aState.pump) {
        actuators.setPump(false);
        DEBUG_PRINTLN("Automation Action: Tank full. Deactivating Pump.");
      }
    }
  } else if (aState.pump) {

    actuators.setPump(false);
  }
}


void processAlertNotifications(const SensorData& sData, const ActuatorState& aState, const SystemSettings& settings) {
  if (!network.isConnected()) return;


  if (!sData.dhtFault) {
    if (sData.temperature > settings.tempHighLimit + 3.0f) {
      if (!alertSentTempHigh) {
        network.sendTelegramAlert("🚨 CRITICAL TEMP WARNING: High Temperature in Poultry House! Currently: " + String(sData.temperature, 1) + "°C");
        alertSentTempHigh = true;
      }
    } else {
      alertSentTempHigh = false;
    }

    if (sData.temperature < settings.tempLowLimit - 3.0f) {
      if (!alertSentTempLow) {
        network.sendTelegramAlert("🚨 CRITICAL TEMP WARNING: Low Temperature in Poultry House! Currently: " + String(sData.temperature, 1) + "°C");
        alertSentTempLow = true;
      }
    } else {
      alertSentTempLow = false;
    }
  }


  if (!sData.gasFault) {
    if (sData.gasLevel > settings.gasHighLimit) {
      if (!alertSentGasHigh) {
        network.sendTelegramAlert("⚠️ AIR QUALITY WARNING: High Ammonia/Smoke levels detected! Level: " + String((int)sData.gasLevel) + " PPM");
        alertSentGasHigh = true;
      }
    } else {
      alertSentGasHigh = false;
    }
  }


  if (sData.feedLevelLow) {
    if (!alertSentFeedLow) {
      network.sendTelegramAlert("⚠️ FEED WARNING: Feed level in silo is critically LOW. Dispenser requires manual refill.");
      alertSentFeedLow = true;
    }
  } else {
    alertSentFeedLow = false;
  }


  if (aState.pumpSafetyTripped) {
    if (!alertSentPumpSafety) {
      network.sendTelegramAlert("🚨 PUMP CRITICAL: Water pump was auto-shutdown after running continuously for too long. Check for dry well or piping leaks!");
      alertSentPumpSafety = true;
    }
  } else {
    alertSentPumpSafety = false;
  }


  if (sData.dhtFault) {
    if (!alertSentDhtFault) {
      network.sendTelegramAlert("🛠️ HARDWARE ERROR: DHT22 Climate sensor is failing to return readings! Check wiring.");
      alertSentDhtFault = true;
    }
  } else {
    alertSentDhtFault = false;
  }
}


void loop() {
  unsigned long now = millis();


  network.update();


  actuators.update();


  if (now - lastSensorReadTime >= SENSOR_INTERVAL) {
    lastSensorReadTime = now;


    sensors.update();


    SensorData sData = sensors.getData();
    SystemSettings settings = prefs.getSettings();
    executeEdgeAutomation(sData, settings);


    processAlertNotifications(sData, actuators.getState(), settings);
  }


  if (now - lastDisplayUpdateTime >= DISPLAY_INTERVAL) {
    lastDisplayUpdateTime = now;

    SensorData sData = sensors.getData();
    ActuatorState aState = actuators.getState();
    String wifiStatus = network.getWifiStatusString();
    String ipAddr = network.getLocalIP();

    display.update(sData, aState, wifiStatus, ipAddr);
  }


  SystemSettings settings = prefs.getSettings();
  if (now - lastCloudSyncTime >= SYNC_INTERVAL_MS) {
    lastCloudSyncTime = now;


    bool synced = network.syncWithCloud(sensors.getData(), actuators.getState());


    if (synced) {
      digitalWrite(PIN_STATUS_LED, LOW);
      delay(50);
      digitalWrite(PIN_STATUS_LED, HIGH);
    }
  }
}