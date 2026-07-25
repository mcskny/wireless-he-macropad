// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file system_state.h
 * @brief Sistem geneli runtime (calisma zamani) durumunu tutan tipler.
 *
 * Bu dosya yalnizca tipleri (enum + struct) tanimlar; global degisken
 * TANIMLANMAZ. State somut olarak main.c icinde olusturulur ve bilesenlere
 * pointer/reference ile gecilir. Bu sayede "God Object" olusmasi engellenir.
 *
 * NVS'den yuklenen kalici degerler (active_power_mode, base_brightness,
 * active_layer) ile runtime-only bayraklar (battery flag'leri, baglanti
 * durumu vb.) ayri alanlarda tutulur.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Aktif guc modu (Bölüm 4).
 *
 * Dijital: analog ve LED MOSFET'leri kapali, light sleep agirlikli, 5B paket.
 * Hibrit : tuş ile uyaninca analog aktif, 15 dk inaktiviteye kadar 23B paket.
 * Agresif: analog her zaman acik, sleep yok, surekli 23B paket (1 kHz).
 */
typedef enum {
    POWER_MODE_DIGITAL = 0,  ///< Bölüm 4.1 — en dusuk guc, light sleep
    POWER_MODE_HYBRID  = 1,  ///< Bölüm 4.2 — varsayilan, 15 dk inaktivite timer
    POWER_MODE_AGGRESSIVE = 2, ///< Bölüm 4.3 — en yuksek guc, sleep yok
    POWER_MODE_COUNT
} power_mode_t;

/**
 * @brief NVS'de kalici olarak saklanan ayarlar (Bölüm 1.1).
 *
 * Bu alanlar ilk acilista NVS'den yuklenir; kullanici etkilesimiyle
 * degistirildiginde tekrar NVS'e yazilir.
 */
typedef struct {
    power_mode_t active_power_mode;  ///< Aktif guc modu (varsayilan: HYBRID)
    uint8_t      base_brightness;    ///< Taban parlaklik %0-100 (varsayilan: 60)
    uint8_t      active_layer;       ///< Aktif katman indeksi (varsayilan: 0)
} persistent_config_t;

/**
 * @brief Pil seviyesi alarm bayraklari (Bölüm 4.4).
 *
 * Her esik icin ayri bayrak, sebekesiz tekrar tetiklenmeyi (flicker) engeller.
 * Sarja takildiginda (Bölüm 5) tum bayraklar sifirlanir.
 */
typedef struct {
    bool flag_50;   ///< Pil <%50 bildirimi verildi mi? (Sari, 6 neopiksel, 3 blink)
    bool flag_25;   ///< Pil <%25 bildirimi verildi mi? (Sari, 3 neopiksel, 3 blink, parlaklik <%30)
    bool flag_15;   ///< Pil <%15 bildirimi verildi mi? (Aktif layer LED, 5 dk'da 5 blink)
    bool flag_5;    ///< Pil <%5 bildirimi verildi mi? (Kirmizi, 12 neopiksel, 3 blink, battery_dead=true)
} battery_flags_t;

/**
 * @brief ESP-NOW baglanti sagligi ve fail-safe durumu (Bölüm 2.2).
 *
 * Tx callback her gonderimden sonra guncellenir. 100 ardisik basarisiz
 * gonderimde sistem arama moduna (1 sn'de 1 paket) gecer.
 */
typedef struct {
    uint32_t consecutive_failed_packets;  ///< Ardisik basarisiz paket sayaci
    bool     is_connected_to_s3;          ///< S3'e bagli mi? (ACK alindi mi?)
    bool     search_mode;                 ///< Fail-safe arama modu aktif mi? (1 Hz)
} link_state_t;

/**
 * @brief Tum sistem geneli runtime durumu.
 *
 * Tek sorumluluk: durum tasiyici. Mantik bu struct icinde YER ALMAZ;
 * mantik ilgili bilesenlerin (power_ctrl, battery, espnow_link, ...) icindedir.
 * Bu struct yalnizca bilesenler arasi ortak durumun tutarli olmasini saglar.
 */
typedef struct {
    persistent_config_t config;          ///< NVS kalici ayarlar
    battery_flags_t     battery_flags;   ///< Pil alarm bayraklari
    link_state_t        link;            ///< ESP-NOW baglanti sagligi

    /* Runtime-only alanlar */
    bool     battery_dead;               ///< Kritik pil kilidi aktif mi? (Bölüm 4.4)
    bool     charging;                   ///< VBUS algilandı mi? (sarj modu)
    bool     charge_complete;            ///< /CHG == HIGH (sarj tamamlandi)
    uint32_t inactivity_timer_ms;        ///< Hibrit mod inaktivite sayaci (ms)
    uint8_t  packet_counter;             ///< 0-255 wrap paket sayaci (5B/23B)
} system_state_t;

#ifdef __cplusplus
}
#endif
