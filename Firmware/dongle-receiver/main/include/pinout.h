// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file pinout.h
 * @brief Xiao ESP32-S3 pin atamalari (minimal - dongle'da harici pin YOK).
 *
 * S3 dongle donanimi: Xiao ESP32-S3 karti, sadece USB-C connector.
 * Harici GPIO baglantisi YOK (kullanici onayli). Tum haberlesme USB uzerinden.
 *
 * Xiao ESP32-S3 dahili:
 *   - USB-C connector (USB-OTG, IO19/IO20 D+/D-) - donanim, GPIO config YOK
 *   - BOOT button (GPIO0, strapping) - firmware'de kullanilmaz
 *   - Dahili LED (GPIO21 bazi versiyonlarda) - kullanilmiyor (stub)
 *
 * ESP-NOW Wi-Fi: dahili anten, GPIO config YOK.
 *
 * @note Bu dosya C6'daki pinout.h'dan FARKLI: C6'da cok sayida GPIO vardi
 *       (SPI, mux, SR, encoder, neopixel, MOSFET, vb.). S3'te HICBIRI yok;
 *       dongle sadece USB + RF.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * USB (donanimsal, GPIO yapilandirmasi YOK)
 * ========================================================================= */
/* Xiao ESP32-S3 USB-C connector:
 *   - IO19 = USB D- (donanim tarafindan yonetilir)
 *   - IO20 = USB D+ (donanim tarafindan yonetilir)
 *   - USB-OTG modu (TinyUSB NKRO HID icin)
 * Firmware'de manuel pin ayari YAPILMAZ. */

/* =========================================================================
 * Status LED (stub - dongle'da LED yok)
 * ========================================================================= */
/* Bölüm 6.3 USB suspend'de LED kapatma gerekiyor ama dongle'da harici LED yok.
   Xiao'nun dahili LED'i var ama kullanilmiyor. Pin -1 = stub. */
#define PIN_STATUS_LED    (-1)   /* STUB: dongle'da LED yok */

/* =========================================================================
 * BOOT button (GPIO0) - CONFIG_AP_MODE gecisi icin
 * ========================================================================= */
/* Xiao ESP32-S3 BOOT button: GPIO0, pull-up, active low (basili = LOW).
   Kısa basış (<1sn) → CONFIG_AP_MODE toggle (web arayüzü). */
#define PIN_BOOT_BUTTON    0

/* =========================================================================
 * UART0 (log, GPIO43/44) - sdkconfig.defaults'ta tanimli
 * ========================================================================= */
/* CONFIG_ESP_CONSOLE_UART_TX_GPIO=43, RX_GPIO=44.
   Harici USB-TTL adapter ile log alinir. Burada sabit tanim yok. */

/* =========================================================================
 * Wi-Fi / ESP-NOW (dahili anten, GPIO config YOK)
 * ========================================================================= */
/* Xiao ESP32-S3 entegre anten. ESP-NOW RX + Wi-Fi AP ayni radyo. */

#ifdef __cplusplus
}
#endif
