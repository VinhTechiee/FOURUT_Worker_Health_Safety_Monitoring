# FOURUT Lambda Test Events

## Overview

This repository contains test events for validating the AWS Lambda component of the **FOURUT Worker Health and Safety Monitoring System**. The Lambda function receives worker telemetry and alert payloads from AWS IoT Core, classifies the worker condition, stores the processed record, and sends supervisor notifications when intervention is required.

The test set covers four representative operating conditions:

- Normal worker condition
- Dangerous biometric condition
- Emergency fall event
- Exhaustion risk based on heart-rate and HRV indicators

## System Context

The Lambda function acts as the cloud-side decision layer for incoming worker safety data. Each event includes biometric measurements, environmental readings, edge alarm information, and fall-detection status. The function converts these inputs into a standardized risk state and determines whether the event requires notification.

## Processing Workflow

The expected processing flow is as follows:

1. **Receive payload**  
   The Lambda function is triggered by a telemetry or alert event received through AWS IoT Core.

2. **Parse and validate input fields**  
   The function extracts worker, biometric, environmental, movement, and edge-alarm values from the payload.

3. **Classify worker condition**  
   The function assigns a numerical `danger_level` and a textual `state` according to the event severity.

4. **Store event record**  
   The processed result is saved to the configured storage layer, such as Amazon S3 and/or DynamoDB, for traceability and later review.

5. **Send notification when required**  
   An SNS email alert is sent only when the event indicates a serious or critical worker-safety condition.

6. **Return validation output**  
   The function returns a structured result containing the classification and action status.

## Risk Classification

| Danger Level | State | Interpretation | Notification |
|---:|---|---|---|
| 0 | `NORMAL` | Worker condition is within acceptable limits. | No |
| 1 | `WARNING` | Early abnormal condition detected. | Deployment-specific |
| 2 | `DANGER` | Serious risk condition requiring supervisor attention. | Yes |
| 3 | `EMERGENCY` | Critical condition requiring urgent response. | Yes |

## Test Event Summary

| File | Scenario | Key Indicators | Expected Level | Expected State | Email |
|---|---|---|---:|---|---|
| `Test_Normal.json` | Normal condition | Stable heart rate, SpO2, HRV, air quality, and no fall | 0 | `NORMAL` | No |
| `Test_Danger.json` | Dangerous biometric condition | Low SpO2 and edge alarm level `2` | 2 | `DANGER` | Yes |
| `Test_Emergency_Fall.json` | Confirmed fall event | `fall_detected = true` and edge alarm level `3` | 3 | `EMERGENCY` | Yes |
| `Test_Exhaustion.json` | Exhaustion risk | RMSSD below `15 ms` and heart rate above `110 BPM` | 3 | `EMERGENCY` | Yes |

## Input Payload Fields

| Field | Type | Description |
|---|---|---|
| `worker_id` | String | Identifier of the monitored worker. |
| `heart_rate` | Number | Heart rate in beats per minute. |
| `spo2` | Number | Blood oxygen saturation percentage. |
| `aqi` | Number | Air Quality Index value. |
| `pm25_ugm3` | Number | PM2.5 concentration in micrograms per cubic meter. |
| `pm10_ugm3` | Number | PM10 concentration in micrograms per cubic meter. |
| `env_node_id` | Number | Identifier of the environmental sensing node. |
| `env_seq` | Number | Environmental data sequence number. |
| `env_age_ms` | Number | Age of the environmental reading in milliseconds. |
| `env_online` | Boolean | Indicates whether the environmental node is online. |
| `svm` | Number | Signal vector magnitude used for motion analysis. |
| `sdnn` | Number | Standard deviation of normal-to-normal intervals. |
| `rmssd` | Number | Root mean square of successive differences, used as a short-term HRV indicator. |
| `ibi_count` | Number | Number of inter-beat intervals used in HRV computation. |
| `alarm_active` | Boolean | Indicates whether the edge device has raised an alarm. |
| `alarm_level` | Number | Edge-side alarm severity level. |
| `fall_detected` | Boolean | Indicates whether a fall has been detected. |

## Expected Lambda Output

Each test validates the following output fields:

| Field | Type | Description |
|---|---|---|
| `danger_level` | Integer | Numerical severity level assigned by the Lambda function. |
| `state` | String | Human-readable worker condition. |
| `s3_saved` | Boolean | Indicates whether the processed event was saved to Amazon S3. |
| `email_sent` | Boolean | Indicates whether an SNS email notification was sent. |

## Test Cases

### 1. Normal Worker Condition

**File:** `Test_Normal.json`

This test verifies that the function can process a stable worker condition without generating a false alert.

Expected output:

```json
{
  "danger_level": 0,
  "state": "NORMAL",
  "s3_saved": true,
  "email_sent": false
}
```

Pass criteria:

- `danger_level` is `0`
- `state` is `NORMAL`
- `s3_saved` is `true`
- `email_sent` is `false`

### 2. Dangerous Biometric Condition

**File:** `Test_Danger.json`

This test verifies that the function detects a serious biometric risk when the worker has low SpO2 and the edge device reports alarm level `2`.

Expected output:

```json
{
  "danger_level": 2,
  "state": "DANGER",
  "s3_saved": true,
  "email_sent": true
}
```

Pass criteria:

- `danger_level` is `2`
- `state` is `DANGER`
- `s3_saved` is `true`
- `email_sent` is `true`

### 3. Emergency Fall Event

**File:** `Test_Emergency_Fall.json`

This test verifies that a confirmed fall is classified as the highest-priority safety event.

Expected output:

```json
{
  "danger_level": 3,
  "state": "EMERGENCY",
  "s3_saved": true,
  "email_sent": true
}
```

Pass criteria:

- `danger_level` is `3`
- `state` is `EMERGENCY`
- `s3_saved` is `true`
- `email_sent` is `true`

### 4. Exhaustion Risk

**File:** `Test_Exhaustion.json`

This test verifies that the function can identify severe exhaustion risk from combined HRV and heart-rate indicators. In this test, RMSSD is below `15 ms` and heart rate is above `110 BPM`.

Expected output:

```json
{
  "danger_level": 3,
  "state": "EMERGENCY",
  "s3_saved": true,
  "email_sent": true
}
```

Pass criteria:

- `danger_level` is `3`
- `state` is `EMERGENCY`
- `s3_saved` is `true`
- `email_sent` is `true`

## Validation Procedure

Use the following procedure to validate the Lambda function:

1. Open the Lambda function in the AWS Console.
2. Create a test event using one of the JSON files in this repository.
3. Run the test event.
4. Inspect the returned `danger_level`, `state`, `s3_saved`, and `email_sent` fields.
5. Confirm that the processed record is available in the configured storage destination.
6. Confirm that SNS email behavior matches the expected result.
7. Repeat the procedure for all four test events.

The function passes the test set only when all scenarios produce the expected classification, storage result, and notification behavior.

## Storage and Notification Policy

All test events are expected to be saved, including normal telemetry. This supports operational traceability, debugging, trend review, and system validation.

SNS email notification is expected only for events that require supervisor attention:

| State | Email Expected | Reason |
|---|---|---|
| `NORMAL` | No | No safety risk is detected. |
| `WARNING` | Optional | Notification depends on deployment policy. |
| `DANGER` | Yes | Serious worker-safety risk is detected. |
| `EMERGENCY` | Yes | Critical condition requires urgent response. |

## Acceptance Criteria

The Lambda function is considered valid for this test set when:

- `Test_Normal.json` is classified as `NORMAL`.
- `Test_Danger.json` is classified as `DANGER`.
- `Test_Emergency_Fall.json` is classified as `EMERGENCY`.
- `Test_Exhaustion.json` is classified as `EMERGENCY`.
- Every processed event is saved successfully.
- SNS email is sent for `DANGER` and `EMERGENCY` events.
- SNS email is not sent for `NORMAL` events.

## Conclusion

The test events provide a focused validation set for the FOURUT Lambda function. They verify that the cloud-side logic can distinguish between normal, dangerous, and emergency worker conditions, while maintaining consistent storage and risk-based notification behavior.
