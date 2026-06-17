#ifndef ACTUATOR_MANAGER_H
#define ACTUATOR_MANAGER_H

#include <ESP32Servo.h>
#include "Config.h"

struct ActuatorState {
  bool fan;
  bool heater;
  bool pump;
  bool feederActive;
  bool pumpSafetyTripped; // Set if pump ran too long
};

class ActuatorManager {
public:
  ActuatorManager();
  void begin();
  void update(); // Processes timed events (servo dispenser, pump safety timeout)
  
  void setFan(bool state);
  void setHeater(bool state);
  void setPump(bool state);
  void triggerFeeder(int durationMs = FEED_SERVO_PORTION_MS);
  void resetPumpSafety();

  ActuatorState getState() const;

private:
  Servo _feederServo;
  ActuatorState _state;

  unsigned long _feederStartTime;
  unsigned long _feederDuration;
  
  unsigned long _pumpStartTime;
  bool _pumpRunning;
};

#endif // ACTUATOR_MANAGER_H
