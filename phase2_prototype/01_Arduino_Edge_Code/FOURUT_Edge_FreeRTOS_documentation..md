# FOURUT Edge FreeRTOS Firmware Documentation

## 1. Purpose

This document explains the main edge-device firmware of the **FOURUT Worker Health and Safety Monitoring System**.

The firmware file documented here is:

```text
FOURUT_Edge_FreeRTOS.ino
```

This is the **main deployed firmware** of the FOURUT system. It runs on an ESP32-based board and performs real-time worker monitoring using biometric and motion sensors.

The firmware supports:

- Heart-rate and SpO2 monitoring
- Motion monitoring using an IMU
- Fall-detection logic
- HRV and fatigue-risk estimation
- Local buzzer alarm control
- Cancel-button handling
- MQTT communication with AWS IoT Core
- JSON payload generation
- FreeRTOS task-based execution

This file is more important than the optional MicroPython prototype because it is the real embedded implementation used by the edge device.

---

## 2. Firmware Role in the FOURUT System

The Arduino FreeRTOS firmware is responsible for the **edge layer** of the FOURUT architecture.

The edge device is the first layer that detects worker risk before data reaches the cloud.

The main workflow is:

```text
MAX30102 + MPU6050 sensors
    ↓
ESP32 edge firmware
    ↓
FreeRTOS tasks
    ↓
Local anomaly detection
    ↓
Buzzer alarm / cancel button
    ↓
MQTT payloads
    ↓
AWS IoT Core
```

The firmware provides local safety response while also sending structured worker data to the cloud backend.

---

## 3. Target Hardware

The firmware header specifies the following target board and sensors:

```text
Board  : XIAO ESP32-C3
Sensors: MAX30102, MPU6050
```

### 3.1 Main Hardware Components

| Component | Purpose |
|---|---|
| XIAO ESP32-C3 | Main microcontroller and Wi-Fi-enabled edge device |
| MAX30102 | Heart-rate and SpO2 sensing |
| MPU6050 | Accelerometer and gyroscope sensing |
| Active buzzer | Local audible alarm |
| Cancel button | Allows the worker to silence or cancel the alarm |
| Wi-Fi module | Connects the ESP32 to AWS IoT Core |

---

## 4. Main Software Libraries

The firmware uses the following libraries:

```cpp
#include <Wire.h>
#include "DFRobot_BloodOxygen_S.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <math.h>
```

### 4.1 Library Responsibilities

| Library | Purpose |
|---|---|
| `Wire.h` | I2C communication |
| `DFRobot_BloodOxygen_S.h` | MAX30102 heart-rate and SpO2 sensor support |
| `Adafruit_MPU6050.h` | MPU6050 accelerometer and gyroscope support |
| `Adafruit_Sensor.h` | Unified sensor event interface |
| `WiFi.h` | Wi-Fi connection |
| `WiFiClientSecure.h` | TLS-secured network client |
| `PubSubClient.h` | MQTT communication |
| `ArduinoJson.h` | JSON payload construction |
| `FreeRTOS` headers | Multitasking, queues, mutexes, and task scheduling |

---

## 5. Wi-Fi and AWS IoT Configuration

The firmware includes Wi-Fi and AWS IoT Core configuration values.

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

const char* mqtt_server = "au08zn44zsegx-ats.iot.ap-southeast-1.amazonaws.com";
const int mqtt_port = 8883;
```

The firmware connects to AWS IoT Core using MQTT over TLS on port:

```text
8883
```

The submitted version uses placeholders for certificates:

```cpp
const char* rootCA = R"EOF(
PASTE_ROOT_CA_HERE
)EOF";

const char* clientCert = R"EOF(
PASTE_CLIENT_CERT_HERE
)EOF";

const char* privateKey = R"EOF(
PASTE_PRIVATE_KEY_HERE
)EOF";
```

These placeholders are appropriate for project submission because private keys and certificates should not be exposed in a public repository.

---

## 6. MQTT Topics

The firmware publishes data to three MQTT topics.

```cpp
const char* topicTelemetry = "worker/W001/telemetry";
const char* topicHRV       = "worker/W001/hrv";
const char* topicAlert     = "worker/W001/alert";
```

### 6.1 Topic Summary

| Topic | Purpose |
|---|---|
| `worker/W001/telemetry` | Periodic worker health and safety telemetry |
| `worker/W001/hrv` | HRV and fatigue summary data |
| `worker/W001/alert` | Warning, danger, and emergency alert events |

The topic design follows the pattern:

```text
worker/{worker_id}/{message_type}
```

This structure allows the cloud backend to separate regular monitoring data from HRV summaries and alert events.

---

## 7. Hardware Pin Configuration

The firmware defines the following hardware pins:

```cpp
#define I2C_SDA         6
#define I2C_SCL         7
#define BUZZER_PIN      2
#define CANCEL_BTN_PIN  3
#define MAX30102_ADDR   0x57
```

### 7.1 Pin Table

| Pin or Address | Function |
|---|---|
| GPIO 6 | I2C SDA |
| GPIO 7 | I2C SCL |
| GPIO 2 | Active buzzer output |
| GPIO 3 | Cancel button input |
| `0x57` | MAX30102 I2C address |

The cancel button uses:

```cpp
pinMode(CANCEL_BTN_PIN, INPUT_PULLUP);
```

This means the button is normally HIGH and becomes LOW when pressed.

---

## 8. FreeRTOS Task Design

The firmware uses FreeRTOS tasks to separate sensing, alarm control, and cloud communication.

### 8.1 Task Priorities

```cpp
#define PRIORITY_ANOMALY   5
#define PRIORITY_ALARM     4
#define PRIORITY_CLOUD     2
#define PRIORITY_DEBUG     1
```

### 8.2 Task Summary

| Task | Priority | Responsibility |
|---|---:|---|
| `Task_AnomalyDetection` | 5 | Reads sensors, detects biometric risk, detects fall events, computes HRV |
| `Task_AlarmControl` | 4 | Controls buzzer and cancel button |
| `Task_CloudPublish` | 2 | Handles Wi-Fi, MQTT reconnect, telemetry publishing, HRV publishing, alert publishing |

The highest priority task is anomaly detection because worker safety assessment should run before cloud publishing.

---

## 9. Shared Data and Synchronization

The firmware uses a shared `SensorData` structure to store the latest worker state.

```cpp
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
```

The firmware also defines an `AlertEvent` structure for alert messages.

```cpp
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
```

### 9.1 Mutex

The firmware uses a mutex to protect access to shared data.

```cpp
SemaphoreHandle_t dataMutex;
```

This prevents multiple FreeRTOS tasks from reading and writing `latestData` at the same time.

### 9.2 Queues

The firmware uses two FreeRTOS queues:

```cpp
QueueHandle_t alertQueue;
QueueHandle_t hrvQueue;
```

| Queue | Purpose |
|---|---|
| `alertQueue` | Sends alert events from anomaly detection to cloud publishing |
| `hrvQueue` | Sends HRV summary data from HRV calculation to cloud publishing |

This design decouples real-time detection from network publishing.

---

## 10. Threshold Configuration

The firmware defines several threshold groups.

### 10.1 SpO2 Thresholds

```cpp
#define SPO2_WARNING    90
#define SPO2_CRITICAL   80
```

| Condition | Threshold | Meaning |
|---|---:|---|
| Warning SpO2 | `< 90` | Mild hypoxemia warning |
| Critical SpO2 | `< 80` | Severe oxygen drop |

### 10.2 Heart-Rate Thresholds

```cpp
#define BPM_BRADY       50
#define BPM_TACHY       120
```

| Condition | Threshold | Meaning |
|---|---:|---|
| Bradycardia | `< 50 BPM` | Heart rate too low |
| Tachycardia | `> 120 BPM` | Heart rate too high |

### 10.3 Fall-Detection Thresholds

```cpp
#define FREE_FALL_THRESH 4.9
#define IMPACT_THRESH    29.4
#define FALL_TIME_WINDOW 1500
#define INACTIVITY_TIME  3000
```

| Threshold | Value | Meaning |
|---|---:|---|
| `FREE_FALL_THRESH` | `4.9 m/s²` | Possible free-fall phase |
| `IMPACT_THRESH` | `29.4 m/s²` | Possible impact phase |
| `FALL_TIME_WINDOW` | `1500 ms` | Maximum time between free fall and impact |
| `INACTIVITY_TIME` | `3000 ms` | Delay before confirming fall after impact |

### 10.4 Fatigue Thresholds

```cpp
#define RMSSD_FATIGUE_LEVEL_1    20.0
#define RMSSD_EXHAUSTION_LEVEL_2 15.0
#define BPM_EXHAUSTION_THRESHOLD 110
```

| Condition | Rule |
|---|---|
| Fatigue level 1 | `RMSSD < 20 ms` for 3 consecutive windows |
| Exhaustion level 2 | `RMSSD < 15 ms` and `BPM > 110` |

---

## 11. Timing Configuration

The firmware defines the following timing values:

```cpp
const unsigned long SENSOR_READ_INTERVAL_MS   = 1000;
const unsigned long CLOUD_PUBLISH_INTERVAL_MS = 5000;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
```

| Timing Variable | Value | Purpose |
|---|---:|---|
| `SENSOR_READ_INTERVAL_MS` | `1000 ms` | Sensor read interval |
| `CLOUD_PUBLISH_INTERVAL_MS` | `5000 ms` | Periodic telemetry publishing interval |
| `MQTT_RECONNECT_INTERVAL_MS` | `5000 ms` | MQTT reconnect attempt interval |

### Important Implementation Note

The current firmware reads both biometric data and IMU data using the same `1000 ms` sensor interval.

This is acceptable for a demonstration, but real fall detection usually benefits from a faster IMU sampling rate, such as 50 Hz or higher.

A recommended future improvement is to separate timing into:

```text
Biometric sampling: 1000 ms
IMU sampling: 20 ms
Telemetry publishing: 5000 ms
```

This would improve the chance of detecting short free-fall and impact events.

---

## 12. Sensor Initialization

The firmware initializes I2C using custom SDA and SCL pins:

```cpp
Wire.begin(I2C_SDA, I2C_SCL);
Wire.setClock(100000);
```

### 12.1 MAX30102 Initialization

The MAX30102 sensor is initialized using:

```cpp
while (max30102.begin() == false) {
    Serial.println("MAX30102 init failed");
    delay(1000);
}
```

After successful initialization, the firmware starts collection:

```cpp
max30102.sensorStartCollect();
```

### 12.2 MPU6050 Initialization

The MPU6050 sensor is initialized using:

```cpp
if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
        delay(10);
    }
}
```

The firmware configures the MPU6050 ranges:

```cpp
mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
mpu.setGyroRange(MPU6050_RANGE_500_DEG);
```

---

## 13. Biometric Monitoring Logic

The firmware reads heart rate and SpO2 from the MAX30102.

```cpp
max30102.getHeartbeatSPO2();

localSpO2 = max30102._sHeartbeatSPO2.SPO2;
localBPM  = max30102._sHeartbeatSPO2.Heartbeat;
```

### 13.1 Data Validation

SpO2 is accepted only if it is within a reasonable range:

```cpp
if (localSpO2 < 70 || localSpO2 > 100) {
    localSpO2 = -1;
}
```

Heart rate is accepted only if it is within:

```cpp
40 BPM to 200 BPM
```

Invalid values are set to:

```text
-1
```

This helps prevent obviously invalid sensor readings from being used as normal data.

---

## 14. IBI and HRV Processing

The firmware estimates IBI from BPM:

```cpp
unsigned long currentIBI = 60000UL / localBPM;
```

The IBI value is then validated:

```cpp
bool isValidIBI(unsigned long ibi) {
    return (ibi >= 300 && ibi <= 1500);
}
```

Valid IBI samples are passed through a median filter and stored in `ibiBuffer`.

### 14.1 Median Filter

The median filter uses a window size of:

```cpp
#define MEDIAN_WINDOW 5
```

This helps reduce the effect of abnormal IBI samples.

### 14.2 HRV Window

The HRV calculation interval is:

```cpp
#define HRV_WINDOW_MS 300000
```

This equals:

```text
5 minutes
```

The firmware requires at least:

```cpp
#define MIN_IBI_COUNT 10
```

IBI samples before calculating HRV.

### 14.3 SDNN Calculation

SDNN is calculated using the standard deviation of IBI samples:

```text
SDNN = standard deviation of IBI values
```

### 14.4 RMSSD Calculation

RMSSD is calculated using successive IBI differences:

```text
RMSSD = sqrt(mean(successive IBI differences²))
```

### 14.5 Important HRV Note

The current firmware estimates IBI from BPM using:

```cpp
IBI = 60000 / BPM
```

This is suitable for demonstrating the HRV workflow, but true HRV should be calculated from actual beat-to-beat timestamps.

For more accurate HRV, the firmware should extract individual heartbeat timestamps from the PPG signal instead of deriving IBI from average BPM.

---

## 15. Fatigue-Risk Detection

Fatigue detection is handled by:

```cpp
assessFatigueRisk(rmssd, bpm, spo2, svm)
```

### 15.1 Exhaustion Level 2

The firmware detects exhaustion when:

```text
RMSSD < 15 ms
and
BPM > 110
```

When this occurs, the firmware:

1. Resets the low-RMSSD counter.
2. Clears the level-1 fatigue flag.
3. Triggers a level-2 alarm.
4. Pushes a fatigue alert to the alert queue.
5. Simulates an SMS notification.

### 15.2 Fatigue Level 1

The firmware detects level-1 fatigue when:

```text
RMSSD < 20 ms for 3 consecutive HRV windows
```

When this occurs, the firmware:

1. Triggers a level-1 alarm.
2. Pushes a fatigue warning alert.
3. Prevents repeated level-1 fatigue messages until the RMSSD condition resets.

This staged design helps reduce false fatigue alerts caused by a single low-RMSSD window.

---

## 16. Motion Monitoring and SVM

The firmware reads accelerometer data from the MPU6050:

```cpp
sensors_event_t accel, gyro, temp;
mpu.getEvent(&accel, &gyro, &temp);
```

It calculates Signal Vector Magnitude:

```cpp
localSVM = sqrt(
    accel.acceleration.x * accel.acceleration.x +
    accel.acceleration.y * accel.acceleration.y +
    accel.acceleration.z * accel.acceleration.z
);
```

The formula is:

```text
SVM = sqrt(ax² + ay² + az²)
```

SVM is used as the main motion feature for fall detection.

---

## 17. Fall-Detection Logic

Fall detection is handled by:

```cpp
assessFallRisk(localSVM, localBPM, localSpO2, localRMSSD)
```

The firmware uses a staged state machine.

```text
Free fall phase
    ↓
Impact phase
    ↓
Inactivity waiting period
    ↓
Fall alert
```

### 17.1 Free Fall Phase

A possible fall sequence begins when:

```text
SVM < 4.9 m/s²
```

The firmware then stores the start time of the fall sequence.

### 17.2 Impact Phase

After the free-fall phase, impact is detected when:

```text
SVM > 29.4 m/s²
```

When impact is detected, the firmware moves into the inactivity waiting stage.

### 17.3 Fall Sequence Timeout

If impact does not occur within:

```text
1500 ms
```

the fall sequence is cancelled.

### 17.4 Inactivity Stage

After impact, the firmware waits for:

```text
3000 ms
```

Then it triggers a level-3 emergency fall alert.

### 17.5 Fall Alert Behavior

When a fall is confirmed, the firmware:

1. Triggers a level-3 alarm.
2. Sends a fall alert into the alert queue.
3. Simulates an urgent SMS message.
4. Marks the event as a detected fall.

### 17.6 Important Fall-Detection Note

In the current implementation, the firmware waits for the inactivity period after impact, but it does not continuously verify that the worker remains inactive during that period.

A more robust implementation would check that SVM remains within a stable range after impact before confirming the fall.

Recommended logic:

```text
free fall
    ↓
impact
    ↓
SVM remains stable near gravity for 3 seconds
    ↓
confirmed fall
```

This would reduce false positives caused by sudden motion followed by normal movement.

---

## 18. Edge Alarm System

The firmware controls the local buzzer using:

```cpp
triggerAlarm(level, message)
cancelAlarm()
Task_AlarmControl()
```

### 18.1 Alarm Levels

| Level | Meaning |
|---:|---|
| 0 | No alarm |
| 1 | Warning |
| 2 | Danger or critical biometric/fatigue condition |
| 3 | Emergency, such as fall detection |

### 18.2 Triggering an Alarm

The alarm is triggered by:

```cpp
triggerAlarm(int level, const char* message)
```

The function only upgrades the alarm if the new alarm level is higher than the current active level:

```cpp
if (alarmActive && level <= currentAlarmLevel) return;
```

This prevents a lower-priority event from overriding a more serious alarm.

### 18.3 Cancel Button

The cancel button is handled by:

```cpp
checkButtonNonBlocking()
```

It uses a simple debounce delay of:

```text
50 ms
```

If the button is pressed while an alarm is active, the firmware calls:

```cpp
cancelAlarm()
```

This turns off the buzzer and resets the current alarm level.

---

## 19. Alert Queue Design

When a risk condition is detected, the anomaly detection task does not publish directly to MQTT.

Instead, it calls:

```cpp
pushAlert(...)
```

This places an `AlertEvent` into `alertQueue`.

The cloud publishing task later reads from the queue and publishes the alert.

This is a good embedded design pattern because it prevents the high-priority anomaly detection task from being blocked by network operations.

---

## 20. MQTT and Cloud Publishing

Cloud publishing is handled by:

```cpp
Task_CloudPublish()
```

This task performs:

1. Non-blocking Wi-Fi reconnect.
2. Non-blocking MQTT reconnect.
3. MQTT client loop handling.
4. Alert queue publishing.
5. HRV queue publishing.
6. Periodic telemetry publishing.

### 20.1 Wi-Fi Reconnect

The firmware reconnects Wi-Fi every 5 seconds if disconnected:

```cpp
connectWiFiNonBlocking()
```

### 20.2 MQTT Reconnect

The firmware reconnects MQTT every 5 seconds if disconnected:

```cpp
connectMQTTNonBlocking()
```

The MQTT client ID is:

```text
WorkerDevice01
```

### 20.3 TLS Security

The firmware configures TLS credentials using:

```cpp
espClient.setCACert(rootCA);
espClient.setCertificate(clientCert);
espClient.setPrivateKey(privateKey);
```

This allows the device to connect securely to AWS IoT Core.

---

## 21. Telemetry Payload

The firmware publishes periodic telemetry to:

```text
worker/W001/telemetry
```

The telemetry payload is generated by:

```cpp
publishTelemetry(const SensorData& data)
```

### 21.1 Telemetry JSON Structure

```json
{
  "worker_id": "W001",
  "heart_rate": 82,
  "spo2": 97,
  "aqi": 50,
  "svm": 9.8,
  "sdnn": -1,
  "rmssd": -1,
  "ibi_count": 20,
  "alarm_active": false,
  "alarm_level": 0,
  "fall_detected": false
}
```

### 21.2 Purpose

Telemetry provides the cloud backend with regular worker status updates, even when no alert is active.

---

## 22. HRV Payload

The firmware publishes HRV summaries to:

```text
worker/W001/hrv
```

The HRV payload is generated by:

```cpp
publishHRVSummary(const SensorData& data)
```

### 22.1 HRV JSON Structure

```json
{
  "worker_id": "W001",
  "sdnn": 42.5,
  "rmssd": 31.8,
  "ibi_count": 128,
  "heart_rate": 82,
  "alarm_level": 0
}
```

### 22.2 Purpose

The HRV topic provides fatigue-related indicators for cloud-side monitoring and classification.

---

## 23. Alert Payload

The firmware publishes alert events to:

```text
worker/W001/alert
```

The alert payload is generated by:

```cpp
publishAlertEvent(const AlertEvent& event)
```

### 23.1 Alert JSON Structure

```json
{
  "worker_id": "W001",
  "category": "FALL",
  "status": "EMERGENCY",
  "alert_level": 3,
  "message": "Worker fall detected",
  "heart_rate": 110,
  "spo2": 94,
  "svm": 31.2,
  "rmssd": 18.4
}
```

### 23.2 Purpose

Alert messages allow the cloud backend to quickly identify risk events and notify supervisors.

---

## 24. Task 1: Anomaly Detection

The main monitoring task is:

```cpp
Task_AnomalyDetection(void* pvParameters)
```

This is the highest priority task.

### 24.1 Responsibilities

The anomaly detection task:

1. Reads MAX30102 data.
2. Validates heart rate and SpO2 values.
3. Estimates IBI from BPM.
4. Applies median filtering.
5. Stores IBI samples.
6. Reads MPU6050 accelerometer data.
7. Calculates SVM.
8. Runs biometric risk assessment.
9. Runs fall-risk assessment.
10. Updates shared latest sensor data.
11. Runs HRV calculation every 5 minutes.
12. Sends HRV summary data to the HRV queue.

### 24.2 Task Delay

The task uses:

```cpp
vTaskDelay(pdMS_TO_TICKS(20));
```

However, actual sensor reading is gated by:

```cpp
SENSOR_READ_INTERVAL_MS = 1000
```

This means the task wakes frequently but only performs sensor acquisition every 1 second.

---

## 25. Task 2: Alarm Control

The alarm task is:

```cpp
Task_AlarmControl(void* pvParameters)
```

### 25.1 Responsibilities

The alarm control task:

1. Checks the cancel button.
2. Turns the buzzer on if an alarm is active.
3. Turns the buzzer off if no alarm is active.
4. Runs independently of cloud connectivity.

### 25.2 Task Delay

The task runs every:

```text
50 ms
```

This provides responsive button handling without blocking the system.

---

## 26. Task 3: Cloud Publish

The cloud task is:

```cpp
Task_CloudPublish(void* pvParameters)
```

### 26.1 Responsibilities

The cloud publish task:

1. Maintains Wi-Fi connection.
2. Maintains MQTT connection.
3. Publishes queued alert events.
4. Publishes queued HRV summaries.
5. Publishes telemetry every 5 seconds.
6. Calls `client.loop()` to maintain the MQTT session.

### 26.2 Task Delay

The task runs every:

```text
200 ms
```

This is suitable for handling queued messages without interfering with anomaly detection.

---

## 27. Setup Function

The `setup()` function performs firmware initialization.

### 27.1 Setup Flow

```text
Start serial monitor
    ↓
Initialize buzzer and cancel button
    ↓
Initialize I2C bus
    ↓
Initialize MAX30102
    ↓
Initialize MPU6050
    ↓
Configure MPU6050 ranges
    ↓
Create mutex and queues
    ↓
Initialize latestData
    ↓
Start Wi-Fi
    ↓
Set TLS certificates
    ↓
Configure MQTT server
    ↓
Create FreeRTOS tasks
```

### 27.2 FreeRTOS Task Creation

The firmware creates three tasks:

```cpp
xTaskCreate(Task_AnomalyDetection, "AnomalyDetection", 8192, NULL, PRIORITY_ANOMALY, NULL);
xTaskCreate(Task_AlarmControl, "AlarmControl", 2048, NULL, PRIORITY_ALARM, NULL);
xTaskCreate(Task_CloudPublish, "CloudPublish", 8192, NULL, PRIORITY_CLOUD, NULL);
```

---

## 28. Loop Function

The Arduino `loop()` is intentionally empty:

```cpp
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

This is correct for a FreeRTOS-based design because the main logic runs inside FreeRTOS tasks.

---

## 29. Edge-to-Cloud Data Flow

The firmware data flow is:

```text
1. Sensors provide biometric and motion data.
2. Task_AnomalyDetection reads and validates sensor values.
3. The firmware computes SVM and HRV indicators.
4. Local risk functions classify biometric, fall, and fatigue risks.
5. Local alarm is triggered if needed.
6. Alert events are pushed into FreeRTOS queues.
7. Task_CloudPublish publishes telemetry, HRV summaries, and alerts.
8. AWS IoT Core receives MQTT payloads.
9. Lambda processes and stores the data.
10. Supervisors are notified if danger or emergency conditions occur.
```

---

## 30. Strengths of the Current Firmware

The current firmware has several strong design choices.

1. It uses FreeRTOS tasks to separate real-time sensing, alarm control, and cloud communication.
2. It uses a mutex to protect shared sensor data.
3. It uses queues to decouple alert generation from MQTT publishing.
4. It supports TLS-based MQTT communication with AWS IoT Core.
5. It provides local buzzer response even if cloud connectivity fails.
6. It includes fall detection, biometric risk detection, and fatigue-risk estimation.
7. It uses structured JSON payloads for cloud processing.
8. It supports a cancel button for human interaction.
9. It avoids placing certificates directly in the public submission by using placeholders.
10. It maintains a clear edge-first safety architecture.

---

## 31. Important Implementation Limitations

The firmware is suitable as the main project implementation, but several limitations should be documented honestly.

### 31.1 IMU Sampling Rate

The IMU is currently sampled every:

```text
1000 ms
```

This may be too slow for reliable fall detection because free-fall and impact phases can occur quickly.

Recommended improvement:

```text
Read IMU every 20 ms to 50 ms
Read MAX30102 every 1000 ms
Publish telemetry every 5000 ms
```

### 31.2 HRV Input Source

The firmware estimates IBI from BPM:

```cpp
IBI = 60000 / BPM
```

This demonstrates HRV workflow but is less accurate than beat-to-beat timestamp extraction.

Recommended improvement:

```text
Use actual heartbeat timestamps from the PPG signal for HRV.
```

### 31.3 Fall Inactivity Verification

After detecting impact, the firmware waits for 3 seconds before confirming fall. It does not verify that the worker remains inactive throughout that period.

Recommended improvement:

```text
Confirm that SVM remains stable near gravity during the inactivity window.
```

### 31.4 Alert Cooldown

Some alert types may be pushed repeatedly if abnormal values persist.

Recommended improvement:

```text
Add per-category alert cooldown timers.
```

### 31.5 AQI Placeholder and PM2.5 Sensor Integration Issue

The current code sets:

```cpp
int localAQI = 50;
```

This means AQI is currently represented as a placeholder rather than data from a real air-quality or dust sensor.

The PM2.5 / dust-sensor module was included in the original system design because environmental monitoring is relevant for worker-safety applications. However, it was not included in the final Arduino FreeRTOS firmware because the available sensor library caused compatibility and stability issues during integration.

The issue was related to library support in the selected embedded environment, including possible conflicts with the ESP32-C3 Arduino core, sensor communication handling, and the existing FreeRTOS-based firmware structure. Since the main firmware already performs real-time MAX30102 sensing, MPU6050 motion detection, buzzer control, and AWS IoT MQTT publishing, the PM2.5 sensor was temporarily excluded to avoid reducing the reliability of the core safety functions.

For this reason, the firmware keeps the AQI field as a fixed placeholder value:

```cpp
int localAQI = 50;
```

This allows the JSON telemetry structure and cloud-processing pipeline to remain complete while clearly indicating that real air-quality sensing is reserved for future integration.

Recommended improvement:

```text
Replace the AQI placeholder with a validated PM2.5 or air-quality sensor driver after resolving library compatibility issues.
```

A suitable production-ready version should verify that the dust-sensor driver can operate reliably together with:

- MAX30102 sensor reading
- MPU6050 sensor reading
- FreeRTOS task scheduling
- Wi-Fi and MQTT over TLS
- JSON payload generation
- Local alarm handling

### 31.6 SMS Function Simulation

The `sendSMS()` function currently prints simulated SMS output to the serial monitor.

```cpp
Serial.printf("SMS simulated ...");
```

Recommended improvement:

```text
Integrate SIM768x or another cellular module for real SMS or call support.
```

---

## 32. Recommended Future Improvements

Future firmware versions could improve reliability and production readiness by adding:

1. Separate IMU and biometric sampling intervals.
2. Faster IMU sampling for fall detection.
3. Real beat-to-beat IBI extraction for HRV.
4. Inactivity verification after impact.
5. Alert cooldown per category.
6. Real PM2.5 or AQI sensor integration.
7. Real SMS or cellular emergency communication.
8. Device-side timestamp support using NTP.
9. MQTT last-will message for device offline status.
10. Cloud-configurable thresholds.
11. More detailed error handling for sensor failures.
12. Ring buffer handling for HRV samples.
13. Watchdog timer support.
14. Battery or power-status telemetry.
15. Worker ID configuration instead of hard-coded `W001`.

---

## 33. Suggested Academic Description

The firmware can be described in a report as follows:

```text
The main FOURUT edge implementation is developed in Arduino C++ for an ESP32-based board using FreeRTOS. The firmware runs separate tasks for anomaly detection, local alarm control, and cloud publishing. It reads biometric data from the MAX30102 sensor and motion data from the MPU6050 sensor. The device performs edge-level biometric screening, fall detection, HRV-based fatigue estimation, and local buzzer activation. Processed telemetry, HRV summaries, and alert events are packaged as JSON and published to AWS IoT Core through MQTT over TLS.
```

This wording clearly identifies the firmware as the main deployed implementation and distinguishes it from the optional MicroPython prototype.

---

## 34. Validation Checklist

The firmware can be considered valid for demonstration if the following checks pass:

- Serial monitor starts successfully.
- MAX30102 initializes successfully.
- MPU6050 initializes successfully.
- Buzzer pin responds to alarm state.
- Cancel button can cancel an active alarm.
- Wi-Fi reconnect logic runs without blocking sensing.
- MQTT reconnect logic connects to AWS IoT Core.
- Telemetry is published to `worker/W001/telemetry`.
- HRV summaries are published to `worker/W001/hrv`.
- Alert events are published to `worker/W001/alert`.
- SpO2 warning and critical conditions generate expected alerts.
- Heart-rate abnormalities generate expected alerts.
- Fall sequence generates an emergency alert.
- HRV fatigue logic generates fatigue warnings or exhaustion alerts.
- Shared sensor data remains consistent across tasks.

---

### 34.1 AQI Validation Note

For the current submitted firmware, AQI is included in the telemetry schema but represented by a placeholder value because the PM2.5 / dust-sensor library produced compatibility issues during integration. The core firmware therefore prioritizes stable biometric monitoring, fall detection, local alarm response, and MQTT communication. Real air-quality sensing is listed as a future integration task after a compatible and reliable sensor driver is validated.


## 35. Submission Notes

For project submission, this file should be presented as the primary embedded firmware.

Recommended repository placement:

```text
01_Arduino_Edge_Code/
└── FOURUT_Edge_FreeRTOS.ino
```

The optional MicroPython prototype may be submitted separately, but it should be clearly labeled as supplementary.

Recommended distinction:

| File | Role |
|---|---|
| `FOURUT_Edge_FreeRTOS.ino` | Main deployed edge firmware |
| `main.py` MicroPython prototype | Optional simplified prototype |
| `config.py` MicroPython prototype | Optional configuration file |

This prevents reviewers from confusing the simplified prototype with the real firmware.

---

## 36. Conclusion

`FOURUT_Edge_FreeRTOS.ino` is the core embedded firmware of the FOURUT system.

It implements the edge-first safety model by combining real sensor integration, FreeRTOS multitasking, local alarm control, HRV and fatigue estimation, fall detection, and secure MQTT communication with AWS IoT Core.

The firmware demonstrates a complete embedded edge workflow for worker health and safety monitoring. While some components can be improved for production-grade deployment, the current design clearly shows the main system logic and supports the overall FOURUT hybrid edge-cloud architecture.
