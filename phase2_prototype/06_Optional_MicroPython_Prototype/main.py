# ============================================================
# FOURUT Edge - MicroPython Prototype
# ------------------------------------------------------------
# Đây là bản prototype chạy MicroPython để demo thuật toán và logic.
# Bản này không có driver MAX30102 thật, chỉ trả về dữ liệu giả lập
# ============================================================
# 1. IMPORTS
# ============================================================

import time
import math
import ujson
import network
import machine
import uasyncio as asyncio

from machine import Pin, I2C
from config import *

try:
    from umqtt.simple import MQTTClient
except ImportError:
    MQTTClient = None


# ============================================================
# 2. HARDWARE INITIALIZATION
# ============================================================

# 2.1 Output/input pins
buzzer = Pin(BUZZER_PIN, Pin.OUT)
cancel_button = Pin(CANCEL_BTN_PIN, Pin.IN, Pin.PULL_UP)

# 2.2 Shared I2C bus for MAX30102 and MPU6050
i2c = I2C(0, scl=Pin(I2C_SCL), sda=Pin(I2C_SDA), freq=100000)


# ============================================================
# 3. GLOBAL RUNTIME STATE
# ============================================================

# 3.1 Alarm state
alarm_active = False
alarm_cancelled = False
current_alarm_level = 0
alarm_start_time = 0

# 3.2 Sensor state
first_mpu_sample = True

# 3.3 HRV state
ibi_buffer = []
ibi_history = []
last_hrv_check = time.ticks_ms()

# 3.4 Fatigue state
rmssd_low_count = 0
fatigue_level_1_sent = False

# 3.5 Alert cooldown state
last_alert_time = {}

# 3.6 Latest telemetry snapshot
# Đây là object được publish định kỳ lên TOPIC_TELEMETRY.
latest_data = {
    "worker_id": WORKER_ID,
    "heart_rate": -1,
    "spo2": -1,
    "aqi": AQI_PLACEHOLDER,
    "svm": 9.8,
    "sdnn": -1,
    "rmssd": -1,
    "ibi_count": 0,
    "alarm_active": False,
    "alarm_level": 0,
    "fall_detected": False,
}

# 3.7 Queues for cloud publishing
# Alert/HRV được queue để tránh mất event nếu MQTT tạm thời chưa publish được.
alert_queue = []
hrv_queue = []

# 3.8 MQTT state
mqtt_client = None
last_mqtt_attempt = 0
last_cloud_publish = 0


# ============================================================
# 4. SENSOR DRIVERS / READERS
# ============================================================

class MPU6050:
    # --------------------------------------------------------
    # 4.1 Minimal MPU6050 reader
    # --------------------------------------------------------
    # Cấu hình:
    #   - Accelerometer range: ±8g
    #   - Gyroscope range: ±500 deg/s
    #
    # Trong prototype này chỉ dùng acceleration để tính SVM.
    # --------------------------------------------------------

    def __init__(self, i2c, addr=MPU6050_ADDR):
        self.i2c = i2c
        self.addr = addr

    def begin(self):
        devices = self.i2c.scan()

        if self.addr not in devices:
            return False

        # Wake up MPU6050
        self.i2c.writeto_mem(self.addr, 0x6B, b"\x00")

        # Accelerometer config register 0x1C:
        # 0x10 => ±8g, sensitivity = 4096 LSB/g
        self.i2c.writeto_mem(self.addr, 0x1C, b"\x10")

        # Gyroscope config register 0x1B:
        # 0x08 => ±500 deg/s
        self.i2c.writeto_mem(self.addr, 0x1B, b"\x08")

        return True

    def _read_i16(self, reg):
        data = self.i2c.readfrom_mem(self.addr, reg, 2)
        value = (data[0] << 8) | data[1]

        if value & 0x8000:
            value -= 65536

        return value

    def read_acceleration(self):
        # Raw acceleration registers
        ax_raw = self._read_i16(0x3B)
        ay_raw = self._read_i16(0x3D)
        az_raw = self._read_i16(0x3F)

        # Convert raw value to m/s^2.
        # Với ±8g, sensitivity = 4096 LSB/g.
        ax = (ax_raw / 4096.0) * 9.80665
        ay = (ay_raw / 4096.0) * 9.80665
        az = (az_raw / 4096.0) * 9.80665

        return ax, ay, az


class MAX30102Reader:
    # --------------------------------------------------------
    # 4.2 Minimal MAX30102 reader / simulator
    # --------------------------------------------------------
    # SIMULATE_MAX30102 = True:
    #   - Trả về HR/SpO2 giả lập để test thuật toán.
    #
    # SIMULATE_MAX30102 = False:
    #   - Chỉ scan I2C để xác nhận sensor có mặt.
    #   - Chưa implement thuật toán đọc FIFO/beat detection thật.
    # --------------------------------------------------------

    def __init__(self, i2c, addr=MAX30102_ADDR):
        self.i2c = i2c
        self.addr = addr
        self.counter = 0

    def begin(self):
        if SIMULATE_MAX30102:
            print("MAX30102 running in simulated mode")
            return True

        devices = self.i2c.scan()

        if self.addr not in devices:
            return False

        print("MAX30102 detected, but real HR/SpO2 driver is not implemented in this prototype")
        return True

    def read_heart_spo2(self):
        if SIMULATE_MAX30102:
            self.counter += 1

            # Pattern giả lập:
            #   - Bình thường: HR dao động nhẹ quanh 82, SpO2 97.
            #   - Định kỳ tạo tachycardia nhẹ để test alert.
            #   - Định kỳ tạo SpO2 thấp để test critical alert.
            phase = self.counter % 60

            if phase == 0:
                return 124, 94

            if phase == 30:
                return 86, 78

            bpm_variation = (phase % 7) - 3
            return 82 + bpm_variation, 97

        # Chưa có driver thật trong prototype
        return -1, -1


mpu = MPU6050(i2c)
max30102 = MAX30102Reader(i2c)


# ============================================================
# 5. UTILITY FUNCTIONS
# ============================================================

def ticks_diff_now(start_ms):
    return time.ticks_diff(time.ticks_ms(), start_ms)


def is_valid_ibi(ibi):
    return 300 <= ibi <= 1500


def median_filter(value):
    # --------------------------------------------------------
    # 5.1 Median filter cho IBI
    # --------------------------------------------------------
    # Giảm nhiễu ngắn hạn trước khi đưa vào HRV buffer.
    # --------------------------------------------------------
    global ibi_history

    ibi_history.append(value)

    if len(ibi_history) > MEDIAN_WINDOW:
        ibi_history.pop(0)

    temp = sorted(ibi_history)
    return temp[len(temp) // 2]


def calculate_sdnn(ibis):
    # --------------------------------------------------------
    # 5.2 SDNN
    # --------------------------------------------------------
    # SDNN = standard deviation of NN intervals.
    # Đơn vị: ms.
    # --------------------------------------------------------
    count = len(ibis)

    if count < 2:
        return -1

    mean = sum(ibis) / count
    variance = sum((x - mean) ** 2 for x in ibis) / count

    return math.sqrt(variance)


def calculate_rmssd(ibis):
    # --------------------------------------------------------
    # 5.3 RMSSD
    # --------------------------------------------------------
    # RMSSD = root mean square of successive differences.
    # Đơn vị: ms.
    # --------------------------------------------------------
    count = len(ibis)

    if count < 3:
        return -1

    sum_sq = 0

    for i in range(1, count):
        diff = ibis[i] - ibis[i - 1]
        sum_sq += diff * diff

    return math.sqrt(sum_sq / (count - 1))


def can_send_alert(key, cooldown_ms=ALERT_COOLDOWN_MS):
    # --------------------------------------------------------
    # 5.4 Alert cooldown
    # --------------------------------------------------------
    # Tránh spam alert cùng loại khi tình trạng bất thường kéo dài.
    # --------------------------------------------------------
    global last_alert_time

    now = time.ticks_ms()

    if key not in last_alert_time:
        last_alert_time[key] = now
        return True

    if time.ticks_diff(now, last_alert_time[key]) >= cooldown_ms:
        last_alert_time[key] = now
        return True

    return False


def update_latest_data(
    bpm=None,
    spo2=None,
    aqi=None,
    svm=None,
    sdnn=None,
    rmssd=None,
    ibi_count=None,
    fall_detected=None,
):
    # --------------------------------------------------------
    # 5.5 Update telemetry snapshot safely
    # --------------------------------------------------------
    # Điểm sửa quan trọng:
    #   - Không ghi đè sdnn/rmssd thành -1 sau khi vừa tính HRV.
    #   - Field nào không có dữ liệu mới thì giữ giá trị cũ.
    # --------------------------------------------------------

    if bpm is not None:
        latest_data["heart_rate"] = bpm

    if spo2 is not None:
        latest_data["spo2"] = spo2

    if aqi is not None:
        latest_data["aqi"] = aqi

    if svm is not None:
        latest_data["svm"] = svm

    if sdnn is not None and sdnn >= 0:
        latest_data["sdnn"] = sdnn

    if rmssd is not None and rmssd >= 0:
        latest_data["rmssd"] = rmssd

    if ibi_count is not None:
        latest_data["ibi_count"] = ibi_count

    latest_data["alarm_active"] = alarm_active
    latest_data["alarm_level"] = current_alarm_level

    if fall_detected is not None:
        latest_data["fall_detected"] = fall_detected


# ============================================================
# 6. ALARM & EVENT FUNCTIONS
# ============================================================

def trigger_alarm(level, message):
    # --------------------------------------------------------
    # 6.1 Local alarm
    # --------------------------------------------------------
    # level:
    #   1 = warning
    #   2 = critical
    #   3 = emergency
    # --------------------------------------------------------
    global alarm_active, alarm_cancelled, alarm_start_time, current_alarm_level

    # Nếu alarm hiện tại đã nghiêm trọng hơn hoặc bằng, không hạ cấp.
    if alarm_active and level <= current_alarm_level:
        return

    alarm_active = True
    alarm_cancelled = False
    alarm_start_time = time.ticks_ms()
    current_alarm_level = level

    buzzer.value(1)

    print("ALARM TRIGGERED - Level {}: {}".format(level, message))


def cancel_alarm():
    # --------------------------------------------------------
    # 6.2 User cancellation
    # --------------------------------------------------------
    # Nút cancel chỉ tắt buzzer local.
    # Các điều kiện nguy hiểm vẫn có thể tạo alert mới sau cooldown.
    # --------------------------------------------------------
    global alarm_active, alarm_cancelled, current_alarm_level

    if not alarm_active:
        return

    alarm_active = False
    alarm_cancelled = True
    current_alarm_level = 0

    buzzer.value(0)

    print("Alarm cancelled by user")


def send_sms(message, urgent):
    # --------------------------------------------------------
    # 6.3 SMS placeholder
    # --------------------------------------------------------
    # Prototype chỉ print SMS.
    # Bản thật có thể thay bằng Twilio/Lambda/SNS.
    # --------------------------------------------------------
    label = "URGENT" if urgent else "Normal"
    print("SMS simulated ({}): {}".format(label, message))


def push_alert(category, status, level, message, bpm, spo2, svm, rmssd):
    # --------------------------------------------------------
    # 6.4 Queue alert event for MQTT publish
    # --------------------------------------------------------
    event = {
        "worker_id": WORKER_ID,
        "category": category,
        "status": status,
        "alert_level": level,
        "message": message,
        "heart_rate": bpm,
        "spo2": spo2,
        "svm": svm,
        "rmssd": rmssd,
        "timestamp_ms": time.ticks_ms(),
    }

    alert_queue.append(event)


# ============================================================
# 7. RISK ASSESSMENT - BIOMETRIC
# ============================================================

def assess_biometric_risk(spo2, bpm, svm, rmssd):
    # --------------------------------------------------------
    # 7.1 SpO2 risk
    # --------------------------------------------------------
    # Critical SpO2 kích hoạt buzzer level 2.
    # Warning SpO2 chỉ push alert level 1.
    # --------------------------------------------------------

    if spo2 > 0 and spo2 < SPO2_CRITICAL:
        if can_send_alert("CRITICAL_SPO2"):
            trigger_alarm(2, "CRITICAL: Severe SpO2 drop detected")

            push_alert(
                "BIOMETRIC",
                "CRITICAL_SPO2",
                2,
                "Severe SpO2 drop detected",
                bpm,
                spo2,
                svm,
                rmssd,
            )

            send_sms("EMERGENCY: Worker has critical low SpO2", True)

    elif spo2 > 0 and spo2 < SPO2_WARNING:
        if can_send_alert("WARNING_SPO2"):
            push_alert(
                "BIOMETRIC",
                "WARNING_SPO2",
                1,
                "Mild hypoxemia detected",
                bpm,
                spo2,
                svm,
                rmssd,
            )

    # --------------------------------------------------------
    # 7.2 Heart-rate risk
    # --------------------------------------------------------

    if bpm > 0 and bpm < BPM_BRADY:
        if can_send_alert("BRADYCARDIA"):
            trigger_alarm(1, "WARNING: Bradycardia detected")

            push_alert(
                "BIOMETRIC",
                "BRADYCARDIA",
                1,
                "Heart rate too low",
                bpm,
                spo2,
                svm,
                rmssd,
            )

    elif bpm > BPM_TACHY:
        if can_send_alert("TACHYCARDIA"):
            trigger_alarm(1, "WARNING: Tachycardia detected")

            push_alert(
                "BIOMETRIC",
                "TACHYCARDIA",
                1,
                "Heart rate too high",
                bpm,
                spo2,
                svm,
                rmssd,
            )


# ============================================================
# 8. RISK ASSESSMENT - FALL DETECTION
# ============================================================
# Thuật toán 3 pha:
#   Phase A: Free fall
#       SVM < FREE_FALL_THRESH
#
#   Phase B: Impact
#       Trong FALL_TIME_WINDOW_MS sau free fall,
#       SVM > IMPACT_THRESH
#
#   Phase C: Post-impact inactivity
#       Sau impact, SVM phải nằm trong vùng ít chuyển động
#       liên tục INACTIVITY_TIME_MS.
#
# Lý do sửa:
#   Bản cũ chỉ đợi 3 giây sau impact rồi báo fall.
#   Bản này kiểm tra "bất động thật sự" để giảm false positive.
# ============================================================

fall_state = {
    "in_fall_sequence": False,
    "waiting_inactivity": False,
    "fall_sequence_start": 0,
    "impact_time": 0,
    "inactivity_start": 0,
}


def reset_fall_state():
    fall_state["in_fall_sequence"] = False
    fall_state["waiting_inactivity"] = False
    fall_state["fall_sequence_start"] = 0
    fall_state["impact_time"] = 0
    fall_state["inactivity_start"] = 0


def is_inactive_svm(svm):
    return INACTIVITY_SVM_LOW <= svm <= INACTIVITY_SVM_HIGH


def assess_fall_risk(svm, bpm, spo2, rmssd):
    now = time.ticks_ms()

    # --------------------------------------------------------
    # 8.1 Phase A - Free fall detection
    # --------------------------------------------------------
    if (
        not fall_state["in_fall_sequence"]
        and not fall_state["waiting_inactivity"]
        and svm < FREE_FALL_THRESH
    ):
        fall_state["in_fall_sequence"] = True
        fall_state["fall_sequence_start"] = now
        print("FREE FALL PHASE DETECTED | SVM: {:.2f}".format(svm))

    # --------------------------------------------------------
    # 8.2 Phase B - Impact detection
    # --------------------------------------------------------
    if fall_state["in_fall_sequence"] and svm > IMPACT_THRESH:
        fall_state["in_fall_sequence"] = False
        fall_state["waiting_inactivity"] = True
        fall_state["impact_time"] = now
        fall_state["inactivity_start"] = 0
        print("IMPACT PHASE DETECTED | SVM: {:.2f}".format(svm))

    # --------------------------------------------------------
    # 8.3 Timeout if free fall is not followed by impact
    # --------------------------------------------------------
    if (
        fall_state["in_fall_sequence"]
        and time.ticks_diff(now, fall_state["fall_sequence_start"]) > FALL_TIME_WINDOW_MS
    ):
        print("Fall sequence cancelled: no impact within time window")
        reset_fall_state()

    # --------------------------------------------------------
    # 8.4 Phase C - Post-impact inactivity confirmation
    # --------------------------------------------------------
    if fall_state["waiting_inactivity"]:
        # Nếu sau impact quá lâu mà không ổn định, hủy chuỗi fall.
        if time.ticks_diff(now, fall_state["impact_time"]) > POST_IMPACT_TIMEOUT_MS:
            print("Fall sequence cancelled: no inactivity after impact")
            reset_fall_state()
            return False

        # Chỉ tính inactivity khi SVM nằm quanh 1g.
        if is_inactive_svm(svm):
            if fall_state["inactivity_start"] == 0:
                fall_state["inactivity_start"] = now

            inactive_duration = time.ticks_diff(now, fall_state["inactivity_start"])

            if inactive_duration >= INACTIVITY_TIME_MS:
                reset_fall_state()

                if can_send_alert("FALL_EMERGENCY", FALL_ALERT_COOLDOWN_MS):
                    trigger_alarm(3, "EMERGENCY: Worker fall detected")

                    push_alert(
                        "FALL",
                        "EMERGENCY",
                        3,
                        "Worker fall detected",
                        bpm,
                        spo2,
                        svm,
                        rmssd,
                    )

                    send_sms(
                        "EMERGENCY: Worker fall detected. Immediate assistance needed",
                        True,
                    )

                return True

        else:
            # Nếu vẫn chuyển động mạnh, reset timer inactivity.
            fall_state["inactivity_start"] = 0

    return False


# ============================================================
# 9. RISK ASSESSMENT - FATIGUE / HRV
# ============================================================

def assess_fatigue_risk(rmssd, bpm, spo2, svm):
    # --------------------------------------------------------
    # 9.1 Fatigue logic
    # --------------------------------------------------------
    # Level 1:
    #   RMSSD thấp trong 3 cửa sổ liên tiếp.
    #
    # Level 2:
    #   RMSSD rất thấp + BPM cao.
    #
    # Ghi chú:
    #   Trong prototype, RMSSD được tính từ BPM-derived IBI.
    #   Đây là demo logic, không phải HRV y tế.
    # --------------------------------------------------------
    global rmssd_low_count, fatigue_level_1_sent

    if rmssd >= 0 and rmssd < RMSSD_EXHAUSTION_LEVEL_2 and bpm > BPM_EXHAUSTION_THRESHOLD:
        rmssd_low_count = 0
        fatigue_level_1_sent = False

        if can_send_alert("FATIGUE_LEVEL_2"):
            trigger_alarm(2, "FATIGUE LEVEL 2: Worker exhaustion detected")

            push_alert(
                "FATIGUE",
                "LEVEL_2_EXHAUSTION",
                2,
                "Worker exhaustion detected",
                bpm,
                spo2,
                svm,
                rmssd,
            )

            send_sms("Worker exhaustion detected. Please check on worker", False)

        return

    if rmssd >= 0 and rmssd < RMSSD_FATIGUE_LEVEL_1:
        rmssd_low_count += 1

        print("Low RMSSD: {:.1f} ms | window {}/3".format(rmssd, rmssd_low_count))

        if rmssd_low_count >= 3 and not fatigue_level_1_sent:
            fatigue_level_1_sent = True

            if can_send_alert("FATIGUE_LEVEL_1"):
                trigger_alarm(1, "FATIGUE LEVEL 1: Worker showing signs of fatigue")

                push_alert(
                    "FATIGUE",
                    "LEVEL_1_WARNING",
                    1,
                    "RMSSD below threshold for 3 consecutive windows",
                    bpm,
                    spo2,
                    svm,
                    rmssd,
                )
    else:
        rmssd_low_count = 0
        fatigue_level_1_sent = False


def add_ibi_from_bpm(bpm):
    # --------------------------------------------------------
    # 9.2 Prototype IBI generation
    # --------------------------------------------------------
    #   IBI thật = thời gian giữa 2 nhịp tim thật.
    #   Prototype này chưa có beat detection từ MAX30102 FIFO.
    #   Vì vậy tạm ước lượng IBI = 60000 / BPM.
    #
    # Khi làm bản thật:
    #   - detect từng beat,
    #   - lưu timestamp,
    #   - IBI = timestamp_beat_n - timestamp_beat_n_minus_1.
    # --------------------------------------------------------
    global ibi_buffer

    if not (40 <= bpm <= 200):
        return

    current_ibi = int(60000 / bpm)

    if not is_valid_ibi(current_ibi):
        return

    filtered_ibi = median_filter(current_ibi)

    if len(ibi_buffer) < MAX_IBI_BUFFER:
        ibi_buffer.append(filtered_ibi)


def update_hrv_on_edge(bpm, spo2, svm):
    # --------------------------------------------------------
    # 9.3 HRV window update
    # --------------------------------------------------------
    # Tính SDNN/RMSSD từ buffer IBI hiện tại.
    # Sau khi publish HRV summary thì reset buffer cho window mới.
    # --------------------------------------------------------
    global ibi_buffer

    if len(ibi_buffer) < MIN_IBI_COUNT:
        print("HRV insufficient data: {}/{}".format(len(ibi_buffer), MIN_IBI_COUNT))
        return

    sdnn = calculate_sdnn(ibi_buffer)
    rmssd = calculate_rmssd(ibi_buffer)

    print("HRV SUMMARY")
    print(
        "SDNN: {:.2f} ms | RMSSD: {:.2f} ms | IBI count: {}".format(
            sdnn,
            rmssd,
            len(ibi_buffer),
        )
    )

    assess_fatigue_risk(rmssd, bpm, spo2, svm)

    update_latest_data(
        sdnn=sdnn,
        rmssd=rmssd,
        ibi_count=len(ibi_buffer),
    )

    hrv_queue.append(dict(latest_data))

    ibi_buffer = []


# ============================================================
# 10. NETWORK & MQTT
# ============================================================

def connect_wifi():
    # --------------------------------------------------------
    # 10.1 Wi-Fi connection
    # --------------------------------------------------------
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)

    if wlan.isconnected():
        return True

    print("Connecting Wi-Fi...")
    wlan.connect(WIFI_SSID, WIFI_PASSWORD)

    for _ in range(20):
        if wlan.isconnected():
            print("Wi-Fi connected:", wlan.ifconfig())
            return True

        time.sleep(0.5)

    print("Wi-Fi connection failed")
    return False


def load_file(path):
    with open(path, "rb") as f:
        return f.read()


def connect_mqtt():
    # --------------------------------------------------------
    # 10.2 AWS IoT MQTT connection
    # --------------------------------------------------------
    global mqtt_client, last_mqtt_attempt

    if MQTTClient is None:
        print("MQTT library is not available")
        return None

    now = time.ticks_ms()

    if time.ticks_diff(now, last_mqtt_attempt) < MQTT_RECONNECT_INTERVAL_MS:
        return mqtt_client

    last_mqtt_attempt = now

    try:
        key = load_file(PRIVATE_KEY_PATH)
        cert = load_file(CLIENT_CERT_PATH)

        ssl_params = {
            "key": key,
            "cert": cert,
            "server_hostname": MQTT_BROKER,
        }

        mqtt_client = MQTTClient(
            CLIENT_ID,
            MQTT_BROKER,
            port=MQTT_PORT,
            keepalive=60,
            ssl=True,
            ssl_params=ssl_params,
        )

        mqtt_client.connect()
        print("Connected to AWS IoT Core")

        return mqtt_client

    except Exception as e:
        mqtt_client = None
        print("MQTT reconnect failed:", e)
        return None


def publish_json(topic, data):
    # --------------------------------------------------------
    # 10.3 JSON publish helper
    # --------------------------------------------------------
    global mqtt_client

    if mqtt_client is None:
        return False

    try:
        payload = ujson.dumps(data)
        mqtt_client.publish(topic, payload)
        print("Published:", topic)
        print(payload)
        return True

    except Exception as e:
        print("Publish failed:", e)
        mqtt_client = None
        return False


# ============================================================
# 11. ASYNC TASKS
# ============================================================

async def anomaly_detection_task():
    # --------------------------------------------------------
    # 11.1 Edge anomaly detection task
    # --------------------------------------------------------
    # Tách 2 tốc độ đọc:
    #   - IMU: 20 ms để bắt fall event.
    #   - Bio: 1000 ms để đọc HR/SpO2.
    # --------------------------------------------------------
    global first_mpu_sample, last_hrv_check

    last_imu_read = time.ticks_ms()
    last_bio_read = time.ticks_ms()

    while True:
        now = time.ticks_ms()

        # ----------------------------------------------------
        # 11.1.1 Fast loop: IMU / fall detection
        # ----------------------------------------------------
        if time.ticks_diff(now, last_imu_read) >= IMU_READ_INTERVAL_MS:
            last_imu_read = now

            local_svm = latest_data["svm"]
            local_fall_detected = False

            try:
                ax, ay, az = mpu.read_acceleration()
                local_svm = math.sqrt(ax * ax + ay * ay + az * az)

                # Bỏ qua sample đầu vì một số board có thể đọc giá trị chưa ổn định.
                if first_mpu_sample:
                    first_mpu_sample = False
                    local_svm = 9.8

            except Exception as e:
                print("MPU6050 read failed:", e)

            local_fall_detected = assess_fall_risk(
                local_svm,
                latest_data["heart_rate"],
                latest_data["spo2"],
                latest_data["rmssd"],
            )

            update_latest_data(
                svm=local_svm,
                fall_detected=local_fall_detected,
            )

        # ----------------------------------------------------
        # 11.1.2 Slow loop: HR/SpO2 / biometric risk
        # ----------------------------------------------------
        if time.ticks_diff(now, last_bio_read) >= BIO_READ_INTERVAL_MS:
            last_bio_read = now

            local_bpm, local_spo2 = max30102.read_heart_spo2()
            local_aqi = AQI_PLACEHOLDER

            # Validate SpO2
            if local_spo2 < 70 or local_spo2 > 100:
                local_spo2 = -1

            # Validate BPM and feed IBI buffer
            if 40 <= local_bpm <= 200:
                add_ibi_from_bpm(local_bpm)
            else:
                local_bpm = -1

            assess_biometric_risk(
                local_spo2,
                local_bpm,
                latest_data["svm"],
                latest_data["rmssd"],
            )

            update_latest_data(
                bpm=local_bpm,
                spo2=local_spo2,
                aqi=local_aqi,
                ibi_count=len(ibi_buffer),
            )

            print(
                "EDGE | HR:{} SpO2:{} AQI:{} SVM:{:.2f} SDNN:{} RMSSD:{} Alarm:{}".format(
                    latest_data["heart_rate"],
                    latest_data["spo2"],
                    latest_data["aqi"],
                    latest_data["svm"],
                    latest_data["sdnn"],
                    latest_data["rmssd"],
                    latest_data["alarm_level"],
                )
            )

        # ----------------------------------------------------
        # 11.1.3 HRV window update
        # ----------------------------------------------------
        if time.ticks_diff(now, last_hrv_check) >= HRV_WINDOW_MS:
            last_hrv_check = now

            update_hrv_on_edge(
                latest_data["heart_rate"],
                latest_data["spo2"],
                latest_data["svm"],
            )

        await asyncio.sleep_ms(5)


async def alarm_control_task():
    # --------------------------------------------------------
    # 11.2 Alarm control task
    # --------------------------------------------------------
    # Đọc nút cancel và điều khiển buzzer.
    # --------------------------------------------------------
    while True:
        if cancel_button.value() == 0 and alarm_active:
            await asyncio.sleep_ms(50)

            if cancel_button.value() == 0:
                cancel_alarm()

                while cancel_button.value() == 0:
                    await asyncio.sleep_ms(10)

        if alarm_active:
            buzzer.value(1)
        else:
            buzzer.value(0)

        await asyncio.sleep_ms(50)


async def cloud_publish_task():
    # --------------------------------------------------------
    # 11.3 Cloud publish task
    # --------------------------------------------------------
    # Ưu tiên publish queued alerts trước, sau đó HRV,
    # cuối cùng là telemetry định kỳ.
    # --------------------------------------------------------
    global last_cloud_publish

    connect_wifi()

    while True:
        if mqtt_client is None:
            connect_mqtt()

        if mqtt_client is not None:
            while alert_queue:
                event = alert_queue.pop(0)
                publish_json(TOPIC_ALERT, event)

            while hrv_queue:
                hrv_data = hrv_queue.pop(0)
                publish_json(TOPIC_HRV, hrv_data)

            now = time.ticks_ms()

            if time.ticks_diff(now, last_cloud_publish) >= CLOUD_PUBLISH_INTERVAL_MS:
                last_cloud_publish = now
                publish_json(TOPIC_TELEMETRY, latest_data)

        await asyncio.sleep_ms(200)


# ============================================================
# 12. PROGRAM ENTRY POINT
# ============================================================

async def main():
    print("FOURUT - MicroPython Edge Prototype")
    print("Main production firmware remains: FOURUT_Edge_FreeRTOS.ino")

    buzzer.value(0)

    print("Initializing MAX30102...")
    if not max30102.begin():
        print("MAX30102 init failed")
    else:
        print("MAX30102 init success")

    print("Initializing MPU6050...")
    if not mpu.begin():
        print("MPU6050 init failed")
    else:
        print("MPU6050 init success")

    asyncio.create_task(anomaly_detection_task())
    asyncio.create_task(alarm_control_task())
    asyncio.create_task(cloud_publish_task())

    while True:
        await asyncio.sleep(1)


asyncio.run(main())
