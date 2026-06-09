
#include <Arduino.h>
#include <Wire.h>

#include "globals.h"
#include "config.h"
#include "sensors.h"

void initializePinsAndSensors() {
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(CANCEL_BTN_PIN, INPUT_PULLUP);
    digitalWrite(BUZZER_PIN, LOW);

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);

    Serial.println("Initializing MAX30102...");

    if (max30102.begin() == true) {
        Serial.println("MAX30102 init success");
        max30102.sensorStartCollect();
        max30102Available = true;
    } else {
        Serial.println("MAX30102 init failed - system will continue without HR/SpO2");
        max30102Available = false;
    }

    Serial.println("Initializing MPU6050...");
    if (!mpu.begin()) {
        Serial.println("Failed to find MPU6050 chip");
        while (1) {
            delay(10);
        }
    }

    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
}

void initializeLatestData() {
    latestData.bpm = -1;
    latestData.spo2 = -1;
    latestData.aqi = -1;
    latestData.svm = 9.8;
    latestData.sdnn = -1;
    latestData.rmssd = -1;
    latestData.ibiCount = 0;
    latestData.alarmLevel = 0;
    latestData.alarmActive = false;
    latestData.fallDetected = false;
    latestData.pm25 = -1;
    latestData.pm10 = -1;
    latestData.envNodeId = 0;
    latestData.envSeq = 0;
    latestData.envLastSeenMs = 0;
    latestData.envOnline = false;
}
