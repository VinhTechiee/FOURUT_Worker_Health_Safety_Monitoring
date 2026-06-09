
#ifndef FOURUT_RISK_ASSESSMENT_H
#define FOURUT_RISK_ASSESSMENT_H

#include "types.h"

void assessBiometricRisk(int spo2, int bpm, float svm, float rmssd);
void assessFatigueRisk(float rmssd, int bpm, int spo2, float svm);
void assessEnvironmentalRisk(const SensorData& data);

#endif
