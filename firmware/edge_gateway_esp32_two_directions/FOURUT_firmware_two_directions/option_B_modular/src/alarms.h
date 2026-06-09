
#ifndef FOURUT_ALARMS_H
#define FOURUT_ALARMS_H

void triggerAlarm(int level, const char* message);
void cancelAlarm();
void checkButtonNonBlocking();
void sendSMS(const char* message, bool urgent);

#endif
