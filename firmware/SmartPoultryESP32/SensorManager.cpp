#include "SensorManager.h"

SensorManager::SensorManager() : _dht(PIN_DHT22, DHT22), _historyIndex(0) {
  _data.temperature = 0.0f;
  _data.humidity = 0.0f;
  _data.gasLevel = 0.0f;
  _data.waterLevel = 0.0f;
  _data.feedLevelLow = false;
  _data.dhtFault = false;
  _data.gasFault = false;
  _data.waterFault = false;
  _data.feedFault = false;
}

void SensorManager::begin() {
  pinMode(PIN_FEED_IR, INPUT);
  pinMode(PIN_GAS_MQ2, INPUT);
  pinMode(PIN_WATER_LEVEL, INPUT);

  // Activate the ESP32's internal pull-up resistor (~45kΩ) on the DHT22 data line.
  // This avoids the need for an external pull-up resistor on the data pin.
  // Note: The internal pull-up is sufficient for short cable runs (<30cm).
  // For longer cables, a 4.7kΩ external resistor between DATA and VCC is still recommended.
  pinMode(PIN_DHT22, INPUT_PULLUP);

  _dht.begin();
  initializeFilters();
  DEBUG_PRINTLN("SensorManager initialized successfully.");
}

void SensorManager::initializeFilters() {
  float initialGas = analogRead(PIN_GAS_MQ2);
  float initialWater = analogRead(PIN_WATER_LEVEL);

  for (int i = 0; i < FILTER_WINDOW; ++i) {
    _gasHistory[i] = initialGas;
    _waterHistory[i] = initialWater;
  }
}

float SensorManager::applyGasFilter(float rawVal) {
  _gasHistory[_historyIndex] = rawVal;
  float sum = 0;
  for (int i = 0; i < FILTER_WINDOW; ++i) {
    sum += _gasHistory[i];
  }
  return sum / FILTER_WINDOW;
}

float SensorManager::applyWaterFilter(float rawVal) {
  _waterHistory[_historyIndex] = rawVal;
  float sum = 0;
  for (int i = 0; i < FILTER_WINDOW; ++i) {
    sum += _waterHistory[i];
  }
  return sum / FILTER_WINDOW;
}

void SensorManager::update() {
  // Read DHT22 (Temperature and Humidity)
  float t = _dht.readTemperature();
  float h = _dht.readHumidity();

  if (isnan(t) || isnan(h) || t < -40.0f || t > 80.0f || h < 0.0f || h > 100.0f) {
    _data.dhtFault = true;
    DEBUG_PRINTLN("Warning: DHT22 Fault detected!");
  } else {
    _data.dhtFault = false;
    _data.temperature = t;
    _data.humidity = h;
  }

  // Read MQ2 Air Quality Sensor (Raw range on ESP32 ADC: 0 - 4095)
  int rawGas = analogRead(PIN_GAS_MQ2);
  float filteredGas = applyGasFilter(rawGas);
  
  // Basic validation for gas sensor (e.g. if completely disconnected, it might hover near 0 or float)
  if (rawGas == 0) {
    _data.gasFault = true;
  } else {
    _data.gasFault = false;
    _data.gasLevel = filteredGas;
  }

  // Read Water Level Sensor
  // Analog values calibrated: 
  // ~300 when completely dry (air moisture/minimal noise), 
  // ~2800 when fully submerged in chicken drinking water.
  int rawWater = analogRead(PIN_WATER_LEVEL);
  float filteredWater = applyWaterFilter(rawWater);

  const float WATER_MIN_ADC = 300.0f;
  const float WATER_MAX_ADC = 2800.0f;

  if (rawWater > 4090) { // ADC max saturated could mean short circuit
    _data.waterFault = true;
    _data.waterLevel = 0.0f;
  } else {
    _data.waterFault = false;
    float waterPct = ((filteredWater - WATER_MIN_ADC) / (WATER_MAX_ADC - WATER_MIN_ADC)) * 100.0f;
    if (waterPct < 0.0f) waterPct = 0.0f;
    if (waterPct > 100.0f) waterPct = 100.0f;
    _data.waterLevel = waterPct;
  }

  // Read Feed Level IR Sensor
  // Standard IR obstacle sensors: 
  // Obstacle detected (feed present blocking beam) -> Output LOW
  // No obstacle (feed depleted below sensor height) -> Output HIGH
  int feedState = digitalRead(PIN_FEED_IR);
  _data.feedLevelLow = (feedState == HIGH);
  _data.feedFault = false; // Simple digital sensor, faults are hard to detect without active checks.

  // Increment moving average ring buffer index
  _historyIndex = (_historyIndex + 1) % FILTER_WINDOW;
}

SensorData SensorManager::getData() const {
  return _data;
}

bool SensorManager::selfTest() {
  update();
  bool passes = !_data.dhtFault && !_data.gasFault && !_data.waterFault;
  if (passes) {
    DEBUG_PRINTLN("Sensor Self-Test Passed.");
  } else {
    DEBUG_PRINTF("Sensor Self-Test Failed: DHT:%d, GAS:%d, WATER:%d\n", 
                 _data.dhtFault, _data.gasFault, _data.waterFault);
  }
  return passes;
}
