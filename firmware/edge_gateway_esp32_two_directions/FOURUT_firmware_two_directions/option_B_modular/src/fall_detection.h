
#ifndef FOURUT_FALL_DETECTION_H
#define FOURUT_FALL_DETECTION_H

float accelerationVectorAngleDeg(
    float ax1,
    float ay1,
    float az1,
    float ax2,
    float ay2,
    float az2
);

bool assessFallRisk(
    float svm,
    float accelX,
    float accelY,
    float accelZ,
    float gyroMagnitude,
    int bpm,
    int spo2,
    float rmssd
);

#endif
