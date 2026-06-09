/**
 * ============================================================
 * FOURUT - APM10 PM2.5 ESP-NOW Environment Node
 * Board  : ESP32S NodeMCU / ESP32 DevKit
 * Sensor : APM10 UART PM sensor
 * Role   : Read PM1.0, PM2.5, PM10, calculate demo AQI,
 *          send environment packet to XIAO ESP32-C3 gateway by ESP-NOW.
 * Cloud  : This node does NOT connect to AWS. The XIAO gateway receives
 *          this packet and publishes it to AWS IoT together with health data.
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>

#if __has_include("esp_arduino_version.h")
#include "esp_arduino_version.h"
#endif

#if __has_include("esp_idf_version.h")
#include "esp_idf_version.h"
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#ifndef ESP_IDF_VERSION_MAJOR
#define ESP_IDF_VERSION_MAJOR 4
#endif

// ============================================================
// USER CONFIGURATION
// ============================================================

// Must match the Wi-Fi channel used by the XIAO gateway.
// Check the XIAO Serial Monitor: "Wi-Fi channel: X"
#define ESPNOW_WIFI_CHANNEL 13

// For quick testing, keep broadcast = true.
// For final demo, set false and fill GATEWAY_MAC with the XIAO MAC.
#define USE_BROADCAST false

uint8_t GATEWAY_MAC[6]   = {0x64, 0xE8, 0x33, 0x84, 0x12, 0x30};
uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ESP32S NodeMCU UART2 pins.
// Wiring:
// APM10 TX  -> ESP32 GPIO16  (PM_RX_PIN)
// APM10 RX  -> ESP32 GPIO17  (PM_TX_PIN)
// APM10 GND -> ESP32 GND
// APM10 VCC -> correct sensor supply voltage
#define PM_RX_PIN 16
#define PM_TX_PIN 17

// You confirmed the sensor works at 115200 in your test.
#define PM_BAUD 1200

const unsigned long SEND_INTERVAL_MS = 2000;
const unsigned long DEBUG_NO_FRAME_INTERVAL_MS = 5000;
const unsigned long APM10_RESPONSE_TIMEOUT_MS = 700;

// Set true only if the checksum formula matches your exact APM10 response.
// Keep false for demo stability because some APM10 modules/firmware variants differ.
#define STRICT_APM10_CHECKSUM false
#define PRINT_RAW_APM10_RESPONSE false

// ============================================================
// ESP-NOW packet - MUST match the receiver/gateway code
// ============================================================
#define ENV_PACKET_MAGIC   0x4655
#define ENV_PACKET_VERSION 1
#define ENV_NODE_ID        1

struct __attribute__((packed)) EnvPacket {
    uint16_t magic;
    uint8_t version;
    uint8_t node_id;
    uint32_t seq;
    float pm1_0;
    float pm2_5;
    float pm10;
    int16_t aqi;
    uint16_t battery_mv;
    uint32_t uptime_ms;
};

struct PMReading {
    float pm1_0;
    float pm2_5;
    float pm10;
};

HardwareSerial PMSerial(2);

uint32_t packetSeq = 0;
unsigned long lastSendMs = 0;
unsigned long lastNoFrameDebugMs = 0;
volatile bool lastSendOk = false;

// APM10 command that worked in the UART test.
// Request PM1.0, PM2.5, and PM10 data.
const uint8_t APM10_CMD_READ_ALL[] = {0xFE, 0xA5, 0x00, 0x01, 0xA6};

// ============================================================
// Helper functions
// ============================================================
void printMacAddress(const uint8_t* mac) {
    Serial.printf(
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );
}

void printHexByte(uint8_t b) {
    if (b < 0x10) Serial.print("0");
    Serial.print(b, HEX);
    Serial.print(" ");
}

uint16_t readU16BE(const uint8_t* frame, int index) {
    return ((uint16_t)frame[index] << 8) | frame[index + 1];
}

uint8_t checksumLowByte(const uint8_t* buf, int start, int endExclusive) {
    uint16_t sum = 0;
    for (int i = start; i < endExclusive; i++) {
        sum += buf[i];
    }
    return sum & 0xFF;
}

int calculateAqiFromPM25(float pm25) {
    if (pm25 < 0) return -1;

    struct Breakpoint {
        float cLow;
        float cHigh;
        int iLow;
        int iHigh;
    };

    const Breakpoint table[] = {
        {0.0,   12.0,   0,   50},
        {12.1,  35.4,   51,  100},
        {35.5,  55.4,   101, 150},
        {55.5,  150.4,  151, 200},
        {150.5, 250.4,  201, 300},
        {250.5, 350.4,  301, 400},
        {350.5, 500.4,  401, 500}
    };

    float c = floor(pm25 * 10.0f) / 10.0f;

    for (const auto& bp : table) {
        if (c >= bp.cLow && c <= bp.cHigh) {
            float aqi = ((float)(bp.iHigh - bp.iLow) / (bp.cHigh - bp.cLow)) *
                        (c - bp.cLow) + bp.iLow;
            return (int)round(aqi);
        }
    }

    return 500;
}

bool readAPM10Frame(PMReading& reading) {
    while (PMSerial.available()) {
        PMSerial.read();
    }

    PMSerial.write(APM10_CMD_READ_ALL, sizeof(APM10_CMD_READ_ALL));
    PMSerial.flush();

    uint8_t buf[40];
    int n = 0;
    unsigned long startMs = millis();

    while (millis() - startMs < APM10_RESPONSE_TIMEOUT_MS) {
        while (PMSerial.available() && n < (int)sizeof(buf)) {
            buf[n++] = PMSerial.read();
        }

        if (n >= 11) {
            break;
        }
        delay(2);
    }

    if (n <= 0) {
        return false;
    }

#if PRINT_RAW_APM10_RESPONSE
    Serial.print("Raw APM10 response: ");
    for (int i = 0; i < n; i++) printHexByte(buf[i]);
    Serial.println();
#endif

    int frameStart = -1;
    for (int i = 0; i <= n - 2; i++) {
        if (buf[i] == 0xFE && buf[i + 1] == 0xA5) {
            frameStart = i;
            break;
        }
    }

    if (frameStart < 0 || n - frameStart < 11) {
        Serial.print("APM10 response exists, but no complete FE A5 frame. Raw: ");
        for (int i = 0; i < n; i++) printHexByte(buf[i]);
        Serial.println();
        return false;
    }

    const uint8_t* frame = &buf[frameStart];

    uint8_t checksumCalc = checksumLowByte(frame, 1, 10);
    uint8_t checksumRecv = frame[10];

    if (checksumCalc != checksumRecv) {
        Serial.printf(
            "APM10 checksum warning. Calc=0x%02X Received=0x%02X\n",
            checksumCalc,
            checksumRecv
        );

#if STRICT_APM10_CHECKSUM
        return false;
#endif
    }

    // Response layout used by the test code:
    // FE A5 02 00 DF11 DF12 DF21 DF22 DF31 DF32 CS
    reading.pm1_0 = (float)readU16BE(frame, 4);
    reading.pm2_5 = (float)readU16BE(frame, 6);
    reading.pm10  = (float)readU16BE(frame, 8);

    return true;
}

void buildEnvPacket(const PMReading& reading, EnvPacket& packet) {
    packet.magic = ENV_PACKET_MAGIC;
    packet.version = ENV_PACKET_VERSION;
    packet.node_id = ENV_NODE_ID;
    packet.seq = ++packetSeq;
    packet.pm1_0 = reading.pm1_0;
    packet.pm2_5 = reading.pm2_5;
    packet.pm10 = reading.pm10;
    packet.aqi = calculateAqiFromPM25(reading.pm2_5);
    packet.battery_mv = 0;
    packet.uptime_ms = millis();
}

void sendEnvPacket(const EnvPacket& packet) {
    uint8_t* targetMac = USE_BROADCAST ? BROADCAST_MAC : GATEWAY_MAC;

    esp_err_t result = esp_now_send(targetMac, (const uint8_t*)&packet, sizeof(packet));

    Serial.printf(
        "SEND | seq:%lu PM1.0:%.1f PM2.5:%.1f PM10:%.1f AQI:%d -> ",
        (unsigned long)packet.seq,
        packet.pm1_0,
        packet.pm2_5,
        packet.pm10,
        packet.aqi
    );

    if (result == ESP_OK) {
        Serial.println("queued");
    } else {
        Serial.printf("failed, esp_err=%d\n", result);
    }
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataSent(const wifi_tx_info_t* txInfo, esp_now_send_status_t status)
#else
void onDataSent(const uint8_t* macAddr, esp_now_send_status_t status)
#endif
{
    lastSendOk = (status == ESP_NOW_SEND_SUCCESS);
    Serial.print("ESP-NOW callback: ");
    Serial.println(lastSendOk ? "delivered" : "not delivered");
}

bool initEspNowSender() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.disconnect(true, true);
    delay(100);

    esp_err_t channelResult = esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (channelResult != ESP_OK) {
        Serial.printf("Failed to set ESP-NOW channel, err=%d\n", channelResult);
        return false;
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return false;
    }

    if (esp_now_register_send_cb(onDataSent) != ESP_OK) {
        Serial.println("ESP-NOW send callback registration failed");
        return false;
    }

    esp_now_peer_info_t peerInfo = {};
    uint8_t* peerMac = USE_BROADCAST ? BROADCAST_MAC : GATEWAY_MAC;
    memcpy(peerInfo.peer_addr, peerMac, 6);
    peerInfo.channel = ESPNOW_WIFI_CHANNEL;
    peerInfo.encrypt = false;
#if ESP_IDF_VERSION_MAJOR >= 4
    peerInfo.ifidx = WIFI_IF_STA;
#endif

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add ESP-NOW peer");
        return false;
    }

    uint8_t nodeMac[6];
    WiFi.macAddress(nodeMac);

    Serial.println("ESP-NOW PM node ready");
    Serial.print("Node MAC: ");
    printMacAddress(nodeMac);
    Serial.println();

    Serial.print("Target MAC: ");
    printMacAddress(peerMac);
    Serial.println(USE_BROADCAST ? "  [broadcast]" : "  [direct]");

    Serial.printf("ESP-NOW channel: %d\n", ESPNOW_WIFI_CHANNEL);
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("========================================");
    Serial.println("FOURUT APM10 ESP-NOW PM NODE");
    Serial.println("========================================");

    PMSerial.begin(PM_BAUD, SERIAL_8N1, PM_RX_PIN, PM_TX_PIN);
    PMSerial.setTimeout(300);

    Serial.printf("PM UART started at %d baud\n", PM_BAUD);
    Serial.printf("PM_RX_PIN=%d, PM_TX_PIN=%d\n", PM_RX_PIN, PM_TX_PIN);
    Serial.println("Wiring reminder:");
    Serial.println("  APM10 TX  -> ESP32 GPIO16");
    Serial.println("  APM10 RX  -> ESP32 GPIO17");
    Serial.println("  APM10 GND -> ESP32 GND");
    Serial.println("  APM10 VCC -> correct supply voltage");

    if (!initEspNowSender()) {
        Serial.println("ESP-NOW setup failed. Restarting in 5 seconds...");
        delay(5000);
        ESP.restart();
    }
}

void loop() {
    PMReading reading;
    unsigned long now = millis();

    if (now - lastSendMs >= SEND_INTERVAL_MS) {
        lastSendMs = now;

        if (readAPM10Frame(reading)) {
            EnvPacket packet;
            buildEnvPacket(reading, packet);
            sendEnvPacket(packet);
        } else {
            if (now - lastNoFrameDebugMs >= DEBUG_NO_FRAME_INTERVAL_MS) {
                lastNoFrameDebugMs = now;
                Serial.println("No valid APM10 frame yet. Check PM_BAUD, TX/RX, power, GND, and SET/UART mode.");
            }
        }
    }

    delay(10);
}
