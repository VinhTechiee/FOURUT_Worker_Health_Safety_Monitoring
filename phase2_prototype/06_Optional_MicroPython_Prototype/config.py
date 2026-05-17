# ============================================================
# FOURUT Edge - MicroPython Prototype Configuration
# ------------------------------------------------------------
# File này chỉ dùng cho bản MicroPython prototype.
# Firmware chính của dự án vẫn là FOURUT_Edge_FreeRTOS.ino.
# ============================================================


# ============================================================
# 1. DEVICE IDENTITY & NETWORK CONFIG
# ============================================================

# 1.1 Wi-Fi credentials
WIFI_SSID = "YOUR_WIFI_SSID"
WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"

# 1.2 AWS IoT Core MQTT endpoint
MQTT_BROKER = "au08zn44zsegx-ats.iot.ap-southeast-1.amazonaws.com"
MQTT_PORT = 8883
CLIENT_ID = "WorkerDevice01"

# 1.3 Worker/device identity
WORKER_ID = "W001"

# 1.4 MQTT topics
TOPIC_TELEMETRY = "worker/W001/telemetry"
TOPIC_HRV = "worker/W001/hrv"
TOPIC_ALERT = "worker/W001/alert"

# 1.5 TLS certificate paths
ROOT_CA_PATH = "AmazonRootCA1.pem"
CLIENT_CERT_PATH = "device-certificate.pem.crt"
PRIVATE_KEY_PATH = "private.pem.key"


# ============================================================
# 2. HARDWARE PIN CONFIG
# ============================================================

# 2.1 I2C pins
I2C_SDA = 6
I2C_SCL = 7

# 2.2 Alarm and button pins
BUZZER_PIN = 2
CANCEL_BTN_PIN = 3

# 2.3 I2C device addresses
MAX30102_ADDR = 0x57
MPU6050_ADDR = 0x68


# ============================================================
# 3. BIOMETRIC RISK THRESHOLDS
# ============================================================
# Các ngưỡng này dùng cho demo phát hiện rủi ro cơ bản.
# Không dùng để chẩn đoán y tế.

# 3.1 SpO2 thresholds
SPO2_WARNING = 90
SPO2_CRITICAL = 80

# 3.2 Heart-rate thresholds
BPM_BRADY = 50
BPM_TACHY = 120


# ============================================================
# 4. FALL DETECTION THRESHOLDS
# ============================================================
# Thuật toán fall detection:
# free fall -> impact -> post-impact inactivity.
#
# Đơn vị SVM: m/s^2.
# 1g xấp xỉ 9.81 m/s^2.
# ============================================================

# 4.1 Free-fall phase: gia tốc tổng thấp hơn khoảng 0.5g
FREE_FALL_THRESH = 4.9

# 4.2 Impact phase: va chạm mạnh hơn khoảng 3g
IMPACT_THRESH = 29.4

# 4.3 Free-fall phải được theo sau bởi impact trong cửa sổ này
FALL_TIME_WINDOW_MS = 1500

# 4.4 Sau impact, cần bất động liên tục trong khoảng này mới xác nhận té ngã
INACTIVITY_TIME_MS = 3000

# 4.5 Vùng SVM được xem là "ít chuyển động / nằm yên"
# Nếu SVM ra ngoài vùng này, bộ đếm inactivity sẽ reset.
INACTIVITY_SVM_LOW = 7.0
INACTIVITY_SVM_HIGH = 12.5

# 4.6 Nếu sau impact quá lâu mà không bất động, hủy chuỗi fall
POST_IMPACT_TIMEOUT_MS = 6000


# ============================================================
# 5. FATIGUE / HRV THRESHOLDS
# ============================================================
# HRV trong prototype này được ước lượng từ BPM-derived IBI.
# Muốn HRV chính xác hơn cần timestamp từng nhịp tim thật từ MAX30102.
# ============================================================

RMSSD_FATIGUE_LEVEL_1 = 20.0
RMSSD_EXHAUSTION_LEVEL_2 = 15.0
BPM_EXHAUSTION_THRESHOLD = 110

# 5.1 HRV window
# 300000 ms = 5 phút. Nếu demo nhanh có thể đổi thành 60000 ms.
HRV_WINDOW_MS = 300000

# 5.2 IBI validation
MIN_IBI_COUNT = 10
MAX_IBI_BUFFER = 500
MEDIAN_WINDOW = 5


# ============================================================
# 6. TASK TIMING CONFIG
# ============================================================
# Tách tần số đọc IMU và biometric:
# - IMU cần nhanh để bắt free-fall/impact.
# - HR/SpO2 có thể đọc chậm hơn.
# ============================================================

# 6.1 IMU sampling for fall detection
IMU_READ_INTERVAL_MS = 20          # 50 Hz

# 6.2 Heart-rate / SpO2 sampling
BIO_READ_INTERVAL_MS = 1000        # 1 Hz

# 6.3 Cloud telemetry interval
CLOUD_PUBLISH_INTERVAL_MS = 5000

# 6.4 MQTT reconnect interval
MQTT_RECONNECT_INTERVAL_MS = 5000


# ============================================================
# 7. ALERT RATE LIMITING
# ============================================================
# Chống spam alert nếu tình trạng xấu kéo dài.
# ============================================================

ALERT_COOLDOWN_MS = 30000
FALL_ALERT_COOLDOWN_MS = 60000


# ============================================================
# 8. PLACEHOLDERS & PROTOTYPE FLAGS
# ============================================================

# 8.1 AQI chưa có sensor thật trong prototype này
AQI_PLACEHOLDER = 50

# 8.2 True: giả lập MAX30102 để demo không cần sensor thật
# False: thử scan MAX30102 qua I2C, nhưng driver thật chưa được implement.
SIMULATE_MAX30102 = True
