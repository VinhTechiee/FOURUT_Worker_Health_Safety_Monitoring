
#include <Arduino.h>
#include <math.h>

#include "globals.h"
#include "config.h"
#include "alerts.h"
#include "alarms.h"
#include "fall_detection.h"

float accelerationVectorAngleDeg(
    float ax1,
    float ay1,
    float az1,
    float ax2,
    float ay2,
    float az2
) {
    float dot = ax1 * ax2 + ay1 * ay2 + az1 * az2;

    float mag1 = sqrt(ax1 * ax1 + ay1 * ay1 + az1 * az1);
    float mag2 = sqrt(ax2 * ax2 + ay2 * ay2 + az2 * az2);

    if (mag1 < 0.001 || mag2 < 0.001) {
        return 0.0;
    }

    float cosAngle = dot / (mag1 * mag2);

    if (cosAngle > 1.0) {
        cosAngle = 1.0;
    } else if (cosAngle < -1.0) {
        cosAngle = -1.0;
    }

    return acos(cosAngle) * 180.0 / PI;
}

bool assessFallRisk(
    float svm,
    float accelX,
    float accelY,
    float accelZ,
    float gyroMagnitude,
    int bpm,
    int spo2,
    float rmssd
) {
    static unsigned long fallSequenceStart = 0;
    static unsigned long impactStart = 0;
    static unsigned long stillStart = 0;

    static bool inFallSequence = false;
    static bool waitingInactivity = false;

    // Last stable posture before the fall-like event.
    static float stableAccelX = 0.0;
    static float stableAccelY = 0.0;
    static float stableAccelZ = 9.8;

    unsigned long now = millis();

    bool normalGravity = (svm >= STILL_SVM_MIN && svm <= STILL_SVM_MAX);
    bool deviceStill = normalGravity && (gyroMagnitude <= STILL_GYRO_THRESH);

    /*
     * Keep updating the reference posture only during normal/still states.
     * This becomes the "before fall" posture used for comparison.
     */
    if (!inFallSequence && !waitingInactivity && deviceStill) {
        stableAccelX = accelX;
        stableAccelY = accelY;
        stableAccelZ = accelZ;
    }

    /*
     * Stage 1: Free-fall phase.
     * This prevents a single strong movement above 3G from immediately
     * triggering a fall alarm.
     */
    if (!inFallSequence && !waitingInactivity && svm < FREE_FALL_THRESH) {
        inFallSequence = true;
        fallSequenceStart = now;
        Serial.println("FREE FALL PHASE DETECTED");
    }

    /*
     * Stage 2: Impact phase.
     */
    if (inFallSequence && svm > IMPACT_THRESH) {
        inFallSequence = false;
        waitingInactivity = true;
        impactStart = now;
        stillStart = 0;
        Serial.println("IMPACT PHASE DETECTED");
    }

    /*
     * Cancel free-fall sequence if no impact follows quickly.
     */
    if (inFallSequence && (now - fallSequenceStart > FALL_TIME_WINDOW)) {
        inFallSequence = false;
    }

    /*
     * Stage 3: Posture change + real inactivity verification.
     * A fall is confirmed only if:
     * 1. posture has changed significantly, and
     * 2. the device remains almost still for 3 seconds.
     */
    if (waitingInactivity) {
        float postureChangeDeg = accelerationVectorAngleDeg(
            stableAccelX,
            stableAccelY,
            stableAccelZ,
            accelX,
            accelY,
            accelZ
        );

        bool postureChanged = postureChangeDeg >= POSTURE_CHANGE_THRESH_DEG;

        if (postureChanged && deviceStill) {
            if (stillStart == 0) {
                stillStart = now;
                Serial.println("POSTURE CHANGE + STILLNESS DETECTED");
            }

            if (now - stillStart >= INACTIVITY_TIME) {
                waitingInactivity = false;
                stillStart = 0;

                triggerAlarm(3, "EMERGENCY: Worker fall detected!");

                pushAlert(
                    "FALL",
                    "EMERGENCY",
                    3,
                    "Worker fall detected",
                    bpm,
                    spo2,
                    svm,
                    rmssd
                );

                sendSMS("EMERGENCY: Worker fall detected! Immediate assistance needed!", true);
                fallLatchUntil = millis() + FALL_LATCH_MS;
                return true;
            }
        } else {
            stillStart = 0;
        }

        /*
         * If the worker keeps moving after impact, treat it as heavy work
         * or a non-fall event and cancel the sequence.
         */
        if (now - impactStart > FALL_CONFIRM_TIMEOUT_MS) {
            waitingInactivity = false;
            stillStart = 0;
            Serial.println("FALL SEQUENCE CANCELLED: no confirmed inactivity/posture change");
        }
    }

    return false;
}
