// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file usb_power.h
 * @brief USB suspend/resume yonetimi (Bölüm 6.3).
 *
 * Bilgisayar uykuya geçince (USB Suspend):
 *   - RF (ESP-NOW) dinleme sıklığı düşür
 *   - LED'ler kapat (dongle'da LED yok ama stub)
 * Resume:
 *   - 1000Hz tam güç moduna dön
 *
 * @note STUB: TinyUSB suspend/resume callback'leri F3 (TinyUSB entegrasyonu)
 *       sonrasi baglanacak. Su an sadece flag + log.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB power yoneticisini baslatir.
 */
esp_err_t usb_power_init(void);

/**
 * @brief USB suspend algilandi (Bölüm 6.3).
 * TinyUSB tud_suspend_cb cagiracak (F3 sonrasi).
 */
void usb_power_on_suspend(void);

/**
 * @brief USB resume algilandi (Bölüm 6.3).
 * TinyUSB tud_resume_cb cagiracak (F3 sonrasi).
 */
void usb_power_on_resume(void);

/**
 * @brief USB suspend durumunda mi?
 */
bool usb_power_is_suspended(void);

#ifdef __cplusplus
}
#endif
