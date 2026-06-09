
#ifndef FOURUT_CONFIG_H
#define FOURUT_CONFIG_H

#include <Arduino.h>


#ifndef PI
#define PI 3.14159265358979323846
#endif

#if __has_include("esp_arduino_version.h")
#include "esp_arduino_version.h"
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#if __has_include("../secrets.h")
#include "../secrets.h"
#else
#error "Missing secrets.h. Copy firmware/edge_gateway_esp32/secrets.example.h to secrets.h and fill in local credentials."
#endif

// ============================================================
// Hardware pins and sensor addresses
// ============================================================
constexpr int I2C_SDA        = 6;
constexpr int I2C_SCL        = 7;
constexpr int BUZZER_PIN     = 2;      // GPIO2 (D0) - Active buzzer
constexpr int CANCEL_BTN_PIN = 3;      // GPIO3 (D1) - Cancel button
constexpr uint8_t MAX30102_ADDR = 0x57;

// ============================================================
// FreeRTOS task priorities
// ============================================================
constexpr int PRIORITY_ANOMALY = 5;
constexpr int PRIORITY_ALARM   = 4;
constexpr int PRIORITY_ENV     = 3;
constexpr int PRIORITY_CLOUD   = 2;
constexpr int PRIORITY_DEBUG   = 1;

// ============================================================
// Risk thresholds
// Keep this file as the single source of truth for edge-side
// thresholds. Mirror cloud-side Lambda thresholds deliberately.
// ============================================================
constexpr int SPO2_WARNING  = 90;
constexpr int SPO2_CRITICAL = 80;

constexpr int BPM_BRADY = 50;
constexpr int BPM_TACHY = 120;

constexpr float FREE_FALL_THRESH = 4.9f;
constexpr float IMPACT_THRESH    = 29.4f;
constexpr unsigned long FALL_TIME_WINDOW = 1500;
constexpr unsigned long INACTIVITY_TIME  = 3000;

// MPU6050 gyro values from the Adafruit library are in rad/s.
constexpr float STILL_SVM_MIN             = 7.0f;
constexpr float STILL_SVM_MAX             = 12.5f;
constexpr float STILL_GYRO_THRESH         = 0.35f;
constexpr float POSTURE_CHANGE_THRESH_DEG = 35.0f;
constexpr unsigned long FALL_CONFIRM_TIMEOUT_MS = 6000;
constexpr unsigned long FALL_LATCH_MS = 30000;

constexpr float RMSSD_FATIGUE_LEVEL_1   = 20.0f;
constexpr float RMSSD_EXHAUSTION_LEVEL_2 = 15.0f;
constexpr int BPM_EXHAUSTION_THRESHOLD = 110;

constexpr int AQI_WARNING_LEVEL = 75;
constexpr int AQI_DANGER_LEVEL  = 150;

constexpr unsigned long ENV_NODE_TIMEOUT_MS = 15000;

// ============================================================
// HRV configuration
// ============================================================
constexpr unsigned long HRV_WINDOW_MS = 60000;
constexpr int MIN_IBI_COUNT  = 10;
constexpr int MAX_IBI_BUFFER = 500;
constexpr int MEDIAN_WINDOW  = 5;

// ============================================================
// Timing
// ============================================================
constexpr unsigned long SENSOR_READ_INTERVAL_MS    = 1000;
constexpr unsigned long MAX30102_READ_INTERVAL_MS  = 1000;
constexpr unsigned long MPU_READ_INTERVAL_MS       = 20;
constexpr unsigned long CLOUD_PUBLISH_INTERVAL_MS  = 5000;
constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = 5000;
constexpr unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
constexpr unsigned long ALERT_COOLDOWN_MS          = 30000;
constexpr unsigned long CANCEL_COOLDOWN_MS         = 30000;

// ============================================================
// ESP-NOW packet metadata
// ============================================================
constexpr uint16_t ENV_PACKET_MAGIC = 0x4655;
constexpr uint8_t ENV_PACKET_VERSION = 1;

// ============================================================
// MQTT / AWS IoT
// ============================================================
constexpr int MQTT_PORT = 8883;
constexpr const char* WORKER_ID = "W001";
constexpr const char* MQTT_CLIENT_ID = "W001";
constexpr const char* TOPIC_TELEMETRY = "worker/W001/telemetry";
constexpr const char* TOPIC_HRV       = "worker/W001/hrv";
constexpr const char* TOPIC_ALERT     = "worker/W001/alert";

// ============================================================
// Alert cooldown table
// ============================================================
constexpr int ALERT_COOLDOWN_SLOTS = 8;

#endif
