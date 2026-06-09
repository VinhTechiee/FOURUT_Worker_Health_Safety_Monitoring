
#ifndef FOURUT_TYPES_H
#define FOURUT_TYPES_H

#include <Arduino.h>

struct AlertCooldown {
    char category[24];
    char status[32];
    unsigned long lastSent;
};

struct __attribute__((packed)) EnvPacket {
    uint16_t magic;
    uint8_t version;
    uint8_t node_id;
    uint32_t seq;
    float pm1_0;
    float pm2_5;
    float pm10;
    int16_t aqi;
    uint16_t battery_mv;
    uint32_t uptime_ms;
};

struct SensorData {
    int bpm;
    int spo2;

    int aqi;
    float pm25;
    float pm10;
    uint8_t envNodeId;
    uint32_t envSeq;
    uint32_t envLastSeenMs;
    bool envOnline;

    float svm;
    float sdnn;
    float rmssd;
    int ibiCount;
    int alarmLevel;
    bool alarmActive;
    bool fallDetected;
};

struct AlertEvent {
    char category[24];
    char status[32];
    char message[96];
    int level;
    int bpm;
    int spo2;
    int aqi;
    float pm25;
    float pm10;
    float svm;
    float rmssd;
};

#endif
