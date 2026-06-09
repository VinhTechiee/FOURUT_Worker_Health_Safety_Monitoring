
#include <Arduino.h>
#include <string.h>

#include "globals.h"
#include "alerts.h"

void updateLatestData(int bpm, int spo2, int aqi, float svm, float sdnn, float rmssd, int localIbiCount, bool fallDetected) {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        latestData.bpm = bpm;
        latestData.spo2 = spo2;
        latestData.svm = svm;

        if (aqi >= 0) {
            latestData.aqi = aqi;
        }

        if (sdnn >= 0) {
            latestData.sdnn = sdnn;
        }

        if (rmssd >= 0) {
            latestData.rmssd = rmssd;
        }

        if (localIbiCount >= 0) {
            latestData.ibiCount = localIbiCount;
        }

        latestData.alarmLevel = currentAlarmLevel;
        latestData.alarmActive = alarmActive;
        latestData.fallDetected = fallDetected;

        xSemaphoreGive(dataMutex);
    }
}

bool shouldSendAlert(const char* category, const char* status, int level) {
    unsigned long now = millis();

    for (int i = 0; i < ALERT_COOLDOWN_SLOTS; i++) {
        if (
            strcmp(alertCooldowns[i].category, category) == 0 &&
            strcmp(alertCooldowns[i].status, status) == 0
        ) {
            if (now - alertCooldowns[i].lastSent < ALERT_COOLDOWN_MS) {
                return false;
            }

            alertCooldowns[i].lastSent = now;
            return true;
        }
    }

    for (int i = 0; i < ALERT_COOLDOWN_SLOTS; i++) {
        if (alertCooldowns[i].category[0] == '\0') {
            strncpy(alertCooldowns[i].category, category, sizeof(alertCooldowns[i].category) - 1);
            alertCooldowns[i].category[sizeof(alertCooldowns[i].category) - 1] = '\0';

            strncpy(alertCooldowns[i].status, status, sizeof(alertCooldowns[i].status) - 1);
            alertCooldowns[i].status[sizeof(alertCooldowns[i].status) - 1] = '\0';

            alertCooldowns[i].lastSent = now;
            return true;
        }
    }

    strncpy(alertCooldowns[0].category, category, sizeof(alertCooldowns[0].category) - 1);
    alertCooldowns[0].category[sizeof(alertCooldowns[0].category) - 1] = '\0';

    strncpy(alertCooldowns[0].status, status, sizeof(alertCooldowns[0].status) - 1);
    alertCooldowns[0].status[sizeof(alertCooldowns[0].status) - 1] = '\0';

    alertCooldowns[0].lastSent = now;
    return true;
}

void pushAlert(
    const char* category,
    const char* status,
    int level,
    const char* message,
    int bpm,
    int spo2,
    float svm,
    float rmssd,
    int aqi,
    float pm25,
    float pm10
) {
    if (!shouldSendAlert(category, status, level)) {
        return;
    }

    AlertEvent event;

    strncpy(event.category, category, sizeof(event.category) - 1);
    event.category[sizeof(event.category) - 1] = '\0';

    strncpy(event.status, status, sizeof(event.status) - 1);
    event.status[sizeof(event.status) - 1] = '\0';

    strncpy(event.message, message, sizeof(event.message) - 1);
    event.message[sizeof(event.message) - 1] = '\0';

    event.level = level;
    event.bpm = bpm;
    event.spo2 = spo2;
    event.svm = svm;
    event.rmssd = rmssd;
    event.aqi = aqi;
    event.pm25 = pm25;
    event.pm10 = pm10;

    xQueueSend(alertQueue, &event, 0);
}
