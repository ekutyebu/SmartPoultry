#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <DHT.h>
#include "Config.h"

struct SensorData {
  float temperature;
  float humidity;
  float gasLevel;      // MQ2 filtered reading
  float waterLevel;     // Water level in percentage (0 - 100)
  bool feedLevelLow;    // Feed level low (true if IR beam is not broken)
  
  // Fault status indicators
  bool dhtFault;
  bool gasFault;
  bool waterFault;
  bool feedFault;
};

class SensorManager {
public:
  SensorManager();
  void begin();
  void update(); // Reads sensors and applies moving average filters
  SensorData getData() const;
  bool selfTest(); // Run simple diagnostics on sensors

private:
  DHT _dht;
  SensorData _data;

  // Moving average filter variables
  static const int FILTER_WINDOW = 10;
  float _gasHistory[FILTER_WINDOW];
  float _waterHistory[FILTER_WINDOW];
  int _historyIndex;

  void initializeFilters();
  float applyGasFilter(float rawVal);
  float applyWaterFilter(float rawVal);
};

#endif // SENSOR_MANAGER_H
