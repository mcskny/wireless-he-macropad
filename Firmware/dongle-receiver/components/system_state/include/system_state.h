// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file system_state.h
 * @brief S3 dongle sistem geneli tipler: state, kalibrasyon, tuş durumu.
 *
 * Bu dosya yalnizca tipleri tanimlar; global degisken TANIMLANMAZ.
 * State somut olarak main.c icinde olusturulur, bilesenlere pointer ile gecilir.
 *
 * S3 dongle iki modda calisir: NORMAL_MODE (ESP-NOW RX + USB HID) ve
 * CONFIG_AP_MODE (Wi-Fi AP + WebSocket kalibrasyon).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Toplam tuş sayisi (C6'dan gelen 12 analog/dijital tuş). */
#define S3_KEY_COUNT 12

/** Katman sayisi (C6'da 4 layer LED'i var). */
#define S3_LAYER_COUNT 4

/** Snap Tap (SOCD) maksimum çift sayisi. */
#define S3_SNAP_TAP_PAIR_MAX 6

/** Joystick eksen sayisi (X+, X-, Y+, Y-). */
#define S3_JOYSTICK_AXIS_COUNT 4

/**
 * @brief S3 ana calisma modu (Bölüm 2 state machine).
 */
typedef enum {
    S3_MODE_NORMAL    = 0,  /**< ESP-NOW RX + Hall motoru + USB HID 1000Hz */
    S3_MODE_CONFIG_AP = 1,  /**< Wi-Fi AP + WebSocket kalibrasyon, ESP-NOW pasif */
} s3_mode_t;

/**
 * @brief C6 guc modu (C6 paket status byte'indan okunur, Bölüm 3 decode).
 *
 * S3 bunu C6'dan alir, kendi guc yonetimi yok (USB powered). Sadece bilgi
 * olarak kullanilir (USB HID raporuna eklenebilir).
 */
typedef enum {
    POWER_MODE_DIGITAL    = 0,  /**< C6 Dijital mod */
    POWER_MODE_HYBRID     = 1,  /**< C6 Hibrit mod */
    POWER_MODE_AGGRESSIVE = 2,  /**< C6 Agresif mod */
} power_mode_t;

/**
 * @brief Bir tuşun tetik modu (Bölüm 4.2).
 */
typedef enum {
    TRIGGER_MODE_TRADITIONAL = 0,  /**< Geleneksel: actuation point + histerezis */
    TRIGGER_MODE_RAPID       = 1,  /**< Rapid Trigger: dinamik üst/dip takip */
} trigger_mode_t;

/**
 * @brief NVS'de saklanan kalibrasyon yapilandirmasi (Bölüm 1.1).
 *
 * Bu struct NVS'ye blob olarak yazilir/okunur. Default degerler
 * calibration_defaults.h'de tanimli; web kalibrasyon NVS'e yazar.
 */
typedef struct {
    uint16_t min_adc[S3_KEY_COUNT];               /**< Kalibrasyon alt sinir (ham ADC) */
    uint16_t max_adc[S3_KEY_COUNT];               /**< Kalibrasyon üst sinir (ham ADC) */
    float    actuation_point[S3_KEY_COUNT];       /**< Geleneksel tetik esiği [0.0-1.0] */
    float    rt_press_sensitivity[S3_KEY_COUNT];  /**< RT bas hassasiyeti (Sp) */
    float    rt_release_sensitivity[S3_KEY_COUNT];/**< RT bırakma hassasiyeti (Sr) */
    trigger_mode_t trigger_mode[S3_KEY_COUNT];    /**< Her tuşun tetik modu */
    uint16_t key_mappings[S3_LAYER_COUNT][S3_KEY_COUNT]; /**< 4 katman × 12 tuş keycode */
    uint8_t  snap_tap_pairs[S3_SNAP_TAP_PAIR_MAX]; /**< Snap Tap çiftleri (tuş indeksleri) */
    uint8_t  joystick_axes[S3_JOYSTICK_AXIS_COUNT]; /**< Joystick eksen eşleme (X+,X-,Y+,Y-) */
} calibration_config_t;

/**
 * @brief Bir tuşun runtime durumu (Hall Effect motoru için, Bölüm 4).
 */
typedef struct {
    float position;       /**< Normalized konum [0.0, 1.0] */
    bool  pressed;        /**< Tuş basili mi (motor ciktisi) */
    float highest_pos;    /**< RT: en derin basili nokta */
    float lowest_pos;     /**< RT: en yuksek birakma nokta */
} key_state_t;

/**
 * @brief S3 dongle global runtime durumu.
 *
 * Tek sorumluluk: durum tasiyici. Mantik bilesenlerde.
 */
typedef struct {
    s3_mode_t mode;                          /**< NORMAL veya CONFIG_AP */
    calibration_config_t calibration;       /**< NVS'den yuklenen kalibrasyon */
    key_state_t keys[S3_KEY_COUNT];         /**< 12 tuş runtime durumu */

    uint8_t  last_sequence_number;          /**< C6 paket seq (kayip takibi) */
    uint32_t c6_connection_watchdog;        /**< C6 baglanti watchdog (ms) */
    uint8_t  current_layer_index;           /**< Aktif katman (0-3, SW2 ile) */
    bool     usb_suspended;                 /**< USB suspend (Bölüm 6.3) */

    /* Hall motoru ciktilari (USB HID raporlama icin) */
    bool     active_keys[S3_KEY_COUNT];     /**< Motor sonucu basili tuşlar */
    int16_t  joystick_x;                    /**< Gamepad X ekseni (-32768..32767) */
    int16_t  joystick_y;                    /**< Gamepad Y ekseni (-32768..32767) */

    /* Paket istatistik */
    uint32_t packets_received;              /**< Toplam basarili paket */
    uint32_t packets_lost;                  /**< Toplam kayip paket */
} s3_system_state_t;

#ifdef __cplusplus
}
#endif
