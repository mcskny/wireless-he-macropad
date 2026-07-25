// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file espnow_link.c
 * @brief ESP-NOW S3 haberlesme + fail-safe implementasyonu.
 */

#include "espnow_link.h"

#include <string.h>
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "espnow";

/** S3 peer MAC adresi. */
static uint8_t s_peer_mac[6] = {0};

/** Fail-safe esigi. */
static uint32_t s_failsafe_threshold = 100;

/* --- Shared state (Tx callback <-> main task) --- */
/** Ardisik basarisiz paket sayaci. */
static uint32_t s_consecutive_failed = 0;
/** S3'e bagli mi? */
static bool s_is_connected = true;
/** Arama modu (fail-safe) aktif mi? */
static bool s_search_mode = false;

/** Critical section guard (shared state korumasi). */
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static bool s_initialized = false;

/**
 * @brief ESP-NOW Tx callback (wifi task context'inde calisir).
 *
 * Basarili: consecutive_failed=0, is_connected=true.
 * Basarisiz: consecutive_failed++, threshold'u gecerse search_mode=true.
 *
 * @note ESP-IDF v6: callback imzasi esp_now_send_info_t kullanir (eski uint8_t
 *       *mac_addr degil). tx_info su an kullanilmiyor (peer zaten kayitli).
 */
static void espnow_tx_callback(const esp_now_send_info_t *tx_info,
                               esp_now_send_status_t status)
{
    (void)tx_info;  /* S3 MAC'i zaten kayitli, tx_info'ya ihtiyac yok */

    portENTER_CRITICAL(&s_lock);
    if (status == ESP_NOW_SEND_SUCCESS) {
        s_consecutive_failed = 0;
        s_is_connected = true;
        s_search_mode = false;
    } else {
        s_consecutive_failed++;
        if (s_consecutive_failed >= s_failsafe_threshold) {
            s_is_connected = false;
            s_search_mode = true;
        }
    }
    portEXIT_CRITICAL(&s_lock);
}

esp_err_t espnow_link_init(const uint8_t peer_mac[6],
                           uint8_t channel,
                           uint32_t failsafe_threshold)
{
    if (s_initialized) {
        return ESP_OK;
    }
    if (peer_mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(s_peer_mac, peer_mac, 6);
    s_failsafe_threshold = (failsafe_threshold > 0) ? failsafe_threshold : 100;

    /* 1. Wi-Fi baslat (ESP-NOW icin gerekli) */
    ESP_ERROR_CHECK(esp_netif_init());

    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event_loop_create hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Wi-Fi kanali ayarla (S3 ile ayni olmali) */
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));

    /* 2. ESP-NOW baslat */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_tx_callback));

    /* 3. S3 peer ekle */
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_peer_mac, 6);
    peer.channel = channel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;  /* sifreleme yok (Bölüm 2.2'de belirtilmemis) */
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    /* State sifirla */
    portENTER_CRITICAL(&s_lock);
    s_consecutive_failed = 0;
    s_is_connected = true;
    s_search_mode = false;
    portEXIT_CRITICAL(&s_lock);

    s_initialized = true;
    ESP_LOGI(TAG, "ESP-NOW baslatildi: peer=%02X:%02X:%02X:%02X:%02X:%02X ch=%u threshold=%lu",
             s_peer_mac[0], s_peer_mac[1], s_peer_mac[2],
             s_peer_mac[3], s_peer_mac[4], s_peer_mac[5],
             channel, (unsigned long)s_failsafe_threshold);
    return ESP_OK;
}

esp_err_t espnow_link_send(const uint8_t *data, size_t len)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_now_send(s_peer_mac, data, len);
}

bool espnow_link_is_connected(void)
{
    bool connected;
    portENTER_CRITICAL(&s_lock);
    connected = s_is_connected;
    portEXIT_CRITICAL(&s_lock);
    return connected;
}

bool espnow_link_is_search_mode(void)
{
    bool mode;
    portENTER_CRITICAL(&s_lock);
    mode = s_search_mode;
    portEXIT_CRITICAL(&s_lock);
    return mode;
}

uint32_t espnow_link_get_failed_count(void)
{
    uint32_t count;
    portENTER_CRITICAL(&s_lock);
    count = s_consecutive_failed;
    portEXIT_CRITICAL(&s_lock);
    return count;
}

void espnow_link_reset(void)
{
    portENTER_CRITICAL(&s_lock);
    s_consecutive_failed = 0;
    s_is_connected = true;
    s_search_mode = false;
    portEXIT_CRITICAL(&s_lock);
    ESP_LOGI(TAG, "Baglanti durumu sifirlandi");
}
