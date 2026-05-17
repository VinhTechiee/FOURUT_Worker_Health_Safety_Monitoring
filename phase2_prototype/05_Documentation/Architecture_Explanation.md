# FOURUT System Architecture Explanation

## 1. Overview

FOURUT follows a **hybrid edge-cloud architecture** designed for worker health and safety monitoring.

The system combines:

- **Edge computing** for immediate safety decisions
- **Cloud computing** for centralized monitoring, storage, notification, and historical analysis
- **Local emergency response** for cases where fast action is required or cloud connectivity is unavailable

This architecture is suitable for safety-related environments because it avoids depending entirely on the cloud for emergency detection. The edge device performs local screening and can trigger alarms immediately, while the cloud layer provides long-term data processing and supervisor notification.

---

## 2. Architecture Summary

The FOURUT system is organized into four main layers:

```text
Sensor Layer
    ↓
Edge Device Layer
    ↓
Cloud Processing Layer
    ↓
Dashboard and Notification Layer
```

Each layer has a specific responsibility.

| Layer | Main Responsibility |
|---|---|
| Sensor Layer | Collect biometric, motion, and environmental data |
| Edge Device Layer | Perform local screening, detect emergency conditions, and publish MQTT messages |
| Cloud Processing Layer | Classify worker condition, store data, and trigger cloud-side workflows |
| Dashboard and Notification Layer | Display worker status and notify supervisors |

---

## 3. System Workflow Diagram

The system workflow is represented in the diagram below.

![FOURUT System Workflow](system_workflow.png)

The diagram shows three major workflows:

1. **HRV / Fatigue Monitoring**
2. **Hybrid Cloud Workflow**
3. **Edge-only Emergency Workflow**

These workflows run together to provide both immediate local protection and cloud-based monitoring.

---

## 4. Sensor Layer

The sensor layer collects raw physical signals from the worker and the surrounding environment.

### 4.1 MAX30102 Sensor

The MAX30102 sensor is used for biometric monitoring.

It provides:

- Heart-rate data
- SpO2 data
- PPG-related signals
- IBI-related data for HRV analysis

In the main firmware, the MAX30102 data is used for heart-rate monitoring, oxygen-saturation monitoring, and fatigue-risk estimation.

### 4.2 MPU6050 Sensor

The MPU6050 sensor is used for motion monitoring.

It provides acceleration data that can be used to detect:

- Sudden movement
- Free fall
- Impact
- Post-impact inactivity

The accelerometer values are used to calculate Signal Vector Magnitude, also called SVM:

```text
SVM = sqrt(ax² + ay² + az²)
```

SVM is used as the main motion indicator for fall-detection logic.

### 4.3 PM2.5 or Air-Quality Sensor

The air-quality sensor is used to monitor environmental risk.

It can provide information related to:

- Dust concentration
- Poor air quality
- Unsafe working conditions

This data helps the system detect environmental hazards in addition to biometric and motion-related risks.

---

## 5. Edge Device Layer

The edge device is the central embedded controller of the system.

In the FOURUT implementation, the real deployed firmware is located at:

```text
01_Arduino_Edge_Code/FOURUT_Edge_FreeRTOS.ino
```

The edge device is responsible for:

- Reading sensor data
- Filtering or preprocessing sensor values
- Running local anomaly detection
- Detecting biometric risk
- Detecting fall events
- Estimating fatigue risk
- Activating local alarms
- Publishing MQTT payloads to AWS IoT Core

The edge layer is important because it allows safety-critical actions to happen immediately without waiting for cloud processing.

---

## 6. Edge-First Safety Design

FOURUT uses an **edge-first safety model**.

This means that the ESP32-based edge device does not only collect data and send it to the cloud. It also performs local screening and can trigger an alarm directly.

This design provides two important benefits:

1. **Low latency emergency response**  
   The buzzer or local alert can be activated immediately when a dangerous event is detected.

2. **Reduced dependency on internet connectivity**  
   If Wi-Fi, MQTT, or cloud services are temporarily unavailable, the edge device can still perform local emergency detection.

This is especially important for worker-safety applications, where delayed response may increase risk.

---

## 7. HRV and Fatigue Monitoring Workflow

The HRV and fatigue-monitoring workflow uses biometric signals from the MAX30102 sensor.

The process follows this structure:

```text
MAX30102
    ↓
IBI collection
    ↓
Median filtering
    ↓
HRV calculation
    ↓
SDNN and RMSSD extraction
    ↓
Fatigue-risk classification
```

### 7.1 IBI Collection

IBI stands for Inter-Beat Interval.

It represents the time interval between two consecutive heartbeats.

IBI values are useful because fatigue and stress may affect the variability of heartbeats.

### 7.2 Median Filtering

Raw IBI values may contain noise caused by:

- Poor sensor contact
- Hand movement
- Motion artifacts
- Temporary signal loss

A median filter is applied to remove abnormal IBI samples and improve the stability of HRV calculation.

### 7.3 HRV Calculation Window

The system calculates HRV indicators over a defined time window.

The diagram uses a 5-minute HRV calculation window, which is a common structure for short-term HRV analysis.

The purpose of using a window is to avoid making fatigue decisions from a single heartbeat or a small number of unstable samples.

### 7.4 SDNN

SDNN measures the overall variability of IBI values.

A lower SDNN may indicate reduced physiological variability, which can be associated with fatigue, stress, or reduced recovery.

### 7.5 RMSSD

RMSSD measures short-term beat-to-beat variability.

The FOURUT system uses RMSSD as an important fatigue indicator because it reflects short-term autonomic changes.

### 7.6 Fatigue Alert Levels

The diagram defines two fatigue-related alert levels.

```text
Fatigue Alert 1:
RMSSD < 20 ms for 3 consecutive windows

Exhaustion Alert 2:
RMSSD < 15 ms and HR > 110 BPM
```

These conditions are used to identify early fatigue risk and more severe exhaustion risk.

---

## 8. Fall-Detection Workflow

The fall-detection workflow uses acceleration data from the MPU6050 sensor.

The process follows a staged pattern:

```text
Free fall detection
    ↓
Impact detection
    ↓
Inactivity confirmation
    ↓
Fall alert
```

This staged design is more reliable than using only one acceleration threshold.

### 8.1 Free Fall Detection

A possible free-fall event is detected when SVM becomes significantly lower than normal gravity.

This may happen when the worker suddenly falls or loses support.

### 8.2 Impact Detection

After free fall, the system checks whether SVM increases sharply.

A high SVM spike may indicate that the worker has hit the ground or another surface.

### 8.3 Inactivity Confirmation

After impact, the system checks whether the worker remains inactive for a short period.

This step is important because impact alone may also occur during normal work activities, such as jumping, dropping the device, or making a sudden movement.

A confirmed fall event is generated only when the following sequence is satisfied:

```text
free fall + impact + inactivity = confirmed fall
```

---

## 9. Biometric Risk Detection

The edge device also monitors biometric risk conditions.

Examples include:

- Very low SpO2
- Abnormally high heart rate
- Abnormally low heart rate
- Combined fatigue and high heart rate
- Unsafe environmental condition

The purpose of biometric risk detection is to identify dangerous physiological states before they become emergency events.

When a risk is detected, the edge device can:

1. Activate the local buzzer
2. Publish an alert message
3. Send telemetry to the cloud
4. Continue monitoring for escalation

---

## 10. Edge-Only Emergency Workflow

The edge-only workflow is designed for immediate local response.

The workflow is:

```text
ESP32-C6 local sensor analysis
    ↓
Emergency check
    ↓
Local alert
    ↓
SIM768x / 4G SMS or call to supervisor
    ↓
On-site response
```

This workflow is important because emergency response should not depend only on dashboard visibility or cloud processing.

### 10.1 Emergency Check

The emergency check evaluates high-risk conditions such as:

- Fall event
- Low SpO2
- High heart rate
- Unsafe air quality
- Severe fatigue or exhaustion risk

### 10.2 Local Alert

If an emergency condition is detected, the device can activate:

- Buzzer
- Vibration motor
- LED warning

These outputs provide immediate feedback to the worker and nearby personnel.

### 10.3 Cellular Emergency Communication

The diagram includes a SIM768x or 4G module.

This module can be used to send an SMS or call a supervisor during emergency cases.

This provides an additional communication path beyond Wi-Fi and MQTT.

---

## 11. Hybrid Cloud Workflow

The hybrid cloud workflow connects the edge device to AWS cloud services.

The process is:

```text
ESP32-C6 local screening
    ↓
AWS IoT Core
    ↓
Lambda / Rules Engine
    ↓
DynamoDB and S3
    ↓
Dashboard and notification
```

This structure separates immediate safety actions from long-term monitoring and data management.

---

## 12. AWS IoT Core

AWS IoT Core receives MQTT messages from the edge device.

The edge device publishes structured JSON payloads using MQTT over TLS.

AWS IoT Core acts as the secure message broker between the physical device and the cloud backend.

The system uses worker-specific MQTT topics so that each worker can be monitored independently.

---

## 13. MQTT Topic Structure

For worker `W001`, the system uses the following MQTT topics:

```text
worker/W001/telemetry
worker/W001/hrv
worker/W001/alert
```

### 13.1 Telemetry Topic

```text
worker/W001/telemetry
```

This topic is used for regular monitoring data.

Typical fields include:

```json
{
  "worker_id": "W001",
  "heart_rate": 82,
  "spo2": 97,
  "aqi": 35,
  "svm": 9.81,
  "alarm_active": false,
  "alarm_level": 0,
  "fall_detected": false,
  "timestamp": 1710000000
}
```

### 13.2 HRV Topic

```text
worker/W001/hrv
```

This topic is used for fatigue and HRV-related information.

Typical fields include:

```json
{
  "worker_id": "W001",
  "sdnn": 42.5,
  "rmssd": 31.8,
  "ibi_count": 128,
  "fatigue_level": "NORMAL",
  "timestamp": 1710000000
}
```

### 13.3 Alert Topic

```text
worker/W001/alert
```

This topic is used for warning, danger, and emergency events.

Typical fields include:

```json
{
  "worker_id": "W001",
  "alert_type": "FALL",
  "severity": "EMERGENCY",
  "alarm_level": 3,
  "message": "Worker fall detected",
  "heart_rate": 118,
  "spo2": 94,
  "svm": 31.2,
  "timestamp": 1710000000
}
```

---

## 14. Lambda and Rules Engine

After MQTT messages arrive at AWS IoT Core, IoT Rules can forward the data to AWS Lambda.

The Lambda function can perform additional cloud-side processing, including:

- Classifying worker condition
- Validating payload structure
- Storing structured records
- Triggering supervisor notification
- Updating dashboard state
- Sending updated thresholds back to the device

The cloud-side classifier can map worker conditions into danger levels.

```text
Level 0: NORMAL
Level 1: WARNING
Level 2: DANGER
Level 3: EMERGENCY
```

This classification helps the dashboard and notification system present risk in a simple and consistent way.

---

## 15. Data Storage Layer

The architecture uses cloud storage for both structured and historical data.

### 15.1 DynamoDB

DynamoDB is suitable for storing recent and structured worker status records.

Examples include:

- Latest worker telemetry
- Current alarm level
- Last known heart rate
- Last known SpO2
- Last known fall status
- Current fatigue-risk level

### 15.2 Amazon S3

Amazon S3 is suitable for storing longer-term logs and historical data.

Examples include:

- Raw telemetry archives
- HRV history
- Alert logs
- Daily or weekly monitoring records
- Data for future analytics

Using both DynamoDB and S3 allows the system to support real-time dashboard access and long-term record keeping.

---

## 16. Dashboard and Notification Layer

The dashboard and notification layer is responsible for presenting worker condition to supervisors.

It may display:

- Worker identity
- Latest telemetry
- Heart rate
- SpO2
- Air quality
- Fall status
- Fatigue status
- Alarm level
- Historical trends
- Recent alerts

When a worker reaches DANGER or EMERGENCY level, the system can notify supervisors through notification services such as email, SMS, or mobile alerts, depending on the final deployment design.

---

## 17. Threshold Update Feedback Loop

The architecture includes a feedback loop from the cloud back to the edge device.

This allows the cloud to send updated rules or thresholds to the ESP32-based device.

Examples of updateable thresholds include:

- Heart-rate warning threshold
- SpO2 warning threshold
- Fall-detection threshold
- Fatigue-risk threshold
- Alert cooldown duration

This feedback loop makes the system more flexible because thresholds can be adjusted without completely rewriting the firmware.

---

## 18. Safety and Reliability Rationale

The architecture is designed around the principle that emergency response should be available even when cloud connectivity is unstable.

### Edge responsibilities

The edge device handles:

- Immediate anomaly detection
- Local alarm activation
- Fast emergency response
- Basic risk classification
- MQTT publishing when network is available

### Cloud responsibilities

The cloud handles:

- Centralized data storage
- Long-term monitoring
- Supervisor notification
- Dashboard updates
- Rule management
- Historical analysis

This separation improves reliability because the cloud does not need to be involved in every safety-critical action.

---

## 19. Relationship Between Arduino Firmware and MicroPython Prototype

The real edge implementation is the Arduino C++ FreeRTOS firmware:

```text
01_Arduino_Edge_Code/FOURUT_Edge_FreeRTOS.ino
```

The optional MicroPython prototype is used only as a supplementary demonstration.

The relationship is:

| Component | Arduino FreeRTOS Firmware | MicroPython Prototype |
|---|---|---|
| Purpose | Main deployed firmware | Optional academic prototype |
| Sensor support | Real MAX30102 and MPU6050 libraries | Simulated or simplified sensor logic |
| Task model | FreeRTOS tasks | Async or sequential prototype logic |
| MQTT communication | WiFiClientSecure and PubSubClient | MicroPython MQTT client |
| JSON construction | ArduinoJson | Python-style JSON construction |
| Safety use | Main implementation | Demonstration only |

The MicroPython version is useful for explaining the system logic, but the Arduino version remains the final firmware for deployment.

---

## 20. Security Considerations

The system uses MQTT over TLS for secure cloud communication.

Sensitive values should not be hard-coded in public repositories.

Examples of sensitive values include:

- Wi-Fi SSID
- Wi-Fi password
- AWS IoT endpoint
- Device certificate
- Device private key
- Root CA certificate
- Supervisor contact information

For academic submission, these values should be replaced with placeholders.

For deployment, credentials should be stored and managed securely according to the target hardware and security policy.

---

## 21. Limitations

The current architecture has several practical limitations that should be considered.

1. Sensor accuracy depends on physical placement and signal quality.
2. HRV calculation requires reliable IBI extraction from the PPG signal.
3. Fall detection may produce false positives during abrupt work movements.
4. Wi-Fi connectivity may not be available in all work environments.
5. Cloud notification depends on network availability.
6. Thresholds may need calibration for different workers and environments.
7. The optional MicroPython prototype should not be treated as production firmware.

These limitations can be reduced through calibration, better sensor placement, improved filtering, and field testing.

---

## 22. Conclusion

The FOURUT architecture uses a hybrid edge-cloud design to balance immediate worker protection with centralized monitoring.

The edge device performs safety-critical screening, activates local alerts, and publishes structured MQTT messages. The cloud layer receives these messages, classifies worker status, stores historical data, updates dashboards, and notifies supervisors when necessary.

This architecture is appropriate for worker health and safety monitoring because it keeps emergency response close to the worker while still enabling cloud-based visibility, logging, and long-term analysis.
