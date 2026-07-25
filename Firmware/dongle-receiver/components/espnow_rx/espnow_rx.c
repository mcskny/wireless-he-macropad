// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file espnow_rx.c
 * @brief ESP-NOW RX implementasyonu: C6 paket alici + MAC/size filter.
 */

#include "espnow_rx.h"

#include <string.h>
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const char *TAG = "espnow_rx";

/** C6 MAC adresi (filter icin). */
static uint8_t s_c6_mac[6] = {0};

/** Paket queue handle. */
static QueueHandle_t s_queue = NULL;

/** Istatistik (atomic yeterli, 32-bit). */
static volatile uint32_t s_packets_received = 0;
static volatile uint32_t s_packets_filtered = 0;

static bool s_initialized = false;

/**
 * @brief ESP-NOW RX callback (wifi task context).
 *
 * MAC filtre + size filtre, geçerli paketi queue'ya koy.
 */
static void espnow_rx_callback(const esp_now_recv_info_t *recv_info,
                               const uint8_t *data, int data_len)
{
    if (recv_info == NULL || data == NULL) {
        return;
    }

    /* 1. MAC filtre: C6'dan mi? */
    if (memcmp(recv_info->src_addr, s_c6_mac, 6) != 0) {
        s_packets_filtered++;
        return;
    }

    /* 2. Size filtre: 5 veya 23 byte mu? */
    if (data_len != 5 && data_len != 23) {
        s_packets_filtered++;
        return;
    }

    /* 3. Queue'ya koy (kopyala) */
    espnow_rx_packet_t pkt;
    pkt.len = (uint8_t)data_len;
    memcpy(pkt.data, data, (size_t)data_len);
    /* Kalan byte'lari sifirla (23 byte sabit boyut icin) */
    if (data_len < ESPNOW_RX_MAX_PACKET_SIZE) {
        memset(pkt.data + data_len, 0, ESPNOW_RX_MAX_PACKET_SIZE - data_len);
    }

    /* Non-blocking send: queue dolu ise eski paketi düşür (overwrite yok) */
    BaseType_t sent = xQueueSend(s_queue, &pkt, 0);
    if (sent == pdTRUE) {
        s_packets_received++;
    } else {
        /* Queue dolu: paket düşürüldü (C6 watchdog tetiklenebilir) */
        s_packets_filtered++;
    }
}

esp_err_t espnow_rx_init(const uint8_t c6_mac[6], uint8_t channel)
{
    if (s_initialized) {
        return ESP_OK;
    }
    if (c6_mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(s_c6_mac, c6_mac, 6);

    /* Queue olustur */
    s_queue = xQueueCreate(ESPNOW_RX_QUEUE_DEPTH, sizeof(espnow_rx_packet_t));
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "Queue olusturulamadi");
        return ESP_ERR_NO_MEM;
    }

    /* Wi-Fi baslat (ESP-NOW icin) */
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event_loop hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));

    /* ESP-NOW baslat + RX callback */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_rx_callback));

    s_initialized = true;
    ESP_LOGI(TAG, "ESP-NOW RX baslatildi: C6=%02X:%02X:%02X:%02X:%02X:%02X ch=%u",
             s_c6_mac[0], s_c6_mac[1], s_c6_mac[2],
             s_c6_mac[3], s_c6_mac[4], s_c6_mac[5], channel);
    return ESP_OK;
}

esp_err_t espnow_rx_get_packet(espnow_rx_packet_t *out, uint32_t timeout_ms)
{
    if (!s_initialized || out == NULL || s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    BaseType_t result = xQueueReceive(s_queue, out, ticks);
    return (result == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

void espnow_rx_get_stats(uint32_t *received, uint32_t *filtered)
{
    if (received != NULL) {
        *received = s_packets_received;
    }
    if (filtered != NULL) {
        *filtered = s_packets_filtered;
    }
}
