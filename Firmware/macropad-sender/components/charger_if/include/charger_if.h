// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file charger_if.h
 * @brief Sarj sensör soyutlama (VBUS//CHG) + sarj animasyonlari (Bölüm 5).
 *
 * VBUS ve /CHG pin routing'i henuz netlesmedigi icin STUB modunda calisir.
 * pin_num = -1 gecilirse sabit false (sarj yok) doner. Donanim karari netlesince
 * pinout.h'den gercek pin numaralari verilir, kod degismez.
 *
 * /CHG (bq253000rter STAT): active low. LOW = sarj oluyor, HIGH = sarj bitti.
 * VBUS: active high. HIGH = USB takili.
 *
 * Bölüm 5.1 Sarj Baslangic Animasyonu: mor basamak (NEO1-3 → 1-6 → 1-9 → 1-12).
 * Bölüm 5.2 Sarj Tamamlandi: 12 neopiksel dusuk guc yesil sabit.
 *
 * @note Sarja takilinca tum pil flag'leri sifirlanir (Bölüm 5).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "system_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sarj sensör pinlerini yapilandirir.
 *
 * @param vbus_gpio VBUS (USB takili) pin. -1 = stub (sabit false).
 * @param chg_gpio  /CHG (bq253000 STAT) pin. -1 = stub (sabit false).
 * @return ESP_OK veya GPIO hatasi (stub modunda her zaman ESP_OK).
 */
esp_err_t charger_if_init(int vbus_gpio, int chg_gpio);

/**
 * @brief USB takili mi? (VBUS == HIGH).
 * @return Stub modunda false. Gerçek pin ile VBUS okunur.
 */
bool charger_if_is_charging(void);

/**
 * @brief Sarj tamamlandi mi? (/CHG == HIGH, yani şarj bitti).
 * @return Stub modunda false. Gerçek pin ile /CHG okunur.
 */
bool charger_if_is_charge_complete(void);

/**
 * @brief Bölüm 5: Sarja takilinca tum pil flag'lerini sifirla.
 * @param flags Pil flag'leri (state->battery_flags).
 */
void charger_if_clear_battery_flags(battery_flags_t *flags);

/**
 * @brief Bölüm 5.1: Sarj baslangic animasyonu (mor basamak).
 *
 * NEO1-3 → NEO1-6 → NEO1-9 → NEO1-12, her adim 500ms mor.
 * Sonunda söndür + LED MOSFET kapat. ~2 saniye bloklar.
 */
void charger_if_play_charge_start_animation(void);

/**
 * @brief Bölüm 5.2: Sarj tamamlandi animasyonu (dusuk guc yesil sabit).
 *
 * 12 neopikseli dusuk parlaklikta sabit yesil yakar. USB cekilene kadar
 * boyle kalir; main.c USB cekilme event'inde neopixel_clear + MOSFET kapatir.
 */
void charger_if_play_charge_complete_animation(void);

#ifdef __cplusplus
}
#endif
