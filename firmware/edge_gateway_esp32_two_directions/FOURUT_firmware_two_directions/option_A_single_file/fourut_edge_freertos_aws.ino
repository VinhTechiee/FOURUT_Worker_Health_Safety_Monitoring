/**
 * ============================================================
 * FOURUT - Health & Safety Monitoring System
 * Board  : XIAO ESP32-C3
 * Sensors: MAX30102 (SpO2 + HR), MPU6050 (IMU)
 * Features: Fall detection, HRV/Fatigue detection, Buzzer alarm, Cancel button
 * Style   : Single-file firmware; credentials isolated in secrets.h
 * ============================================================
 */

#include <Wire.h>
#include "DFRobot_BloodOxygen_S.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "secrets.h"

#if __has_include("esp_arduino_version.h")
#include "esp_arduino_version.h"
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"


// ======================
// Wi-Fi & AWS IoT CONFIG
// ======================
//
// Keep this sketch as a single-file firmware for readability and
// demonstration, but load all sensitive credentials from secrets.h.
//
// Setup:
//   1. Copy secrets.example.h to secrets.h
//   2. Fill in local Wi-Fi and AWS IoT values
//   3. Never commit secrets.h

const char* ssid        = WIFI_SSID;
const char* password    = WIFI_PASSWORD;

const char* mqtt_server = AWS_IOT_ENDPOINT;
const int mqtt_port     = 8883;

const char* topicTelemetry = "worker/W001/telemetry";
const char* topicHRV       = "worker/W001/hrv";
const char* topicAlert     = "worker/W001/alert";

const char* rootCA     = AWS_CERT_CA;
const char* clientCert = AWS_CERT_CRT;
const char* privateKey = AWS_CERT_PRIVATE;

// ============================================================
// HARDWARE PINS & SENSORS
// ============================================================
#define I2C_SDA         6
#define I2C_SCL         7
#define BUZZER_PIN      2      // GPIO2 (D0) - Active buzzer
#define CANCEL_BTN_PIN  3      // GPIO3 (D1) - Cancel button
#define MAX30102_ADDR   0x57

DFRobot_BloodOxygen_S_I2C max30102(&Wire, MAX30102_ADDR);
Adafruit_MPU6050 mpu;


// ============================================================
// TASK PRIORITIES
// ============================================================
#define PRIORITY_ANOMALY   5
#define PRIORITY_ALARM     4
#define PRIORITY_ENV       3
#define PRIORITY_CLOUD     2
#define PRIORITY_DEBUG     1


// ============================================================
// THRESHOLDS
// ============================================================
#define SPO2_WARNING    90
#define SPO2_CRITICAL   80

#define BPM_BRADY       50
#define BPM_TACHY       120

#define FREE_FALL_THRESH 4.9    
#define IMPACT_THRESH    29.4   
#define FALL_TIME_WINDOW 1500   
#define INACTIVITY_TIME  3000   

// Fall confirmation thresholds
// MPU6050 gyro values from Adafruit library are in rad/s.
#define STILL_SVM_MIN              7.0
#define STILL_SVM_MAX              12.5
#define STILL_GYRO_THRESH          0.35
#define POSTURE_CHANGE_THRESH_DEG  35.0
#define FALL_CONFIRM_TIMEOUT_MS    6000

#define RMSSD_FATIGUE_LEVEL_1   20.0
#define RMSSD_EXHAUSTION_LEVEL_2 15.0
#define BPM_EXHAUSTION_THRESHOLD 110

#define AQI_WARNING_LEVEL 75
#define AQI_DANGER_LEVEL 150

const unsigned long ENV_NODE_TIMEOUT_MS = 15000;

// ============================================================
// HRV CONFIG
// ============================================================
#define HRV_WINDOW_MS      60000
#define MIN_IBI_COUNT      10
#define MAX_IBI_BUFFER     500
#define MEDIAN_WINDOW      5


// ============================================================
// TIMING
// ============================================================
const unsigned long SENSOR_READ_INTERVAL_MS  = 1000;
const unsigned long MAX30102_READ_INTERVAL_MS = 1000;
const unsigned long MPU_READ_INTERVAL_MS = 20;
const unsigned long CLOUD_PUBLISH_INTERVAL_MS = 5000;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
const unsigned long ALERT_COOLDOWN_MS = 30000;
const unsigned long CANCEL_COOLDOWN_MS = 30000;

// ============================================================
// GLOBAL VARIABLES
// ============================================================
volatile bool alarmActive = false;
volatile bool alarmCancelled = false;
volatile int currentAlarmLevel = 0;

unsigned long cancelCooldownUntil = 0;

#define ALERT_COOLDOWN_SLOTS 8

struct AlertCooldown {
    char category[24];
    char status[32];
    unsigned long lastSent;
};

AlertCooldown alertCooldowns[ALERT_COOLDOWN_SLOTS];

bool firstMpuSample = true;
bool max30102Available = false;

unsigned long alarmStartTime = 0;
unsigned long lastHRVCheck = 0;

int rmssdLowCount = 0;
bool fatigueLevel1Sent = false;

unsigned long ibiBuffer[MAX_IBI_BUFFER];
int ibiCount = 0;

unsigned long ibiHistory[MEDIAN_WINDOW];
int historyIndex = 0;
int historyCount = 0;

unsigned long fallLatchUntil = 0;
const unsigned long FALL_LATCH_MS = 30000;

// ============================================================
// ESP-NOW packet
// ============================================================
#define ENV_PACKET_MAGIC 0x4655
#define ENV_PACKET_VERSION 1

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

// ============================================================
// SHARED DATA STRUCTURES
// ============================================================
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
SensorData latestData;

SemaphoreHandle_t dataMutex;
QueueHandle_t alertQueue;
QueueHandle_t hrvQueue;
QueueHandle_t envQueue;

// ======================
// MQTT CLIENT
// ======================
WiFiClientSecure espClient;
PubSubClient client(espClient);


// ============================================================
// HELPER FUNCTIONS
// ============================================================
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
    int aqi = -1,
    float pm25 = -1,
    float pm10 = -1
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

// ============================================================
// EDGE ALARM FUNCTIONS
// ============================================================
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


// ============================================================
// EDGE RISK ASSESSMENT 
// ============================================================
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

// ============================================================
// WIFI / MQTT FUNCTIONS
// ============================================================
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len)
#else
void onEspNowRecv(const uint8_t* mac, const uint8_t* data, int len)
#endif
{
    if (len != sizeof(EnvPacket)) {
        return;
    }

    EnvPacket packet;
    memcpy(&packet, data, sizeof(packet));

    if (packet.magic != ENV_PACKET_MAGIC) {
        return;
    }

    if (packet.version != ENV_PACKET_VERSION) {
        return;
    }

    if (envQueue != NULL) {
        if (xQueueSend(envQueue, &packet, 0) != pdTRUE) {
            // Queue full, packet dropped
        }
    }
}

void initEspNowGateway() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return;
    }

    esp_now_register_recv_cb(onEspNowRecv);

    uint8_t mac[6];
    WiFi.macAddress(mac);

    Serial.printf(
        "ESP-NOW gateway ready. XIAO MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );
}

void connectWiFiNonBlocking() {
    static unsigned long lastWiFiAttempt = 0;

    if (WiFi.status() == WL_CONNECTED) return;

    unsigned long now = millis();

    if (now - lastWiFiAttempt >= 5000) {
        lastWiFiAttempt = now;
        Serial.println("Trying Wi-Fi reconnect...");
        WiFi.disconnect(false);
        WiFi.begin(ssid, password);
    }
}

// AWS IoT TLS needs the ESP32 clock to be valid.
// Without NTP, the board often starts at year 1970 and the certificate is rejected.
bool syncTimeNonBlocking() {
    static bool ntpStarted = false;
    static bool timeSynced = false;
    static unsigned long lastPrint = 0;

    if (timeSynced) return true;

    if (!ntpStarted) {
        ntpStarted = true;
        configTime(0, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");
        Serial.println("Starting NTP time sync...");
    }

    time_t now = time(nullptr);

    // 1700000000 = 2023-11-14. This confirms time is no longer 1970.
    if (now > 1700000000) {
        timeSynced = true;
        Serial.print("NTP time synced. Epoch: ");
        Serial.println((long)now);
        return true;
    }

    if (millis() - lastPrint >= 2000) {
        lastPrint = millis();
        Serial.println("Waiting for NTP time sync before MQTT...");
    }

    return false;
}

void connectMQTTNonBlocking() {
    static unsigned long lastMQTTAttempt = 0;

    if (client.connected()) return;

    // Do not try MQTT until Wi-Fi and NTP time are ready.
    if (WiFi.status() != WL_CONNECTED) return;
    if (!syncTimeNonBlocking()) return;

    unsigned long now = millis();

    if (now - lastMQTTAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
        lastMQTTAttempt = now;

        Serial.println("Trying MQTT reconnect...");
        Serial.print("Endpoint: ");
        Serial.println(mqtt_server);
        Serial.print("Wi-Fi RSSI: ");
        Serial.println(WiFi.RSSI());
        Serial.print("Free heap: ");
        Serial.println(ESP.getFreeHeap());

        if (client.connect("W001")) {
            Serial.println("Connected to AWS IoT Core");
        } else {
            Serial.print("MQTT reconnect failed, rc=");
            Serial.println(client.state());

            char errBuf[128];
            espClient.lastError(errBuf, sizeof(errBuf));
            Serial.print("TLS last error: ");
            Serial.println(errBuf);
        }
    }
}

void publishTelemetry(const SensorData& data) {
    StaticJsonDocument<1024> doc;

    doc["worker_id"] = "W001";
    doc["heart_rate"] = data.bpm;
    doc["spo2"] = data.spo2;
    doc["aqi"] = data.aqi;
    doc["pm25_ugm3"] = data.pm25;
    doc["pm10_ugm3"] = data.pm10;
    doc["env_node_id"] = data.envNodeId;
    doc["env_seq"] = data.envSeq;
    doc["env_online"] = data.envOnline;

    if (data.envLastSeenMs > 0) {
        doc["env_age_ms"] = millis() - data.envLastSeenMs;
    } else {
        doc["env_age_ms"] = -1;
    }

    doc["svm"] = data.svm;
    doc["sdnn"] = data.sdnn;
    doc["rmssd"] = data.rmssd;
    doc["ibi_count"] = data.ibiCount;
    doc["alarm_active"] = data.alarmActive;
    doc["alarm_level"] = data.alarmLevel;
    doc["fall_detected"] = data.fallDetected;

    char payload[1024];
    serializeJson(doc, payload, sizeof(payload));

    bool ok = client.publish(topicTelemetry, payload);

    if (ok) {
        Serial.println("Telemetry published");
    } else {
        Serial.println("Telemetry publish failed");
    }
}

void publishHRVSummary(const SensorData& data) {
    StaticJsonDocument<256> doc;

    doc["worker_id"] = "W001";
    doc["sdnn"] = data.sdnn;
    doc["rmssd"] = data.rmssd;
    doc["ibi_count"] = data.ibiCount;
    doc["heart_rate"] = data.bpm;
    doc["alarm_level"] = data.alarmLevel;

    char payload[256];
    serializeJson(doc, payload, sizeof(payload));

    bool ok = client.publish(topicHRV, payload);

    if (ok) {
        Serial.println("HRV summary published");
    } else {
        Serial.println("HRV summary publish failed");
    }
}

void publishAlertEvent(const AlertEvent& event) {
    StaticJsonDocument<512> doc;

    doc["worker_id"] = "W001";
    doc["category"] = event.category;
    doc["status"] = event.status;
    doc["alert_level"] = event.level;
    doc["alarm_level"] = event.level;
    doc["message"] = event.message;
    doc["heart_rate"] = event.bpm;
    doc["spo2"] = event.spo2;
    doc["aqi"] = event.aqi;
    doc["pm25_ugm3"] = event.pm25;
    doc["pm10_ugm3"] = event.pm10;
    doc["svm"] = event.svm;
    doc["rmssd"] = event.rmssd;

    char payload[512];
    serializeJson(doc, payload, sizeof(payload));

    bool ok = client.publish(topicAlert, payload);

    if (ok) {
        Serial.println("Alert event published");
    } else {
        Serial.println("Alert event publish failed");
    }
}


// ============================================================
// TASK 1: HIGHEST PRIORITY - ANOMALY DETECTION
// ============================================================
void Task_AnomalyDetection(void* pvParameters) {
    unsigned long lastBiometricRead = 0;
    unsigned long lastMpuRead = 0;

    int currentBPM = -1;
    int currentSpO2 = -1;

    float currentSVM = 9.8;
    float currentRMSSD = -1;

    bool currentFallDetected = false;

    while (true) {
        unsigned long now = millis();

        /*
        * BLOCK 1: Reads MPU6050 quickly every 20ms
        * Used for fall detection.
         */
        if (now - lastMpuRead >= MPU_READ_INTERVAL_MS) {
            lastMpuRead = now;

            sensors_event_t accel, gyro, temp;
            mpu.getEvent(&accel, &gyro, &temp);

            currentSVM = sqrt(
                accel.acceleration.x * accel.acceleration.x +
                accel.acceleration.y * accel.acceleration.y +
                accel.acceleration.z * accel.acceleration.z
            );

            if (firstMpuSample) {
                firstMpuSample = false;
                currentSVM = 9.8;
            }

            float gyroMagnitude = sqrt(
                gyro.gyro.x * gyro.gyro.x +
                gyro.gyro.y * gyro.gyro.y +
                gyro.gyro.z * gyro.gyro.z
            );

            bool newFall = assessFallRisk(
                currentSVM,
                accel.acceleration.x,
                accel.acceleration.y,
                accel.acceleration.z,
                gyroMagnitude,
                currentBPM,
                currentSpO2,
                currentRMSSD
            );

            currentFallDetected = newFall || (millis() < fallLatchUntil);

            updateLatestData(
                currentBPM,
                currentSpO2,
                -1,
                currentSVM,
                -1,              
                -1,              
                ibiCount,
                currentFallDetected
            );
        }

        /*
         * BLOCK 2: Reads MAX30102 more slowly, every 1000ms
         * Used for BPM, SpO2, IBI.
         */
        
        if (now - lastBiometricRead >= MAX30102_READ_INTERVAL_MS) {
            lastBiometricRead = now;

            if (!max30102Available) {
                currentBPM = -1;
                currentSpO2 = -1;

                updateLatestData(
                    currentBPM,
                    currentSpO2,
                    -1,
                    currentSVM,
                    -1,
                    -1,
                    ibiCount,
                    currentFallDetected
                );

                Serial.printf(
                    "EDGE | MAX30102: OFFLINE | SVM:%.2f Alarm:%d\n",
                    currentSVM,
                    currentAlarmLevel
                );
            } else {
                max30102.getHeartbeatSPO2();

                int readSpO2 = max30102._sHeartbeatSPO2.SPO2;
                int readBPM  = max30102._sHeartbeatSPO2.Heartbeat;

                if (readSpO2 >= 70 && readSpO2 <= 100) {
                    currentSpO2 = readSpO2;
                } else {
                    currentSpO2 = -1;
                }

                if (readBPM >= 40 && readBPM <= 200) {
                    currentBPM = readBPM;

                    unsigned long currentIBI = 60000UL / currentBPM;

                    if (isValidIBI(currentIBI)) {
                        unsigned long filteredIBI = medianFilter(currentIBI);

                        if (filteredIBI > 0 && ibiCount < MAX_IBI_BUFFER) {
                            ibiBuffer[ibiCount++] = filteredIBI;
                        } 
                    }
                } else {
                    currentBPM = -1;
                }

        /*
         * Get the latest RMSSD if HRV has been calculated previously.
         */
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            currentRMSSD = latestData.rmssd;
            xSemaphoreGive(dataMutex);
        }

        assessBiometricRisk(
            currentSpO2,
            currentBPM,
            currentSVM,
            currentRMSSD
        );

        updateLatestData(
            currentBPM,
            currentSpO2,
            -1,
            currentSVM,
            -1,
            -1,
            ibiCount,
            currentFallDetected
        );

        int debugAQI = -1;
        float debugPM25 = -1;
        bool debugEnvOnline = false;

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            debugAQI = latestData.aqi;
            debugPM25 = latestData.pm25;
            debugEnvOnline = latestData.envOnline;
            xSemaphoreGive(dataMutex);
        }

        Serial.printf(
            "EDGE | HR:%d SpO2:%d AQI:%d PM2.5:%.1f ENV:%s SVM:%.2f Alarm:%d\n",
            currentBPM,
            currentSpO2,
            debugAQI,
            debugPM25,
            debugEnvOnline ? "ON" : "OFF",
            currentSVM,
            currentAlarmLevel
        );
    }
}

        /*
        * BLOCK 3: Calculate HRV according to the HRV_WINDOW_MS time window.
         */
        if (now - lastHRVCheck >= HRV_WINDOW_MS) {
            lastHRVCheck = now;

            SensorData snapshot;
            bool hasSnapshot = false;

            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                snapshot = latestData;
                hasSnapshot = true;
                xSemaphoreGive(dataMutex);
            }

            if (hasSnapshot) {
                updateHRVOnEdge(snapshot.bpm, snapshot.spo2, snapshot.svm);
            } else {
                Serial.println("HRV skipped: data mutex timeout");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void Task_EnvReceiver(void* pvParameters) {
    EnvPacket packet;

    while (true) {
        if (xQueueReceive(envQueue, &packet, pdMS_TO_TICKS(1000)) == pdTRUE) {
            SensorData snapshot;
            bool hasSnapshot = false;

            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                latestData.aqi = packet.aqi;
                latestData.pm25 = packet.pm2_5;
                latestData.pm10 = packet.pm10;
                latestData.envNodeId = packet.node_id;
                latestData.envSeq = packet.seq;
                latestData.envLastSeenMs = millis();
                latestData.envOnline = true;

                snapshot = latestData;
                hasSnapshot = true;

                xSemaphoreGive(dataMutex);
            }

            Serial.printf(
                "ENV | node:%u seq:%lu PM2.5:%.1f PM10:%.1f AQI:%d\n",
                packet.node_id,
                (unsigned long)packet.seq,
                packet.pm2_5,
                packet.pm10,
                packet.aqi
            );

            if (hasSnapshot) {
                assessEnvironmentalRisk(snapshot);
            }
        }

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (
                latestData.envOnline &&
                latestData.envLastSeenMs > 0 &&
                millis() - latestData.envLastSeenMs > ENV_NODE_TIMEOUT_MS
            ) {
                latestData.envOnline = false;
                Serial.println("ENV node offline");
            }

            xSemaphoreGive(dataMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================
// TASK 2: HIGH PRIORITY - LOCAL ALARM CONTROL
// ============================================================
void Task_AlarmControl(void* pvParameters) {
    while (true) {
        checkButtonNonBlocking();

        if (alarmActive) {
            digitalWrite(BUZZER_PIN, HIGH);
        } else {
            digitalWrite(BUZZER_PIN, LOW);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============================================================
// TASK 3: MEDIUM PRIORITY - CLOUD PUBLISH
// ============================================================
void printGatewayWiFiInfoOnce() {
    static bool printed = false;

    if (printed) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        printed = true;

        uint8_t mac[6];
        WiFi.macAddress(mac);

        Serial.println("\n========== GATEWAY INFO ==========");
        Serial.printf(
            "Gateway MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
        );
        Serial.printf("Wi-Fi channel: %d\n", WiFi.channel());
        Serial.println("Use this MAC and channel in APM10 ESP-NOW sender.");
        Serial.println("==================================\n");
    }
}

void Task_CloudPublish(void* pvParameters) {
    unsigned long lastPublish = 0;

    while (true) {
        connectWiFiNonBlocking();

        if (WiFi.status() == WL_CONNECTED) {
            printGatewayWiFiInfoOnce();
            connectMQTTNonBlocking();

            if (client.connected()) {
                client.loop();

                AlertEvent alertEvent;

                while (xQueueReceive(alertQueue, &alertEvent, 0) == pdTRUE) {
                    publishAlertEvent(alertEvent);
                }

                SensorData hrvData;

                while (xQueueReceive(hrvQueue, &hrvData, 0) == pdTRUE) {
                    publishHRVSummary(hrvData);
                }

                unsigned long now = millis();

                if (now - lastPublish >= CLOUD_PUBLISH_INTERVAL_MS) {
                    lastPublish = now;

                    SensorData snapshot;

                    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        snapshot = latestData;
                        xSemaphoreGive(dataMutex);

                        publishTelemetry(snapshot);
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    WiFi.mode(WIFI_STA);
    delay(100);

    Serial.print("XIAO Gateway MAC: ");
    Serial.println(WiFi.macAddress());

    Serial.print("Wi-Fi channel: ");
    Serial.println(WiFi.channel());

    Serial.println("\n========================================");
    Serial.println("FOURUT - Edge-first Health & Safety");
    Serial.println("========================================\n");

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(CANCEL_BTN_PIN, INPUT_PULLUP);
    digitalWrite(BUZZER_PIN, LOW);

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);

    Serial.println("Initializing MAX30102...");

    if (max30102.begin() == true) {
        Serial.println("MAX30102 init success");
        max30102.sensorStartCollect();
        max30102Available = true;
    } else {
        Serial.println("MAX30102 init failed - system will continue without HR/SpO2");
        max30102Available = false;
    }

    Serial.println("Initializing MPU6050...");
    if (!mpu.begin()) {
        Serial.println("Failed to find MPU6050 chip");
        while (1) {
            delay(10);
        }
    }

    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);

    dataMutex = xSemaphoreCreateMutex();
    alertQueue = xQueueCreate(10, sizeof(AlertEvent));
    hrvQueue = xQueueCreate(5, sizeof(SensorData));
    envQueue = xQueueCreate(10, sizeof(EnvPacket));

    if (dataMutex == NULL || alertQueue == NULL || hrvQueue == NULL || envQueue == NULL) {
    Serial.println("FreeRTOS object creation failed");
        while (1) {
            delay(1000);
        }
    }

    latestData.bpm = -1;
    latestData.spo2 = -1;
    latestData.aqi = -1;
    latestData.svm = 9.8;
    latestData.sdnn = -1;
    latestData.rmssd = -1;
    latestData.ibiCount = 0;
    latestData.alarmLevel = 0;
    latestData.alarmActive = false;
    latestData.fallDetected = false;
    latestData.pm25 = -1;
    latestData.pm10 = -1;
    latestData.envNodeId = 0;
    latestData.envSeq = 0;
    latestData.envLastSeenMs = 0;
    latestData.envOnline = false;

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.begin(ssid, password);

    initEspNowGateway();

    espClient.setCACert(rootCA);
    espClient.setCertificate(clientCert);
    espClient.setPrivateKey(privateKey);

    client.setBufferSize(1024);
    client.setSocketTimeout(30);   // AWS TLS/MQTT handshake may need longer than default
    client.setKeepAlive(60);
    client.setServer(mqtt_server, mqtt_port);

    xTaskCreate(
        Task_AnomalyDetection,
        "AnomalyDetection",
        8192,
        NULL,
        PRIORITY_ANOMALY,
        NULL
    );

    xTaskCreate(
        Task_AlarmControl,
        "AlarmControl",
        2048,
        NULL,
        PRIORITY_ALARM,
        NULL
    );

    xTaskCreate(
    Task_EnvReceiver,
    "EnvReceiver",
    4096,
    NULL,
    PRIORITY_ENV,
    NULL
    );

    xTaskCreate(
        Task_CloudPublish,
        "CloudPublish",
        8192,
        NULL,
        PRIORITY_CLOUD,
        NULL
    );

    Serial.println("FreeRTOS tasks started");
}

// ============================================================
// LOOP EMPTY
// ============================================================
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}