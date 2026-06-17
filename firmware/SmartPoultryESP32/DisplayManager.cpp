#include "DisplayManager.h"

// Define Custom Characters
byte charTemp[8] = {
  0b00100,
  0b01010,
  0b01010,
  0b01110,
  0b01110,
  0b11111,
  0b11111,
  0b01110
};

byte charWater[8] = {
  0b00100,
  0b00100,
  0b01010,
  0b01010,
  0b10001,
  0b10001,
  0b01110,
  0b00000
};

byte charFan[8] = {
  0b00000,
  0b10001,
  0b01010,
  0b00100,
  0b01010,
  0b10001,
  0b00000,
  0b00000
};

byte charLamp[8] = {
  0b01110,
  0b10001,
  0b10001,
  0b10101,
  0b01110,
  0b01110,
  0b00100,
  0b00000
};

byte charPump[8] = {
  0b00110,
  0b01001,
  0b01111,
  0b00100,
  0b11111,
  0b00100,
  0b00100,
  0b00000
};

byte charWifi[8] = {
  0b00000,
  0b11111,
  0b00001,
  0b01110,
  0b00100,
  0b10101,
  0b00100,
  0b00000
};

byte charBell[8] = {
  0b00100,
  0b01110,
  0b01110,
  0b01110,
  0b11111,
  0b00000,
  0b00100,
  0b00000
};

DisplayManager::DisplayManager() 
  : _lcd(LCD_I2C_ADDRESS, 16, 2), 
    _currentPage(PAGE_CLIMATE), 
    _lastPageChangeTime(0), 
    _lastFlashTime(0),
    _flashToggle(false),
    _alertActive(false), 
    _alertMessage("") {}

void DisplayManager::begin() {
  Wire.begin(PIN_SDA, PIN_SCL);
  _lcd.init();
  _lcd.backlight();
  createCustomChars();
  
  _lcd.clear();
  _lcd.setCursor(0, 0);
  _lcd.print("Smart PoultrySys");
  _lcd.setCursor(0, 1);
  _lcd.print("Initializing...");
  
  _lastPageChangeTime = millis();
}

void DisplayManager::createCustomChars() {
  _lcd.createChar(0, charTemp);
  _lcd.createChar(1, charWater);
  _lcd.createChar(2, charFan);
  _lcd.createChar(3, charLamp);
  _lcd.createChar(4, charPump);
  _lcd.createChar(5, charWifi);
  _lcd.createChar(6, charBell);
}

void DisplayManager::triggerAlertFlash(const String& message) {
  _alertActive = true;
  _alertMessage = message;
}

void DisplayManager::update(const SensorData& sensorData, const ActuatorState& actuatorState, const String& wifiStatus, const String& ipAddr) {
  unsigned long now = millis();

  // Check if there are active sensor faults or warnings to auto-trigger alert flashing
  if (sensorData.dhtFault || sensorData.gasFault || sensorData.waterFault || sensorData.feedLevelLow || sensorData.gasLevel > DEFAULT_GAS_LIMIT || actuatorState.pumpSafetyTripped) {
    _alertActive = true;
    if (sensorData.dhtFault) _alertMessage = "DHT22 FAULT!";
    else if (sensorData.gasFault) _alertMessage = "MQ2 GAS FAULT!";
    else if (sensorData.waterFault) _alertMessage = "WATER SNSR FLT!";
    else if (actuatorState.pumpSafetyTripped) _alertMessage = "PUMP SAFETY CUT!";
    else if (sensorData.gasLevel > DEFAULT_GAS_LIMIT) _alertMessage = "DANGER: GAS HIGH";
    else if (sensorData.feedLevelLow) _alertMessage = "WARNING: FEED LOW";
  } else {
    _alertActive = false;
  }

  // Handle alert flashing screen override
  if (_alertActive) {
    if (now - _lastFlashTime >= 500) { // flash every 500ms
      _lastFlashTime = now;
      _flashToggle = !_flashToggle;
      showAlertPage();
    }
    return; // Block regular screens while alert is flashing
  }

  // Normal page rotation (every 3.5 seconds)
  if (now - _lastPageChangeTime >= 3500) {
    _lastPageChangeTime = now;
    _currentPage = static_cast<LCDPage>((static_cast<int>(_currentPage) + 1) % PAGE_COUNT);
    _lcd.clear();
  }

  switch (_currentPage) {
    case PAGE_CLIMATE:
      showClimatePage(sensorData);
      break;
    case PAGE_RESOURCES:
      showResourcesPage(sensorData);
      break;
    case PAGE_ACTUATORS:
      showActuatorsPage(actuatorState, sensorData);
      break;
    case PAGE_NETWORK:
      showNetworkPage(wifiStatus, ipAddr);
      break;
    default:
      break;
  }
}

void DisplayManager::showClimatePage(const SensorData& data) {
  _lcd.setCursor(0, 0);
  _lcd.write(byte(0)); // Temp Icon
  _lcd.print(" Temp: ");
  if (data.dhtFault) {
    _lcd.print("ERR   ");
  } else {
    _lcd.print(data.temperature, 1);
    _lcd.print((char)223); // Degree symbol
    _lcd.print("C  ");
  }

  _lcd.setCursor(0, 1);
  _lcd.write(byte(1)); // Water droplet for humidity
  _lcd.print(" Hum:  ");
  if (data.dhtFault) {
    _lcd.print("ERR   ");
  } else {
    _lcd.print(data.humidity, 1);
    _lcd.print("%   ");
  }
}

void DisplayManager::showResourcesPage(const SensorData& data) {
  _lcd.setCursor(0, 0);
  _lcd.print("AirGas: ");
  if (data.gasFault) {
    _lcd.print("ERR    ");
  } else {
    _lcd.print((int)data.gasLevel);
    _lcd.print(" PPM  ");
  }

  _lcd.setCursor(0, 1);
  _lcd.write(byte(1));
  _lcd.print(" Water: ");
  if (data.waterFault) {
    _lcd.print("ERR    ");
  } else {
    _lcd.print((int)data.waterLevel);
    _lcd.print("%   ");
  }
}

void DisplayManager::showActuatorsPage(const ActuatorState& state, const SensorData& data) {
  _lcd.setCursor(0, 0);
  _lcd.print("Act:");
  _lcd.write(byte(2)); // Fan Icon
  _lcd.print(state.fan ? "+" : "-");
  _lcd.write(byte(3)); // Lamp Icon
  _lcd.print(state.heater ? "+" : "-");
  _lcd.write(byte(4)); // Pump Icon
  _lcd.print(state.pump ? "+" : "-");

  _lcd.setCursor(0, 1);
  _lcd.print("Feed: ");
  if (data.feedLevelLow) {
    _lcd.print("LOW    ");
  } else {
    _lcd.print("NORMAL ");
  }
}

void DisplayManager::showNetworkPage(const String& wifiStatus, const String& ipAddr) {
  _lcd.setCursor(0, 0);
  _lcd.write(byte(5)); // WiFi Icon
  _lcd.print(" ");
  _lcd.print(wifiStatus.substring(0, 14));

  _lcd.setCursor(0, 1);
  _lcd.print(ipAddr.substring(0, 16));
}

void DisplayManager::showAlertPage() {
  _lcd.setCursor(0, 0);
  if (_flashToggle) {
    _lcd.write(byte(6)); // Bell Icon
    _lcd.print(" ** WARNING ** ");
    _lcd.setCursor(0, 1);
    // Center-align or pad the message
    String paddedMsg = _alertMessage;
    while (paddedMsg.length() < 16) paddedMsg = " " + paddedMsg;
    _lcd.print(paddedMsg);
  } else {
    _lcd.clear(); // Flashing effect
  }
}
