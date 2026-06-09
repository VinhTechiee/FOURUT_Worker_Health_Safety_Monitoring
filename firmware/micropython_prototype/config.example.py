# ============================================================
# FOURUT Edge - MicroPython Prototype Configuration
# ------------------------------------------------------------
# This file contains configuration values for the optional
# MicroPython prototype. The main deployed firmware remains
# FOURUT_Edge_FreeRTOS.ino.
# ============================================================


# ============================================================
# 1. DEVICE IDENTITY AND NETWORK CONFIGURATION
# ============================================================

# Wi-Fi credentials. Replace these values before deployment.
WIFI_SSID = "YOUR_WIFI_SSID"
WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"

# AWS IoT Core MQTT endpoint.
MQTT_BROKER = "au08zn44zsegx-ats.iot.ap-southeast-1.amazonaws.com"
MQTT_PORT = 8883
CLIENT_ID = "WorkerDevice01"

# Worker identity used in telemetry and MQTT topics.
WORKER_ID = "W001"

# MQTT topic structure: worker/{worker_id}/{message_type}
TOPIC_TELEMETRY = "worker/{}/telemetry".format(WORKER_ID)
TOPIC_HRV = "worker/{}/hrv".format(WORKER_ID)
TOPIC_ALERT = "worker/{}/alert".format(WORKER_ID)

# TLS certificate file paths on the MicroPython filesystem.
ROOT_CA_PATH = "AmazonRootCA1.pem"
CLIENT_CERT_PATH = "device-certificate.pem.crt"
PRIVATE_KEY_PATH = "private.pem.key"


# ============================================================
# 2. HARDWARE PIN CONFIGURATION
# ============================================================

# I2C bus pins.
I2C_SDA = 6
I2C_SCL = 7

# Local alarm and user input pins.
BUZZER_PIN = 2
CANCEL_BTN_PIN = 3

# I2C device addresses.
MAX30102_ADDR = 0x57
MPU6050_ADDR = 0x68


# ============================================================
# 3. SENSOR AND PROTOTYPE MODES
# ============================================================

# The MicroPython prototype does not implement the full MAX30102
# FIFO and beat-detection driver. When this flag is enabled, the
# prototype generates controlled HR and SpO2 values for demonstration.
SIMULATE_MAX30102 = True

# Environmental sensing is represented as a placeholder in this
# MicroPython prototype. In the main Arduino firmware, environmental
# data is designed to come from a separate PM2.5 node through ESP-NOW.
SIMULATE_ENVIRONMENT = True
AQI_PLACEHOLDER = 50
PM25_PLACEHOLDER = 12.0
PM10_PLACEHOLDER = 25.0
ENV_NODE_ID = 1


# ============================================================
# 4. BIOMETRIC RISK THRESHOLDS
# ============================================================
# These thresholds support project-level risk demonstration only.
# They are not intended for medical diagnosis.

SPO2_WARNING = 90
SPO2_CRITICAL = 80

BPM_BRADY = 50
BPM_TACHY = 120


# ============================================================
# 5. FALL-DETECTION THRESHOLDS
# ============================================================
# Units:
#   - Acceleration/SVM: m/s^2
#   - Gyroscope magnitude: rad/s
#   - Time: ms

# Stage 1: possible free-fall phase, approximately below 0.5g.
FREE_FALL_THRESH = 4.9

# Stage 2: impact phase, approximately above 3g.
IMPACT_THRESH = 29.4

# Maximum time allowed between free fall and impact.
FALL_TIME_WINDOW_MS = 1500

# Required stillness duration after impact before a fall is confirmed.
INACTIVITY_TIME_MS = 3000

# Stable acceleration range around gravity for stillness verification.
STILL_SVM_MIN = 7.0
STILL_SVM_MAX = 12.5

# Gyroscope threshold used to verify that the device is almost still.
STILL_GYRO_THRESH = 0.35

# Minimum posture change between the pre-fall posture and post-impact posture.
POSTURE_CHANGE_THRESH_DEG = 35.0

# Cancel the fall sequence if stillness/posture confirmation does not occur.
FALL_CONFIRM_TIMEOUT_MS = 6000

# Time during which the confirmed fall flag remains visible in telemetry.
FALL_LATCH_MS = 30000


# ============================================================
# 6. HRV AND FATIGUE THRESHOLDS
# ============================================================
# The prototype estimates IBI from BPM. Accurate HRV requires true
# beat-to-beat timestamps from a validated PPG processing pipeline.

RMSSD_FATIGUE_LEVEL_1 = 20.0
RMSSD_EXHAUSTION_LEVEL_2 = 15.0
BPM_EXHAUSTION_THRESHOLD = 110

# For demonstration, the HRV window is shortened to 60 seconds.
# A 5-minute window is more appropriate for the deployed design.
HRV_WINDOW_MS = 60000

MIN_IBI_COUNT = 10
MAX_IBI_BUFFER = 500
MEDIAN_WINDOW = 5


# ============================================================
# 7. TASK TIMING CONFIGURATION
# ============================================================

# Fast motion sampling for fall detection.
IMU_READ_INTERVAL_MS = 20

# Slower biometric sampling for HR and SpO2.
BIO_READ_INTERVAL_MS = 1000

# Environmental placeholder update interval.
ENV_READ_INTERVAL_MS = 5000

# Cloud telemetry publishing interval.
CLOUD_PUBLISH_INTERVAL_MS = 5000

# Wi-Fi and MQTT reconnect intervals.
WIFI_RECONNECT_INTERVAL_MS = 5000
MQTT_RECONNECT_INTERVAL_MS = 5000


# ============================================================
# 8. ALERT RATE LIMITING
# ============================================================

ALERT_COOLDOWN_MS = 30000
FALL_ALERT_COOLDOWN_MS = 60000
