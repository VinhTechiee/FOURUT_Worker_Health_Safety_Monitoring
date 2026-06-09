# FOURUT Lambda WorkerDataProcessor

## Overview

`lambda_function.py` implements the cloud-side processing function for the FOURUT Worker Health and Safety Monitoring System. The function receives worker telemetry, HRV summaries, and alert payloads from AWS IoT Core, evaluates the worker condition, stores the processed event, and sends supervisor notifications for high-risk cases.

The Lambda function is designed as a rule-based safety classifier. It does not replace clinical assessment or workplace safety procedures; it provides automated monitoring support by converting edge-device data into structured risk states and traceable cloud records.

## System Role

The Lambda function is positioned after AWS IoT Core in the cloud processing pipeline.

```text
ESP32 Edge Device
    ↓ MQTT over TLS
AWS IoT Core
    ↓ IoT Rule
AWS Lambda: WorkerDataProcessor
    ↓
DynamoDB / Amazon S3 / Amazon SNS
```

Its responsibilities are separated into five operations:

```text
1. Normalize the incoming event.
2. Extract the worker or device identifier.
3. Classify the worker condition.
4. Store the processed record.
5. Notify supervisors when intervention is required.
```

## Source File

```text
lambda_function.py
```

## Runtime Dependencies

The function uses the Python AWS SDK and standard library modules.

| Dependency | Purpose |
|---|---|
| `boto3` | Access DynamoDB, Amazon S3, and Amazon SNS |
| `json` | Parse and serialize JSON payloads |
| `os` | Read Lambda environment variables |
| `uuid` | Generate unique event identifiers |
| `datetime` | Generate UTC processing timestamps |
| `decimal.Decimal` | Convert floating-point values for DynamoDB compatibility |

## AWS Services

| Service | Interface | Role |
|---|---|---|
| DynamoDB | `boto3.resource("dynamodb")` | Stores structured worker records |
| Amazon S3 | `boto3.client("s3")` | Stores historical JSON event logs |
| Amazon SNS | `boto3.client("sns")` | Sends supervisor alert emails |

## Environment Variables

The function uses environment variables to keep resource names outside the source code.

| Variable | Required | Default | Description |
|---|---:|---|---|
| `DDB_TABLE_NAME` | Yes | `WorkerHealthData` | DynamoDB table for processed records |
| `S3_BUCKET_NAME` | Recommended | Empty string | S3 bucket for historical JSON logs |
| `SNS_TOPIC_ARN` | Required for alerts | Empty string | SNS topic ARN for supervisor email alerts |

Example configuration:

```text
DDB_TABLE_NAME = WorkerHealthData
S3_BUCKET_NAME = worker-historical-data
SNS_TOPIC_ARN = arn:aws:sns:ap-southeast-1:<ACCOUNT_ID>:WorkerAlerts
```

If `S3_BUCKET_NAME` is empty, the function still processes the event but skips S3 logging. If `SNS_TOPIC_ARN` is empty, the function still stores the event but skips email notification.

## Supported Input Formats

The function accepts multiple event structures to support local tests, IoT Rule integration, and API-style invocation.

### Direct JSON Event

```json
{
  "worker_id": "W001",
  "heart_rate": 82,
  "spo2": 97,
  "aqi": 45,
  "rmssd": 35.2,
  "alarm_level": 0,
  "fall_detected": false
}
```

### API-Style Event

```json
{
  "body": "{\"worker_id\":\"W001\",\"heart_rate\":82,\"spo2\":97}"
}
```

### Wrapped Payload Event

```json
{
  "payload": {
    "worker_id": "W001",
    "heart_rate": 82,
    "spo2": 97
  }
}
```

The `normalize_event()` function extracts the actual payload from these formats before classification.

## Worker Identifier Handling

The function accepts several worker or device identifier fields:

```text
worker_id
WorkerID
DeviceID
device_id
```

If none of these fields are available, the worker identifier is set to:

```text
UNKNOWN
```

This approach improves compatibility between edge firmware, test events, and cloud-side records.

## Data Normalization Utilities

The helper functions improve input robustness before classification and storage.

| Function | Purpose |
|---|---|
| `get_number()` | Converts numeric fields safely and returns a default value when missing or invalid |
| `get_bool()` | Converts boolean-like values such as `true`, `"true"`, `"1"`, and `"yes"` |
| `to_decimal()` | Converts Python floats to `Decimal` values before writing to DynamoDB |
| `extract_worker_id()` | Resolves the worker identifier from supported field names |
| `normalize_event()` | Extracts payload data from direct, wrapped, or API-style events |

## Classification Model

The classifier is implemented in:

```python
classify_danger_level(data)
```

It assigns one of four safety states.

| Danger Level | State | Meaning | Notification |
|---:|---|---|---|
| 0 | `NORMAL` | No abnormal condition detected | No |
| 1 | `WARNING` | Early abnormal condition detected | No |
| 2 | `DANGER` | Serious risk requiring supervisor attention | Yes |
| 3 | `EMERGENCY` | Critical condition requiring immediate response | Yes |

The classifier checks conditions in descending severity order:

```text
EMERGENCY
    ↓
DANGER
    ↓
WARNING
    ↓
NORMAL
```

This priority order ensures that critical conditions, such as a fall event, override lower-level abnormalities.

## Classification Rules

### Level 3: Emergency

An event is classified as `EMERGENCY` when any of the following conditions is true.

| Condition | Rule |
|---|---|
| Fall detected | `fall_detected == true` |
| Critical edge alarm | `alarm_level >= 3` |
| Emergency alert status | `category == "FALL"` or `status == "EMERGENCY"` |
| Critical oxygen saturation | `spo2 < 80` |
| Exhaustion risk | `rmssd < 15` and `heart_rate > 110` |

Expected result:

```json
{
  "danger_level": 3,
  "state": "EMERGENCY"
}
```

### Level 2: Danger

An event is classified as `DANGER` when a serious non-emergency risk is detected.

| Condition | Rule |
|---|---|
| Edge danger alarm | `alarm_level == 2` |
| Low oxygen saturation | `spo2 < 90` |
| High heart rate | `heart_rate > 120` |
| Low heart rate | `heart_rate < 50` |
| Very low RMSSD | `rmssd < 15` |
| Dangerous air quality | `aqi > 150` |

Expected result:

```json
{
  "danger_level": 2,
  "state": "DANGER"
}
```

### Level 1: Warning

An event is classified as `WARNING` when early abnormal signs are detected.

| Condition | Rule |
|---|---|
| Edge warning alarm | `alarm_level == 1` |
| Slightly low oxygen saturation | `spo2 < 94` |
| Slightly high heart rate | `heart_rate > 110` |
| Possible fatigue | `rmssd < 20` |
| Poor air quality | `aqi > 75` |

Expected result:

```json
{
  "danger_level": 1,
  "state": "WARNING"
}
```

### Level 0: Normal

If no warning, danger, or emergency rule is matched, the event is classified as `NORMAL`.

```json
{
  "danger_level": 0,
  "state": "NORMAL",
  "reason": "No abnormal condition detected",
  "action": "No action required"
}
```

## Processing Workflow

The Lambda entry point is:

```python
lambda_handler(event, context)
```

The handler executes the following sequence:

```text
1. Receive the event from AWS IoT Core or a test invocation.
2. Generate a UTC timestamp.
3. Normalize the payload format.
4. Classify the worker condition.
5. Save the structured record to DynamoDB.
6. Save a historical JSON log to Amazon S3, if configured.
7. Send an SNS email for DANGER or EMERGENCY events.
8. Return a structured processing response.
```

## DynamoDB Record

The function stores each processed event in the configured DynamoDB table.

Default table:

```text
WorkerHealthData
```

Primary key structure:

| Key | Field |
|---|---|
| Partition key | `DeviceID` |
| Sort key | `Timestamp` |

Stored fields include:

| Field | Description |
|---|---|
| `DeviceID` | Worker or device identifier |
| `Timestamp` | Millisecond timestamp generated by Lambda |
| `event_id` | Unique event identifier |
| `received_at` | UTC processing time |
| `danger_level` | Assigned numerical risk level |
| `state` | Assigned worker state |
| `reason` | Classification rationale |
| `action` | Recommended response |
| `heart_rate` | Heart-rate reading |
| `spo2` | Oxygen-saturation reading |
| `aqi` | Air-quality index |
| `svm` | Acceleration magnitude |
| `sdnn` | HRV SDNN value |
| `rmssd` | HRV RMSSD value |
| `ibi_count` | Number of IBI samples |
| `alarm_active` | Edge alarm status |
| `alarm_level` | Edge alarm severity |
| `fall_detected` | Fall-detection flag |
| `raw_payload` | Original input payload |

## Amazon S3 Historical Logging

When `S3_BUCKET_NAME` is configured, the function saves a JSON log for each processed event.

Object key format:

```text
worker-safety-logs/{worker_id}/{safe_time}_{event_id}.json
```

The log object contains:

```json
{
  "event_id": "generated-event-id",
  "worker_id": "W001",
  "received_at": "UTC timestamp",
  "classification": {
    "danger_level": 0,
    "state": "NORMAL",
    "reason": "No abnormal condition detected",
    "action": "No action required"
  },
  "raw_payload": {}
}
```

S3 logging preserves both the original event and the classification result for review, debugging, and audit purposes.

## SNS Notification Policy

SNS email is sent only when the assigned danger level is `2` or higher.

| State | Email Sent |
|---|---|
| `NORMAL` | No |
| `WARNING` | No |
| `DANGER` | Yes |
| `EMERGENCY` | Yes |

The email includes the worker ID, event ID, timestamp, risk level, state, reason, recommended action, sensor values, and raw payload.

Subject format:

```text
[DANGER] Worker Safety Alert - W001
[EMERGENCY] Worker Safety Alert - W001
```

## Lambda Response

The function returns an HTTP-style response.

```json
{
  "statusCode": 200,
  "body": "{\"event_id\":\"...\",\"worker_id\":\"W001\",\"danger_level\":0,\"state\":\"NORMAL\",\"reason\":\"No abnormal condition detected\",\"action\":\"No action required\",\"s3_saved\":true,\"email_sent\":false}"
}
```

The response body contains:

| Field | Description |
|---|---|
| `event_id` | Unique event ID generated by Lambda |
| `worker_id` | Worker or device identifier |
| `danger_level` | Assigned risk level |
| `state` | Assigned worker state |
| `reason` | Explanation of the classification |
| `action` | Recommended response |
| `s3_saved` | Indicates whether S3 logging was completed |
| `email_sent` | Indicates whether an SNS email was sent |

## Expected Behavior

| Event Type | Classification | DynamoDB | S3 | SNS |
|---|---|---|---|---|
| Normal telemetry | `NORMAL` | Save | Save if configured | No |
| Warning telemetry | `WARNING` | Save | Save if configured | No |
| Danger event | `DANGER` | Save | Save if configured | Yes |
| Emergency event | `EMERGENCY` | Save | Save if configured | Yes |

## Test Cases

### Normal Event

Input:

```json
{
  "worker_id": "W001",
  "heart_rate": 82,
  "spo2": 97,
  "aqi": 45,
  "rmssd": 35.2,
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

### Danger Event

Input:

```json
{
  "worker_id": "W001",
  "heart_rate": 118,
  "spo2": 88,
  "aqi": 80,
  "rmssd": 16.2,
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

### Emergency Fall Event

Input:

```json
{
  "worker_id": "W001",
  "heart_rate": 126,
  "spo2": 93,
  "aqi": 70,
  "svm": 31.2,
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

### Exhaustion Event

Input:

```json
{
  "worker_id": "W001",
  "heart_rate": 116,
  "spo2": 94,
  "aqi": 55,
  "rmssd": 13.8,
  "alarm_level": 2,
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

## Deployment Requirements

The Lambda execution role must allow the function to write to DynamoDB, write objects to S3, and publish messages to SNS.

Minimum permissions:

| Service | Required Action | Resource Scope |
|---|---|---|
| DynamoDB | `dynamodb:PutItem` | Target table only |
| Amazon S3 | `s3:PutObject` | Target bucket or prefix only |
| Amazon SNS | `sns:Publish` | Target topic only |

Broad permissions such as `dynamodb:*`, `s3:*`, and `sns:*` should be avoided.

## Validation Checklist

The implementation is valid when the following conditions are satisfied:

- Lambda receives payloads from AWS IoT Core.
- `normalize_event()` correctly handles direct, `body`, and `payload` inputs.
- Worker identifiers are extracted from supported ID fields.
- Normal telemetry is classified as `NORMAL`.
- Warning conditions are classified as `WARNING`.
- Danger conditions are classified as `DANGER`.
- Fall and exhaustion conditions are classified as `EMERGENCY`.
- All processed events are saved to DynamoDB.
- S3 logging succeeds when `S3_BUCKET_NAME` is configured.
- SNS notification is sent only for `DANGER` and `EMERGENCY`.
- The response includes `event_id`, `worker_id`, `danger_level`, `state`, `reason`, `action`, `s3_saved`, and `email_sent`.

## Security Considerations

Sensitive values should not be hard-coded in the source file. Environment variables or secure configuration mechanisms should be used for resource identifiers and deployment-specific values.

Do not expose the following values in public repositories:

```text
AWS account ID
SNS topic ARN with real account ID
Private S3 bucket names, if sensitive
Supervisor email addresses
Device certificates
Private keys
Wi-Fi credentials
```

The IAM role should follow least-privilege access and restrict each permission to the specific DynamoDB table, S3 bucket prefix, and SNS topic used by the deployment.

## Limitations

The current implementation is suitable for prototype validation and academic demonstration. The following limitations should be considered in future development:

| Limitation | Possible Improvement |
|---|---|
| Thresholds are hard-coded | Store thresholds in DynamoDB or AWS Systems Manager Parameter Store |
| No schema validation library is used | Add JSON Schema validation |
| No duplicate-alert suppression is implemented | Add alert cooldown or deduplication logic |
| AWS write failures are not wrapped in full handler-level exception handling | Add structured error handling and fallback responses |
| Worker-specific profiles are not supported | Load individualized thresholds from a worker profile table |
| Logging uses `print()` statements | Use structured logs and CloudWatch metrics |

## Conclusion

`WorkerDataProcessor` provides the cloud-side decision and persistence layer for the FOURUT monitoring system. It normalizes worker payloads, classifies risk using deterministic safety rules, stores structured records in DynamoDB, logs historical events to S3, and sends SNS notifications when the worker state requires supervisor attention. The design supports traceability, modular cloud integration, and clear separation between edge sensing and cloud-side safety processing.
