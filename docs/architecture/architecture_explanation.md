# FOURUT Architecture Explanation

## 1. Overview

FOURUT follows a hybrid edge-cloud architecture for worker health and safety monitoring. The design combines local embedded intelligence with cloud-based storage, classification, notification, and historical review.

The central architectural principle is **edge-first safety**. Critical events should be detected and acted on locally before cloud services are involved. Cloud services improve visibility and record keeping, but they should not be the only path for emergency response.

---

## 2. Why a Hybrid Edge-Cloud Model Is Used

A pure cloud model would require raw or near-raw sensor data to be transmitted before a decision can be made. That approach is not suitable for emergency wearable systems because network delay, Wi-Fi failure, or cloud unavailability could delay local response.

A pure edge-only model would provide local response but would reduce supervisor visibility, historical analysis, and centralized event tracking.

FOURUT therefore uses a hybrid model:

| Function | Edge Device | Cloud Backend |
|---|---|---|
| Immediate fall and danger screening | yes | optional verification |
| Local buzzer or LED alarm | yes | no |
| Telemetry storage | limited | yes |
| Supervisor notification | optional local/cellular | yes |
| Long-term trend analysis | limited | yes |
| Rule and threshold management | possible | yes |
| Dashboard visibility | no | yes |

This division keeps safety-critical behavior close to the worker while preserving the advantages of cloud infrastructure.

---

## 3. End-to-End Architecture

![End-to-end architecture](assets/end_to_end_architecture.png)

The system is composed of:

1. **Worker-side sensors** for physiological and motion monitoring.
2. **Environmental sensor node** for local air-quality monitoring.
3. **ESP32 edge gateway** for local fusion, decision-making, and alert control.
4. **Cloud ingestion layer** for secure MQTT message reception.
5. **Cloud processing layer** for classification, storage, and notification.
6. **Supervisor interface** for response and post-event review.

---

## 4. Edge-First Safety Rationale

The edge device performs emergency logic locally because the following events require fast response:

- confirmed fall;
- severe oxygen saturation drop;
- dangerous heart-rate condition;
- exhaustion condition;
- hazardous environmental exposure.

The local alarm should activate even when:

- MQTT connection is unavailable;
- AWS IoT Core cannot be reached;
- Wi-Fi coverage is unstable;
- cloud notification is delayed.

This design improves resilience in construction sites where wireless connectivity may be inconsistent.

---

## 5. Algorithmic Flow

![Edge algorithm pipeline](assets/edge_algorithm_pipeline.png)

The edge algorithm operates as a feature-based rule system. It does not attempt to stream all raw data continuously. Instead, it extracts compact safety features from each sensing stream.

### 5.1 Physiological Stream

Input:

- heart rate;
- SpO2;
- pulse-derived timing information.

Processing:

- range validation;
- stability checks;
- threshold evaluation;
- HRV window calculation when enough samples are available.

Output:

- normal state;
- warning;
- danger;
- fatigue or exhaustion indicator.

### 5.2 Motion Stream

Input:

- acceleration;
- gyroscope data.

Processing:

- SVM calculation;
- free-fall candidate detection;
- impact candidate detection;
- posture-change estimation;
- post-impact stillness confirmation.

Output:

- normal movement;
- suspicious motion;
- confirmed fall.

### 5.3 Environmental Stream

Input:

- PM2.5 or AQI estimate from the environmental node.

Processing:

- packet validation;
- latest-value update;
- environmental threshold classification.

Output:

- normal air quality;
- air-quality warning;
- air-quality danger.

---

## 6. Fall-Detection Rationale

A single acceleration threshold is not sufficient for a worker-safety device. Normal construction activities can include sudden movements, vibration, impacts, and device shocks. FOURUT therefore uses a sequence-based method.

```text
free fall
  -> impact
  -> posture change
  -> post-impact inactivity
  -> confirmed fall
```

This design reduces false alarms because a fall is confirmed only when several conditions occur in a plausible order.

Representative logic:

| Stage | Evidence |
|---|---|
| Free fall | SVM drops below low-acceleration threshold |
| Impact | SVM rapidly exceeds high-acceleration threshold |
| Posture change | orientation differs from the pre-event reference |
| Stillness | acceleration and gyroscope remain stable after impact |
| Confirmation | all stages occur within defined timing windows |

---

## 7. HRV and Fatigue Rationale

Fatigue screening uses HRV-related metrics such as SDNN and RMSSD. These features summarize variation in inter-beat intervals.

```text
IBI = 60000 / BPM
SDNN = standard deviation of IBI values
RMSSD = sqrt(mean(successive IBI difference²))
```

In the prototype, IBI may be derived from BPM. This is acceptable for demonstrating the algorithmic concept, but it is not physiologically equivalent to beat-level PPG peak timing. A stronger implementation should detect PPG peaks directly and compute true beat-to-beat intervals.

Fatigue thresholds should be treated as screening rules, not medical diagnosis.

---

## 8. Alert-Level Model

The system uses a simple four-level model to support both firmware logic and cloud classification.

| Level | Label | Meaning | Example Conditions |
|---:|---|---|---|
| 0 | Normal | No active risk | safe biometric and environmental values |
| 1 | Warning | early abnormality | low SpO2 warning, mild AQI warning, fatigue warning |
| 2 | Danger | critical non-fall risk | severe SpO2 drop, exhaustion, dangerous air quality |
| 3 | Emergency | immediate emergency | confirmed fall or equivalent critical event |

This model is easy to implement in firmware and easy to display in a dashboard.

---

## 9. Cloud Workflow Rationale

Cloud services are used for:

- secure ingestion of MQTT payloads;
- event classification and validation;
- structured storage of recent worker state;
- archival storage of raw or detailed event logs;
- notification to supervisors;
- future analytics and reporting.

The cloud layer should receive structured payloads rather than excessive raw data. Compact telemetry reduces network load, improves privacy, and makes downstream processing easier.

---

## 10. MQTT Topic Model

The recommended worker topic structure separates regular telemetry, HRV summaries, and event alerts.

![MQTT topic separation](assets/mqtt_topic_separation.png)

| Topic | Purpose |
|---|---|
| `worker/W001/telemetry` | periodic worker state and sensor summary |
| `worker/W001/hrv` | HRV and fatigue summary per calculation window |
| `worker/W001/alert` | event-driven warning, danger, or emergency messages |

This separation makes cloud routing simpler. Telemetry can be stored regularly, HRV can be analyzed periodically, and alerts can be routed immediately.

---

## 11. Data Storage Rationale

The cloud backend can use both structured and archival storage.

| Storage | Recommended Use |
|---|---|
| DynamoDB | current worker state, latest alarm level, recent telemetry, dashboard lookup |
| S3 | raw event archives, long-term logs, batch analysis, exported reports |

Using both storage types supports both real-time dashboard needs and long-term academic or operational analysis.

---

## 12. Threshold Update Feedback Loop

The architecture can support a feedback channel from cloud to edge. This channel may update non-sensitive configuration values such as:

- SpO2 warning threshold;
- heart-rate warning threshold;
- AQI threshold;
- alert cooldown duration;
- telemetry interval.

Threshold updates must be validated carefully. Safety-critical thresholds should have safe defaults in firmware so the device remains functional even if cloud updates fail.

---

## 13. Security Rationale

The gateway communicates with AWS IoT Core using TLS certificate-based authentication. Sensitive values must not appear in public documentation or committed source code.

Sensitive values include:

- Wi-Fi SSID and password;
- AWS IoT endpoint;
- device certificate;
- private key;
- supervisor contact information;
- local phone numbers or private emails.

The repository should include only example configuration files such as `secrets.example.h`.

---

## 14. Relationship Between Prototype and Final Deployment

The architecture may contain both implemented and planned components. The documentation should label them clearly.

| Element | Recommended Wording |
|---|---|
| Wi-Fi MQTT edge gateway | implemented prototype path |
| AWS IoT Core and Lambda | cloud backend path |
| Environmental ESP-NOW node | local expansion path |
| SIM768x 4G SMS/call | deployment extension or optional emergency fallback |
| Solar charging or advanced battery management | future hardware enhancement |

This distinction improves academic credibility and prevents the report from appearing inconsistent.

---

## 15. Limitations and Research Opportunities

Known limitations:

- sensor placement and motion artifacts affect biometric reliability;
- BPM-derived HRV is weaker than beat-level HRV;
- fall detection requires field validation with normal work movements;
- PM2.5 readings require calibration and environmental context;
- cellular fallback increases power and enclosure complexity;
- thresholds may need personalization;
- the system requires ethical data governance for worker monitoring.

Research opportunities:

- adaptive threshold tuning;
- sensor-fusion fall classification;
- improved PPG peak detection;
- power-aware scheduling;
- privacy-preserving worker dashboards;
- field validation under realistic construction activities.

---

## 16. Summary

FOURUT uses local embedded intelligence for immediate safety response and cloud infrastructure for monitoring, storage, notification, and analysis. This hybrid architecture is suitable for a worker-safety prototype because it balances response latency, network resilience, system observability, and future extensibility.
