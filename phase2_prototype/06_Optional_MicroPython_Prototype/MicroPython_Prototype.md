# FOURUT Optional MicroPython Prototype

## 1. Purpose

This folder contains an **optional MicroPython prototype** for the **FOURUT Worker Health and Safety Monitoring System**.

The purpose of this prototype is to provide a lightweight and readable implementation of the edge-device workflow, including:

- Simulated worker biometric and motion data acquisition
- Basic edge-level risk classification
- Fall-detection logic based on accelerometer magnitude
- Fatigue-risk estimation using prototype HRV indicators
- JSON payload construction
- MQTT publishing to worker-specific telemetry, HRV, and alert topics

This prototype is included to support documentation, demonstration, and system explanation. It is **not intended to replace the main deployed firmware**.

---

## 2. Important Note

The MicroPython implementation is **not the main production firmware** of the FOURUT system.

The main edge-device implementation is the Arduino C++ FreeRTOS firmware located at:

```text
01_Arduino_Edge_Code/FOURUT_Edge_FreeRTOS.ino
```

The Arduino firmware remains the primary deployed implementation because it provides real-time task scheduling, stronger hardware-library support, secure MQTT communication, and direct integration with the actual sensor modules used in the system.

The MicroPython prototype should therefore be treated as a **supplementary demonstration artifact**.

---

## 3. Why This Prototype Is Included

This prototype is included for academic and documentation purposes. It helps make the system architecture easier to understand by presenting the core edge logic in a simplified script format.

Specifically, it demonstrates:

1. How worker health and motion data can be represented at the edge device.
2. How sensor readings can be converted into structured JSON payloads.
3. How edge-level logic can classify biometric, fatigue, and fall-related risks.
4. How telemetry, HRV, and alert data can be published to MQTT topics.
5. How the MicroPython version conceptually maps to the main Arduino FreeRTOS firmware.

This makes the prototype useful for reviewers, evaluators, or readers who want to understand the logic flow without first analyzing the full embedded Arduino implementation.

---

## 4. Relationship to the Main Arduino Firmware

The FOURUT project uses the Arduino C++ FreeRTOS implementation as the real edge firmware.

### Main deployed firmware

```text
01_Arduino_Edge_Code/FOURUT_Edge_FreeRTOS.ino
```

### Optional prototype

```text
Optional_MicroPython_Prototype/
├── config.py
├── main.py
└── README.md
```

The Arduino version is used for the actual device because it supports:

- FreeRTOS task prioritization
- Real MAX30102 heart-rate and SpO2 sensor integration
- Real MPU6050 accelerometer and gyroscope integration
- WiFiClientSecure for TLS-based communication
- PubSubClient for MQTT communication
- ArduinoJson for structured JSON payload construction
- Real-time buzzer control
- AWS IoT Core MQTT over TLS
- More reliable timing control for sensor sampling and alert handling

The MicroPython version mirrors the same conceptual workflow, but in a simplified and more readable form.

---

## 5. Prototype Scope

The MicroPython prototype focuses on the following edge-device functions:

### 5.1 Sensor Data Simulation

The prototype simulates or abstracts the following data types:

- Heart rate
- SpO2
- Air-quality index
- Accelerometer readings
- Signal Vector Magnitude, also called SVM
- Estimated HRV indicators

### 5.2 Edge Classification

The prototype performs basic risk classification for:

- Abnormal heart rate
- Low SpO2
- Fatigue-risk indication
- Fall-risk events

### 5.3 Fall Detection

The fall-detection logic follows a staged pattern:

```text
free fall detection → impact detection → inactivity confirmation → fall alert
```

This is more robust than triggering an alert immediately after a single acceleration spike.

### 5.4 HRV and Fatigue Estimation

The prototype includes HRV-related calculations such as:

- SDNN
- RMSSD

In this MicroPython version, HRV values may be estimated from BPM-derived IBI values for demonstration purposes. This is suitable for illustrating the algorithmic flow, but it should not be treated as clinical-grade HRV measurement.

### 5.5 MQTT Data Publishing

The prototype demonstrates how structured payloads are published to MQTT topics for cloud-side processing and monitoring.

---

## 6. File Description

### 6.1 `config.py`

This file stores configuration values used by the prototype.

It includes:

- Wi-Fi configuration placeholders
- MQTT broker configuration placeholders
- Worker ID
- MQTT topic definitions
- Sensor sampling intervals
- Biometric thresholds
- Fall-detection thresholds
- HRV and fatigue thresholds
- Alert cooldown settings

Example configuration categories:

```text
Network configuration
MQTT configuration
Worker identity
Sampling intervals
Biometric thresholds
Fall-detection thresholds
HRV thresholds
Alert control
```

### 6.2 `main.py`

This file contains the main prototype logic.

It includes:

- Hardware initialization placeholders
- Simulated or abstracted sensor-reading functions
- Biometric risk analysis
- Fall-detection state machine
- HRV and fatigue-risk estimation
- Alarm and alert control
- JSON payload construction
- MQTT publishing
- Asynchronous task scheduling

The file is intentionally organized with layered comments to make the logic easier to read and explain in an academic or project-submission context.

---

## 7. MQTT Topics

The prototype uses worker-specific MQTT topics.

For worker `W001`, the topics are:

```text
worker/W001/telemetry
worker/W001/hrv
worker/W001/alert
```

### 7.1 Telemetry Topic

```text
worker/W001/telemetry
```

Used for general worker status data, such as:

- Worker ID
- Heart rate
- SpO2
- Air quality
- SVM
- Alarm status
- Fall status
- Timestamp

### 7.2 HRV Topic

```text
worker/W001/hrv
```

Used for HRV and fatigue-related indicators, such as:

- Worker ID
- SDNN
- RMSSD
- IBI sample count
- Fatigue-risk level
- Timestamp

### 7.3 Alert Topic

```text
worker/W001/alert
```

Used for warning or emergency events, such as:

- Critical SpO2 drop
- Abnormal heart rate
- Fatigue risk
- Confirmed fall event
- Alarm level
- Alert message
- Timestamp

---

## 8. Prototype Algorithm Overview

The MicroPython prototype follows the edge-processing workflow below.

```text
Start device
    ↓
Load configuration
    ↓
Initialize Wi-Fi and MQTT connection
    ↓
Read or simulate sensor data
    ↓
Compute derived indicators
    ↓
Run biometric risk analysis
    ↓
Run fall-detection state machine
    ↓
Run HRV and fatigue-risk estimation
    ↓
Update local alarm state
    ↓
Package data into JSON
    ↓
Publish telemetry, HRV, or alert payloads to MQTT topics
```

---

## 9. Biometric Risk Logic

The biometric risk logic checks whether worker vital signs exceed predefined thresholds.

Example conditions include:

- Heart rate is too high
- Heart rate is too low
- SpO2 is below the warning threshold
- SpO2 is below the critical threshold

When a risk condition is detected, the prototype may:

1. Update the local alarm state.
2. Generate an alert payload.
3. Publish the alert to the MQTT alert topic.
4. Apply cooldown logic to reduce repeated duplicate alerts.

---

## 10. Fall-Detection Logic

The prototype uses Signal Vector Magnitude, or SVM, to estimate body-motion intensity.

SVM is calculated as:

```text
SVM = sqrt(ax² + ay² + az²)
```

The fall-detection process contains three stages.

### Stage 1: Free Fall Detection

A possible free-fall event is detected when SVM drops below the configured free-fall threshold.

### Stage 2: Impact Detection

After a free-fall event, the system waits for a high SVM spike that indicates body impact.

### Stage 3: Inactivity Confirmation

After impact, the system checks whether the worker remains relatively inactive for a configured period.

A fall alert is generated only when all three stages are satisfied:

```text
free fall + impact + inactivity = confirmed fall event
```

This staged approach helps reduce false positives compared with using a single acceleration threshold.

---

## 11. HRV and Fatigue Logic

The prototype includes HRV-related indicators to demonstrate fatigue-risk analysis.

The two main indicators are:

### SDNN

SDNN represents the standard deviation of IBI samples.

### RMSSD

RMSSD represents the root mean square of successive differences between IBI samples.

In this prototype, IBI values may be estimated from heart-rate readings:

```text
IBI ≈ 60000 / BPM
```

This method is useful for demonstrating the data-processing workflow, but it does not replace real beat-to-beat interval extraction from the MAX30102 sensor.

For real HRV measurement, the production firmware should use actual beat timestamps or a validated beat-detection algorithm.

---

## 12. Security Note

Sensitive credentials and certificates are intentionally replaced with placeholders in this submission.

The following values should not be hard-coded in a public repository:

- Wi-Fi SSID
- Wi-Fi password
- AWS IoT endpoint
- Device private key
- Device certificate
- Root CA certificate

For deployment, credentials should be stored securely and managed according to the security requirements of the target platform.

---

## 13. Prototype Limitations

This MicroPython prototype has several important limitations.

1. Sensor values may be simulated or simplified.
2. Real MAX30102 and MPU6050 drivers are not fully implemented in this prototype.
3. HRV values may be derived from BPM-estimated IBI rather than true beat-to-beat intervals.
4. AWS IoT TLS certificate handling depends on the MicroPython firmware build and board support.
5. Timing accuracy may be less reliable than the Arduino FreeRTOS implementation.
6. The prototype is intended for demonstration and documentation, not final deployment.
7. Clinical or safety-critical decisions should not rely on this prototype alone.

These limitations are expected because the purpose of this folder is to demonstrate the logic in a lightweight and readable format.

---

## 14. How to Run the Prototype

A typical MicroPython deployment flow is:

1. Flash MicroPython firmware to a supported microcontroller board.
2. Upload `config.py` to the board.
3. Upload `main.py` to the board.
4. Replace placeholder Wi-Fi and MQTT values in `config.py`.
5. Run `main.py`.

Example file layout on the board:

```text
/
├── config.py
└── main.py
```

The actual setup process may vary depending on the board, firmware version, and MicroPython toolchain.

---

## 15. Recommended Use in the FOURUT Submission

This folder should be presented as an optional prototype only.

Recommended wording:

```text
The MicroPython implementation is provided as an optional supplementary prototype. 
It demonstrates the simplified edge-processing workflow, JSON data packaging, 
risk-classification logic, and MQTT publishing structure. The real deployed edge 
firmware is the Arduino C++ FreeRTOS implementation located in 
01_Arduino_Edge_Code/FOURUT_Edge_FreeRTOS.ino.
```

---

## 16. Summary

The MicroPython prototype provides a clean and readable representation of the FOURUT edge-device logic. It is useful for explaining the data flow from worker sensing to MQTT-based cloud communication.

However, the main deployed firmware remains the Arduino C++ FreeRTOS implementation because it provides stronger support for real sensors, secure MQTT communication, task scheduling, and real-time alarm handling.

Therefore, this prototype should be understood as a **supplementary academic and demonstration component**, not as the final production firmware.
