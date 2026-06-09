
#include <Arduino.h>
#include <WiFi.h>
#include <math.h>

#include "globals.h"
#include "config.h"
#include "alerts.h"
#include "alarms.h"
#include "fall_detection.h"
#include "hrv.h"
#include "risk_assessment.h"
#include "mqtt_client.h"
#include "tasks.h"

void Task_AnomalyDetection(void* pvParameters) {
    unsigned long lastBiometricRead = 0;
    unsigned long lastMpuRead = 0;

    int currentBPM = -1;
    int currentSpO2 = -1;

    float currentSVM = 9.8;
    float currentRMSSD = -1;

    bool currentFallDetected = false;

    while (true) {
        unsigned long now = millis();

        /*
        * BLOCK 1: Reads MPU6050 quickly every 20ms
        * Used for fall detection.
         */
        if (now - lastMpuRead >= MPU_READ_INTERVAL_MS) {
            lastMpuRead = now;

            sensors_event_t accel, gyro, temp;
            mpu.getEvent(&accel, &gyro, &temp);

            currentSVM = sqrt(
                accel.acceleration.x * accel.acceleration.x +
                accel.acceleration.y * accel.acceleration.y +
                accel.acceleration.z * accel.acceleration.z
            );

            if (firstMpuSample) {
                firstMpuSample = false;
                currentSVM = 9.8;
            }

            float gyroMagnitude = sqrt(
                gyro.gyro.x * gyro.gyro.x +
                gyro.gyro.y * gyro.gyro.y +
                gyro.gyro.z * gyro.gyro.z
            );

            bool newFall = assessFallRisk(
                currentSVM,
                accel.acceleration.x,
                accel.acceleration.y,
                accel.acceleration.z,
                gyroMagnitude,
                currentBPM,
                currentSpO2,
                currentRMSSD
            );

            currentFallDetected = newFall || (millis() < fallLatchUntil);

            updateLatestData(
                currentBPM,
                currentSpO2,
                -1,
                currentSVM,
                -1,              
                -1,              
                ibiCount,
                currentFallDetected
            );
        }

        /*
         * BLOCK 2: Reads MAX30102 more slowly, every 1000ms
         * Used for BPM, SpO2, IBI.
         */
        
        if (now - lastBiometricRead >= MAX30102_READ_INTERVAL_MS) {
            lastBiometricRead = now;

            if (!max30102Available) {
                currentBPM = -1;
                currentSpO2 = -1;

                updateLatestData(
                    currentBPM,
                    currentSpO2,
                    -1,
                    currentSVM,
                    -1,
                    -1,
                    ibiCount,
                    currentFallDetected
                );

                Serial.printf(
                    "EDGE | MAX30102: OFFLINE | SVM:%.2f Alarm:%d\n",
                    currentSVM,
                    currentAlarmLevel
                );
            } else {
                max30102.getHeartbeatSPO2();

                int readSpO2 = max30102._sHeartbeatSPO2.SPO2;
                int readBPM  = max30102._sHeartbeatSPO2.Heartbeat;

                if (readSpO2 >= 70 && readSpO2 <= 100) {
                    currentSpO2 = readSpO2;
                } else {
                    currentSpO2 = -1;
                }

                if (readBPM >= 40 && readBPM <= 200) {
                    currentBPM = readBPM;

                    unsigned long currentIBI = 60000UL / currentBPM;

                    if (isValidIBI(currentIBI)) {
                        unsigned long filteredIBI = medianFilter(currentIBI);

                        if (filteredIBI > 0 && ibiCount < MAX_IBI_BUFFER) {
                            ibiBuffer[ibiCount++] = filteredIBI;
                        } 
                    }
                } else {
                    currentBPM = -1;
                }

        /*
         * Get the latest RMSSD if HRV has been calculated previously.
         */
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            currentRMSSD = latestData.rmssd;
            xSemaphoreGive(dataMutex);
        }

        assessBiometricRisk(
            currentSpO2,
            currentBPM,
            currentSVM,
            currentRMSSD
        );

        updateLatestData(
            currentBPM,
            currentSpO2,
            -1,
            currentSVM,
            -1,
            -1,
            ibiCount,
            currentFallDetected
        );

        int debugAQI = -1;
        float debugPM25 = -1;
        bool debugEnvOnline = false;

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            debugAQI = latestData.aqi;
            debugPM25 = latestData.pm25;
            debugEnvOnline = latestData.envOnline;
            xSemaphoreGive(dataMutex);
        }

        Serial.printf(
            "EDGE | HR:%d SpO2:%d AQI:%d PM2.5:%.1f ENV:%s SVM:%.2f Alarm:%d\n",
            currentBPM,
            currentSpO2,
            debugAQI,
            debugPM25,
            debugEnvOnline ? "ON" : "OFF",
            currentSVM,
            currentAlarmLevel
        );
    }
}

        /*
        * BLOCK 3: Calculate HRV according to the HRV_WINDOW_MS time window.
         */
        if (now - lastHRVCheck >= HRV_WINDOW_MS) {
            lastHRVCheck = now;

            SensorData snapshot;
            bool hasSnapshot = false;

            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                snapshot = latestData;
                hasSnapshot = true;
                xSemaphoreGive(dataMutex);
            }

            if (hasSnapshot) {
                updateHRVOnEdge(snapshot.bpm, snapshot.spo2, snapshot.svm);
            } else {
                Serial.println("HRV skipped: data mutex timeout");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void Task_EnvReceiver(void* pvParameters) {
    EnvPacket packet;

    while (true) {
        if (xQueueReceive(envQueue, &packet, pdMS_TO_TICKS(1000)) == pdTRUE) {
            SensorData snapshot;
            bool hasSnapshot = false;

            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                latestData.aqi = packet.aqi;
                latestData.pm25 = packet.pm2_5;
                latestData.pm10 = packet.pm10;
                latestData.envNodeId = packet.node_id;
                latestData.envSeq = packet.seq;
                latestData.envLastSeenMs = millis();
                latestData.envOnline = true;

                snapshot = latestData;
                hasSnapshot = true;

                xSemaphoreGive(dataMutex);
            }

            Serial.printf(
                "ENV | node:%u seq:%lu PM2.5:%.1f PM10:%.1f AQI:%d\n",
                packet.node_id,
                (unsigned long)packet.seq,
                packet.pm2_5,
                packet.pm10,
                packet.aqi
            );

            if (hasSnapshot) {
                assessEnvironmentalRisk(snapshot);
            }
        }

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (
                latestData.envOnline &&
                latestData.envLastSeenMs > 0 &&
                millis() - latestData.envLastSeenMs > ENV_NODE_TIMEOUT_MS
            ) {
                latestData.envOnline = false;
                Serial.println("ENV node offline");
            }

            xSemaphoreGive(dataMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================
// TASK 2: HIGH PRIORITY - LOCAL ALARM CONTROL
// ============================================================
void Task_AlarmControl(void* pvParameters) {
    while (true) {
        checkButtonNonBlocking();

        if (alarmActive) {
            digitalWrite(BUZZER_PIN, HIGH);
        } else {
            digitalWrite(BUZZER_PIN, LOW);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============================================================
// TASK 3: MEDIUM PRIORITY - CLOUD PUBLISH
// ============================================================
void Task_CloudPublish(void* pvParameters) {
    unsigned long lastPublish = 0;

    while (true) {
        connectWiFiNonBlocking();

        if (WiFi.status() == WL_CONNECTED) {
            printGatewayWiFiInfoOnce();
            connectMQTTNonBlocking();

            if (client.connected()) {
                client.loop();

                AlertEvent alertEvent;

                while (xQueueReceive(alertQueue, &alertEvent, 0) == pdTRUE) {
                    publishAlertEvent(alertEvent);
                }

                SensorData hrvData;

                while (xQueueReceive(hrvQueue, &hrvData, 0) == pdTRUE) {
                    publishHRVSummary(hrvData);
                }

                unsigned long now = millis();

                if (now - lastPublish >= CLOUD_PUBLISH_INTERVAL_MS) {
                    lastPublish = now;

                    SensorData snapshot;

                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        snapshot = latestData;
                        xSemaphoreGive(dataMutex);

                        publishTelemetry(snapshot);
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
