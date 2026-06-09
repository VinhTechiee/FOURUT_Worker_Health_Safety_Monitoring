import json
import os
import uuid
import boto3
from datetime import datetime, timezone
from decimal import Decimal


# ============================================================
# AWS CLIENTS
# ============================================================
dynamodb = boto3.resource("dynamodb")
sns = boto3.client("sns")
s3 = boto3.client("s3")


# ============================================================
# ENVIRONMENT VARIABLES
# ============================================================
DDB_TABLE_NAME = os.environ.get("DDB_TABLE_NAME", "WorkerHealthData").strip()
SNS_TOPIC_ARN = os.environ.get("SNS_TOPIC_ARN", "").strip()
S3_BUCKET_NAME = os.environ.get("S3_BUCKET_NAME", "").strip()

table = dynamodb.Table(DDB_TABLE_NAME)


# ============================================================
# UTILITY FUNCTIONS
# ============================================================
def extract_worker_id(data):
    return (
        data.get("worker_id")
        or data.get("WorkerID")
        or data.get("DeviceID")
        or data.get("device_id")
        or "UNKNOWN"
    )

def to_decimal(obj):
    if isinstance(obj, float):
        return Decimal(str(obj))
    if isinstance(obj, dict):
        return {k: to_decimal(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [to_decimal(v) for v in obj]
    return obj


def get_number(data, key, default=-1):
    value = data.get(key, default)

    try:
        if value is None:
            return default
        return float(value)
    except (ValueError, TypeError):
        return default


def get_bool(data, key, default=False):
    value = data.get(key, default)

    if isinstance(value, bool):
        return value

    if isinstance(value, str):
        return value.lower() in ["true", "1", "yes"]

    return bool(value)


def normalize_event(event):
    """
    Supports:
    1. Direct JSON from Lambda test
    2. IoT Rule payload
    3. API-style event with body
    4. Wrapped event with payload
    """

    if isinstance(event, dict) and "body" in event:
        body = event["body"]

        if isinstance(body, str):
            try:
                return json.loads(body)
            except json.JSONDecodeError:
                return {"raw_body": body}

        if isinstance(body, dict):
            return body

    if isinstance(event, dict) and "payload" in event:
        payload = event["payload"]

        if isinstance(payload, str):
            try:
                return json.loads(payload)
            except json.JSONDecodeError:
                return {"raw_payload": payload}

        if isinstance(payload, dict):
            return payload

    return event


# ============================================================
# CLOUD CLASSIFICATION: 4 DANGER LEVELS
# ============================================================
def classify_danger_level(data):
    hr = get_number(data, "heart_rate", -1)
    spo2 = get_number(data, "spo2", -1)
    aqi = get_number(data, "aqi", -1)
    rmssd = get_number(data, "rmssd", -1)

    alarm_level = int(get_number(data, "alarm_level", 0))
    fall_detected = get_bool(data, "fall_detected", False)

    category = str(data.get("category", "")).upper()
    status = str(data.get("status", "")).upper()

    # ========================================================
    # LEVEL 3 - EMERGENCY
    # ========================================================
    if fall_detected:
        return {
            "danger_level": 3,
            "state": "EMERGENCY",
            "reason": "Fall detected by edge device",
            "action": "Immediate supervisor response required"
        }

    if alarm_level >= 3:
        return {
            "danger_level": 3,
            "state": "EMERGENCY",
            "reason": "Critical edge alarm level received",
            "action": "Immediate supervisor response required"
        }

    if category == "FALL" or status == "EMERGENCY":
        return {
            "danger_level": 3,
            "state": "EMERGENCY",
            "reason": "Emergency alert event received from edge device",
            "action": "Immediate supervisor response required"
        }

    if spo2 > 0 and spo2 < 80:
        return {
            "danger_level": 3,
            "state": "EMERGENCY",
            "reason": "Critical SpO2 drop below 80%",
            "action": "Immediate medical check required"
        }

    if rmssd > 0 and rmssd < 15 and hr > 110:
        return {
            "danger_level": 3,
            "state": "EMERGENCY",
            "reason": "Exhaustion risk: RMSSD below 15 ms and heart rate above 110 BPM",
            "action": "Stop work and check worker immediately"
        }

    # ========================================================
    # LEVEL 2 - DANGER
    # ========================================================
    if alarm_level == 2:
        return {
            "danger_level": 2,
            "state": "DANGER",
            "reason": "High-risk edge alarm level received",
            "action": "Supervisor should check the worker"
        }

    if spo2 > 0 and spo2 < 90:
        return {
            "danger_level": 2,
            "state": "DANGER",
            "reason": "Low SpO2 below 90%",
            "action": "Supervisor should check worker breathing condition"
        }

    if hr > 120:
        return {
            "danger_level": 2,
            "state": "DANGER",
            "reason": "Tachycardia: heart rate above 120 BPM",
            "action": "Worker should rest and be checked"
        }

    if hr > 0 and hr < 50:
        return {
            "danger_level": 2,
            "state": "DANGER",
            "reason": "Bradycardia: heart rate below 50 BPM",
            "action": "Supervisor should check the worker"
        }

    if rmssd > 0 and rmssd < 15:
        return {
            "danger_level": 2,
            "state": "DANGER",
            "reason": "Very low RMSSD below 15 ms",
            "action": "Possible fatigue or exhaustion risk"
        }

    if aqi > 150:
        return {
            "danger_level": 2,
            "state": "DANGER",
            "reason": "Dangerous air quality detected",
            "action": "Worker should leave the polluted area"
        }

    # ========================================================
    # LEVEL 1 - WARNING
    # ========================================================
    if alarm_level == 1:
        return {
            "danger_level": 1,
            "state": "WARNING",
            "reason": "Warning edge alarm level received",
            "action": "Monitor worker condition"
        }

    if spo2 > 0 and spo2 < 94:
        return {
            "danger_level": 1,
            "state": "WARNING",
            "reason": "SpO2 slightly below normal range",
            "action": "Monitor oxygen level"
        }

    if hr > 110:
        return {
            "danger_level": 1,
            "state": "WARNING",
            "reason": "Heart rate slightly high",
            "action": "Monitor heart rate trend"
        }

    if rmssd > 0 and rmssd < 20:
        return {
            "danger_level": 1,
            "state": "WARNING",
            "reason": "Possible fatigue: RMSSD below 20 ms",
            "action": "Monitor fatigue trend"
        }

    if aqi > 75:
        return {
            "danger_level": 1,
            "state": "WARNING",
            "reason": "Poor air quality warning",
            "action": "Monitor workplace air quality"
        }

    # ========================================================
    # LEVEL 0 - NORMAL
    # ========================================================
    return {
        "danger_level": 0,
        "state": "NORMAL",
        "reason": "No abnormal condition detected",
        "action": "No action required"
    }


# ============================================================
# DYNAMODB STORAGE
# ============================================================
def save_to_dynamodb(data, classification, received_at):
    worker_id = extract_worker_id(data)

    event_id = str(uuid.uuid4())
    timestamp_number = int(datetime.now(timezone.utc).timestamp() * 1000)

    item = {
        "DeviceID": worker_id,
        "Timestamp": timestamp_number,

        "worker_id": worker_id,
        "event_id": event_id,
        "received_at": received_at,

        "danger_level": classification["danger_level"],
        "state": classification["state"],
        "reason": classification["reason"],
        "action": classification["action"],

        "heart_rate": data.get("heart_rate"),
        "spo2": data.get("spo2"),
        "aqi": data.get("aqi"),
        "svm": data.get("svm"),
        "sdnn": data.get("sdnn"),
        "rmssd": data.get("rmssd"),
        "ibi_count": data.get("ibi_count"),
        "alarm_active": data.get("alarm_active"),
        "alarm_level": data.get("alarm_level"),
        "fall_detected": data.get("fall_detected"),

        "raw_payload": data
    }

    table.put_item(Item=to_decimal(item))

    return event_id


# ============================================================
# S3 LOGGING
# ============================================================
def save_to_s3(data, classification, received_at, event_id):
    if not S3_BUCKET_NAME:
        print("S3_BUCKET_NAME is empty. Skip S3 logging.")
        return False

    worker_id = extract_worker_id(data)

    safe_time = received_at.replace(":", "-")

    log_object = {
        "event_id": event_id,
        "worker_id": worker_id,
        "received_at": received_at,
        "classification": classification,
        "raw_payload": data
    }

    key = f"worker-safety-logs/{worker_id}/{safe_time}_{event_id}.json"

    s3.put_object(
        Bucket=S3_BUCKET_NAME,
        Key=key,
        Body=json.dumps(log_object, indent=2),
        ContentType="application/json"
    )

    return True


# ============================================================
# SNS EMAIL NOTIFICATION
# ============================================================
def send_sns_notification(data, classification, received_at, event_id):
    if not SNS_TOPIC_ARN:
        print("SNS_TOPIC_ARN is empty. Skip notification.")
        return False

    worker_id = data.get("worker_id", "UNKNOWN")

    hr = data.get("heart_rate", "N/A")
    spo2 = data.get("spo2", "N/A")
    aqi = data.get("aqi", "N/A")
    svm = data.get("svm", "N/A")
    sdnn = data.get("sdnn", "N/A")
    rmssd = data.get("rmssd", "N/A")
    alarm_level = data.get("alarm_level", "N/A")
    fall_detected = data.get("fall_detected", "N/A")

    if classification["danger_level"] == 3:
        subject = f"[EMERGENCY] Worker Safety Alert - {worker_id}"
    elif classification["danger_level"] == 2:
        subject = f"[DANGER] Worker Safety Alert - {worker_id}"
    else:
        subject = f"[WARNING] Worker Safety Alert - {worker_id}"

    message = f"""
Worker Safety Monitoring Alert

Worker ID: {worker_id}
Event ID: {event_id}
Time: {received_at}

Danger Level: {classification["danger_level"]}
State: {classification["state"]}
Reason: {classification["reason"]}
Recommended Action: {classification["action"]}

Sensor Data:
- Heart Rate: {hr} BPM
- SpO2: {spo2} %
- AQI: {aqi}
- SVM: {svm}
- SDNN: {sdnn} ms
- RMSSD: {rmssd} ms
- Edge Alarm Level: {alarm_level}
- Fall Detected: {fall_detected}

Raw Payload:
{json.dumps(data, indent=2)}
"""

    sns.publish(
        TopicArn=SNS_TOPIC_ARN,
        Subject=subject[:100],
        Message=message
    )

    return True


# ============================================================
# MAIN LAMBDA HANDLER
# ============================================================
def lambda_handler(event, context):
    print("Received event:")
    print(json.dumps(event))

    received_at = datetime.now(timezone.utc).isoformat()

    data = normalize_event(event)

    classification = classify_danger_level(data)

    event_id = save_to_dynamodb(data, classification, received_at)

    s3_saved = save_to_s3(data, classification, received_at, event_id)

    email_sent = False

    # Level 0: store only
    # Level 1: store + dashboard/log only
    # Level 2 & Level 3: send email notification
    if classification["danger_level"] >= 2:
        email_sent = send_sns_notification(data, classification, received_at, event_id)

    response = {
        "event_id": event_id,
        "worker_id": extract_worker_id(data),
        "danger_level": classification["danger_level"],
        "state": classification["state"],
        "reason": classification["reason"],
        "action": classification["action"],
        "s3_saved": s3_saved,
        "email_sent": email_sent
    }

    print("Lambda response:")
    print(json.dumps(response))

    return {
        "statusCode": 200,
        "body": json.dumps(response)
    }