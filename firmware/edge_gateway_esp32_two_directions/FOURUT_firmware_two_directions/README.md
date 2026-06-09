# FOURUT Edge Gateway Firmware

## Overview

The edge gateway firmware implements the real-time sensing, local risk assessment, alert generation, and cloud telemetry layer of the **FOURUT Worker Health and Safety Monitoring System**. The firmware is designed for an ESP32-based wearable/edge gateway that combines biometric monitoring, motion-based fall detection, environmental hazard reception, local alarm control, and AWS IoT MQTT publishing.

This directory contains **two equivalent firmware directions**:

1. **Option A — Single-file firmware**  
   A compact Arduino sketch that keeps the full firmware logic in one `.ino` file. This version is useful for demonstration, teaching, code walkthroughs, and quick debugging.

2. **Option B — Modular firmware**  
   A maintainable implementation that separates configuration, sensor acquisition, fall detection, HRV/fatigue estimation, alert logic, MQTT publishing, and FreeRTOS tasks into dedicated source files.

Both options follow the same system behavior and use the same security rule: **credentials are stored only in `secrets.h`, which must not be committed to source control**.

---

## Directory Structure

```text
edge_gateway_esp32/
├── README.md
├── option_A_single_file/
│   └── fourut_edge_freertos_aws/
│       ├── fourut_edge_freertos_aws.ino
│       └── secrets.example.h
└── option_B_modular/
    └── fourut_edge_freertos_aws/
        ├── fourut_edge_freertos_aws.ino
        ├── secrets.example.h
        └── src/
            ├── config.h
            ├── types.h
            ├── globals.h
            ├── globals.cpp
            ├── sensors.h
            ├── sensors.cpp
            ├── fall_detection.h
            ├── fall_detection.cpp
            ├── hrv.h
            ├── hrv.cpp
            ├── risk_assessment.h
            ├── risk_assessment.cpp
            ├── alarms.h
            ├── alarms.cpp
            ├── alerts.h
            ├── alerts.cpp
            ├── espnow_env.h
            ├── espnow_env.cpp
            ├── mqtt_client.h
            ├── mqtt_client.cpp
            ├── tasks.h
            └── tasks.cpp
```

---

## Firmware Options

| Aspect | Option A: Single-file | Option B: Modular |
|---|---|---|
| Primary purpose | Demonstration and explanation | Maintainability and final repository quality |
| Main logic location | One `.ino` file | `.ino` plus `src/` modules |
| Best for presentation | High | Medium |
| Best for long-term development | Medium | High |
| Easier for beginners to read | High | Medium |
| Easier to test and extend | Medium | High |
| Credential handling | `secrets.h` | `secrets.h` |
| Core algorithm | Same | Same |

### Recommended Usage

Use **Option A** when presenting the full firmware flow in a single file.

Use **Option B** when submitting the repository as a clean engineering artifact or when continuing development.

A final report can describe this design choice as follows:

> The firmware is provided in two equivalent forms. The single-file version supports transparent demonstration and reviewer walkthrough, while the modular version improves maintainability, separation of concerns, and future extension. Both versions share the same edge-side detection logic and externalize credentials through a non-committed `secrets.h` file.

---

## System Architecture

```mermaid
flowchart LR
    W[Worker] --> B[MAX30102<br/>Heart Rate + SpO2]
    W --> M[MPU6050<br/>Acceleration + Gyroscope]

    E[Environmental Node<br/>PM1.0 / PM2.5 / PM10 / AQI] -- ESP-NOW --> G[ESP32 Edge Gateway]

    B --> G
    M --> G

    G --> L[Local Alarm<br/>Buzzer + Cancel Button]
    G -- MQTT over TLS --> A[AWS IoT Core]
    A --> C[Cloud Processing<br/>Lambda / Rules]
    C --> D[(DynamoDB / S3)]
    C --> N[SNS / Notification Layer]
```

The edge gateway acts as the local decision layer. It does not wait for cloud confirmation before activating urgent alarms. Cloud services are used for storage, dashboard integration, and remote notification.

---

## Firmware Runtime Model

The firmware uses multiple FreeRTOS tasks to separate time-sensitive sensing from networking and alarm handling.

```mermaid
flowchart TD
    T1[Task_AnomalyDetection<br/>High priority] --> S1[Read MPU6050 every 20 ms]
    T1 --> S2[Read MAX30102 every 1000 ms]
    T1 --> S3[Compute SVM, BPM, SpO2, IBI]
    T1 --> S4[Run biometric, fall, and fatigue assessment]

    T2[Task_EnvReceiver] --> E1[Receive ESP-NOW environmental packets]
    T2 --> E2[Update AQI / PM2.5 / PM10]
    T2 --> E3[Detect environmental hazards]

    T3[Task_AlarmControl] --> A1[Drive buzzer]
    T3 --> A2[Handle cancel button]

    T4[Task_CloudPublisher] --> C1[Publish telemetry]
    T4 --> C2[Publish HRV summary]
    T4 --> C3[Publish alert events]

    Q1[(Shared SensorData + Mutex)] --- T1
    Q1 --- T2
    Q1 --- T4

    Q2[(Alert Queue)] --- T1
    Q2 --- T2
    Q2 --- T4
```

This structure prevents slow cloud operations from blocking fall detection or alarm control.

---

## Hardware and Sensors

| Component | Role |
|---|---|
| XIAO ESP32-C3 / ESP32-class board | Edge gateway, FreeRTOS tasks, Wi-Fi, MQTT, ESP-NOW |
| MAX30102 | Heart rate and SpO2 sensing |
| MPU6050 | Acceleration and gyroscope sensing for fall detection |
| Buzzer | Local audible alarm |
| Cancel button | Manual alarm acknowledgement |
| Environmental node | External particulate matter and AQI data source transmitted by ESP-NOW |

Default pins:

| Signal | GPIO |
|---|---:|
| I2C SDA | 6 |
| I2C SCL | 7 |
| Buzzer | 2 |
| Cancel button | 3 |
| MAX30102 I2C address | `0x57` |

---

## Required Arduino Libraries

Install these libraries in Arduino IDE or PlatformIO:

| Library | Purpose |
|---|---|
| `DFRobot_BloodOxygen_S` | MAX30102 heart rate and SpO2 acquisition |
| `Adafruit_MPU6050` | MPU6050 IMU access |
| `Adafruit_Sensor` | Unified sensor abstraction required by Adafruit MPU6050 |
| `WiFi` | ESP32 Wi-Fi connectivity |
| `esp_now` | ESP-NOW environmental packet reception |
| `WiFiClientSecure` | TLS connection to AWS IoT |
| `PubSubClient` | MQTT client |
| `ArduinoJson` | JSON telemetry and alert payload generation |

---

## Security and Credential Handling

Real credentials must never be stored in committed firmware files.

Each option includes:

```text
secrets.example.h
```

Before building, copy it to:

```text
secrets.h
```

Then fill in local values:

```cpp
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define AWS_IOT_ENDPOINT "your-endpoint-ats.iot.your-region.amazonaws.com"

static const char AWS_CERT_CA[] = R"EOF(
-----BEGIN CERTIFICATE-----
YOUR_ROOT_CA_CERTIFICATE
-----END CERTIFICATE-----
)EOF";

static const char AWS_CERT_CRT[] = R"EOF(
-----BEGIN CERTIFICATE-----
YOUR_DEVICE_CERTIFICATE
-----END CERTIFICATE-----
)EOF";

static const char AWS_CERT_PRIVATE[] = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
YOUR_PRIVATE_KEY
-----END RSA PRIVATE KEY-----
)EOF";
```

The root repository should ignore every local secret file:

```gitignore
secrets.h
**/secrets.h
```

If a real certificate or private key was ever committed or shared, revoke the certificate in AWS IoT and create a new one.

---

## Build Instructions

### Option A: Single-file Firmware

Open this sketch in Arduino IDE:

```text
edge_gateway_esp32/option_A_single_file/fourut_edge_freertos_aws/fourut_edge_freertos_aws.ino
```

Then create:

```text
edge_gateway_esp32/option_A_single_file/fourut_edge_freertos_aws/secrets.h
```

### Option B: Modular Firmware

Open this sketch in Arduino IDE:

```text
edge_gateway_esp32/option_B_modular/fourut_edge_freertos_aws/fourut_edge_freertos_aws.ino
```

Then create:

```text
edge_gateway_esp32/option_B_modular/fourut_edge_freertos_aws/secrets.h
```

The folder name should match the `.ino` file name:

```text
fourut_edge_freertos_aws/
└── fourut_edge_freertos_aws.ino
```

This naming convention avoids Arduino IDE sketch import issues.

---

## Core Data Model

The firmware maintains a shared `SensorData` structure that represents the latest state of the worker and surrounding environment.

```text
SensorData
├── bpm
├── spo2
├── aqi
├── pm25
├── pm10
├── envNodeId
├── envSeq
├── envOnline
├── svm
├── sdnn
├── rmssd
├── ibiCount
├── alarmLevel
├── alarmActive
└── fallDetected
```

Alerts are represented as event objects with category, status, message, severity level, and relevant sensor values.

```text
AlertEvent
├── category
├── status
├── message
├── level
├── bpm
├── spo2
├── aqi
├── pm25
├── pm10
├── svm
└── rmssd
```

---

## Edge Algorithms

The firmware uses deterministic, rule-based algorithms suitable for embedded real-time execution. The goal is not to replace medical diagnosis, but to provide early warning and emergency escalation under constrained edge conditions.

---

### 1. Motion Feature Extraction

The MPU6050 provides acceleration on the x, y, and z axes. The firmware computes the signal vector magnitude:

```text
SVM = sqrt(ax² + ay² + az²)
```

SVM is used to detect free-fall, impact, and stillness. The gyroscope magnitude is also computed to verify whether the device is still after an impact-like event.

---

### 2. Multi-stage Fall Detection

A fall is not triggered by a single acceleration spike. The firmware uses a staged confirmation process to reduce false positives from heavy work, jumping, or tool vibration.

```mermaid
stateDiagram-v2
    [*] --> Stable

    Stable --> FreeFallCandidate: SVM < FREE_FALL_THRESH
    FreeFallCandidate --> ImpactCandidate: SVM > IMPACT_THRESH
    FreeFallCandidate --> Stable: No impact within FALL_TIME_WINDOW

    ImpactCandidate --> PostureCheck: Impact detected
    PostureCheck --> StillnessCheck: Posture change >= threshold
    PostureCheck --> Stable: No valid posture change

    StillnessCheck --> ConfirmedFall: Still for INACTIVITY_TIME
    StillnessCheck --> Stable: Movement resumes or timeout

    ConfirmedFall --> LocalAlarm
    ConfirmedFall --> AlertQueue
    ConfirmedFall --> CloudPublish
```

Fall detection stages:

| Stage | Condition | Purpose |
|---|---|---|
| Free-fall candidate | `SVM < 4.9 m/s²` | Detect possible loss of support |
| Impact candidate | `SVM > 29.4 m/s²` | Detect collision with ground or surface |
| Posture change | Angle change `>= 35°` | Verify body/device orientation changed |
| Stillness | SVM near gravity and low gyroscope movement for `3000 ms` | Confirm inactivity after impact |
| Confirmation timeout | `6000 ms` | Cancel sequence if evidence is incomplete |

This design is more robust than threshold-only impact detection because it requires a temporal sequence: low acceleration, high acceleration, changed posture, and post-impact inactivity.

---

### 3. Biometric Risk Assessment

The MAX30102 provides heart rate and SpO2 readings. The firmware rejects biologically unrealistic values before using them in risk assessment.

| Parameter | Valid / Risk Rule |
|---|---|
| SpO2 valid range | 70–100% |
| Heart rate valid range | 40–200 bpm |
| SpO2 warning | `< 90%` |
| SpO2 critical | `< 80%` |
| Bradycardia warning | `< 50 bpm` |
| Tachycardia warning | `> 120 bpm` |

Risk actions:

| Condition | Alert level | Action |
|---|---:|---|
| Mild SpO2 drop | 1 | Cloud alert |
| Critical SpO2 drop | 2 | Local alarm + cloud alert + emergency message |
| Bradycardia | 1 | Local warning + cloud alert |
| Tachycardia | 1 | Local warning + cloud alert |

---

### 4. HRV and Fatigue Estimation

The firmware estimates inter-beat interval from heart rate:

```text
IBI = 60000 / BPM
```

Each IBI sample is filtered using a small median window before being stored. At the configured HRV interval, the firmware calculates:

```text
SDNN  = standard deviation of IBI samples
RMSSD = sqrt(mean(successive IBI differences²))
```

The fatigue logic uses RMSSD as the primary indicator and heart rate as a supporting indicator.

```mermaid
flowchart TD
    B[BPM sample] --> I[Estimate IBI = 60000 / BPM]
    I --> V{IBI valid?<br/>300-1500 ms}
    V -- No --> X[Discard sample]
    V -- Yes --> M[Median filter]
    M --> W[Append to IBI buffer]
    W --> H{HRV window reached?}
    H -- No --> B
    H -- Yes --> R[Calculate SDNN and RMSSD]
    R --> F{Fatigue rule}
    F --> A[Alert if threshold is met]
```

Fatigue thresholds:

| Condition | Interpretation | Action |
|---|---|---|
| `RMSSD < 20 ms` for 3 consecutive windows | Fatigue warning | Level 1 alert |
| `RMSSD < 15 ms` and `BPM > 110` | Exhaustion risk | Level 2 alert + message |

Important limitation: the current HRV implementation derives IBI from BPM rather than raw beat-to-beat timestamps. This is acceptable for prototype-level fatigue screening but should be replaced by beat-level PPG peak detection for stronger physiological validity.

---

### 5. Environmental Risk Assessment

Environmental data is received from a separate node over ESP-NOW. The packet includes particulate matter readings and AQI.

```mermaid
sequenceDiagram
    participant Env as Environmental Node
    participant Edge as ESP32 Edge Gateway
    participant Risk as Risk Assessment
    participant Cloud as AWS IoT

    Env->>Edge: ESP-NOW packet<br/>PM1.0, PM2.5, PM10, AQI
    Edge->>Risk: Update shared SensorData
    Risk->>Risk: Compare AQI with thresholds
    Risk-->>Edge: Generate alert if needed
    Edge->>Cloud: Publish telemetry / alert
```

Environmental thresholds:

| AQI Rule | Alert |
|---|---|
| `AQI > 75` | Warning |
| `AQI > 150` | Danger |

If no environmental packet is received within the timeout window, the firmware marks the environmental node as offline. This prevents stale AQI values from being interpreted as current measurements.

---

### 6. Alert Generation and Cooldown

The firmware classifies alerts into categories and status codes. Each alert is assigned a severity level:

| Level | Meaning | Typical response |
|---:|---|---|
| 0 | Normal | Telemetry only |
| 1 | Warning | Cloud alert / local warning |
| 2 | Danger | Local alarm + cloud alert |
| 3 | Emergency | Local alarm + high-priority message |

To prevent repeated duplicate notifications, the firmware uses category-status cooldown slots. The same alert type cannot be repeatedly sent within the cooldown interval.

```mermaid
flowchart LR
    R[Risk condition] --> C{Cooldown active?}
    C -- Yes --> S[Suppress duplicate alert]
    C -- No --> Q[Push AlertEvent to queue]
    Q --> L[Local alarm if severity requires]
    Q --> M[MQTT publish]
    Q --> U[Update cooldown timestamp]
```

---

## MQTT Topics and Cloud Payloads

Default MQTT topics:

| Topic | Purpose |
|---|---|
| `worker/W001/telemetry` | Periodic worker and environment telemetry |
| `worker/W001/hrv` | HRV summary payload |
| `worker/W001/alert` | Event-driven alert payload |

Typical telemetry payload:

```json
{
  "worker_id": "W001",
  "bpm": 82,
  "spo2": 97,
  "aqi": 42,
  "pm25": 11.5,
  "pm10": 23.0,
  "svm": 9.81,
  "rmssd": 32.4,
  "alarm_level": 0,
  "fall_detected": false,
  "env_online": true
}
```

Typical alert payload:

```json
{
  "worker_id": "W001",
  "category": "FALL",
  "status": "EMERGENCY",
  "message": "Worker fall detected",
  "level": 3,
  "bpm": 86,
  "spo2": 95,
  "svm": 9.76,
  "rmssd": 28.1
}
```

---

## End-to-end Data Flow

```mermaid
sequenceDiagram
    participant MAX as MAX30102
    participant MPU as MPU6050
    participant ENV as Env Node
    participant EDGE as Edge Gateway
    participant ALARM as Local Alarm
    participant AWS as AWS IoT Core
    participant CLOUD as Cloud Processing

    MPU->>EDGE: Acceleration + Gyroscope
    MAX->>EDGE: BPM + SpO2
    ENV->>EDGE: ESP-NOW AQI + PM values

    EDGE->>EDGE: Validate sensor values
    EDGE->>EDGE: Compute SVM / IBI / HRV
    EDGE->>EDGE: Apply fall, biometric, fatigue, environment rules

    alt Warning / Danger / Emergency
        EDGE->>ALARM: Activate buzzer if needed
        EDGE->>AWS: Publish alert event
        AWS->>CLOUD: Route alert payload
    else Normal state
        EDGE->>AWS: Publish periodic telemetry
    end
```

---

## Threshold Summary

| Domain | Parameter | Threshold |
|---|---|---:|
| Biometric | SpO2 warning | `< 90%` |
| Biometric | SpO2 critical | `< 80%` |
| Biometric | Bradycardia | `< 50 bpm` |
| Biometric | Tachycardia | `> 120 bpm` |
| Fall | Free-fall SVM | `< 4.9 m/s²` |
| Fall | Impact SVM | `> 29.4 m/s²` |
| Fall | Posture change | `>= 35°` |
| Fall | Inactivity duration | `3000 ms` |
| Fatigue | RMSSD warning | `< 20 ms` for 3 windows |
| Fatigue | Exhaustion | `RMSSD < 15 ms` and `BPM > 110` |
| Environment | AQI warning | `> 75` |
| Environment | AQI danger | `> 150` |

These thresholds should be treated as prototype parameters. For deployment, they should be calibrated with controlled experiments, sensor placement tests, and domain review.

---

## Validation Checklist

Use the following checklist before submitting or demonstrating the firmware:

| Check | Expected result |
|---|---|
| `secrets.h` exists locally | Firmware compiles |
| `secrets.h` is not committed | Repository remains safe to publish |
| MAX30102 detected | Heart rate and SpO2 update every second |
| MPU6050 detected | SVM updates every 20 ms |
| Fall simulation | Alert appears only after sequence confirmation |
| Buzzer test | Alarm activates and cancel button works |
| ESP-NOW environmental packet | AQI and PM values update |
| AWS IoT connection | MQTT connects over TLS |
| Telemetry publish | Payload appears on `worker/W001/telemetry` |
| Alert publish | Payload appears on `worker/W001/alert` |
| HRV window | SDNN/RMSSD summary appears after enough IBI samples |

---

## Suggested Visual Evidence for Reports

For a stronger academic report or presentation, include the following figures and tables:

| Evidence | Purpose |
|---|---|
| System architecture diagram | Shows sensing, edge gateway, cloud, and notification layers |
| FreeRTOS task diagram | Explains concurrency and priority design |
| Fall detection state machine | Shows temporal logic behind emergency confirmation |
| MQTT payload flow diagram | Explains edge-to-cloud communication |
| Threshold table | Makes risk logic reproducible |
| Serial monitor screenshot | Demonstrates local runtime behavior |
| AWS IoT MQTT test client screenshot | Demonstrates cloud telemetry |
| DynamoDB/S3 screenshot | Demonstrates data persistence |
| Test-case result table | Shows validation against normal, danger, emergency, and fatigue cases |

Recommended test table format:

| Test case | Input condition | Expected output | Observed output | Result |
|---|---|---|---|---|
| Normal worker | Stable BPM, SpO2, AQI, SVM | Telemetry only |  |  |
| Low SpO2 | SpO2 below warning/critical threshold | Biometric alert |  |  |
| Fall event | Free-fall + impact + inactivity | Emergency alert |  |  |
| Fatigue | Low RMSSD over windows | Fatigue warning |  |  |
| AQI danger | AQI above danger threshold | Environmental alert |  |  |

---

## Design Limitations

1. **Prototype HRV limitation**  
   IBI is estimated from BPM rather than extracted from raw PPG peaks. This limits the physiological precision of SDNN and RMSSD.

2. **Threshold calibration**  
   Current thresholds are deterministic prototype values. Real deployments require calibration across workers, sensor positions, physical tasks, and environmental conditions.

3. **Sensor placement sensitivity**  
   Fall detection performance depends on how the wearable is mounted. Loose placement can increase false positives or false negatives.

4. **Environmental node dependency**  
   AQI and particulate matter risk assessment depends on timely ESP-NOW packets from the external environmental node.

5. **Connectivity dependency**  
   Local alarms work offline, but cloud logging and remote notifications require Wi-Fi and MQTT connectivity.

---

## Academic Positioning

The firmware demonstrates an edge-first monitoring approach for worker safety. Its main academic value is the integration of:

- real-time embedded sensing,
- multi-stage fall detection,
- biometric threshold assessment,
- prototype HRV-based fatigue screening,
- environmental hazard reception,
- local alarm actuation,
- and secure MQTT-based cloud telemetry.

The edge gateway prioritizes immediate local response while preserving cloud connectivity for traceability, remote monitoring, and later analysis.

---

## Repository Hygiene Notes

Keep this firmware directory focused and minimal:

```text
Keep:
- README.md
- option_A_single_file/
- option_B_modular/
- secrets.example.h files
- source code files

Avoid committing:
- secrets.h
- duplicated .gitignore files inside option folders
- build artifacts
- temporary diffs
- personal notes
- compiled binaries
```

Recommended root-level `.gitignore` entries:

```gitignore
secrets.h
**/secrets.h
.pio/
.vscode/
build/
*.bin
*.elf
*.map
.DS_Store
```

---

## Maintenance Recommendation

During active development, update the modular version first when changing thresholds or algorithms. Then mirror the change into the single-file version before demonstrations. This keeps both options consistent while preserving the readability advantage of the single-file sketch.
