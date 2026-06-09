# ============================================================
# FOURUT Edge - Optional MicroPython Prototype
# ------------------------------------------------------------
# This prototype demonstrates the edge-device workflow in a
# compact MicroPython implementation. The deployed firmware remains
# the Arduino C++ FreeRTOS version: FOURUT_Edge_FreeRTOS.ino.
# ============================================================

import time
import math
import ujson
import network
import uasyncio as asyncio

from machine import Pin, I2C
from config import *

try:
    from umqtt.simple import MQTTClient
except ImportError:
    MQTTClient = None


# ============================================================
# 1. HARDWARE INITIALIZATION
# ============================================================

buzzer = Pin(BUZZER_PIN, Pin.OUT)
cancel_button = Pin(CANCEL_BTN_PIN, Pin.IN, Pin.PULL_UP)

i2c = I2C(0, scl=Pin(I2C_SCL), sda=Pin(I2C_SDA), freq=100000)


# ============================================================
# 2. GLOBAL RUNTIME STATE
# ============================================================

alarm_active = False
alarm_cancelled = False
current_alarm_level = 0
alarm_start_time = 0

first_mpu_sample = True
max30102_available = False
mpu_available = False

ibi_buffer = []
ibi_history = []
last_hrv_check = time.ticks_ms()

rmssd_low_count = 0
fatigue_level_1_sent = False

last_alert_time = {}
fall_latch_until = 0

latest_data = {
    "worker_id": WORKER_ID,
    "heart_rate": -1,
    "spo2": -1,
    "aqi": AQI_PLACEHOLDER,
    "pm25_ugm3": PM25_PLACEHOLDER,
    "pm10_ugm3": PM10_PLACEHOLDER,
    "env_node_id": ENV_NODE_ID,
    "env_online": SIMULATE_ENVIRONMENT,
    "svm": 9.8,
    "sdnn": -1,
    "rmssd": -1,
    "ibi_count": 0,
    "alarm_active": False,
    "alarm_level": 0,
    "fall_detected": False,
}

alert_queue = []
hrv_queue = []

mqtt_client = None
last_wifi_attempt = 0
last_mqtt_attempt = 0
last_cloud_publish = 0
last_env_update = 0


# ============================================================
# 3. SENSOR READERS
# ============================================================

class MPU6050:
    """Minimal MPU6050 reader for acceleration and gyroscope data."""

    def __init__(self, i2c, addr=MPU6050_ADDR):
        self.i2c = i2c
        self.addr = addr

    def begin(self):
        devices = self.i2c.scan()

        if self.addr not in devices:
            return False

        # Wake up the device.
        self.i2c.writeto_mem(self.addr, 0x6B, bytes([0x00]))

        # Accelerometer: ±8g, sensitivity = 4096 LSB/g.
        self.i2c.writeto_mem(self.addr, 0x1C, bytes([0x10]))

        # Gyroscope: ±500 deg/s, sensitivity = 65.5 LSB/(deg/s).
        self.i2c.writeto_mem(self.addr, 0x1B, bytes([0x08]))

        return True

    def _read_i16(self, reg):
        data = self.i2c.readfrom_mem(self.addr, reg, 2)
        value = (data[0] << 8) | data[1]

        if value & 0x8000:
            value -= 65536

        return value

    def read_motion(self):
        ax_raw = self._read_i16(0x3B)
        ay_raw = self._read_i16(0x3D)
        az_raw = self._read_i16(0x3F)

        gx_raw = self._read_i16(0x43)
        gy_raw = self._read_i16(0x45)
        gz_raw = self._read_i16(0x47)

        ax = (ax_raw / 4096.0) * 9.80665
        ay = (ay_raw / 4096.0) * 9.80665
        az = (az_raw / 4096.0) * 9.80665

        gx = (gx_raw / 65.5) * math.pi / 180.0
        gy = (gy_raw / 65.5) * math.pi / 180.0
        gz = (gz_raw / 65.5) * math.pi / 180.0

        return ax, ay, az, gx, gy, gz


class MAX30102Reader:
    """Prototype MAX30102 reader.

    In simulated mode, the class returns controlled HR and SpO2 values.
    In non-simulated mode, it only checks device presence on I2C. A full
    MAX30102 FIFO and beat-detection driver is outside this prototype.
    """

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

        print("MAX30102 detected; real HR/SpO2 driver is not implemented in this prototype")
        return True

    def read_heart_spo2(self):
        if not SIMULATE_MAX30102:
            return -1, -1

        self.counter += 1
        phase = self.counter % 60

        # Periodic values are used to exercise alert paths during testing.
        if phase == 0:
            return 124, 94

        if phase == 30:
            return 86, 78

        bpm_variation = (phase % 7) - 3
        return 82 + bpm_variation, 97


mpu = MPU6050(i2c)
max30102 = MAX30102Reader(i2c)


# ============================================================
# 4. UTILITY FUNCTIONS
# ============================================================

def is_valid_ibi(ibi):
    return 300 <= ibi <= 1500


def median_filter(value):
    global ibi_history

    ibi_history.append(value)

    if len(ibi_history) > MEDIAN_WINDOW:
        ibi_history.pop(0)

    temp = sorted(ibi_history)
    return temp[len(temp) // 2]


def calculate_sdnn(ibis):
    count = len(ibis)

    if count < 2:
        return -1

    mean = sum(ibis) / count
    variance = sum((x - mean) ** 2 for x in ibis) / count

    return math.sqrt(variance)


def calculate_rmssd(ibis):
    count = len(ibis)

    if count < 3:
        return -1

    sum_sq = 0

    for i in range(1, count):
        diff = ibis[i] - ibis[i - 1]
        sum_sq += diff * diff

    return math.sqrt(sum_sq / (count - 1))


def vector_angle_deg(ax1, ay1, az1, ax2, ay2, az2):
    dot = ax1 * ax2 + ay1 * ay2 + az1 * az2
    mag1 = math.sqrt(ax1 * ax1 + ay1 * ay1 + az1 * az1)
    mag2 = math.sqrt(ax2 * ax2 + ay2 * ay2 + az2 * az2)

    if mag1 < 0.001 or mag2 < 0.001:
        return 0.0

    cos_angle = dot / (mag1 * mag2)
    cos_angle = max(-1.0, min(1.0, cos_angle))

    return math.acos(cos_angle) * 180.0 / math.pi


def can_send_alert(key, cooldown_ms=ALERT_COOLDOWN_MS):
    global last_alert_time

    now = time.ticks_ms()

    if key not in last_alert_time:
        last_alert_time[key] = now
        return True

    if time.ticks_diff(now, last_alert_time[key]) >= cooldown_ms:
        last_alert_time[key] = now
        return True

    return False


def is_latched(until_ms):
    return time.ticks_diff(until_ms, time.ticks_ms()) > 0


def ticks_add_ms(start_ms, delta_ms):
    """Compatibility wrapper for MicroPython builds without time.ticks_add()."""
    if hasattr(time, "ticks_add"):
        return time.ticks_add(start_ms, delta_ms)

    return start_ms + delta_ms


def update_latest_data(
    bpm=None,
    spo2=None,
    aqi=None,
    pm25=None,
    pm10=None,
    env_online=None,
    svm=None,
    sdnn=None,
    rmssd=None,
    ibi_count=None,
    fall_detected=None,
):
    if bpm is not None:
        latest_data["heart_rate"] = bpm

    if spo2 is not None:
        latest_data["spo2"] = spo2

    if aqi is not None:
        latest_data["aqi"] = aqi

    if pm25 is not None:
        latest_data["pm25_ugm3"] = pm25

    if pm10 is not None:
        latest_data["pm10_ugm3"] = pm10

    if env_online is not None:
        latest_data["env_online"] = env_online

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
# 5. ALARM AND EVENT FUNCTIONS
# ============================================================

def trigger_alarm(level, message):
    global alarm_active, alarm_cancelled, alarm_start_time, current_alarm_level

    if alarm_active and level <= current_alarm_level:
        return

    alarm_active = True
    alarm_cancelled = False
    alarm_start_time = time.ticks_ms()
    current_alarm_level = level

    buzzer.value(1)
    print("ALARM TRIGGERED - Level {}: {}".format(level, message))


def cancel_alarm():
    global alarm_active, alarm_cancelled, current_alarm_level

    if not alarm_active:
        return

    alarm_active = False
    alarm_cancelled = True
    current_alarm_level = 0

    buzzer.value(0)
    print("Alarm cancelled by user")


def send_sms(message, urgent):
    label = "URGENT" if urgent else "Normal"
    print("SMS simulated ({}): {}".format(label, message))


def push_alert(category, status, level, message, bpm, spo2, svm, rmssd, aqi=None, pm25=None, pm10=None):
    event = {
        "worker_id": WORKER_ID,
        "category": category,
        "status": status,
        "alert_level": level,
        "alarm_level": level,
        "message": message,
        "heart_rate": bpm,
        "spo2": spo2,
        "aqi": latest_data["aqi"] if aqi is None else aqi,
        "pm25_ugm3": latest_data["pm25_ugm3"] if pm25 is None else pm25,
        "pm10_ugm3": latest_data["pm10_ugm3"] if pm10 is None else pm10,
        "svm": svm,
        "rmssd": rmssd,
        "timestamp_ms": time.ticks_ms(),
    }

    alert_queue.append(event)


# ============================================================
# 6. RISK ASSESSMENT
# ============================================================

def assess_biometric_risk(spo2, bpm, svm, rmssd):
    if spo2 > 0 and spo2 < SPO2_CRITICAL:
        if can_send_alert("CRITICAL_SPO2"):
            trigger_alarm(2, "CRITICAL: Severe SpO2 drop detected")
            push_alert("BIOMETRIC", "CRITICAL_SPO2", 2, "Severe SpO2 drop detected", bpm, spo2, svm, rmssd)
            send_sms("EMERGENCY: Worker has critical low SpO2", True)

    elif spo2 > 0 and spo2 < SPO2_WARNING:
        if can_send_alert("WARNING_SPO2"):
            push_alert("BIOMETRIC", "WARNING_SPO2", 1, "Mild hypoxemia detected", bpm, spo2, svm, rmssd)

    if bpm > 0 and bpm < BPM_BRADY:
        if can_send_alert("BRADYCARDIA"):
            trigger_alarm(1, "WARNING: Bradycardia detected")
            push_alert("BIOMETRIC", "BRADYCARDIA", 1, "Heart rate too low", bpm, spo2, svm, rmssd)

    elif bpm > BPM_TACHY:
        if can_send_alert("TACHYCARDIA"):
            trigger_alarm(1, "WARNING: Tachycardia detected")
            push_alert("BIOMETRIC", "TACHYCARDIA", 1, "Heart rate too high", bpm, spo2, svm, rmssd)


fall_state = {
    "in_fall_sequence": False,
    "waiting_inactivity": False,
    "fall_sequence_start": 0,
    "impact_time": 0,
    "still_start": 0,
    "stable_ax": 0.0,
    "stable_ay": 0.0,
    "stable_az": 9.8,
}


def reset_fall_state():
    fall_state["in_fall_sequence"] = False
    fall_state["waiting_inactivity"] = False
    fall_state["fall_sequence_start"] = 0
    fall_state["impact_time"] = 0
    fall_state["still_start"] = 0


def assess_fall_risk(svm, ax, ay, az, gyro_magnitude, bpm, spo2, rmssd):
    global fall_latch_until

    now = time.ticks_ms()
    normal_gravity = STILL_SVM_MIN <= svm <= STILL_SVM_MAX
    device_still = normal_gravity and gyro_magnitude <= STILL_GYRO_THRESH

    if (
        not fall_state["in_fall_sequence"]
        and not fall_state["waiting_inactivity"]
        and device_still
    ):
        fall_state["stable_ax"] = ax
        fall_state["stable_ay"] = ay
        fall_state["stable_az"] = az

    if (
        not fall_state["in_fall_sequence"]
        and not fall_state["waiting_inactivity"]
        and svm < FREE_FALL_THRESH
    ):
        fall_state["in_fall_sequence"] = True
        fall_state["fall_sequence_start"] = now
        print("FREE FALL PHASE DETECTED | SVM: {:.2f}".format(svm))

    if fall_state["in_fall_sequence"] and svm > IMPACT_THRESH:
        fall_state["in_fall_sequence"] = False
        fall_state["waiting_inactivity"] = True
        fall_state["impact_time"] = now
        fall_state["still_start"] = 0
        print("IMPACT PHASE DETECTED | SVM: {:.2f}".format(svm))

    if (
        fall_state["in_fall_sequence"]
        and time.ticks_diff(now, fall_state["fall_sequence_start"]) > FALL_TIME_WINDOW_MS
    ):
        print("Fall sequence cancelled: no impact within the time window")
        reset_fall_state()

    if fall_state["waiting_inactivity"]:
        posture_change = vector_angle_deg(
            fall_state["stable_ax"],
            fall_state["stable_ay"],
            fall_state["stable_az"],
            ax,
            ay,
            az,
        )

        posture_changed = posture_change >= POSTURE_CHANGE_THRESH_DEG

        if posture_changed and device_still:
            if fall_state["still_start"] == 0:
                fall_state["still_start"] = now
                print("POSTURE CHANGE + STILLNESS DETECTED")

            if time.ticks_diff(now, fall_state["still_start"]) >= INACTIVITY_TIME_MS:
                reset_fall_state()

                if can_send_alert("FALL_EMERGENCY", FALL_ALERT_COOLDOWN_MS):
                    trigger_alarm(3, "EMERGENCY: Worker fall detected")
                    push_alert("FALL", "EMERGENCY", 3, "Worker fall detected", bpm, spo2, svm, rmssd)
                    send_sms("EMERGENCY: Worker fall detected. Immediate assistance needed", True)

                fall_latch_until = ticks_add_ms(now, FALL_LATCH_MS)
                return True
        else:
            fall_state["still_start"] = 0

        if time.ticks_diff(now, fall_state["impact_time"]) > FALL_CONFIRM_TIMEOUT_MS:
            print("Fall sequence cancelled: no confirmed stillness or posture change")
            reset_fall_state()

    return False


def assess_environmental_risk():
    if not latest_data["env_online"]:
        return

    aqi = latest_data["aqi"]

    if aqi > AQI_DANGER_LEVEL:
        if can_send_alert("AQI_DANGER"):
            trigger_alarm(2, "DANGER: Poor air quality detected")
            push_alert(
                "ENVIRONMENT",
                "AQI_DANGER",
                2,
                "Dangerous air quality detected",
                latest_data["heart_rate"],
                latest_data["spo2"],
                latest_data["svm"],
                latest_data["rmssd"],
                latest_data["aqi"],
                latest_data["pm25_ugm3"],
                latest_data["pm10_ugm3"],
            )

    elif aqi > AQI_WARNING_LEVEL:
        if can_send_alert("AQI_WARNING"):
            push_alert(
                "ENVIRONMENT",
                "AQI_WARNING",
                1,
                "Poor air quality warning",
                latest_data["heart_rate"],
                latest_data["spo2"],
                latest_data["svm"],
                latest_data["rmssd"],
                latest_data["aqi"],
                latest_data["pm25_ugm3"],
                latest_data["pm10_ugm3"],
            )


def assess_fatigue_risk(rmssd, bpm, spo2, svm):
    global rmssd_low_count, fatigue_level_1_sent

    if rmssd >= 0 and rmssd < RMSSD_EXHAUSTION_LEVEL_2 and bpm > BPM_EXHAUSTION_THRESHOLD:
        rmssd_low_count = 0
        fatigue_level_1_sent = False

        if can_send_alert("FATIGUE_LEVEL_2"):
            trigger_alarm(2, "FATIGUE LEVEL 2: Worker exhaustion detected")
            push_alert("FATIGUE", "LEVEL_2_EXHAUSTION", 2, "Worker exhaustion detected", bpm, spo2, svm, rmssd)
            send_sms("Worker exhaustion detected. Please check on worker", False)

        return

    if rmssd >= 0 and rmssd < RMSSD_FATIGUE_LEVEL_1:
        rmssd_low_count += 1
        print("Low RMSSD: {:.1f} ms | window {}/3".format(rmssd, rmssd_low_count))

        if rmssd_low_count >= 3 and not fatigue_level_1_sent:
            fatigue_level_1_sent = True

            if can_send_alert("FATIGUE_LEVEL_1"):
                trigger_alarm(1, "FATIGUE LEVEL 1: Worker showing signs of fatigue")
                push_alert("FATIGUE", "LEVEL_1_WARNING", 1, "RMSSD below threshold for 3 consecutive windows", bpm, spo2, svm, rmssd)
    else:
        rmssd_low_count = 0
        fatigue_level_1_sent = False


def add_ibi_from_bpm(bpm):
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
    global ibi_buffer

    if len(ibi_buffer) < MIN_IBI_COUNT:
        print("HRV insufficient data: {}/{}".format(len(ibi_buffer), MIN_IBI_COUNT))
        return

    sdnn = calculate_sdnn(ibi_buffer)
    rmssd = calculate_rmssd(ibi_buffer)

    print("HRV SUMMARY")
    print("SDNN: {:.2f} ms | RMSSD: {:.2f} ms | IBI count: {}".format(sdnn, rmssd, len(ibi_buffer)))

    assess_fatigue_risk(rmssd, bpm, spo2, svm)

    update_latest_data(sdnn=sdnn, rmssd=rmssd, ibi_count=len(ibi_buffer))
    hrv_queue.append(dict(latest_data))

    ibi_buffer = []


# ============================================================
# 7. NETWORK AND MQTT
# ============================================================

def connect_wifi():
    global last_wifi_attempt

    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)

    if wlan.isconnected():
        return True

    now = time.ticks_ms()

    if time.ticks_diff(now, last_wifi_attempt) < WIFI_RECONNECT_INTERVAL_MS:
        return False

    last_wifi_attempt = now
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
    except Exception as e:
        mqtt_client = None
        print("MQTT certificate/key file not found or unreadable:", e)
        return None

    ssl_variants = [
        {
            "key": key,
            "cert": cert,
            "server_hostname": MQTT_BROKER,
        },
        {
            "key": key,
            "cert": cert,
        },
    ]

    for ssl_params in ssl_variants:
        try:
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

        except TypeError as e:
            print("MQTT TLS parameter variant not supported:", e)
            mqtt_client = None

        except Exception as e:
            print("MQTT reconnect failed:", e)
            mqtt_client = None

    return None

def publish_json(topic, data):
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
# 8. ASYNCHRONOUS TASKS
# ============================================================

async def anomaly_detection_task():
    global first_mpu_sample, last_hrv_check, last_env_update

    last_imu_read = time.ticks_ms()
    last_bio_read = time.ticks_ms()

    while True:
        now = time.ticks_ms()

        if time.ticks_diff(now, last_imu_read) >= IMU_READ_INTERVAL_MS:
            last_imu_read = now

            local_svm = latest_data["svm"]
            local_fall_detected = is_latched(fall_latch_until)

            try:
                if mpu_available:
                    ax, ay, az, gx, gy, gz = mpu.read_motion()
                else:
                    ax, ay, az, gx, gy, gz = 0.0, 0.0, 9.8, 0.0, 0.0, 0.0

                local_svm = math.sqrt(ax * ax + ay * ay + az * az)
                gyro_magnitude = math.sqrt(gx * gx + gy * gy + gz * gz)

                if first_mpu_sample:
                    first_mpu_sample = False
                    local_svm = 9.8

                confirmed_fall = assess_fall_risk(
                    local_svm,
                    ax,
                    ay,
                    az,
                    gyro_magnitude,
                    latest_data["heart_rate"],
                    latest_data["spo2"],
                    latest_data["rmssd"],
                )

                local_fall_detected = confirmed_fall or is_latched(fall_latch_until)

            except Exception as e:
                print("MPU6050 read failed:", e)

            update_latest_data(svm=local_svm, fall_detected=local_fall_detected)

        if time.ticks_diff(now, last_bio_read) >= BIO_READ_INTERVAL_MS:
            last_bio_read = now

            if not max30102_available:
                local_bpm = -1
                local_spo2 = -1
                print("EDGE | MAX30102: OFFLINE | SVM:{:.2f} Alarm:{}".format(latest_data["svm"], latest_data["alarm_level"]))
            else:
                local_bpm, local_spo2 = max30102.read_heart_spo2()

                if local_spo2 < 70 or local_spo2 > 100:
                    local_spo2 = -1

                if 40 <= local_bpm <= 200:
                    add_ibi_from_bpm(local_bpm)
                else:
                    local_bpm = -1

                assess_biometric_risk(local_spo2, local_bpm, latest_data["svm"], latest_data["rmssd"])

            update_latest_data(bpm=local_bpm, spo2=local_spo2, ibi_count=len(ibi_buffer))

            print(
                "EDGE | HR:{} SpO2:{} AQI:{} PM2.5:{:.1f} ENV:{} SVM:{:.2f} Alarm:{}".format(
                    latest_data["heart_rate"],
                    latest_data["spo2"],
                    latest_data["aqi"],
                    latest_data["pm25_ugm3"],
                    "ON" if latest_data["env_online"] else "OFF",
                    latest_data["svm"],
                    latest_data["alarm_level"],
                )
            )

        if time.ticks_diff(now, last_env_update) >= ENV_READ_INTERVAL_MS:
            last_env_update = now

            if SIMULATE_ENVIRONMENT:
                update_latest_data(
                    aqi=AQI_PLACEHOLDER,
                    pm25=PM25_PLACEHOLDER,
                    pm10=PM10_PLACEHOLDER,
                    env_online=True,
                )

            assess_environmental_risk()

        if time.ticks_diff(now, last_hrv_check) >= HRV_WINDOW_MS:
            last_hrv_check = now
            update_hrv_on_edge(latest_data["heart_rate"], latest_data["spo2"], latest_data["svm"])

        await asyncio.sleep_ms(5)


async def alarm_control_task():
    while True:
        if cancel_button.value() == 0 and alarm_active:
            await asyncio.sleep_ms(50)

            if cancel_button.value() == 0:
                cancel_alarm()

                while cancel_button.value() == 0:
                    await asyncio.sleep_ms(10)

        buzzer.value(1 if alarm_active else 0)
        await asyncio.sleep_ms(50)


async def cloud_publish_task():
    global last_cloud_publish

    while True:
        if connect_wifi():
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
# 9. PROGRAM ENTRY POINT
# ============================================================

async def main():
    global max30102_available, mpu_available

    print("FOURUT - MicroPython Edge Prototype")
    print("Primary deployed firmware: FOURUT_Edge_FreeRTOS.ino")

    buzzer.value(0)

    print("Initializing MAX30102...")
    max30102_available = max30102.begin()

    if max30102_available:
        print("MAX30102 init success")
    else:
        print("MAX30102 init failed - prototype will continue without HR/SpO2")

    print("Initializing MPU6050...")
    mpu_available = mpu.begin()

    if mpu_available:
        print("MPU6050 init success")
    else:
        print("MPU6050 init failed - prototype will use neutral motion values")

    asyncio.create_task(anomaly_detection_task())
    asyncio.create_task(alarm_control_task())
    asyncio.create_task(cloud_publish_task())

    while True:
        await asyncio.sleep(1)


asyncio.run(main())
