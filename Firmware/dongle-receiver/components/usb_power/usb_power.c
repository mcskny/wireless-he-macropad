// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file usb_power.c
 * @brief USB suspend/resume implementasyonu (Bölüm 6.3).
 */

#include "usb_power.h"

#include "esp_log.h"

static const char *TAG = "usb_power";

static bool s_suspended = false;

esp_err_t usb_power_init(void)
{
    s_suspended = false;
    ESP_LOGI(TAG, "USB power manager baslatildi");
    return ESP_OK;
}

void usb_power_on_suspend(void)
{
    if (!s_suspended) {
        s_suspended = true;
        ESP_LOGI(TAG, "USB Suspend: RF dusuk guc, LED kapat (Bölüm 6.3)");
        /* TinyUSB entegrasyonu sonrasi:
         *   - espnow_rx poll süresi artir (düşük guç)
         *   - LED kapat (dongale'da LED yok, stub)
         */
    }
}

void usb_power_on_resume(void)
{
    if (s_suspended) {
        s_suspended = false;
        ESP_LOGI(TAG, "USB Resume: 1000Hz tam guc (Bölüm 6.3)");
        /* TinyUSB entegrasyonu sonrasi:
         *   - espnow_rx normal hiza don
         *   - watchdog sifirla
         */
    }
}

bool usb_power_is_suspended(void)
{
    return s_suspended;
}
