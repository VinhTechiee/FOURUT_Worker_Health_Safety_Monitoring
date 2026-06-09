
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

#include "config.h"
#include "globals.h"
#include "espnow_env.h"

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len)
#else
void onEspNowRecv(const uint8_t* mac, const uint8_t* data, int len)
#endif
{
    if (len != sizeof(EnvPacket)) {
        return;
    }

    EnvPacket packet;
    memcpy(&packet, data, sizeof(packet));

    if (packet.magic != ENV_PACKET_MAGIC) {
        return;
    }

    if (packet.version != ENV_PACKET_VERSION) {
        return;
    }

    if (envQueue != NULL) {
        if (xQueueSend(envQueue, &packet, 0) != pdTRUE) {
            // Queue full, packet dropped
        }
    }
}

void initEspNowGateway() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return;
    }

    esp_now_register_recv_cb(onEspNowRecv);

    uint8_t mac[6];
    WiFi.macAddress(mac);

    Serial.printf(
        "ESP-NOW gateway ready. XIAO MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );
}
