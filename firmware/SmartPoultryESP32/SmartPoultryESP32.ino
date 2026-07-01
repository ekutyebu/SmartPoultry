#include <Arduino.h>
#include "Config.h"
#include "PreferencesManager.h"
#include "SensorManager.h"
#include "ActuatorManager.h"
#include "DisplayManager.h"
#include "NetworkManager.h"

// Instantiate Global Module Controllers
PreferencesManager prefs;
SensorManager sensors;
ActuatorManager actuators;
DisplayManager display;
NetworkManager network(prefs, sensors, actuators);

// Non-blocking Timer Intervals
const unsigned long SENSOR_INTERVAL = 1000;    // Read sensors every 1 second
const unsigned long DISPLAY_INTERVAL = 200;    // Update LCD screen contents every 200ms
unsigned long lastSensorReadTime = 0;
unsigned long lastDisplayUpdateTime = 0;
unsigned long lastCloudSyncTime = 0;

// Telegram Alert State Flags (Prevents spamming notifications in loop)
bool alertSentTempHigh = false;
bool alertSentTempLow = false;
bool alertSentGasHigh = false;
bool alertSentWaterLow = false;
bool alertSentFeedLow = false;
bool alertSentPumpSafety = false;
bool alertSentDhtFault = false;

// Setup function
void setup() {
  #if DEBUG_SERIAL
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- Starting Smart Poultry Farm Controller ---");
  #endif

  // Initialize NVS Preferences first
  prefs.begin();

  // Initialize Hardware Modules
  sensors.begin();
  actuators.begin();
  display.begin();

  // Initialize WiFi & Server Connections
  network.begin();

  // Verify Sensor self-test
  sensors.selfTest();

  // Heartbeat initialization completed
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, HIGH);
}

// Edge Automation Rules Loop
void executeEdgeAutomation(const SensorData& sData, const SystemSettings& settings) {
  // If manual override mode is enabled, exit automation feedback loop immediately
  if (!settings.autoMode) return;

  ActuatorState aState = actuators.getState();

  // 1. Temperature Control (Fans vs Heating Lamp)
  if (!sData.dhtFault) {
    // High Temp Cooling logic
    if (sData.temperature > settings.tempHighLimit) {
      if (!aState.fan) actuators.setFan(true);
      if (aState.heater) actuators.setHeater(false);
    } 
    // Low Temp Heating logic
    else if (sData.temperature < settings.tempLowLimit) {
      if (aState.fan) actuators.setFan(false);
      if (!aState.heater) actuators.setHeater(true);
    } 
    // Hysteresis threshold to prevent flapping
    else {
      if (aState.fan && sData.temperature <= (settings.tempHighLimit - HYSTERESIS_TEMP)) {
        // Only turn off fan if Gas Level is also normal
        if (sData.gasFault || sData.gasLevel <= settings.gasHighLimit) {
          actuators.setFan(false);
        }
      }
      if (aState.heater && sData.temperature >= (settings.tempLowLimit + HYSTERESIS_TEMP)) {
        actuators.setHeater(false);
      }
    }
  } else {
    // Default Failsafe state: If climate sensor fails, turn fan ON and heater OFF to protect birds
    // from unknown high-heat conditions. Fan ON is safer than OFF when temp is unknown.
    if (!aState.fan) actuators.setFan(true);
    if (aState.heater) actuators.setHeater(false);
  }

  // 2. Air Quality Control (Gas MQ2)
  // Only apply gas override when DHT22 is healthy. If DHT22 is faulted, the failsafe
  // above has already set the fan to ON, so this block is redundant and would fight
  // the failsafe path causing rapid ON/OFF toggling every sensor cycle.
  if (!sData.gasFault && !sData.dhtFault) {
    if (sData.gasLevel > settings.gasHighLimit) {
      if (!aState.fan) {
        actuators.setFan(true); // Override Fan to ON to ventilate ammonia/smoke
        DEBUG_PRINTLN("Automation Override: High Gas levels. Activating Ventilator Fan!");
      }
    }
  }

  // 3. Water Level Control (Pump)
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
    // Fault failsafe
    actuators.setPump(false);
  }
}

// Telegram Incident Tracking
void processAlertNotifications(const SensorData& sData, const ActuatorState& aState, const SystemSettings& settings) {
  if (!network.isConnected()) return;

  // 1. Temperature Warning
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

  // 2. Air Quality Warning
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

  // 3. Feed Warning
  if (sData.feedLevelLow) {
    if (!alertSentFeedLow) {
      network.sendTelegramAlert("⚠️ FEED WARNING: Feed level in silo is critically LOW. Dispenser requires manual refill.");
      alertSentFeedLow = true;
    }
  } else {
    alertSentFeedLow = false;
  }

  // 4. Water Pump Safety Trip Warning
  if (aState.pumpSafetyTripped) {
    if (!alertSentPumpSafety) {
      network.sendTelegramAlert("🚨 PUMP CRITICAL: Water pump was auto-shutdown after running continuously for too long. Check for dry well or piping leaks!");
      alertSentPumpSafety = true;
    }
  } else {
    alertSentPumpSafety = false;
  }

  // 5. Sensor Failure Warnings
  if (sData.dhtFault) {
    if (!alertSentDhtFault) {
      network.sendTelegramAlert("🛠️ HARDWARE ERROR: DHT22 Climate sensor is failing to return readings! Check wiring.");
      alertSentDhtFault = true;
    }
  } else {
    alertSentDhtFault = false;
  }
}

// Main non-blocking system loop
void loop() {
  unsigned long now = millis();

  // 1. Keep Local Web Server active and process requests
  network.update();

  // 2. Process actuator timed events (servo sweeps, safety cuts)
  actuators.update();

  // 3. Read Sensors non-blockingly
  if (now - lastSensorReadTime >= SENSOR_INTERVAL) {
    lastSensorReadTime = now;
    
    // Read raw hardware values & compute filters
    sensors.update();
    
    // Execute edge automation control rules
    SensorData sData = sensors.getData();
    SystemSettings settings = prefs.getSettings();
    executeEdgeAutomation(sData, settings);

    // Track state and alert the farmer via Telegram if necessary
    processAlertNotifications(sData, actuators.getState(), settings);
  }

  // 4. Cycle and Update Local display screen non-blockingly
  if (now - lastDisplayUpdateTime >= DISPLAY_INTERVAL) {
    lastDisplayUpdateTime = now;
    
    SensorData sData = sensors.getData();
    ActuatorState aState = actuators.getState();
    String wifiStatus = network.getWifiStatusString();
    String ipAddr = network.getLocalIP();
    
    display.update(sData, aState, wifiStatus, ipAddr);
  }

  // 5. Sync Data to Next.js Cloud Server
  SystemSettings settings = prefs.getSettings();
  if (now - lastCloudSyncTime >= SYNC_INTERVAL_MS) {
    lastCloudSyncTime = now;
    
    // Attempt HTTPS upload of telemetry logs and download commands
    bool synced = network.syncWithCloud(sensors.getData(), actuators.getState());
    
    // Blink onboard LED to confirm successful online sync
    if (synced) {
      digitalWrite(PIN_STATUS_LED, LOW);
      delay(50);
      digitalWrite(PIN_STATUS_LED, HIGH);
    }
  }
}
