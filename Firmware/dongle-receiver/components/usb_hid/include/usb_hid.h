// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file usb_hid.h
 * @brief TinyUSB composite USB HID: 6KRO klavye + Consumer + Gamepad (Bölüm 6).
 *
 * 3 report ID tek HID interface:
 *   1. 6KRO Klavye (bInterval=1, 1000Hz, modifier + 6 keycode)
 *   2. Consumer Control (medya tuşlari: vol+/-, play, vb.)
 *   3. Gamepad (X/Y analog eksenler)
 *
 * Rapor sadece durum degisince gonderilir (Bölüm 6.1, gereksiz bandwidth yok).
 * USB suspend/resume callback'leri usb_power ile entegre (Bölüm 6.3).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** NKRO maksimum eszamanli tuş (Kconfig). */
#define USB_HID_NKRO_MAX_KEYS  32

/**
 * @brief TinyUSB composite cihaz baslatir (6KRO + Consumer + Gamepad).
 *
 * tinyusb_driver_install + descriptor + callback kaydi.
 * @return ESP_OK veya TinyUSB hatasi.
 */
esp_err_t usb_hid_init(void);

/**
 * @brief NKRO klavye raporu gonder (Bölüm 6).
 *
 * Sadece durum degisirse gonderir (rapor karsilastirma, gereksiz bandwidth yok).
 *
 * @param active_keys  12 tuş basili durumu (Hall motoru ciktisi).
 * @param key_mappings 4 katman × 12 tuş keycode tablosu (NVS'den).
 * @param layer         Aktif katman indeksi (0-3).
 * @return ESP_OK (stub) veya TinyUSB hatasi.
 */
esp_err_t usb_hid_send_keyboard(const bool active_keys[12],
                                const uint16_t key_mappings[4][12],
                                uint8_t layer);

/**
 * @brief Gamepad analog eksen raporu gonder (Bölüm 4.4 + 6).
 * @param joy_x X ekseni (-32768..32767).
 * @param joy_y Y ekseni (-32768..32767).
 * @return ESP_OK (stub).
 */
esp_err_t usb_hid_send_gamepad(int16_t joy_x, int16_t joy_y);

/**
 * @brief Consumer Control raporu gonder (medya tuşlari).
 * @param usage HID Consumer Control usage code (0 = release).
 * @return ESP_OK (stub).
 */
esp_err_t usb_hid_send_consumer(uint16_t usage);

/**
 * @brief Tum tuşlari birak (watchdog timeout icin, Bölüm 3).
 * @return ESP_OK (stub).
 */
esp_err_t usb_hid_release_all(void);

/**
 * @brief USB suspend durumu (Bölüm 6.3).
 * @return true = USB suspend (bilgisayar uyudu), false = aktif.
 */
bool usb_hid_is_suspended(void);

#ifdef __cplusplus
}
#endif
