/**
 * ============================================================
 * FOURUT - Health & Safety Monitoring System
 * Board  : XIAO ESP32-C3
 * Sensors: MAX30102 (SpO2 + HR), MPU6050 (IMU)
 * Features: Fall detection, HRV/Fatigue detection, Buzzer alarm, Cancel button
 * ============================================================
 */

#include <Wire.h>
#include "DFRobot_BloodOxygen_S.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"


// ======================
// Wi-Fi & AWS IoT CONFIG
// ======================
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

const char* mqtt_server = "au08zn44zsegx-ats.iot.ap-southeast-1.amazonaws.com"; 
const int mqtt_port = 8883;

const char* topicTelemetry = "worker/W001/telemetry";
const char* topicHRV       = "worker/W001/hrv";
const char* topicAlert     = "worker/W001/alert";

// Certs
const char* rootCA = R"EOF(
PASTE_ROOT_CA_HERE
)EOF";

const char* clientCert = R"EOF(
PASTE_CLIENT_CERT_HERE
)EOF";

const char* privateKey = R"EOF(
PASTE_PRIVATE_KEY_HERE
)EOF";


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

#define RMSSD_FATIGUE_LEVEL_1   20.0
#define RMSSD_EXHAUSTION_LEVEL_2 15.0
#define BPM_EXHAUSTION_THRESHOLD 110

// ============================================================
// HRV CONFIG
// ============================================================
#define HRV_WINDOW_MS      300000
#define MIN_IBI_COUNT      10
#define MAX_IBI_BUFFER     500
#define MEDIAN_WINDOW      5


// ============================================================
// TIMING
// ============================================================
const unsigned long SENSOR_READ_INTERVAL_MS  = 1000;
const unsigned long CLOUD_PUBLISH_INTERVAL_MS = 5000;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;

// ============================================================
// GLOBAL VARIABLES
// ============================================================
volatile bool alarmActive = false;
volatile bool alarmCancelled = false;
volatile int currentAlarmLevel = 0;

bool firstMpuSample = true;

unsigned long alarmStartTime = 0;
unsigned long lastHRVCheck = 0;

int rmssdLowCount = 0;
bool fatigueLevel1Sent = false;

unsigned long ibiBuffer[MAX_IBI_BUFFER];
int ibiCount = 0;

unsigned long ibiHistory[MEDIAN_WINDOW];
int historyIndex = 0;
int historyCount = 0;


// ============================================================
// SHARED DATA STRUCTURES
// ============================================================
struct SensorData {
    int bpm;
    int spo2;
    int aqi;
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
    float svm;
    float rmssd;
};

SensorData latestData;

SemaphoreHandle_t dataMutex;
QueueHandle_t alertQueue;
QueueHandle_t hrvQueue;


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
        latestData.aqi = aqi;
        latestData.svm = svm;
        latestData.sdnn = sdnn;
        latestData.rmssd = rmssd;
        latestData.ibiCount = localIbiCount;
        latestData.alarmLevel = currentAlarmLevel;
        latestData.alarmActive = alarmActive;
        latestData.fallDetected = fallDetected;

        xSemaphoreGive(dataMutex);
    }
}


void pushAlert(const char* category, const char* status, int level, const char* message, int bpm, int spo2, float svm, float rmssd) {
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

    xQueueSend(alertQueue, &event, 0);
}

// ============================================================
// EDGE ALARM FUNCTIONS
// ============================================================
void triggerAlarm(int level, const char* message) {
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

    alarmActive = false;
    alarmCancelled = true;
    currentAlarmLevel = 0;

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

bool assessFallRisk(float svm, int bpm, int spo2, float rmssd) {
    static unsigned long fallSequenceStart = 0;
    static unsigned long inactivityStart = 0;
    static bool inFallSequence = false;
    static bool waitingInactivity = false;

    unsigned long now = millis();

    if (!inFallSequence && !waitingInactivity && svm < FREE_FALL_THRESH) {
        inFallSequence = true;
        fallSequenceStart = now;
        Serial.println("FREE FALL PHASE DETECTED");
    }

    if (inFallSequence && svm > IMPACT_THRESH) {
        inFallSequence = false;
        waitingInactivity = true;
        inactivityStart = now;
        Serial.println("IMPACT PHASE DETECTED");
    }

    if (inFallSequence && (now - fallSequenceStart > FALL_TIME_WINDOW)) {
        inFallSequence = false;
    }

    if (waitingInactivity && (now - inactivityStart >= INACTIVITY_TIME)) {
        waitingInactivity = false;

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

        return true;
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

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        latestData.sdnn = sdnn;
        latestData.rmssd = rmssd;
        latestData.ibiCount = ibiCount;
        hrvData = latestData;
        xSemaphoreGive(dataMutex);
    }

    xQueueSend(hrvQueue, &hrvData, 0);

    ibiCount = 0;
}

// ============================================================
// WIFI / MQTT FUNCTIONS
// ============================================================
void connectWiFiNonBlocking() {
    static unsigned long lastWiFiAttempt = 0;

    if (WiFi.status() == WL_CONNECTED) return;

    unsigned long now = millis();

    if (now - lastWiFiAttempt >= 5000) {
        lastWiFiAttempt = now;
        Serial.println("Trying Wi-Fi reconnect...");
        WiFi.disconnect();
        WiFi.begin(ssid, password);
    }
}

void connectMQTTNonBlocking() {
    static unsigned long lastMQTTAttempt = 0;

    if (client.connected()) return;

    unsigned long now = millis();

    if (now - lastMQTTAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
        lastMQTTAttempt = now;

        Serial.println("Trying MQTT reconnect...");

        if (client.connect("WorkerDevice01")) {
            Serial.println("Connected to AWS IoT Core");
        } else {
            Serial.print("MQTT reconnect failed, rc=");
            Serial.println(client.state());
        }
    }
}

void publishTelemetry(const SensorData& data) {
    StaticJsonDocument<384> doc;

    doc["worker_id"] = "W001";
    doc["heart_rate"] = data.bpm;
    doc["spo2"] = data.spo2;
    doc["aqi"] = data.aqi;
    doc["svm"] = data.svm;
    doc["sdnn"] = data.sdnn;
    doc["rmssd"] = data.rmssd;
    doc["ibi_count"] = data.ibiCount;
    doc["alarm_active"] = data.alarmActive;
    doc["alarm_level"] = data.alarmLevel;
    doc["fall_detected"] = data.fallDetected;

    char payload[384];
    serializeJson(doc, payload);

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
    serializeJson(doc, payload);

    bool ok = client.publish(topicHRV, payload);

    if (ok) {
        Serial.println("HRV summary published");
    } else {
        Serial.println("HRV summary publish failed");
    }
}

void publishAlertEvent(const AlertEvent& event) {
    StaticJsonDocument<384> doc;

    doc["worker_id"] = "W001";
    doc["category"] = event.category;
    doc["status"] = event.status;
    doc["alert_level"] = event.level;
    doc["message"] = event.message;
    doc["heart_rate"] = event.bpm;
    doc["spo2"] = event.spo2;
    doc["svm"] = event.svm;
    doc["rmssd"] = event.rmssd;

    char payload[384];
    serializeJson(doc, payload);

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
    unsigned long lastRead = 0;

    while (true) {
        unsigned long now = millis();

        if (now - lastRead >= SENSOR_READ_INTERVAL_MS) {
            lastRead = now;

            int localBPM = -1;
            int localSpO2 = -1;
            int localAQI = 50;
            float localSVM = 9.8;
            float localSDNN = -1;
            float localRMSSD = -1;
            bool localFallDetected = false;

            max30102.getHeartbeatSPO2();

            localSpO2 = max30102._sHeartbeatSPO2.SPO2;
            localBPM  = max30102._sHeartbeatSPO2.Heartbeat;

            if (localSpO2 < 70 || localSpO2 > 100) {
                localSpO2 = -1;
            }

            if (localBPM >= 40 && localBPM <= 200) {
                unsigned long currentIBI = 60000UL / localBPM;

                if (isValidIBI(currentIBI)) {
                    unsigned long filteredIBI = medianFilter(currentIBI);

                    if (filteredIBI > 0 && ibiCount < MAX_IBI_BUFFER) {
                        ibiBuffer[ibiCount++] = filteredIBI;
                    }
                }
            }
            else {
                localBPM = -1;
            }

            sensors_event_t accel, gyro, temp;
            mpu.getEvent(&accel, &gyro, &temp);

            localSVM = sqrt(
                accel.acceleration.x * accel.acceleration.x +
                accel.acceleration.y * accel.acceleration.y +
                accel.acceleration.z * accel.acceleration.z
            );

            if (firstMpuSample) {
                firstMpuSample = false;
                localSVM = 9.8;
            }

            assessBiometricRisk(localSpO2, localBPM, localSVM, localRMSSD);

            localFallDetected = assessFallRisk(localSVM, localBPM, localSpO2, localRMSSD);

            updateLatestData(
                localBPM,
                localSpO2,
                localAQI,
                localSVM,
                localSDNN,
                localRMSSD,
                ibiCount,
                localFallDetected
            );

            Serial.printf(
                "EDGE | HR:%d SpO2:%d AQI:%d SVM:%.2f Alarm:%d\n",
                localBPM,
                localSpO2,
                localAQI,
                localSVM,
                currentAlarmLevel
            );
        }

        if (now - lastHRVCheck >= HRV_WINDOW_MS) {
            lastHRVCheck = now;

            SensorData snapshot;

            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                snapshot = latestData;
                xSemaphoreGive(dataMutex);
            }

            updateHRVOnEdge(snapshot.bpm, snapshot.spo2, snapshot.svm);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
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
void Task_CloudPublish(void* pvParameters) {
    unsigned long lastPublish = 0;

    while (true) {
        connectWiFiNonBlocking();

        if (WiFi.status() == WL_CONNECTED) {
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

    Serial.println("\n========================================");
    Serial.println("FOURUT - Edge-first Health & Safety");
    Serial.println("========================================\n");

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(CANCEL_BTN_PIN, INPUT_PULLUP);
    digitalWrite(BUZZER_PIN, LOW);

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);

    Serial.println("Initializing MAX30102...");
    while (max30102.begin() == false) {
        Serial.println("MAX30102 init failed");
        delay(1000);
    }

    Serial.println("MAX30102 init success");
    max30102.sensorStartCollect();

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

    latestData.bpm = -1;
    latestData.spo2 = -1;
    latestData.aqi = 50;
    latestData.svm = 9.8;
    latestData.sdnn = -1;
    latestData.rmssd = -1;
    latestData.ibiCount = 0;
    latestData.alarmLevel = 0;
    latestData.alarmActive = false;
    latestData.fallDetected = false;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    espClient.setCACert(rootCA);
    espClient.setCertificate(clientCert);
    espClient.setPrivateKey(privateKey);

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