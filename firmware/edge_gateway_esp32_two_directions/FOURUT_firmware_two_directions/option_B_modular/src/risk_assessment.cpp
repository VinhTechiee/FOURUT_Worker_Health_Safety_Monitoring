
#include <Arduino.h>

#include "config.h"
#include "globals.h"
#include "types.h"
#include "alerts.h"
#include "alarms.h"
#include "risk_assessment.h"

void assessBiometricRisk(int spo2, int bpm, float svm, float rmssd) {
    if (spo2 < SPO2_CRITICAL && spo2 > 0) {
        triggerAlarm(2, "CRITICAL: Severe SpO2 drop detected!");

        pushAlert(
            "BIOMETRIC",
            "CRITICAL_SPO2",
            2,
            "Severe SpO2 drop detected",
            bpm,
            spo2,
            svm,
            rmssd
        );

        sendSMS("EMERGENCY: Worker has critical low SpO2!", true);
    }
    else if (spo2 < SPO2_WARNING && spo2 > 0) {
        pushAlert(
            "BIOMETRIC",
            "WARNING_SPO2",
            1,
            "Mild hypoxemia detected",
            bpm,
            spo2,
            svm,
            rmssd
        );
    }

    if (bpm < BPM_BRADY && bpm > 0) {
        triggerAlarm(1, "WARNING: Bradycardia detected!");

        pushAlert(
            "BIOMETRIC",
            "BRADYCARDIA",
            1,
            "Heart rate too low",
            bpm,
            spo2,
            svm,
            rmssd
        );
    }
    else if (bpm > BPM_TACHY && bpm > 0) {
        triggerAlarm(1, "WARNING: Tachycardia detected!");

        pushAlert(
            "BIOMETRIC",
            "TACHYCARDIA",
            1,
            "Heart rate too high",
            bpm,
            spo2,
            svm,
            rmssd
        );
    }
}

void assessFatigueRisk(float rmssd, int bpm, int spo2, float svm) {
    if (rmssd >= 0 && rmssd < RMSSD_EXHAUSTION_LEVEL_2 && bpm > BPM_EXHAUSTION_THRESHOLD) {
        rmssdLowCount = 0;
        fatigueLevel1Sent = false;

        triggerAlarm(2, "FATIGUE LEVEL 2: Worker exhaustion detected!");

        pushAlert(
            "FATIGUE",
            "LEVEL_2_EXHAUSTION",
            2,
            "Worker exhaustion detected",
            bpm,
            spo2,
            svm,
            rmssd
        );

        sendSMS("Worker exhaustion detected. Please check on worker.", false);

        return;
    }

    if (rmssd >= 0 && rmssd < RMSSD_FATIGUE_LEVEL_1) {
        rmssdLowCount++;

        Serial.printf("Low RMSSD: %.1f ms | window %d/3\n", rmssd, rmssdLowCount);

        if (rmssdLowCount >= 3 && !fatigueLevel1Sent) {
            fatigueLevel1Sent = true;

            triggerAlarm(1, "FATIGUE LEVEL 1: Worker showing signs of fatigue");

            pushAlert(
                "FATIGUE",
                "LEVEL_1_WARNING",
                1,
                "RMSSD below 20 ms for 3 consecutive windows",
                bpm,
                spo2,
                svm,
                rmssd
            );
        }
    }
    else {
        rmssdLowCount = 0;
        fatigueLevel1Sent = false;
    }
}

void assessEnvironmentalRisk(const SensorData& data) {
    if (!data.envOnline) {
        return;
    }

    if (data.aqi > AQI_DANGER_LEVEL) {
        triggerAlarm(2, "DANGER: Poor air quality detected!");

        pushAlert(
            "ENVIRONMENT",
            "AQI_DANGER",
            2,
            "Dangerous air quality detected",
            data.bpm,
            data.spo2,
            data.svm,
            data.rmssd,
            data.aqi,
            data.pm25,
            data.pm10
        );
    }
    else if (data.aqi > AQI_WARNING_LEVEL) {
        pushAlert(
            "ENVIRONMENT",
            "AQI_WARNING",
            1,
            "Poor air quality warning",
            data.bpm,
            data.spo2,
            data.svm,
            data.rmssd,
            data.aqi,
            data.pm25,
            data.pm10
        );
    }
}
