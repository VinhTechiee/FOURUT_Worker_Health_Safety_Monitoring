# FOURUT Lambda Test Event Descriptions and Expected Results

## 1. Purpose of the Test Events

This document describes the test events used to validate the AWS Lambda function in the **FOURUT Worker Health and Safety Monitoring System**.

The Lambda function is responsible for processing worker telemetry and alert payloads received from AWS IoT Core. Based on the incoming data, it classifies the worker condition, stores the record, and triggers notifications when required.

The test events are designed to verify that the Lambda function can correctly handle the following cases:

- Normal worker condition
- Dangerous biometric condition
- Emergency fall event
- Exhaustion risk based on HRV and heart-rate data

---

## 2. Lambda Processing Objective

The Lambda function is expected to perform four main operations:

1. Parse the incoming worker data payload.
2. Classify the worker condition into a danger level and state.
3. Store the processed record in DynamoDB and/or Amazon S3.
4. Send an SNS email alert when the condition requires supervisor attention.

The expected classification levels are:

| Danger Level | State | Meaning |
|---:|---|---|
| 0 | NORMAL | Worker condition is within safe limits |
| 1 | WARNING | Early abnormal condition detected |
| 2 | DANGER | Serious risk detected and notification required |
| 3 | EMERGENCY | Critical condition requiring urgent response |

---

## 3. Test Event Summary

| Test Event File | Scenario | Expected Danger Level | Expected State | Email Alert |
|---|---|---:|---|---|
| `Test_Normal.json` | Normal worker condition | 0 | NORMAL | No |
| `Test_Danger.json` | Low SpO2 and edge alarm level 2 | 2 | DANGER | Yes |
| `Test_Emergency_Fall.json` | Confirmed fall event | 3 | EMERGENCY | Yes |
| `Test_Exhaustion.json` | Exhaustion risk based on HRV and heart rate | 3 | EMERGENCY | Yes |

---

## 4. Test Case 1: `Test_Normal.json`

### 4.1 Purpose

This test checks whether the Lambda function can process normal worker health data without triggering an unnecessary alert.

The test validates that normal telemetry is still stored for monitoring and historical analysis, but no SNS email notification is sent.

### 4.2 Input Condition

The input represents a healthy and stable worker condition.

Expected input characteristics:

- Heart rate is within the normal range.
- SpO2 is within the normal range.
- Air-quality index is within the normal range.
- RMSSD and SDNN are within normal HRV ranges.
- No fall is detected.
- Edge alarm level is `0`.

### 4.3 Expected Result

The Lambda function should return or produce the following result:

```json
{
  "danger_level": 0,
  "state": "NORMAL",
  "s3_saved": true,
  "email_sent": false
}
```

### 4.4 Expected System Behavior

The Lambda function should:

1. Parse the incoming telemetry payload.
2. Identify that no emergency or danger condition is present.
3. Classify the worker state as `NORMAL`.
4. Save the record to DynamoDB and/or S3.
5. Avoid sending an SNS email alert.

### 4.5 Explanation

The worker condition is normal, so the system only records the data for monitoring and historical analysis. Since there is no safety risk, no supervisor notification is required.

### 4.6 Pass Criteria

This test is considered successful if:

- `danger_level` equals `0`
- `state` equals `NORMAL`
- `s3_saved` equals `true`
- `email_sent` equals `false`

---

## 5. Test Case 2: `Test_Danger.json`

### 5.1 Purpose

This test checks whether the Lambda function can detect a dangerous biometric condition and send an email alert to the supervisor.

The test validates cloud-side classification when the edge device already reports an elevated alarm level.

### 5.2 Input Condition

The input represents a worker in a dangerous condition.

Expected input characteristics:

- SpO2 is below the safe threshold.
- Edge alarm level is `2`.
- The worker condition requires supervisor attention.
- The event does not necessarily represent a fall, but it indicates serious physiological risk.

### 5.3 Expected Result

The Lambda function should return or produce the following result:

```json
{
  "danger_level": 2,
  "state": "DANGER",
  "s3_saved": true,
  "email_sent": true
}
```

### 5.4 Expected System Behavior

The Lambda function should:

1. Parse the incoming telemetry or alert payload.
2. Detect the low-SpO2 risk condition.
3. Use the edge alarm level as supporting evidence.
4. Classify the worker state as `DANGER`.
5. Store the event record.
6. Send an SNS email alert to the supervisor.

### 5.5 Explanation

The worker has low SpO2 and the edge device reports alarm level `2`. This indicates a serious health risk. Therefore, the Lambda function should classify the condition as `DANGER` and send an SNS email alert.

### 5.6 Pass Criteria

This test is considered successful if:

- `danger_level` equals `2`
- `state` equals `DANGER`
- `s3_saved` equals `true`
- `email_sent` equals `true`

---

## 6. Test Case 3: `Test_Emergency_Fall.json`

### 6.1 Purpose

This test checks whether the Lambda function can detect and classify an emergency fall event.

The test validates that confirmed fall detection is treated as the highest-priority emergency condition.

### 6.2 Input Condition

The input represents a confirmed fall event.

Expected input characteristics:

- Fall detection is `true`.
- Edge alarm level is `3`.
- The event indicates an emergency safety condition.
- Immediate supervisor notification is required.

### 6.3 Expected Result

The Lambda function should return or produce the following result:

```json
{
  "danger_level": 3,
  "state": "EMERGENCY",
  "s3_saved": true,
  "email_sent": true
}
```

### 6.4 Expected System Behavior

The Lambda function should:

1. Parse the incoming fall-related payload.
2. Detect that `fall_detected` is `true`.
3. Confirm that the edge alarm level indicates emergency severity.
4. Classify the worker state as `EMERGENCY`.
5. Store the event record.
6. Send an urgent SNS email alert to the supervisor.

### 6.5 Explanation

A confirmed fall is a safety-critical event. Since fall detection is true and the edge alarm level is `3`, the Lambda function should classify the event as `EMERGENCY` and send an urgent SNS notification.

### 6.6 Pass Criteria

This test is considered successful if:

- `danger_level` equals `3`
- `state` equals `EMERGENCY`
- `s3_saved` equals `true`
- `email_sent` equals `true`

---

## 7. Test Case 4: `Test_Exhaustion.json`

### 7.1 Purpose

This test checks whether the Lambda function can detect worker exhaustion risk using HRV and heart-rate data.

The test validates that the cloud-side classifier can identify a severe fatigue condition even when the event is not caused by a fall.

### 7.2 Input Condition

The input represents a worker with exhaustion-risk indicators.

Expected input characteristics:

- RMSSD is below `15 ms`.
- Heart rate is above `110 BPM`.
- The combination of low HRV and high heart rate indicates severe fatigue or exhaustion risk.
- Immediate supervisor notification is required.

### 7.3 Expected Result

The Lambda function should return or produce the following result:

```json
{
  "danger_level": 3,
  "state": "EMERGENCY",
  "s3_saved": true,
  "email_sent": true
}
```

### 7.4 Expected System Behavior

The Lambda function should:

1. Parse the incoming HRV or telemetry payload.
2. Check the RMSSD value.
3. Check the heart-rate value.
4. Identify the exhaustion-risk condition.
5. Classify the worker state as `EMERGENCY`.
6. Store the event record.
7. Send an SNS email alert to the supervisor.

### 7.5 Explanation

RMSSD below `15 ms` indicates very low short-term heart-rate variability. When this condition is combined with heart rate above `110 BPM`, the system treats it as an exhaustion-risk event. Therefore, the Lambda function should classify the worker state as `EMERGENCY`.

### 7.6 Pass Criteria

This test is considered successful if:

- `danger_level` equals `3`
- `state` equals `EMERGENCY`
- `s3_saved` equals `true`
- `email_sent` equals `true`

---

## 8. Expected Lambda Output Fields

The following fields are used to validate Lambda behavior.

| Field | Type | Description |
|---|---|---|
| `danger_level` | Integer | Numerical risk level assigned by Lambda |
| `state` | String | Human-readable worker condition |
| `s3_saved` | Boolean | Indicates whether the event was saved to Amazon S3 |
| `email_sent` | Boolean | Indicates whether an SNS email notification was sent |

---

## 9. Data Storage Expectation

For all test cases, the expected value of `s3_saved` is:

```json
"s3_saved": true
```

This means that both normal and abnormal events should be saved for traceability, debugging, and future analysis.

Even when no alert is sent, normal data remains useful for:

- Worker health history
- Trend analysis
- Model improvement
- System validation
- Audit records

---

## 10. Notification Expectation

SNS email notification should only be sent when the worker condition requires supervisor attention.

| State | Email Expected | Reason |
|---|---|---|
| NORMAL | No | No risk condition detected |
| WARNING | Optional | Depends on deployment policy |
| DANGER | Yes | Serious risk condition detected |
| EMERGENCY | Yes | Critical condition requiring urgent response |

In the provided test set, email notification is expected for:

- `Test_Danger.json`
- `Test_Emergency_Fall.json`
- `Test_Exhaustion.json`

Email notification is not expected for:

- `Test_Normal.json`

---

## 11. Validation Checklist

The Lambda function passes the test set if all of the following conditions are met:

- Normal telemetry is classified as `NORMAL`.
- Dangerous biometric data is classified as `DANGER`.
- Fall events are classified as `EMERGENCY`.
- Exhaustion-risk events are classified as `EMERGENCY`.
- All records are saved to S3.
- SNS email is sent only for dangerous or emergency cases.
- No SNS email is sent for normal worker data.

---

## 12. Conclusion

These test events validate the core decision-making behavior of the FOURUT Lambda function.

The test set confirms that the cloud backend can distinguish between normal, dangerous, and emergency worker conditions. It also verifies that the system stores all records while only sending email alerts for cases that require supervisor attention.

This behavior supports the overall FOURUT architecture by combining reliable data logging with risk-based notification.
