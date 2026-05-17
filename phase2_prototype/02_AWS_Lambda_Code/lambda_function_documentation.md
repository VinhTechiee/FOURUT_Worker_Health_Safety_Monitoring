# FOURUT Lambda Function Documentation

## 1. Purpose

This document explains the AWS Lambda function used in the **FOURUT Worker Health and Safety Monitoring System**.

The Lambda function acts as the main cloud-side processing component. It receives worker telemetry, HRV, and alert data from AWS IoT Core, classifies the worker condition, stores the result, and sends supervisor notifications when needed.

The source file documented here is:

```text
lambda_function.py
```

---

## 2. Role in the FOURUT Architecture

The Lambda function is positioned between AWS IoT Core and the cloud storage or notification services.

The processing flow is:

```text
ESP32 Edge Device
    ↓ MQTT
AWS IoT Core
    ↓ IoT Rule
AWS Lambda: WorkerDataProcessor
    ↓
DynamoDB / Amazon S3 / Amazon SNS
```

The Lambda function is responsible for:

- Receiving MQTT payloads forwarded by AWS IoT Core
- Normalizing different event formats
- Extracting worker identity
- Classifying worker condition into danger levels
- Saving structured records to DynamoDB
- Saving historical JSON logs to Amazon S3
- Sending SNS email alerts for dangerous or emergency conditions
- Returning a structured processing result

---

## 3. Main AWS Services Used

The function uses three AWS services through `boto3`.

| AWS Service | Client or Resource | Purpose |
|---|---|---|
| DynamoDB | `boto3.resource("dynamodb")` | Stores structured worker records |
| Amazon S3 | `boto3.client("s3")` | Stores historical JSON event logs |
| Amazon SNS | `boto3.client("sns")` | Sends supervisor email notifications |

The relevant initialization section is:

```python
dynamodb = boto3.resource("dynamodb")
sns = boto3.client("sns")
s3 = boto3.client("s3")
```

---

## 4. Environment Variables

The function uses environment variables to avoid hard-coding AWS resource names.

| Environment Variable | Default Value | Purpose |
|---|---|---|
| `DDB_TABLE_NAME` | `WorkerHealthData` | DynamoDB table for processed records |
| `SNS_TOPIC_ARN` | Empty string | SNS topic used for alert emails |
| `S3_BUCKET_NAME` | Empty string | S3 bucket used for historical logs |

The function reads these values at startup:

```python
DDB_TABLE_NAME = os.environ.get("DDB_TABLE_NAME", "WorkerHealthData").strip()
SNS_TOPIC_ARN = os.environ.get("SNS_TOPIC_ARN", "").strip()
S3_BUCKET_NAME = os.environ.get("S3_BUCKET_NAME", "").strip()
```

If `SNS_TOPIC_ARN` is empty, email notification is skipped.  
If `S3_BUCKET_NAME` is empty, S3 logging is skipped.

---

## 5. Input Event Formats

The Lambda function supports several input formats.

This is handled by the `normalize_event()` function.

Supported formats include:

1. Direct JSON input from Lambda test events
2. MQTT payload forwarded by AWS IoT Core
3. API-style event with a `body` field
4. Wrapped event with a `payload` field

This makes the Lambda function easier to test and more flexible during integration.

### 5.1 Direct JSON Event

Example:

```json
{
  "worker_id": "W001",
  "heart_rate": 82,
  "spo2": 97,
  "aqi": 40,
  "alarm_level": 0,
  "fall_detected": false
}
```

### 5.2 API-style Event

Example:

```json
{
  "body": "{\"worker_id\": \"W001\", \"heart_rate\": 82}"
}
```

### 5.3 Wrapped Payload Event

Example:

```json
{
  "payload": {
    "worker_id": "W001",
    "heart_rate": 82,
    "spo2": 97
  }
}
```

---

## 6. Worker ID Extraction

The function uses `extract_worker_id()` to support multiple possible worker or device ID field names.

Supported field names include:

```text
worker_id
WorkerID
DeviceID
device_id
```

If none of these fields exist, the worker ID is set to:

```text
UNKNOWN
```

This makes the function tolerant of small naming differences between edge firmware, test events, and cloud records.

---

## 7. Utility Functions

The Lambda function includes helper functions to make input parsing safer.

### 7.1 `get_number()`

This function safely converts a value into a number.

If the value is missing, invalid, or `None`, it returns a default value.

This prevents classification errors when a payload does not contain all fields.

### 7.2 `get_bool()`

This function safely converts different boolean representations into a Python boolean.

It supports values such as:

```text
true
"true"
"1"
"yes"
false
```

This is useful because IoT payloads and test events may represent boolean values differently.

### 7.3 `to_decimal()`

DynamoDB does not accept Python floating-point values directly in the same way as standard JSON.

The `to_decimal()` function converts `float` values into `Decimal` values before saving data to DynamoDB.

This avoids DynamoDB serialization errors.

---

## 8. Danger-Level Classification Model

The core logic of the Lambda function is implemented in:

```python
classify_danger_level(data)
```

The function classifies worker condition into four levels.

| Danger Level | State | Meaning |
|---:|---|---|
| 0 | NORMAL | No abnormal condition detected |
| 1 | WARNING | Early abnormal condition detected |
| 2 | DANGER | Serious condition requiring supervisor attention |
| 3 | EMERGENCY | Critical condition requiring immediate response |

The classifier uses both edge-generated indicators and raw sensor values.

Main input fields include:

- `heart_rate`
- `spo2`
- `aqi`
- `rmssd`
- `alarm_level`
- `fall_detected`
- `category`
- `status`

---

## 9. Level 3: Emergency Conditions

A worker condition is classified as `EMERGENCY` when any critical condition is detected.

### 9.1 Fall Detected

If the edge device reports a fall, the function immediately classifies the event as emergency.

Condition:

```python
fall_detected == True
```

Expected classification:

```json
{
  "danger_level": 3,
  "state": "EMERGENCY",
  "reason": "Fall detected by edge device",
  "action": "Immediate supervisor response required"
}
```

### 9.2 Critical Edge Alarm Level

Condition:

```python
alarm_level >= 3
```

This means the edge device has already classified the event as critical.

### 9.3 Emergency Category or Status

Condition:

```python
category == "FALL" or status == "EMERGENCY"
```

This allows alert payloads to directly indicate emergency conditions.

### 9.4 Critical SpO2 Drop

Condition:

```python
spo2 > 0 and spo2 < 80
```

This represents a severe oxygen-saturation risk.

### 9.5 Exhaustion Risk

Condition:

```python
rmssd > 0 and rmssd < 15 and heart_rate > 110
```

This condition combines very low HRV with high heart rate, which is treated as an exhaustion-risk event.

---

## 10. Level 2: Danger Conditions

A worker condition is classified as `DANGER` when a serious but not immediately highest-priority risk is detected.

Level 2 conditions include:

| Condition | Rule |
|---|---|
| Edge danger alarm | `alarm_level == 2` |
| Low SpO2 | `spo2 < 90` |
| High heart rate | `heart_rate > 120` |
| Low heart rate | `heart_rate < 50` |
| Very low RMSSD | `rmssd < 15` |
| Dangerous air quality | `aqi > 150` |

The expected output state is:

```text
DANGER
```

Danger-level events trigger SNS email notification.

---

## 11. Level 1: Warning Conditions

A worker condition is classified as `WARNING` when an early abnormal condition is detected.

Level 1 conditions include:

| Condition | Rule |
|---|---|
| Edge warning alarm | `alarm_level == 1` |
| Slightly low SpO2 | `spo2 < 94` |
| Slightly high heart rate | `heart_rate > 110` |
| Possible fatigue | `rmssd < 20` |
| Poor air quality | `aqi > 75` |

The expected output state is:

```text
WARNING
```

In the current implementation, warning-level events are stored but do not trigger SNS email notification.

---

## 12. Level 0: Normal Condition

If none of the warning, danger, or emergency rules are matched, the Lambda function classifies the worker condition as normal.

Expected classification:

```json
{
  "danger_level": 0,
  "state": "NORMAL",
  "reason": "No abnormal condition detected",
  "action": "No action required"
}
```

Normal events are still saved for monitoring and historical analysis.

---

## 13. Classification Priority

The classifier checks emergency conditions first, then danger conditions, then warning conditions, and finally normal status.

The priority order is:

```text
EMERGENCY
    ↓
DANGER
    ↓
WARNING
    ↓
NORMAL
```

This order is important because a payload may contain multiple abnormal indicators.

For example, if a worker has both:

```text
fall_detected = true
spo2 = 88
```

The function should classify the event as `EMERGENCY`, not only as `DANGER`.

---

## 14. DynamoDB Storage

The function saves processed records using:

```python
save_to_dynamodb(data, classification, received_at)
```

### 14.1 Table Name

The table name comes from:

```text
DDB_TABLE_NAME
```

Default value:

```text
WorkerHealthData
```

### 14.2 Primary Keys

The saved item uses:

| Key | Description |
|---|---|
| `DeviceID` | Worker or device identifier |
| `Timestamp` | Millisecond timestamp generated by Lambda |

### 14.3 Stored Fields

The DynamoDB record includes:

- `DeviceID`
- `Timestamp`
- `worker_id`
- `event_id`
- `received_at`
- `danger_level`
- `state`
- `reason`
- `action`
- `heart_rate`
- `spo2`
- `aqi`
- `svm`
- `sdnn`
- `rmssd`
- `ibi_count`
- `alarm_active`
- `alarm_level`
- `fall_detected`
- `raw_payload`

The function also generates a unique event ID using:

```python
uuid.uuid4()
```

---

## 15. Amazon S3 Historical Logging

The function saves historical JSON logs using:

```python
save_to_s3(data, classification, received_at, event_id)
```

### 15.1 S3 Bucket

The bucket name comes from:

```text
S3_BUCKET_NAME
```

If the bucket name is empty, the function skips S3 logging and returns:

```python
False
```

### 15.2 S3 Object Key Format

The S3 object key follows this structure:

```text
worker-safety-logs/{worker_id}/{safe_time}_{event_id}.json
```

Example:

```text
worker-safety-logs/W001/2026-05-14T10-30-00.123456+00-00_abcd-event-id.json
```

### 15.3 S3 Log Content

The S3 log includes:

- `event_id`
- `worker_id`
- `received_at`
- `classification`
- `raw_payload`

This helps preserve both the original input and the processed classification result.

---

## 16. SNS Email Notification

The function sends email notifications using:

```python
send_sns_notification(data, classification, received_at, event_id)
```

### 16.1 Notification Condition

SNS email is sent only when:

```python
danger_level >= 2
```

This means email is sent for:

```text
DANGER
EMERGENCY
```

Email is not sent for:

```text
NORMAL
WARNING
```

### 16.2 SNS Topic

The SNS topic ARN comes from:

```text
SNS_TOPIC_ARN
```

If this value is empty, the function skips notification and returns:

```python
False
```

### 16.3 Email Subject

The function uses different subject prefixes based on severity.

| Danger Level | Email Subject Prefix |
|---:|---|
| 3 | `[EMERGENCY]` |
| 2 | `[DANGER]` |
| 1 | `[WARNING]` |

In the current handler, only level 2 and level 3 events send email.

### 16.4 Email Message Content

The SNS email message includes:

- Worker ID
- Event ID
- Received time
- Danger level
- State
- Reason
- Recommended action
- Heart rate
- SpO2
- AQI
- SVM
- SDNN
- RMSSD
- Edge alarm level
- Fall-detection status
- Raw payload

This provides supervisors with both the decision result and the supporting sensor data.

---

## 17. Main Lambda Handler

The entry point is:

```python
lambda_handler(event, context)
```

The handler performs the following steps:

```text
1. Print the received event for debugging.
2. Generate a UTC received timestamp.
3. Normalize the input event.
4. Classify the danger level.
5. Save the processed record to DynamoDB.
6. Save the historical log to S3.
7. Send SNS email if danger_level >= 2.
8. Build a response object.
9. Return HTTP-style statusCode 200 with the response body.
```

---

## 18. Response Format

The Lambda function returns a response in this format:

```json
{
  "statusCode": 200,
  "body": "{\"event_id\": \"...\", \"worker_id\": \"W001\", \"danger_level\": 0, \"state\": \"NORMAL\", \"reason\": \"No abnormal condition detected\", \"action\": \"No action required\", \"s3_saved\": true, \"email_sent\": false}"
}
```

The body contains:

| Field | Description |
|---|---|
| `event_id` | Unique ID generated for the processed event |
| `worker_id` | Worker or device identifier |
| `danger_level` | Numerical risk level |
| `state` | Human-readable worker condition |
| `reason` | Explanation of why the level was assigned |
| `action` | Recommended response |
| `s3_saved` | Indicates whether S3 logging succeeded |
| `email_sent` | Indicates whether SNS notification was sent |

---

## 19. Expected Behavior by Event Type

| Event Type | Expected State | Store in DynamoDB | Save to S3 | Send SNS Email |
|---|---|---|---|---|
| Normal telemetry | NORMAL | Yes | Yes, if bucket configured | No |
| Warning telemetry | WARNING | Yes | Yes, if bucket configured | No |
| Danger event | DANGER | Yes | Yes, if bucket configured | Yes |
| Emergency event | EMERGENCY | Yes | Yes, if bucket configured | Yes |

---

## 20. Example Test Cases

### 20.1 Normal Event

Input:

```json
{
  "worker_id": "W001",
  "heart_rate": 82,
  "spo2": 97,
  "aqi": 40,
  "rmssd": 32,
  "alarm_level": 0,
  "fall_detected": false
}
```

Expected result:

```json
{
  "danger_level": 0,
  "state": "NORMAL",
  "email_sent": false
}
```

### 20.2 Danger Event

Input:

```json
{
  "worker_id": "W001",
  "heart_rate": 95,
  "spo2": 88,
  "aqi": 50,
  "alarm_level": 2,
  "fall_detected": false
}
```

Expected result:

```json
{
  "danger_level": 2,
  "state": "DANGER",
  "email_sent": true
}
```

### 20.3 Emergency Fall Event

Input:

```json
{
  "worker_id": "W001",
  "heart_rate": 110,
  "spo2": 94,
  "aqi": 45,
  "alarm_level": 3,
  "fall_detected": true
}
```

Expected result:

```json
{
  "danger_level": 3,
  "state": "EMERGENCY",
  "email_sent": true
}
```

### 20.4 Exhaustion Event

Input:

```json
{
  "worker_id": "W001",
  "heart_rate": 118,
  "spo2": 95,
  "aqi": 45,
  "rmssd": 12,
  "alarm_level": 0,
  "fall_detected": false
}
```

Expected result:

```json
{
  "danger_level": 3,
  "state": "EMERGENCY",
  "email_sent": true
}
```

---

## 21. Error Handling and Robustness

The Lambda function includes several robustness features:

1. It supports multiple event formats through `normalize_event()`.
2. It safely handles missing numerical values through `get_number()`.
3. It safely handles boolean variations through `get_bool()`.
4. It converts floats to `Decimal` before writing to DynamoDB.
5. It skips S3 logging if the bucket name is missing.
6. It skips SNS notification if the topic ARN is missing.
7. It stores the raw payload for traceability.

These features make the function easier to test and more tolerant of different input sources.

---

## 22. Security Considerations

The function should be deployed with least-privilege IAM permissions.

Required permissions include:

- `dynamodb:PutItem` for the target DynamoDB table
- `s3:PutObject` for the target S3 bucket path
- `sns:Publish` for the target SNS topic

Sensitive values should not be hard-coded in the source file.

Sensitive configuration should be provided through environment variables or secure configuration mechanisms.

Examples of sensitive values include:

- AWS account ID
- SNS topic ARN
- S3 bucket name, if private
- Supervisor email addresses
- Device identifiers, if considered sensitive
- Any credential or certificate material

---

## 23. Recommended IAM Policy Scope

The IAM role should grant access only to the required resources.

Recommended resource scope:

| Service | Scope |
|---|---|
| DynamoDB | Specific table only |
| S3 | Specific bucket and prefix only |
| SNS | Specific topic only |

This is preferred over broad permissions such as:

```text
dynamodb:*
s3:*
sns:*
```

---

## 24. Validation Checklist

The Lambda function is working correctly if the following checks pass:

- Lambda receives MQTT payloads from AWS IoT Core.
- `normalize_event()` correctly extracts the payload.
- Worker ID is extracted from supported ID fields.
- Normal payloads are classified as `NORMAL`.
- Warning conditions are classified as `WARNING`.
- Danger conditions are classified as `DANGER`.
- Fall events are classified as `EMERGENCY`.
- Exhaustion-risk events are classified as `EMERGENCY`.
- All processed events are saved to DynamoDB.
- S3 logging returns `true` when `S3_BUCKET_NAME` is configured.
- SNS email is sent only for danger level `2` or `3`.
- Lambda returns a structured response with `statusCode = 200`.

---

## 25. Limitations

The current Lambda function is suitable for academic demonstration and system validation, but it has several limitations.

1. Thresholds are hard-coded inside the classifier.
2. Classification is rule-based rather than machine-learning-based.
3. No duplicate-alert suppression is implemented in the Lambda layer.
4. No schema validation library is used.
5. No explicit try-except block wraps the full handler.
6. DynamoDB and S3 write failures may cause the function invocation to fail.
7. Worker-specific thresholds are not yet loaded from a database.
8. Email notification policy is fixed to danger level `2` and `3`.

These limitations can be improved in future versions by adding schema validation, exception handling, configurable thresholds, and alert cooldown logic.

---

## 26. Future Improvements

Possible improvements include:

- Store thresholds in DynamoDB and load them dynamically.
- Add JSON schema validation for incoming payloads.
- Add try-except error handling around AWS service calls.
- Add duplicate-alert cooldown logic.
- Add worker profile support for personalized thresholds.
- Add CloudWatch metrics for alert counts and processing errors.
- Add structured logging instead of plain `print()` statements.
- Add dead-letter queue support for failed processing events.

---

## 27. Conclusion

The Lambda function provides the main cloud-side intelligence for the FOURUT system.

It receives worker data from AWS IoT Core, classifies health and safety risk, stores records in DynamoDB, logs historical events to S3, and sends SNS notifications for danger and emergency cases.

This design supports the hybrid edge-cloud architecture of FOURUT by combining local edge detection with centralized cloud processing, storage, and supervisor notification.
