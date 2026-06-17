#include "ActuatorManager.h"

// Define relay logical levels (adjust in case of active-low relays)
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

ActuatorManager::ActuatorManager() 
  : _feederStartTime(0), _feederDuration(0), _pumpStartTime(0), _pumpRunning(false) {
  _state.fan = false;
  _state.heater = false;
  _state.pump = false;
  _state.feederActive = false;
  _state.pumpSafetyTripped = false;
}

void ActuatorManager::begin() {
  // Setup Relay Pin Modes
  pinMode(PIN_RELAY_FAN, OUTPUT);
  pinMode(PIN_RELAY_HEATER, OUTPUT);
  pinMode(PIN_RELAY_PUMP, OUTPUT);

  // Initialize relays to OFF
  digitalWrite(PIN_RELAY_FAN, RELAY_OFF);
  digitalWrite(PIN_RELAY_HEATER, RELAY_OFF);
  digitalWrite(PIN_RELAY_PUMP, RELAY_OFF);

  // Allocate hardware PWM timers for servo and attach
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  _feederServo.setPeriodHertz(50); // Standard 50Hz servo
  _feederServo.attach(PIN_SERVO_FEEDER, 500, 2400); // Attach with min/max pulse width in microseconds
  
  // Set servo to closed position initially
  _feederServo.write(0); 

  DEBUG_PRINTLN("ActuatorManager initialized successfully.");
}

void ActuatorManager::setFan(bool state) {
  _state.fan = state;
  digitalWrite(PIN_RELAY_FAN, state ? RELAY_ON : RELAY_OFF);
  DEBUG_PRINTF("Fan state changed to: %s\n", state ? "ON" : "OFF");
}

void ActuatorManager::setHeater(bool state) {
  _state.heater = state;
  digitalWrite(PIN_RELAY_HEATER, state ? RELAY_ON : RELAY_OFF);
  DEBUG_PRINTF("Heater state changed to: %s\n", state ? "ON" : "OFF");
}

void ActuatorManager::setPump(bool state) {
  if (state) {
    if (_state.pumpSafetyTripped) {
      DEBUG_PRINTLN("Error: Cannot turn on water pump, safety switch tripped. Reset first.");
      return;
    }
    if (!_pumpRunning) {
      _pumpRunning = true;
      _pumpStartTime = millis();
    }
  } else {
    _pumpRunning = false;
  }
  
  _state.pump = state;
  digitalWrite(PIN_RELAY_PUMP, state ? RELAY_ON : RELAY_OFF);
  DEBUG_PRINTF("Pump state changed to: %s\n", state ? "ON" : "OFF");
}

void ActuatorManager::triggerFeeder(int durationMs) {
  if (_state.feederActive) {
    DEBUG_PRINTLN("Feeder trigger ignored: Dispenser already active.");
    return;
  }

  _state.feederActive = true;
  _feederStartTime = millis();
  _feederDuration = durationMs;
  
  // Move servo 90 degrees to open dispenser chute
  _feederServo.write(90);
  DEBUG_PRINTLN("Feeding triggered - Feeder servo opened.");
}

void ActuatorManager::resetPumpSafety() {
  _state.pumpSafetyTripped = false;
  DEBUG_PRINTLN("Water pump safety switch reset.");
}

void ActuatorManager::update() {
  unsigned long now = millis();

  // 1. Process Feed Dispenser Servo Timeout
  if (_state.feederActive) {
    if (now - _feederStartTime >= _feederDuration) {
      _state.feederActive = false;
      _feederServo.write(0); // Move servo back to 0 degrees to close dispenser chute
      DEBUG_PRINTLN("Feeding complete - Feeder servo closed.");
    }
  }

  // 2. Process Water Pump Safety Timeout
  if (_pumpRunning) {
    if (now - _pumpStartTime >= WATER_PUMP_TIMEOUT) {
      // Pump has been running longer than the maximum allowed safety limit
      _state.pumpSafetyTripped = true;
      setPump(false); // Force turn off pump
      DEBUG_PRINTLN("CRITICAL WARNING: Water pump run-time exceeded safety limit. Automatic shutdown triggered.");
    }
  }
}

ActuatorState ActuatorManager::getState() const {
  return _state;
}
