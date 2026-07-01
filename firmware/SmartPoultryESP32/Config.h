#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// Debug Settings
// =============================================================================
#define DEBUG_SERIAL 1 // Set to 1 to enable Serial logs

#ifdef DEBUG_PRINT
  #undef DEBUG_PRINT
#endif
#ifdef DEBUG_PRINTLN
  #undef DEBUG_PRINTLN
#endif
#ifdef DEBUG_PRINTF
  #undef DEBUG_PRINTF
#endif

#if DEBUG_SERIAL
  #define DEBUG_PRINT(x)     Serial.print(x)
  #define DEBUG_PRINTLN(x)   Serial.println(x)
  #define DEBUG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// =============================================================================
// Hardware Pinout Configuration
// =============================================================================
// Sensors
#define PIN_DHT22         23   // DHT22 Data Pin
#define PIN_GAS_MQ2       34   // MQ2 Analog Output (ADC1_CH6)
#define PIN_WATER_LEVEL   35   // Water Level Sensor Analog Output (ADC1_CH7)
#define PIN_FEED_IR       32   // IR Sensor Digital Output (Low Feed Indicator)

// Actuators
#define PIN_SERVO_FEEDER  13   // Feeder Servo Motor Signal (PWM)
#define PIN_RELAY_FAN     25   // Cooling Fan Relay (Active High/Low depends on config)
#define PIN_RELAY_HEATER  26   // Heating Lamp Relay
#define PIN_RELAY_PUMP    27   // Water Pump Relay

// Local Display (I2C)
#define LCD_I2C_ADDRESS   0x27 // Default 16x2 LCD I2C Address
#define PIN_SDA           21
#define PIN_SCL           22

// System Status Indication
#define PIN_STATUS_LED    2    // Onboard LED for status feedback

// =============================================================================
// Safety & Hysteresis Constants
// =============================================================================
#define HYSTERESIS_TEMP      0.5   // Celsius offset to prevent rapid relay cycling
#define WATER_PUMP_TIMEOUT   30000 // Max time water pump runs before safety shutoff (ms)
#define FEED_SERVO_PORTION_MS 1500 // Time servo remains open for feeding cycle (ms)

// =============================================================================
// Default Automation Thresholds (Updated from cloud settings)
// =============================================================================
#define DEFAULT_TEMP_HIGH    30.0  // Temp high threshold (°C) -> Fan turns on
#define DEFAULT_TEMP_LOW     20.0  // Temp low threshold (°C) -> Heater turns on
#define DEFAULT_GAS_LIMIT    400.0 // Air Quality threshold (analog units/PPM) -> Fan turns on
#define DEFAULT_WATER_LOW    25.0  // Water level percent threshold -> Pump turns on

// =============================================================================
// Network & Integration Configuration
// =============================================================================
// Fallback Access Point (If main WiFi fails, ESP32 creates this AP for local settings dashboard)
#define DEFAULT_AP_SSID       "DarkDev"
#define DEFAULT_AP_PASS       "Man2001@"

// Structure for multiple Wi-Fi networks configuration
struct WiFiCredential {
    const char* ssid;
    const char* password;
};

// Define multiple Wi-Fi credentials. The firmware will scan and connect to the strongest available AP.
static const WiFiCredential WIFI_NETWORKS[] = {
    {"DarkDev", "Man2001@"},
    {"Javis", "1234567890"},
    {"La melo", "valdes60"}
};
static const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

// Production Cloud Server Configuration
#define DEFAULT_CLOUD_URL     "https://smart-poultry-six.vercel.app"
#define DEVICE_KEY            "poultry_house_01" // Unique identifier for the house
#define SYNC_INTERVAL_MS      5000               // Synchronization frequency with Cloud Dashboard (ms)

// Telegram Notification Settings (Backup/Offline alerts directly from ESP32)
#define DEFAULT_TELEGRAM_BOT_TOKEN "7382910398:AAH-YOUR-DEFAULT-BOT-TOKEN"
#define DEFAULT_TELEGRAM_CHAT_ID   "123456789"

#endif // CONFIG_H
