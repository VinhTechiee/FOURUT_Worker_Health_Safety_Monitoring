
#include <Arduino.h>
#include <math.h>

#include "globals.h"
#include "config.h"
#include "alerts.h"
#include "risk_assessment.h"
#include "hrv.h"

bool isValidIBI(unsigned long ibi) {
    return (ibi >= 300 && ibi <= 1500);
}

unsigned long medianFilter(unsigned long value) {
    ibiHistory[historyIndex] = value;
    historyIndex = (historyIndex + 1) % MEDIAN_WINDOW;

    if (historyCount < MEDIAN_WINDOW) {
        historyCount++;
    }

    unsigned long temp[MEDIAN_WINDOW];

    for (int i = 0; i < historyCount; i++) {
        temp[i] = ibiHistory[i];
    }

    for (int i = 0; i < historyCount - 1; i++) {
        for (int j = 0; j < historyCount - i - 1; j++) {
            if (temp[j] > temp[j + 1]) {
                unsigned long t = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = t;
            }
        }
    }

    return temp[historyCount / 2];
}

float calculateSDNN(unsigned long ibis[], int count) {
    if (count < 2) return -1;

    float sum = 0;

    for (int i = 0; i < count; i++) {
        sum += ibis[i];
    }

    float mean = sum / count;
    float variance = 0;

    for (int i = 0; i < count; i++) {
        float diff = (float)ibis[i] - mean;
        variance += diff * diff;
    }

    return sqrt(variance / count);
}

float calculateRMSSD(unsigned long ibis[], int count) {
    if (count < 3) return -1;

    float sumSq = 0;

    for (int i = 1; i < count; i++) {
        float diff = (float)ibis[i] - (float)ibis[i - 1];
        sumSq += diff * diff;
    }

    return sqrt(sumSq / (count - 1));
}

void updateHRVOnEdge(int bpm, int spo2, float svm) {
    if (ibiCount < MIN_IBI_COUNT) {
        Serial.printf("HRV insufficient data: %d/%d IBI\n", ibiCount, MIN_IBI_COUNT);
        return;
    }

    float sdnn = calculateSDNN(ibiBuffer, ibiCount);
    float rmssd = calculateRMSSD(ibiBuffer, ibiCount);

    Serial.printf("\nHRV SUMMARY\n");
    Serial.printf("SDNN: %.2f ms | RMSSD: %.2f ms | IBI count: %d\n", sdnn, rmssd, ibiCount);

    assessFatigueRisk(rmssd, bpm, spo2, svm);

    SensorData hrvData;
    bool hasData = false;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        latestData.sdnn = sdnn;
        latestData.rmssd = rmssd;
        latestData.ibiCount = ibiCount;

        hrvData = latestData;
        hasData = true;

        xSemaphoreGive(dataMutex);
    }

    if (hasData) {
        xQueueSend(hrvQueue, &hrvData, 0);
        ibiCount = 0;
    } else {
        Serial.println("HRV update skipped: data mutex timeout");
    }
}
