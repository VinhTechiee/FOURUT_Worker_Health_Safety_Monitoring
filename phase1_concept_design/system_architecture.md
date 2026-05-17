# System Overview

## 1. Purpose and Scope

The proposed system is a wearable Hybrid IoT platform designed for real-time health and safety monitoring of construction workers. Its main objective is to detect and respond to occupational risks that are difficult to identify through conventional Personal Protective Equipment alone, such as hypoxemia, abnormal heart rate, fall incidents, and exposure to hazardous fine particulate matter.

The system integrates physiological monitoring, motion-based fall detection, environmental sensing, edge processing, and hybrid wireless communication. By combining these functions in a compact wearable architecture, the system aims to support early risk detection, reduce emergency response latency, and improve worker protection in construction and industrial environments.

Unlike traditional medical monitoring devices, which are often unsuitable for manual labor, the proposed system is designed for continuous field deployment. It prioritizes low latency, wearable usability, network resilience, and autonomous operation under dynamic working conditions.

---

## 2. System Architecture

The system adopts a three-layer architecture consisting of:

1. **Sensing Layer**
2. **Processing Layer**
3. **Communication Layer**

This layered organization separates data acquisition, local intelligence, and external connectivity. As a result, the device can perform continuous monitoring during normal operation while still supporting rapid escalation when critical events are detected.

---

## 3. Sensing Layer

The sensing layer collects multimodal data from both the worker and the surrounding environment. It includes physiological sensing, inertial motion sensing, and environmental air-quality sensing.

### 3.1 Physiological Sensing

Physiological monitoring is performed using the **MAX30102** optical sensor. The sensor measures heart rate and blood oxygen saturation through reflective photoplethysmography.

The MAX30102 uses red and infrared light to detect blood-volume-related optical changes in tissue. These signals are processed to estimate:

- Heart rate
- Inter-beat interval
- Oxygen saturation
- Potential hypoxemia-related risk

This sensing module is essential for detecting internal physiological abnormalities that may not be externally visible during physical labor.

### 3.2 Motion Sensing

Motion monitoring is performed using the **MPU6050**, a six-degree-of-freedom inertial measurement unit. It includes:

- A three-axis accelerometer
- A three-axis gyroscope

The accelerometer provides motion data used to detect fall-like events. The system calculates Signal Vector Magnitude from acceleration values and evaluates the sequence of free fall, impact, and post-impact inactivity.

### 3.3 Environmental Sensing

Environmental monitoring is performed using a **PM2.5 sensor**. The sensor estimates fine particulate matter concentration based on optical light-scattering principles.

The measured PM2.5 concentration is used to evaluate air-quality risk and support occupational hazard assessment. This is particularly important in construction environments, where dust exposure can vary rapidly depending on work activity and site conditions.

---

## 4. Processing Layer

The processing layer is implemented on the **XIAO ESP32C6 microcontroller**. This layer is responsible for local data processing, feature extraction, threshold evaluation, and event classification.

Edge processing is a central design feature of the system. Instead of sending all raw data to the cloud for analysis, the ESP32C6 performs local computation directly on the wearable device. This approach reduces communication delay, improves reliability, and allows the system to respond even when external network infrastructure is unavailable.

### 4.1 Signal Conditioning

Raw sensor signals may contain noise caused by body movement, environmental interference, or temporary sensor instability. Therefore, the processing layer applies signal-conditioning techniques before event evaluation.

For physiological data, filtering is used to reduce baseline drift and motion artifacts. For PM2.5 data, smoothing methods such as moving averages or exponential moving averages may be applied to reduce short-term fluctuations.

### 4.2 Feature Extraction

After filtering, the system extracts meaningful features from each data stream.

For physiological monitoring, extracted features include:

- Heart rate
- SpO2 value
- Peak interval
- Abnormal physiological trends

For motion monitoring, extracted features include:

- Signal Vector Magnitude
- Free-fall pattern
- Impact peak
- Post-impact inactivity

For environmental monitoring, extracted features include:

- PM2.5 concentration
- Smoothed particulate level
- AQI-based risk category

### 4.3 Decision Logic

The processed features are evaluated using threshold-based decision logic. The system classifies events into different operating states, including normal monitoring, suspicious event detection, and emergency response.

This decision logic allows the wearable device to distinguish between routine variations and potentially dangerous events that require immediate action.

---

## 5. Communication Layer

The communication layer uses a hybrid communication architecture combining **Wi-Fi 6** and **4G LTE**.

The purpose of this dual-channel design is to separate routine data synchronization from emergency alert transmission. Routine data can tolerate minor delays, while emergency alerts must be delivered with minimal latency.

### 5.1 Wi-Fi 6 for Routine Monitoring

Under normal operating conditions, the ESP32C6 transmits non-critical biometric and environmental data through Wi-Fi 6. This channel is used for:

- Routine telemetry
- Dashboard updates
- Local gateway communication
- Cloud synchronization
- Historical monitoring

Wi-Fi 6 provides efficient data transmission in local environments and is suitable for regular monitoring tasks.

### 5.2 4G LTE for Emergency Alerts

For critical events, the system activates the **SIM768x 4G LTE module**. This module is used to send direct emergency alerts through:

- SMS
- Voice call
- Supervisor notification
- Emergency contact notification

The 4G module supports a cloud-bypassing emergency mechanism. This means that alerts can be sent directly without depending on local Wi-Fi infrastructure or cloud-based processing.

This design improves reliability in construction sites where Wi-Fi coverage may be unstable, unavailable, or delayed.

---

## 6. Operational Workflow

The system operates through four main phases:

1. Sensor data acquisition
2. Signal conditioning and feature extraction
3. Event evaluation and decision logic
4. Communication and response coordination

### 6.1 Phase 1: Sensor Data Acquisition

The wearable device continuously collects data from the integrated sensors.

The MAX30102 provides heart rate and SpO2-related optical signals. The MPU6050 provides acceleration and angular velocity data. The PM2.5 sensor provides environmental air-quality measurements.

The ESP32C6 coordinates these data streams and prepares them for local processing.

### 6.2 Phase 2: Signal Conditioning and Feature Extraction

After data acquisition, the ESP32C6 processes the sensor data to reduce noise and extract useful features.

This phase improves the reliability of measurements by addressing motion artifacts, signal drift, and short-term environmental fluctuation.

### 6.3 Phase 3: Event Evaluation

The extracted features are evaluated against predefined safety thresholds.

The system continuously checks for:

- Low SpO2
- Abnormal heart rate
- Fall-like motion pattern
- High PM2.5 concentration
- Hazardous air-quality level

If an abnormal condition is detected, the system determines whether it should trigger a warning or emergency response.

### 6.4 Phase 4: Alert and Communication

Depending on the severity of the detected event, the system activates one of several response mechanisms.

In normal conditions, data are uploaded through Wi-Fi. In suspicious cases, local indicators such as buzzer or LED alerts may be activated. In emergency cases, the system activates the SIM768x 4G module and sends SMS or voice-call alerts to supervisors or emergency contacts.

---

## 7. Operating Modes

The system supports three primary operating modes.

### 7.1 Normal Monitoring Mode

In normal monitoring mode, the system continuously collects sensor data, processes it locally, and transmits routine information through Wi-Fi.

This mode is used when no critical risk is detected.

### 7.2 Suspicious Event Detection Mode

Suspicious event detection mode is activated when the system detects minor or early-stage anomalies.

Examples include:

- Slight SpO2 decrease
- Temporary heart-rate irregularity
- Short environmental risk increase
- Unusual but unconfirmed motion pattern

In this mode, the system may activate local warning indicators to notify the worker or nearby personnel.

### 7.3 Emergency Response Mode

Emergency response mode is activated when a confirmed high-risk event is detected.

Examples include:

- Severe SpO2 drop
- Confirmed fall
- Sustained abnormal heart rate
- Hazardous PM2.5 level
- Loss of local Wi-Fi during a critical event

In this mode, the system activates local alerts and sends emergency notifications through the 4G module.

---

## 8. Risk Detection Logic

## 8.1 SpO2 Risk Detection

Blood oxygen saturation is monitored continuously. The system uses predefined thresholds to classify oxygen-related risk.

| Condition | Threshold | System Response |
|---|---:|---|
| Normal | SpO2 >= 90% | Continue monitoring |
| Warning | SpO2 < 90% | Notify supervisor or activate warning |
| Emergency | SpO2 < 80% | Trigger emergency response |

An SpO2 value below 90% indicates possible hypoxemia, while values below 80% represent a severe emergency condition.

### 8.2 Heart-Rate Risk Detection

Heart rate is evaluated to detect bradycardia and tachycardia.

| Condition | Threshold | System Response |
|---|---:|---|
| Bradycardia | BPM < 50 | Trigger warning |
| Tachycardia | BPM > 120 for more than 15 seconds | Trigger warning or emergency evaluation |

The tachycardia threshold is sustained over time to reduce false alarms during physical labor.

### 8.3 Fall Detection

Fall detection is based on acceleration data from the MPU6050. The system computes Signal Vector Magnitude using three-axis acceleration values.

The fall-detection logic evaluates three sequential stages:

1. **Free fall**
   - Signal Vector Magnitude drops below the free-fall threshold.

2. **Impact**
   - Signal Vector Magnitude rises sharply above the impact threshold.

3. **Post-impact inactivity**
   - The worker remains inactive after impact.

A fall alert is generated when these stages occur within a defined time window.

Typical parameters include:

| Parameter | Value | Description |
|---|---:|---|
| Free-fall threshold | 0.5g | Detects loss of body support |
| Impact threshold | 3g | Detects sudden deceleration |
| Fall time window | 1.5 seconds | Maximum interval between free fall and impact |
| Inactivity duration | 3 seconds | Minimum post-impact inactivity period |

### 8.4 PM2.5 Risk Detection

The PM2.5 sensor provides particulate matter concentration data. The system filters the measured value and maps it to AQI-based risk categories.

| AQI Range | Risk Level | Recommended Response |
|---:|---|---|
| AQI < 100 | Normal | Continue monitoring |
| 100 <= AQI <= 150 | Moderate | Limit prolonged exposure |
| 150 < AQI <= 300 | Alert | Recommend protective equipment |
| AQI > 300 | Hazardous | Trigger emergency action or evacuation |

This classification enables the system to detect hazardous environmental conditions and support protective decision-making.

---

## 9. Hardware Components

The system integrates five main hardware components.

| Component | Measurement / Function | Role in System |
|---|---|---|
| MAX30102 | Heart rate and SpO2 | Provides optical physiological sensing using red and infrared LEDs |
| MPU6050 | Six-degree-of-freedom motion tracking | Supports fall detection using acceleration and angular velocity data |
| PM2.5 Sensor | Particulate matter concentration | Measures airborne dust exposure |
| XIAO ESP32C6 | Central microcontroller | Executes local processing, decision logic, and communication control |
| SIM768x 4G LTE Module | Emergency communication | Sends SMS and voice-call alerts during critical events |

---

## 10. Power Management Strategy

Energy efficiency is a key design requirement because the system is intended for continuous wearable operation.

The power-management strategy includes:

- Component-level duty cycling
- Deep sleep mode
- Interrupt-driven wake-up
- Selective activation of high-current modules
- Hybrid communication energy optimization

### 10.1 Duty Cycling

High-power components such as the PM2.5 sensor and SIM768x 4G module are not kept active continuously. Instead, they are activated only when required.

The PM2.5 sensor can operate intermittently for environmental sampling. The SIM768x module is activated mainly during emergency events or when Wi-Fi connectivity is unavailable.

### 10.2 Interrupt-Driven Wake-Up

The ESP32C6 can remain in a low-power state during idle periods. The MPU6050 can act as a wake-up source when abnormal motion is detected.

This mechanism reduces unnecessary energy consumption while maintaining responsiveness to possible fall events.

### 10.3 Communication Energy Optimization

Wi-Fi is used for routine telemetry because it is more suitable for regular local data synchronization. The 4G module is reserved for critical alerts because cellular communication consumes more power.

This tiered communication strategy balances energy efficiency and emergency reliability.

---

## 11. Safety and Reliability Considerations

The system is designed for safety-critical operation in construction and industrial environments.

Key reliability features include:

- Local edge processing
- Hybrid Wi-Fi and 4G communication
- Cloud-bypassing emergency alerts
- Local buzzer and LED warning
- Threshold-based risk classification
- Event-driven power management

Edge processing ensures that the system can detect risks without waiting for cloud computation. Hybrid communication ensures that emergency messages can still be delivered when local Wi-Fi fails. Local alerts provide immediate feedback to the worker and nearby personnel.

Together, these features improve the system’s resilience under unstable field conditions.

---

## 12. Summary

The proposed system is a wearable Hybrid IoT platform for construction worker safety monitoring. It combines physiological sensing, fall detection, PM2.5 monitoring, local edge processing, and hybrid wireless communication.

By using the ESP32C6 as the central controller and the SIM768x module as an emergency communication backbone, the system provides both routine monitoring and reliable emergency response.

The architecture emphasizes:

- Real-time risk detection
- Low-latency emergency alerts
- Wearable deployment
- Network resilience
- Energy-aware operation
- Practical suitability for construction environments

Overall, the system provides a scalable and practical framework for proactive occupational health and safety management.