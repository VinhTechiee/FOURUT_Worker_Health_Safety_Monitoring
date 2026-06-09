
#ifndef FOURUT_ALERTS_H
#define FOURUT_ALERTS_H

#include "types.h"

void updateLatestData(int bpm, int spo2, int aqi, float svm, float sdnn, float rmssd, int localIbiCount, bool fallDetected);

bool shouldSendAlert(const char* category, const char* status, int level);

void pushAlert(
    const char* category,
    const char* status,
    int level,
    const char* message,
    int bpm,
    int spo2,
    float svm,
    float rmssd,
    int aqi = -1,
    float pm25 = -1,
    float pm10 = -1
);

#endif
