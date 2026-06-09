
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include "config.h"
#include "globals.h"
#include "mqtt_client.h"

void connectWiFiNonBlocking() {
    static unsigned long lastWiFiAttempt = 0;

    if (WiFi.status() == WL_CONNECTED) return;

    unsigned long now = millis();

    if (now - lastWiFiAttempt >= WIFI_RECONNECT_INTERVAL_MS) {
        lastWiFiAttempt = now;
        Serial.println("Trying Wi-Fi reconnect...");
        WiFi.disconnect(false);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
}

// AWS IoT TLS needs the ESP32 clock to be valid.
// Without NTP, the board often starts at year 1970 and the certificate is rejected.
bool syncTimeNonBlocking() {
    static bool ntpStarted = false;
    static bool timeSynced = false;
    static unsigned long lastPrint = 0;

    if (timeSynced) return true;

    if (!ntpStarted) {
        ntpStarted = true;
        configTime(0, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");
        Serial.println("Starting NTP time sync...");
    }

    time_t now = time(nullptr);

    // 1700000000 = 2023-11-14. This confirms time is no longer 1970.
    if (now > 1700000000) {
        timeSynced = true;
        Serial.print("NTP time synced. Epoch: ");
        Serial.println((long)now);
        return true;
    }

    if (millis() - lastPrint >= 2000) {
        lastPrint = millis();
        Serial.println("Waiting for NTP time sync before MQTT...");
    }

    return false;
}

void connectMQTTNonBlocking() {
    static unsigned long lastMQTTAttempt = 0;

    if (client.connected()) return;

    // Do not try MQTT until Wi-Fi and NTP time are ready.
    if (WiFi.status() != WL_CONNECTED) return;
    if (!syncTimeNonBlocking()) return;

    unsigned long now = millis();

    if (now - lastMQTTAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
        lastMQTTAttempt = now;

        Serial.println("Trying MQTT reconnect...");
        Serial.print("Endpoint: ");
        Serial.println(AWS_IOT_ENDPOINT);
        Serial.print("Wi-Fi RSSI: ");
        Serial.println(WiFi.RSSI());
        Serial.print("Free heap: ");
        Serial.println(ESP.getFreeHeap());

        if (client.connect(MQTT_CLIENT_ID)) {
            Serial.println("Connected to AWS IoT Core");
        } else {
            Serial.print("MQTT reconnect failed, rc=");
            Serial.println(client.state());

            char errBuf[128];
            espClient.lastError(errBuf, sizeof(errBuf));
            Serial.print("TLS last error: ");
            Serial.println(errBuf);
        }
    }
}

void publishTelemetry(const SensorData& data) {
    StaticJsonDocument<1024> doc;

    doc["worker_id"] = WORKER_ID;
    doc["heart_rate"] = data.bpm;
    doc["spo2"] = data.spo2;
    doc["aqi"] = data.aqi;
    doc["pm25_ugm3"] = data.pm25;
    doc["pm10_ugm3"] = data.pm10;
    doc["env_node_id"] = data.envNodeId;
    doc["env_seq"] = data.envSeq;
    doc["env_online"] = data.envOnline;

    if (data.envLastSeenMs > 0) {
        doc["env_age_ms"] = millis() - data.envLastSeenMs;
    } else {
        doc["env_age_ms"] = -1;
    }

    doc["svm"] = data.svm;
    doc["sdnn"] = data.sdnn;
    doc["rmssd"] = data.rmssd;
    doc["ibi_count"] = data.ibiCount;
    doc["alarm_active"] = data.alarmActive;
    doc["alarm_level"] = data.alarmLevel;
    doc["fall_detected"] = data.fallDetected;

    char payload[1024];
    serializeJson(doc, payload, sizeof(payload));

    bool ok = client.publish(TOPIC_TELEMETRY, payload);

    if (ok) {
        Serial.println("Telemetry published");
    } else {
        Serial.println("Telemetry publish failed");
    }
}

void publishHRVSummary(const SensorData& data) {
    StaticJsonDocument<256> doc;

    doc["worker_id"] = WORKER_ID;
    doc["sdnn"] = data.sdnn;
    doc["rmssd"] = data.rmssd;
    doc["ibi_count"] = data.ibiCount;
    doc["heart_rate"] = data.bpm;
    doc["alarm_level"] = data.alarmLevel;

    char payload[256];
    serializeJson(doc, payload, sizeof(payload));

    bool ok = client.publish(TOPIC_HRV, payload);

    if (ok) {
        Serial.println("HRV summary published");
    } else {
        Serial.println("HRV summary publish failed");
    }
}

void publishAlertEvent(const AlertEvent& event) {
    StaticJsonDocument<512> doc;

    doc["worker_id"] = WORKER_ID;
    doc["category"] = event.category;
    doc["status"] = event.status;
    doc["alert_level"] = event.level;
    doc["alarm_level"] = event.level;
    doc["message"] = event.message;
    doc["heart_rate"] = event.bpm;
    doc["spo2"] = event.spo2;
    doc["aqi"] = event.aqi;
    doc["pm25_ugm3"] = event.pm25;
    doc["pm10_ugm3"] = event.pm10;
    doc["svm"] = event.svm;
    doc["rmssd"] = event.rmssd;

    char payload[512];
    serializeJson(doc, payload, sizeof(payload));

    bool ok = client.publish(TOPIC_ALERT, payload);

    if (ok) {
        Serial.println("Alert event published");
    } else {
        Serial.println("Alert event publish failed");
    }
}

void printGatewayWiFiInfoOnce() {
    static bool printed = false;

    if (printed) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        printed = true;

        uint8_t mac[6];
        WiFi.macAddress(mac);

        Serial.println("\n========== GATEWAY INFO ==========");
        Serial.printf(
            "Gateway MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
        );
        Serial.printf("Wi-Fi channel: %d\n", WiFi.channel());
        Serial.println("Use this MAC and channel in APM10 ESP-NOW sender.");
        Serial.println("==================================\n");
    }
}
