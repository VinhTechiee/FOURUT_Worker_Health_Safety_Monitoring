
#ifndef FOURUT_GLOBALS_H
#define FOURUT_GLOBALS_H

#include <Arduino.h>
#include <Wire.h>
#include "DFRobot_BloodOxygen_S.h"
#include <Adafruit_MPU6050.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "config.h"
#include "types.h"

extern DFRobot_BloodOxygen_S_I2C max30102;
extern Adafruit_MPU6050 mpu;

extern volatile bool alarmActive;
extern volatile bool alarmCancelled;
extern volatile int currentAlarmLevel;

extern unsigned long cancelCooldownUntil;
extern AlertCooldown alertCooldowns[ALERT_COOLDOWN_SLOTS];

extern bool firstMpuSample;
extern bool max30102Available;

extern unsigned long alarmStartTime;
extern unsigned long lastHRVCheck;

extern int rmssdLowCount;
extern bool fatigueLevel1Sent;

extern unsigned long ibiBuffer[MAX_IBI_BUFFER];
extern int ibiCount;

extern unsigned long ibiHistory[MEDIAN_WINDOW];
extern int historyIndex;
extern int historyCount;

extern unsigned long fallLatchUntil;

extern SensorData latestData;

extern SemaphoreHandle_t dataMutex;
extern QueueHandle_t alertQueue;
extern QueueHandle_t hrvQueue;
extern QueueHandle_t envQueue;

extern WiFiClientSecure espClient;
extern PubSubClient client;

#endif
