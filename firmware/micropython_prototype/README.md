# FOURUT Optional MicroPython Prototype

## 1. Purpose

This folder contains an optional MicroPython prototype for the FOURUT Worker Health and Safety Monitoring System. The prototype provides a compact implementation of the edge-device workflow for demonstration, documentation, and algorithm explanation.

The prototype demonstrates:

- biometric data handling for heart rate and SpO2;
- motion monitoring using MPU6050 acceleration and gyroscope readings;
- staged fall detection using free fall, impact, posture change, and stillness verification;
- prototype HRV and fatigue-risk estimation;
- local buzzer and cancel-button handling;
- structured JSON payload generation; and
- MQTT publishing for telemetry, HRV summaries, and alert events.

This prototype is not the deployed firmware. The main embedded implementation remains the Arduino C++ FreeRTOS firmware.

---

## 2. Relationship to the Main Firmware

The primary FOURUT edge firmware is:

```text
01_Arduino_Edge_Code/FOURUT_Edge_FreeRTOS.ino
```

The MicroPython prototype is supplementary. It mirrors the main edge workflow in a simpler and more readable format, while the Arduino FreeRTOS firmware remains the implementation used for the actual device.

The Arduino firmware is preferred for deployment because it provides stronger support for:

- FreeRTOS task prioritization;
- real MAX30102 integration through the selected Arduino library;
- real MPU6050 acceleration and gyroscope readings;
- ESP-NOW environmental-node reception;
- MQTT over TLS through `WiFiClientSecure` and `PubSubClient`;
- structured JSON payloads through `ArduinoJson`; and
- more reliable timing control for embedded monitoring.

---

## 3. Folder Contents

```text
micropython_prototype/
├── config.py
├── main.py
└── README.md
```

| File | Role |
|---|---|
| `config.py` | Stores network settings, device identity, MQTT topics, thresholds, timing values, and prototype flags. |
| `main.py` | Implements the MicroPython prototype logic, including sensing, risk assessment, alarm control, HRV calculation, and MQTT publishing. |
| `README.md` | Documents the purpose, scope, limitations, and expected use of the prototype. |

---

## 4. Prototype Scope

The MicroPython version is designed to demonstrate the system logic rather than replace the main firmware. Its scope includes:

1. reading or simulating biometric values;
2. reading MPU6050 acceleration and gyroscope values where hardware support is available;
3. estimating Signal Vector Magnitude, or SVM;
4. detecting fall-like events using a staged state machine;
5. calculating prototype HRV indicators from BPM-derived IBI values;
6. generating fatigue, biometric, environmental, and fall alerts;
7. controlling a local buzzer and cancel button; and
8. publishing JSON payloads to MQTT topics.

---

## 5. MQTT Topic Structure

The prototype uses the same worker-based topic structure as the main firmware:

```text
worker/{worker_id}/telemetry
worker/{worker_id}/hrv
worker/{worker_id}/alert
```

For worker `W001`, the topics are:

```text
worker/W001/telemetry
worker/W001/hrv
worker/W001/alert
```

This keeps the prototype compatible with the same cloud-side data organization used by the main edge firmware.

---

## 6. Configuration Design

The `config.py` file groups configuration values into clear sections:

- device identity and network configuration;
- MQTT and TLS certificate paths;
- hardware pin definitions;
- sensor and prototype modes;
- biometric thresholds;
- fall-detection thresholds;
- HRV and fatigue thresholds;
- task timing intervals; and
- alert cooldown intervals.

Sensitive credentials are represented by placeholders and should be replaced only in a private deployment environment.

---

## 7. Fall-Detection Logic

The prototype follows a staged fall-detection design:

```text
free fall → impact → posture change → stillness verification → confirmed fall
```

The logic uses the following indicators:

- **SVM**, calculated from acceleration values;
- **impact magnitude**, based on a high SVM threshold;
- **posture change**, calculated from the angle between the stable pre-event acceleration vector and the post-impact vector;
- **gyroscope stillness**, used to confirm that the device is no longer moving significantly; and
- **inactivity duration**, used to reduce false positives after strong movements.

This approach is more robust than using a single acceleration threshold because heavy construction activities may generate short high-G movements without representing a true fall.

---

## 8. HRV and Fatigue Logic

The prototype calculates two HRV-related indicators:

| Indicator | Meaning |
|---|---|
| `SDNN` | Standard deviation of IBI values. |
| `RMSSD` | Root mean square of successive IBI differences. |

In this prototype, IBI is estimated from BPM:

```text
IBI ≈ 60000 / BPM
```

This is acceptable for demonstrating the data-processing pipeline. It is not equivalent to clinical-grade HRV because accurate HRV requires true beat-to-beat timestamps from a validated PPG or ECG signal-processing method.

---

## 9. Environmental Data Handling

The main Arduino firmware is designed to receive environmental data from a separate PM2.5 node through ESP-NOW. The MicroPython prototype does not implement that ESP-NOW receiver. Instead, it keeps environmental values as placeholders so that the telemetry schema remains compatible with the cloud pipeline.

The relevant fields are:

```text
aqi
pm25_ugm3
pm10_ugm3
env_node_id
env_online
```

This allows the cloud data structure to remain consistent while clearly separating the prototype from the deployed environmental-node integration.

---

## 10. Local Alarm and Alert Handling

The prototype supports three alarm levels:

| Level | Meaning |
|---:|---|
| 0 | No active alarm. |
| 1 | Warning condition. |
| 2 | Danger or critical condition. |
| 3 | Emergency condition, such as confirmed fall. |

The local buzzer is activated when an alarm is triggered. The cancel button can silence the local alarm, while alert cooldown logic reduces repeated messages for the same abnormal condition.

Alert events are stored in an internal queue and published by the cloud-publishing task. This keeps risk detection separate from network operations.

---

## 11. Asynchronous Task Structure

The prototype uses `uasyncio` to approximate the task separation used in the FreeRTOS firmware.

| Task | Responsibility |
|---|---|
| `anomaly_detection_task()` | Reads or simulates sensor data, runs fall detection, biometric risk checks, HRV calculation, fatigue assessment, and environmental checks. |
| `alarm_control_task()` | Handles the cancel button and controls the local buzzer. |
| `cloud_publish_task()` | Maintains Wi-Fi/MQTT connectivity and publishes telemetry, HRV summaries, and alert events. |

The task structure reflects the same edge-first design used in the main firmware, although it does not provide the same deterministic scheduling guarantees as FreeRTOS.

---

## 12. Security and Credential Handling

The prototype uses placeholders for Wi-Fi and AWS IoT credentials. Private keys and certificates should not be committed to a public repository.

The following values should be managed securely in deployment:

- Wi-Fi SSID;
- Wi-Fi password;
- device certificate;
- private key;
- root CA certificate; and
- AWS IoT endpoint details when required by the project policy.

---

## 13. Limitations

The MicroPython prototype has the following limitations:

1. MAX30102 data is simulated unless a separate validated driver is implemented.
2. Environmental PM2.5 data is represented through placeholders rather than ESP-NOW reception.
3. HRV is estimated from BPM-derived IBI values instead of true beat-to-beat timestamps.
4. MicroPython TLS support may vary depending on the board and firmware build.
5. `uasyncio` task scheduling is suitable for demonstration but is not equivalent to FreeRTOS task prioritization.
6. The prototype is not intended for medical, industrial, or safety-critical deployment.

These limitations are expected because the purpose of the folder is to explain and demonstrate the system logic in a readable form.

---

## 14. How to Run the Prototype

A typical MicroPython setup flow is:

1. Flash MicroPython firmware to a supported ESP32-based board.
2. Upload `config.py` and `main.py` to the board.
3. Replace Wi-Fi and MQTT placeholders in `config.py`.
4. Upload certificate files if MQTT over TLS is tested.
5. Run `main.py`.

Example board file layout:

```text
/
├── config.py
├── main.py
├── AmazonRootCA1.pem
├── device-certificate.pem.crt
└── private.pem.key
```

The exact setup may vary depending on the board, MicroPython firmware build, and MQTT/TLS library support.

---

## 15. Recommended Use in Submission

This folder should be presented as an optional prototype only. A suitable description is:

```text
The MicroPython implementation is provided as a supplementary prototype. It demonstrates the simplified edge-processing workflow, local risk-classification logic, JSON payload structure, and MQTT publishing process. The deployed embedded firmware remains the Arduino C++ FreeRTOS implementation located in 01_Arduino_Edge_Code/FOURUT_Edge_FreeRTOS.ino.
```

---

## 16. Conclusion

The MicroPython prototype provides a concise representation of the FOURUT edge-device workflow. It is useful for explaining how sensor readings are transformed into local risk decisions and cloud-ready JSON messages.

However, the deployed implementation remains the Arduino C++ FreeRTOS firmware because it provides stronger real-sensor support, better embedded timing control, secure MQTT integration, and closer alignment with the final hardware design.
