# FOURUT Worker Health and Safety Monitoring System

A two-phase wearable IoT research project for real-time health, fatigue, fall-risk, and safety monitoring of outdoor construction and infrastructure workers.

---

## Table of Contents

- [Overview](#overview)
- [Project Motivation](#project-motivation)
- [Sustainable Development Goals](#sustainable-development-goals)
- [Development Roadmap](#development-roadmap)
- [Phase 1 — Concept and System Design](#phase-1--concept-and-system-design)
- [Phase 2 — Functional Prototype and Cloud Integration](#phase-2--functional-prototype-and-cloud-integration)
- [System Comparison Between Phase 1 and Phase 2](#system-comparison-between-phase-1-and-phase-2)
- [Repository Structure](#repository-structure)
- [Core Technical Features](#core-technical-features)
- [Hardware Overview](#hardware-overview)
- [Embedded Firmware Overview](#embedded-firmware-overview)
- [Cloud Architecture Overview](#cloud-architecture-overview)
- [MQTT Topic Design](#mqtt-topic-design)
- [Demonstration and Validation](#demonstration-and-validation)
- [Security, Privacy, and Ethics](#security-privacy-and-ethics)
- [Limitations](#limitations)
- [Future Work](#future-work)
- [Authors](#authors)
- [License](#license)

---

## Overview

**FOURUT Worker Health and Safety Monitoring System** is a wearable Internet of Things (IoT) project designed to support real-time safety supervision for construction and outdoor infrastructure workers.

The system aims to detect and report worker-level safety risks such as:

- Abnormal heart rate
- Low blood oxygen saturation
- Fall-related incidents
- Fatigue-related physiological decline
- Environmental dust exposure
- Delayed emergency response

The project was developed through two technical phases:

| Phase | Main Purpose | Technical Maturity |
|---|---|---|
| **Phase 1** | Concept and system design | Architecture-level proposal |
| **Phase 2** | Functional prototype and cloud integration | Implemented edge-cloud proof-of-concept |

Phase 1 defines the original hybrid IoT vision, including biometric monitoring, fall detection, PM2.5 sensing, Wi-Fi/4G communication, and emergency response logic.

Phase 2 implements the core wearable prototype using an ESP32-C3, physiological and motion sensors, local buzzer alerting, Wi-Fi/MQTT telemetry, AWS IoT cloud ingestion, Lambda processing, data storage, and notification services.

---

## Project Motivation

Construction workers often operate in physically demanding and hazardous environments. They may be exposed to fall risks, cardiovascular stress, heat stress, airborne dust, fatigue, and delayed emergency response. Conventional occupational safety measures, such as personal protective equipment and manual supervision, are essential but mostly passive.

These traditional approaches have several limitations:

1. **They cannot continuously monitor internal physiological deterioration.**
   - A supervisor may not immediately recognize low SpO2, abnormal heart rate, or fatigue-related decline.

2. **They depend heavily on human observation.**
   - Accidents or early warning signs may be missed in large or noisy construction sites.

3. **They may delay emergency response.**
   - A fall or hypoxemia event may not be reported immediately if the worker is isolated.

4. **They provide limited traceability.**
   - Manual safety records are often incomplete, delayed, or difficult to analyze.

This project addresses these limitations by designing a wearable IoT system that combines:

- **Continuous sensing**
- **Edge-level risk detection**
- **Immediate local warning**
- **Cloud-based monitoring**
- **Structured safety-event records**
- **Supervisor notification**

The intended outcome is a safer and more proactive worker-protection workflow.

---

## Sustainable Development Goals

This project aligns with the United Nations Sustainable Development Goals:

### SDG 3 — Good Health and Well-being

The system supports preventive worker health protection by monitoring heart rate, SpO2, fall-risk events, and fatigue-related indicators. Local alerts and cloud notifications can reduce response time when abnormal conditions occur.

### SDG 11 — Sustainable Cities and Communities

Construction and infrastructure work are essential to urban development. By improving worker safety visibility and incident traceability, the system contributes to safer and more resilient infrastructure development practices.

---

## Development Roadmap

The project is organized into two development phases.

```text
Phase 1: Concept and System Design
        |
        |-- Hybrid IoT wearable architecture
        |-- Biometric, motion, and PM2.5 sensing concept
        |-- Wi-Fi / 4G emergency communication design
        |-- Edge decision logic
        |-- Cloud and dashboard workflow proposal
        |
        v
Phase 2: Functional Prototype and Cloud Integration
        |
        |-- ESP32-C3 wearable prototype
        |-- MAX30102 heart-rate and SpO2 sensing
        |-- MPU6050 motion sensing and fall-risk logic
        |-- HRV-based fatigue analysis
        |-- Local buzzer and cancel button
        |-- Wi-Fi / MQTT telemetry
        |-- AWS IoT Core, Lambda, DynamoDB, S3, SNS
```

---

## Phase 1 — Concept and System Design

### Phase 1 Objective

Phase 1 defines the initial concept of a low-latency hybrid IoT wearable for construction worker health and safety monitoring.

The main objective of this phase is to design a system architecture capable of:

- Monitoring worker physiological status
- Detecting fall-related motion events
- Monitoring PM2.5 environmental exposure
- Performing local risk evaluation at the edge
- Sending routine data through Wi-Fi
- Sending critical alerts through 4G emergency communication
- Supporting cloud or dashboard-based supervision

At this stage, the project focuses on architecture, sensing principles, communication workflow, and system feasibility rather than a fully validated field deployment.

### Phase 1 System Concept

The Phase 1 design proposes a wearable system built around a hybrid communication and edge-processing architecture.

The proposed system includes:

| Subsystem | Function |
|---|---|
| Biometric sensing | Measures heart rate and SpO2 |
| Motion sensing | Detects abnormal movement and fall patterns |
| Environmental sensing | Monitors PM2.5 dust concentration |
| Edge computing | Filters data, extracts features, and evaluates thresholds |
| Local alerting | Warns the worker or nearby personnel through buzzer or LED |
| Wi-Fi communication | Sends routine telemetry to cloud or local gateway |
| 4G communication | Sends emergency SMS or voice-call alerts |
| Cloud/dashboard layer | Supports remote monitoring and safety management |

### Phase 1 Operational Workflow

The Phase 1 workflow is divided into four conceptual stages.

#### 1. Sensor Data Acquisition

The wearable device continuously collects data from multiple sensors:

- MAX30102 for heart rate and SpO2
- MPU6050 for acceleration and motion tracking
- PM2.5 sensor for air-quality monitoring

These data streams provide a multimodal view of worker condition, including physiological, motion-related, and environmental risk factors.

#### 2. Signal Conditioning and Feature Extraction

The ESP32-C6 microcontroller performs local signal processing to improve data reliability.

This includes:

- Noise reduction
- Motion-artifact handling
- Moving-average smoothing
- Feature extraction
- Threshold preparation

This stage is important because wearable sensors are exposed to vibration, movement, sweat, unstable contact, and outdoor environmental noise.

#### 3. Event Evaluation and Decision Logic

After preprocessing, the system evaluates whether the worker is in a normal, suspicious, or emergency condition.

The decision logic checks for:

- Low SpO2
- Abnormal heart rate
- Fall-like acceleration pattern
- Hazardous dust exposure
- Combined biometric and environmental risk

When a risk is detected, the system triggers a local alert and prepares a communication response.

#### 4. Communication, Cloud, and User Interface

The system uses a hybrid communication model:

- Wi-Fi / MQTT or HTTP for normal telemetry
- SIM768x 4G LTE for critical emergency alerts

The purpose of this design is to avoid depending entirely on cloud connectivity during emergencies. If Wi-Fi is unstable or unavailable, the SIM768x module can send SMS or voice-call alerts directly to a supervisor or emergency contact.

### Phase 1 Proposed Hardware

| Component | Measurement / Function | Role in System |
|---|---|---|
| XIAO ESP32-C6 | Central MCU | Performs sensing coordination, filtering, decision logic, and communication control |
| MAX30102 | Heart rate and SpO2 | Provides reflective PPG-based physiological monitoring |
| MPU6050 | 6-DOF motion sensing | Provides acceleration and gyroscope data for fall-risk detection |
| PM2.5 Sensor | Fine-dust concentration | Measures airborne particulate risk in construction environments |
| SIM768x 4G LTE Module | SMS / voice-call communication | Provides emergency communication when Wi-Fi or cloud access is unreliable |
| Local Buzzer / LED | Local alerting | Warns worker and nearby personnel immediately |
| Battery System | Portable power | Supports wearable operation |

### Phase 1 Biometric Monitoring Concept

The Phase 1 design uses the MAX30102 optical sensor for reflective photoplethysmography.

The sensor estimates:

- Heart rate
- Blood oxygen saturation
- Pulse-related optical signal changes

The intended physiological alert thresholds include:

| Condition | Proposed Threshold | Meaning |
|---|---|---|
| SpO2 Warning | SpO2 < 90% | Possible hypoxemia |
| SpO2 Emergency | SpO2 < 80% | Severe oxygen desaturation risk |
| Bradycardia Alert | BPM < 50 | Abnormally low heart rate |
| Tachycardia Alert | BPM > 120 | Elevated heart rate during work |

These thresholds provide early safety warnings, but they should be calibrated carefully for real deployments because physical labor can naturally elevate heart rate.

### Phase 1 Fall Detection Concept

Fall detection is based on the MPU6050 accelerometer and gyroscope.

The core motion feature is Signal Vector Magnitude:

```text
SVM = sqrt(ax² + ay² + az²)
```

The proposed fall pattern consists of three stages:

1. **Free-fall phase**
   - The body loses support.
   - SVM drops close to 0g.

2. **Impact phase**
   - The body hits the ground or another surface.
   - SVM spikes above a high threshold.

3. **Post-impact inactivity**
   - The worker remains still after impact.
   - This may indicate injury or unconsciousness.

The conceptual fall alert condition can be described as:

```text
Fall Alert = Free-fall detected
             AND impact detected within a time window
             AND inactivity detected after impact
```

### Phase 1 PM2.5 Monitoring Concept

The Phase 1 architecture includes PM2.5 sensing to evaluate air-quality risks in dusty construction environments.

The PM2.5 subsystem is intended to:

- Measure fine particulate concentration
- Smooth short-term fluctuations using filtering
- Map PM2.5 concentration to air-quality risk levels
- Trigger warnings when exposure becomes unsafe

A simplified risk-level model is:

| AQI Level | Interpretation | Suggested Response |
|---|---|---|
| AQI < 100 | Normal | Continue monitoring |
| 100 ≤ AQI ≤ 150 | Moderate | Reduce prolonged exposure |
| 150 < AQI ≤ 300 | Alert | Use protective equipment |
| AQI > 300 | Hazardous | Evacuate or suspend dust-generating tasks |

### Phase 1 Communication Design

Phase 1 proposes a redundant communication strategy.

#### Tier 1 — Routine Data Synchronization

Routine worker data are transmitted using Wi-Fi.

Examples of routine data include:

- Heart rate
- SpO2
- Motion status
- PM2.5 concentration
- Device state
- Timestamp

#### Tier 2 — Emergency Broadcasting

Critical alerts are transmitted using the SIM768x 4G LTE module.

Emergency communication may include:

- SMS notification
- Voice-call alert
- Worker ID
- Emergency category
- Last known sensor status
- Event timestamp

This tiered model is intended to improve reliability in construction sites where local Wi-Fi may be unstable.

### Phase 1 Power-Management Strategy

The conceptual design also considers wearable power constraints.

Power-saving strategies include:

- Duty cycling high-power sensors
- Keeping the PM2.5 sensor inactive except during sampling windows
- Activating the SIM768x module only during critical alerts
- Using accelerometer interrupts to wake the microcontroller
- Supporting future solar or energy-harvesting extensions

This is important because wearable devices must operate across long work shifts while remaining compact and comfortable.

### Phase 1 Status

Phase 1 should be interpreted as a system design and architecture proposal.

It establishes the technical foundation of the project but does not represent the final working prototype. Some Phase 1 features, especially PM2.5 sensing and SIM768x-based 4G emergency communication, are treated as part of the complete system vision and are later marked as planned or simulated extensions in Phase 2.


## Phase 2 — Functional Prototype and Cloud Integration

### Phase 2 Objective

Phase 2 transforms the conceptual architecture into a functional proof-of-concept prototype.

The main objective of Phase 2 is to demonstrate that a wearable device can:

- Acquire heart-rate and SpO2 data
- Acquire motion data
- Detect physiological and motion-related risk conditions
- Estimate fatigue risk using HRV-related indicators
- Activate local warning through a buzzer
- Allow manual alarm cancellation
- Publish telemetry and alerts through Wi-Fi/MQTT
- Integrate with AWS IoT Core
- Process worker data in AWS Lambda
- Store structured records in DynamoDB
- Save historical logs in Amazon S3
- Send notifications through Amazon SNS

### Phase 2 Implemented Prototype

The Phase 2 prototype uses the following implemented components:

| Component | Status | Function |
|---|---|---|
| XIAO ESP32-C3 | Implemented | Main edge controller |
| MAX30102 | Implemented | Heart-rate and SpO2 monitoring |
| MPU6050 | Implemented | Motion sensing and fall-risk detection |
| Active Buzzer | Implemented | Local warning output |
| Cancel Button | Implemented | Manual alarm cancellation |
| Wi-Fi / MQTT | Implemented | Cloud telemetry and alert publishing |
| AWS IoT Core | Implemented | MQTT message ingestion |
| AWS Lambda | Implemented | Cloud-side processing and classification |
| DynamoDB | Implemented | Structured worker data storage |
| Amazon S3 | Implemented | Historical JSON log storage |
| Amazon SNS | Implemented | Email notification for danger or emergency events |
| PM2.5 Sensor | Planned / simulated | Future physical air-quality monitoring |
| SIM768x 4G LTE | Planned / simulated | Future SMS or voice-call emergency communication |

### Phase 2 Edge-First Design

The Phase 2 prototype follows an edge-first safety architecture.

This means that safety-critical decisions are made locally on the ESP32-C3 before data are transmitted to the cloud.

This design is important because:

- Local alarms should not wait for cloud response.
- Safety warnings should still work when the network is delayed.
- The device can immediately notify the worker or nearby personnel.
- Cloud services can be used for monitoring, logging, analytics, and notification.

### Phase 2 Firmware Architecture

The embedded firmware is implemented using Arduino C++ with FreeRTOS-style task separation.

The firmware is organized into three main tasks.

#### 1. Anomaly Detection Task

This is the highest-priority safety task.

It is responsible for:

- Reading MAX30102 data
- Reading MPU6050 data
- Validating sensor readings
- Computing motion features
- Estimating HRV-related values
- Checking physiological thresholds
- Checking fall-risk logic
- Generating warning or emergency events

#### 2. Alarm Control Task

This task controls local warning behavior.

It is responsible for:

- Activating the buzzer
- Handling the cancel button
- Silencing false alarms when needed
- Keeping sensor monitoring active even after alarm cancellation

#### 3. Cloud Publish Task

This task handles cloud communication.

It is responsible for:

- Maintaining Wi-Fi connection
- Maintaining MQTT connection
- Publishing telemetry payloads
- Publishing HRV summary payloads
- Publishing alert payloads

This separation prevents slow network operations from blocking local safety detection.

### Phase 2 Sensor Processing

#### Heart Rate and SpO2

The MAX30102 provides heart-rate and SpO2 readings.

The firmware checks whether the readings are physiologically plausible before using them in safety classification.

Implemented physiological risk thresholds include:

| Condition | Threshold | System Response |
|---|---|---|
| SpO2 warning | SpO2 < 90% | Hypoxemia warning |
| Critical SpO2 | SpO2 < 80% | High-severity emergency alert |
| Bradycardia warning | BPM < 50 | Physiological warning |
| Tachycardia warning | BPM > 120 | Physiological warning |

#### Motion and Fall-Risk Detection

The MPU6050 provides acceleration and gyroscope data.

The firmware computes Signal Vector Magnitude:

```text
SVM = sqrt(ax² + ay² + az²)
```

The implemented fall-risk logic checks for:

- Free-fall phase
- Impact phase
- Post-impact confirmation

Implemented motion thresholds include:

| Condition | Threshold | Meaning |
|---|---|---|
| Free-fall phase | SVM < 4.9 m/s² | Possible loss of support |
| Impact phase | SVM > 29.4 m/s² within 1500 ms | Possible impact after free fall |
| Post-impact confirmation | 3000 ms waiting period | Confirms fall-risk emergency |

#### HRV-Based Fatigue Monitoring

Phase 2 adds fatigue analysis using HRV-related indicators.

Because the prototype uses heart-rate readings rather than direct beat-to-beat PPG timestamps, the inter-beat interval is approximated as:

```text
IBI = 60000 / BPM
```

The firmware computes:

| Indicator | Meaning |
|---|---|
| SDNN | Overall heart-rate variability |
| RMSSD | Short-term HRV indicator used for fatigue-warning logic |

The fatigue-alert logic is:

| Fatigue Condition | Threshold | Response |
|---|---|---|
| Fatigue Warning | RMSSD < 20 ms for three consecutive windows | Level-1 warning |
| Exhaustion Risk | RMSSD < 15 ms and HR > 110 BPM | Level-2 emergency alert |

This implementation demonstrates the fatigue-monitoring workflow. For production-grade HRV analysis, beat-to-beat PPG peak timestamps should be used instead of BPM-derived IBI approximation.

---

## Cloud Architecture Overview

The Phase 2 cloud system uses AWS services to process worker telemetry and alerts.

The implemented cloud pipeline is:

```text
ESP32-C3 Edge Device
        |
        | MQTT over TLS
        v
AWS IoT Core
        |
        | IoT Rule: SELECT * FROM 'worker/+/+'
        v
AWS Lambda: WorkerDataProcessor
        |
        |-- Classifies worker condition
        |-- Normalizes payload format
        |-- Extracts worker ID and timestamp
        |-- Generates danger-level result
        |
        |--> DynamoDB: structured records
        |--> Amazon S3: historical JSON logs
        |--> Amazon SNS: danger/emergency email notification
```

### Cloud Components

| AWS Service | Role |
|---|---|
| AWS IoT Core | Receives MQTT messages from the ESP32-C3 device |
| AWS IoT Rule | Captures worker-related topics and forwards payloads to Lambda |
| AWS Lambda | Processes, normalizes, and classifies worker data |
| DynamoDB | Stores structured worker health and safety records |
| Amazon S3 | Stores historical JSON logs for traceability |
| Amazon SNS | Sends supervisor email notifications for danger and emergency states |

---

## MQTT Topic Design

The MQTT topic pattern is:

```text
worker/{worker_id}/{message_type}
```

For the current prototype worker ID `W001`, the implemented topics are:

| Topic | Purpose |
|---|---|
| `worker/W001/telemetry` | Publishes periodic worker status |
| `worker/W001/hrv` | Publishes HRV summary data |
| `worker/W001/alert` | Publishes warning, danger, and emergency events |

The AWS IoT MQTT test client can subscribe to:

```text
worker/#
```

to observe all worker-related messages.

### Example Data Flow

A typical end-to-end data cycle is:

1. MAX30102 and MPU6050 collect physiological and motion data.
2. ESP32-C3 validates sensor readings.
3. ESP32-C3 computes safety-related features such as SVM, SDNN, and RMSSD.
4. The firmware evaluates local risk thresholds.
5. If a warning or emergency is detected, the buzzer is activated.
6. The device generates a JSON payload.
7. The payload is published to AWS IoT Core over MQTT/TLS.
8. An AWS IoT Rule forwards the message to Lambda.
9. Lambda classifies the worker state.
10. DynamoDB stores structured records.
11. S3 stores historical JSON logs.
12. SNS sends an email notification for danger or emergency cases.

---

## System Comparison Between Phase 1 and Phase 2

| Category | Phase 1 — Concept Design | Phase 2 — Functional Prototype |
|---|---|---|
| Main goal | Define full hybrid IoT system architecture | Demonstrate core edge-cloud monitoring |
| Main MCU | XIAO ESP32-C6 | XIAO ESP32-C3 |
| Physiological sensor | MAX30102 | MAX30102 implemented |
| Motion sensor | MPU6050 | MPU6050 implemented |
| PM2.5 sensing | Proposed | Planned / simulated |
| Emergency 4G module | SIM768x proposed | Planned / simulated |
| Local alert | Buzzer / LED proposed | Active buzzer implemented |
| Alarm cancellation | Reset/cancel concept | Cancel button implemented |
| Communication | Wi-Fi / 4G hybrid design | Wi-Fi / MQTT implemented |
| Cloud platform | General cloud/dashboard concept | AWS IoT Core, Lambda, DynamoDB, S3, SNS |
| Fatigue monitoring | Not the central focus | HRV-based fatigue analysis added |
| Validation | Architecture-level design | Controlled prototype testing |

---
 
## Repository Structure

A clean repository structure is recommended as follows:

```text
FOURUT_Worker_Health_Safety_Monitoring/
├── README.md
├── docs/
│   ├── NL26_Round1_FOURUT.pdf
│   └── NL26_Round2_FOURUT.pdf
├── phase1_concept_design/
│   ├── README.md
│   ├── system_architecture.md
│   ├── hardware_design.md
│   ├── communication_workflow.md
│   ├── power_management.md
│   └── figures/
├── phase2_prototype/
│   ├── 01_Arduino_Edge_Code/
│   ├── 02_AWS_Lambda_Code/
│   ├── 03_AWS_Config/
│   ├── 04_Test_Events/
│   ├── 05_Documentation/
│   └── 06_Optional_MicroPython_Prototype/
├── demo/
│   └── NL26_Round2_FOURUT.mp4
│
└── README_assets/
```

### Folder Description

| Folder | Description |
|---|---|
| `docs/` | Stores official Round 1 and Round 2 reports |
| `phase1_concept_design/` | Contains conceptual design documents from Phase 1 |
| `phase1_concept_design/figures/` | Stores Phase 1 architecture and workflow diagrams |
| `phase2_prototype/01_Arduino_Edge_Code/` | Contains embedded firmware for the ESP32-C3 prototype |
| `phase2_prototype/02_AWS_Lambda_Code/` | Contains AWS Lambda function code |
| `phase2_prototype/03_AWS_Config/` | Contains AWS IoT rule and cloud configuration references |
| `phase2_prototype/04_Test_Events/` | Contains JSON test events for validation |
| `phase2_prototype/05_Documentation/` | Contains supporting implementation documentation |
| `phase2_prototype/06_Optional_MicroPython_Prototype/` | Contains optional MicroPython prototype materials |
| `demo/` | Stores demonstration video and screenshots |
| `README_assets/` | Stores diagrams and images used in this README |

---

## Core Technical Features

The project demonstrates the following technical features:

### Edge Monitoring

- Real-time sensor acquisition
- Local physiological screening
- Fall-risk detection
- HRV-based fatigue estimation
- Local alarm activation

### Hybrid Safety Response

- Immediate buzzer warning at the device level
- Cloud-assisted monitoring for supervisors
- Planned 4G/SMS emergency communication extension

### Cloud Integration

- MQTT/TLS telemetry transmission
- AWS IoT Core message ingestion
- Lambda-based event processing
- DynamoDB structured storage
- S3 historical logging
- SNS supervisor notification

### Worker-Safety Intelligence

- Normal, warning, danger, and emergency classification
- Test-event validation
- Traceable records for later review
- Extensible architecture for dashboard visualization

---

## Demonstration and Validation

Phase 2 was validated through controlled test scenarios.

| Scenario | Input Example | Expected Cloud State | SNS Notification |
|---|---|---|---|
| Normal state | HR = 82 BPM, SpO2 = 97%, SVM = 9.8, RMSSD = 35.2 | NORMAL | No |
| Biometric danger | HR = 118 BPM, SpO2 = 88%, SVM = 9.7, RMSSD = 16.2 | DANGER | Yes |
| Fall emergency | HR = 126 BPM, SpO2 = 93%, SVM = 31.2, RMSSD = 14.1, fall = true | EMERGENCY | Yes |
| Exhaustion risk | HR = 116 BPM, SpO2 = 94%, SVM = 9.8, RMSSD = 13.8 | EMERGENCY | Yes |

The validation confirms the core edge-cloud workflow:

```text
Sensor Reading
    -> Local Risk Evaluation
    -> Buzzer Activation
    -> MQTT Publishing
    -> AWS IoT Rule Forwarding
    -> Lambda Classification
    -> DynamoDB Storage
    -> S3 Logging
    -> SNS Notification
```

---

## Security, Privacy, and Ethics

This project is designed for worker protection, not disciplinary surveillance.

### Data Security

The communication layer should use MQTT over TLS. Sensitive information such as AWS certificates, private keys, account IDs, and credentials should never be hard-coded in public source files.

### Least-Privilege Cloud Access

Cloud permissions should follow the principle of least privilege. For example, the Lambda execution role should only receive the permissions required for:

- Writing records to DynamoDB
- Writing logs to S3
- Publishing notifications to SNS

### Worker Privacy

Worker data should be collected with informed consent. Identifiable information should only be accessible to authorized supervisors. Data used for analytics should be anonymized whenever possible.

### Ethical Use

Physiological and fatigue data should not be used to punish workers. The intended use is to support:

- Rest-break recommendations
- Hydration and recovery
- Emergency response
- Safer task allocation
- Preventive safety management

---

## Limitations

The current implementation is a proof-of-concept and has the following limitations:

### Controlled Testbench Validation

The system has not yet been validated through long-term field deployment on construction sites.

### PM2.5 Sensing Not Physically Integrated

PM2.5 monitoring remains a planned or simulated extension in the current prototype.

### SIM768x Emergency Communication Not Physically Integrated

SMS or voice-call emergency communication is represented as planned or simulated logic.

### Approximate HRV Estimation

HRV analysis currently uses BPM-derived IBI approximation.

Production-grade HRV should use beat-to-beat PPG timestamps.

### Wearable Enclosure Not Finalized

The current form factor needs improvement for comfort, durability, sweat resistance, and impact protection.

### Battery Life Constraints

Longer work shifts require improved duty cycling, battery optimization, or energy-harvesting support.

### Threshold Personalization

Current thresholds are rule-based.

Personalized baselines may improve fatigue and abnormal-condition detection.

---

## Future Work

Future development will focus on:

- Physical integration of the PM2.5 sensor
- Physical integration of SIM768x 4G/SMS emergency alerts
- Improved PPG peak detection for production-grade HRV
- Personalized fatigue detection using worker-specific baselines
- Cloud-to-device threshold updates
- Real-time supervisor dashboard
- GNSS or UWB-based location tracking
- Battery optimization and low-power firmware design
- Ruggedized wearable enclosure
- Solar or thin-film battery support
- Multi-worker deployment testing
- Longitudinal field studies at real construction sites

---

## Academic and Engineering Contributions

This project contributes a practical edge-cloud IoT framework for worker safety monitoring.

The main contributions are:

### A Two-Phase Development Model

The project clearly evolves from conceptual architecture to functional prototype.

### A Wearable Edge-First Safety Design

Safety-critical alerts are processed locally before cloud transmission.

### A Multimodal Sensing Approach

The system combines physiological sensing, motion sensing, and planned environmental sensing.

### A Hybrid Supervision Model

Local alerts support immediate response, while cloud processing supports supervisor visibility.

### An HRV-Based Fatigue-Monitoring Extension

Phase 2 introduces SDNN and RMSSD-based fatigue-warning logic.

### An AWS-Based Cloud Processing Workflow

The prototype demonstrates MQTT ingestion, Lambda classification, DynamoDB storage, S3 logging, and SNS notification.

---
## Demonstration Video

The full demonstration video is not stored directly in this repository because of GitHub file-size limitations.

Demo video link: https://drive.google.com/drive/folders/1aB-XhZCt7b9TdDc2LUzQUkd2hUs8xxb0

---

## Authors

- Anh Khoa Ho
- Hoang Nam Nguyen Le
- Khanh Duy Nguyen Le
- Hien Vinh Le

Ho Chi Minh University of Technology  
Vietnam National University Ho Chi Minh City  
HCMUT, VNU-HCM

---

## License

This repository is intended for academic research, educational demonstration, and prototype development.

A formal open-source license should be added before public reuse, redistribution, or commercial deployment.
