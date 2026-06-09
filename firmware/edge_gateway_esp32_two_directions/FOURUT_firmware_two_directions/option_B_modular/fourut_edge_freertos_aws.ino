/**
 * ============================================================
 * FOURUT - Health & Safety Monitoring System
 * Board  : XIAO ESP32-C3
 * Sensors: MAX30102 (SpO2 + HR), MPU6050 (IMU)
 * Features: Fall detection, HRV/Fatigue detection, Buzzer alarm,
 *           ESP-NOW environmental node, AWS IoT MQTT publishing.
 * ============================================================
 *
 * This sketch is intentionally small. System logic lives in src/.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "src/config.h"
#include "src/globals.h"
#include "src/types.h"
#include "src/sensors.h"
#include "src/espnow_env.h"
#include "src/mqtt_client.h"
#include "src/tasks.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    WiFi.mode(WIFI_STA);
    delay(100);

    Serial.print("XIAO Gateway MAC: ");
    Serial.println(WiFi.macAddress());

    Serial.print("Wi-Fi channel: ");
    Serial.println(WiFi.channel());

    Serial.println("\n========================================");
    Serial.println("FOURUT - Edge-first Health & Safety");
    Serial.println("========================================\n");

    initializePinsAndSensors();

    dataMutex = xSemaphoreCreateMutex();
    alertQueue = xQueueCreate(10, sizeof(AlertEvent));
    hrvQueue = xQueueCreate(5, sizeof(SensorData));
    envQueue = xQueueCreate(10, sizeof(EnvPacket));

    if (dataMutex == NULL || alertQueue == NULL || hrvQueue == NULL || envQueue == NULL) {
        Serial.println("FreeRTOS object creation failed");
        while (1) {
            delay(1000);
        }
    }

    initializeLatestData();

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    initEspNowGateway();

    espClient.setCACert(AWS_CERT_CA);
    espClient.setCertificate(AWS_CERT_CRT);
    espClient.setPrivateKey(AWS_CERT_PRIVATE);

    client.setBufferSize(1024);
    client.setSocketTimeout(30);   // AWS TLS/MQTT handshake may need longer than default
    client.setKeepAlive(60);
    client.setServer(AWS_IOT_ENDPOINT, MQTT_PORT);

    xTaskCreate(
        Task_AnomalyDetection,
        "AnomalyDetection",
        8192,
        NULL,
        PRIORITY_ANOMALY,
        NULL
    );

    xTaskCreate(
        Task_AlarmControl,
        "AlarmControl",
        2048,
        NULL,
        PRIORITY_ALARM,
        NULL
    );

    xTaskCreate(
        Task_EnvReceiver,
        "EnvReceiver",
        4096,
        NULL,
        PRIORITY_ENV,
        NULL
    );

    xTaskCreate(
        Task_CloudPublish,
        "CloudPublish",
        8192,
        NULL,
        PRIORITY_CLOUD,
        NULL
    );

    Serial.println("FreeRTOS tasks started");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
