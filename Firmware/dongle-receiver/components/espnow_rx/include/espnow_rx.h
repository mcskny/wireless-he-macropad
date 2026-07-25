// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file espnow_rx.h
 * @brief ESP-NOW alici - C6'dan gelen paketleri yakalar (Bölüm 3).
 *
 * RX callback'de:
 *   1. MAC filtre: C6 PEER MAC'den mi?
 *   2. Size filtre: 5 veya 23 byte mu?
 *   3. Geçerli paketi FreeRTOS queue'ya koy.
 *
 * Main döngü queue'dan paketi alir, packet_decoder ile cozer, watchdog sifirlar.
 *
 * Thread-safety: FreeRTOS queue kendi kilidine sahip. Callback queue'ya send,
 * main queue'dan receive. Critical section gerekmez (AI_GUIDELINES.md kural 17).
 *
 * @note ESP-IDF v6: RX callback esp_now_recv_info_t kullanir (eski uint8_t
 *       *mac_addr degil). src_addr ile MAC filtre yapilir.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maksimum paket boyutu (23 byte tam paket). */
#define ESPNOW_RX_MAX_PACKET_SIZE 23

/** Queue derinligi (1 kHz'de ~8 paket buffer). */
#define ESPNOW_RX_QUEUE_DEPTH 8

/** Gelen paket (queue elemani). */
typedef struct {
    uint8_t data[ESPNOW_RX_MAX_PACKET_SIZE];  /**< Paket verisi */
    uint8_t len;                               /**< Paket boyutu (5 veya 23) */
} espnow_rx_packet_t;

/**
 * @brief Wi-Fi + ESP-NOW baslatir, RX callback + C6 MAC filter kaydeder.
 * @param c6_mac  C6 gonderici MAC adresi (6 byte, filtre icin).
 * @param channel Wi-Fi kanali (C6 ile ayni).
 * @return ESP_OK veya hata.
 */
esp_err_t espnow_rx_init(const uint8_t c6_mac[6], uint8_t channel);

/**
 * @brief Queue'dan paket alir (blocking veya timeout).
 * @param out        Cikis paket bufferi.
 * @param timeout_ms Bekeleme suresi (0 = non-blocking, portMAX_DELAY = sonsuz).
 * @return ESP_OK (paket alindi) veya ESP_ERR_TIMEOUT.
 */
esp_err_t espnow_rx_get_packet(espnow_rx_packet_t *out, uint32_t timeout_ms);

/**
 * @brief Istatistik: toplam alinan + filtrelenen paket sayisi.
 */
void espnow_rx_get_stats(uint32_t *received, uint32_t *filtered);

#ifdef __cplusplus
}
#endif
