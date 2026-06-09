
#include <Arduino.h>

#include "globals.h"
#include "alarms.h"

void triggerAlarm(int level, const char* message) {
    if (level < 3 && millis() < cancelCooldownUntil) {
        return;
    }

    if (alarmActive && level <= currentAlarmLevel) return;

    alarmActive = true;
    alarmCancelled = false;
    alarmStartTime = millis();
    currentAlarmLevel = level;

    digitalWrite(BUZZER_PIN, HIGH);

    Serial.printf("\nALARM TRIGGERED - Level %d: %s\n", level, message);
}

void cancelAlarm() {
    if (!alarmActive) return;

    int cancelledLevel = currentAlarmLevel;

    alarmActive = false;
    alarmCancelled = true;
    currentAlarmLevel = 0;

    if (cancelledLevel < 3) {
        cancelCooldownUntil = millis() + CANCEL_COOLDOWN_MS;
    } else {
        cancelCooldownUntil = 0;
    }

    digitalWrite(BUZZER_PIN, LOW);

    Serial.println("Alarm cancelled by user");
}

void checkButtonNonBlocking() {
    static bool lastButtonState = HIGH;
    static unsigned long lastDebounceTime = 0;

    bool reading = digitalRead(CANCEL_BTN_PIN);

    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > 50) {
        if (reading == LOW && alarmActive) {
            cancelAlarm();
        }
    }

    lastButtonState = reading;
}

void sendSMS(const char* message, bool urgent) {
    Serial.printf("SMS simulated (%s): %s\n", urgent ? "URGENT" : "Normal", message);
}
