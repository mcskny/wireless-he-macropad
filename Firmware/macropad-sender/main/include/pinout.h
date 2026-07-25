// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file pinout.h
 * @brief ESP32-C6-MINI-1 donanım pin atamalarının tek kaynağı (single source of truth).
 *
 * Bu dosya projedeki TUM GPIO atamalarini icerir. Baska bir .c/.h dosyasinda
 * somut pin numarasi KULLANILMAMALIDIR; tum bilesenler pin numaralarini
 * init fonksiyonlari uzerinden main.c tarafindan alir.
 *
 * Kaynak: pcb_pinler.md (Hall Effect Macropad pin haritasi).
 *
 * Onemli notlar:
 *  - RXD0 (GPIO30) ve TXD0 (GPIO31) UART0 pinleri LED shift register'a
 *    ayrilmistir. Bu nedenle log cikisi USB-CDC uzerinden yapilir
 *    (bkz. sdkconfig.defaults: CONFIG_ESP_CONSOLE_USB_CDC=y).
 *  - IO8 ve IO9 strapping pinleridir; firmware'de kullanilmaz.
 *  - IO12/IO13 USB D+/D- pinleridir; USB peripheral tarafindan yonetilir,
 *    manuel GPIO yapilandirmasi gerektirmez.
 *  - VBUS ve /CHG sinyallerinin pin routing'i henuz netlesmedigi icin
 *    -1 (stub) olarak tanimlanmistir; charger_if bileseni bu durumu soyutlar.
 *  - SW2 (Layer Butonu) ve ENC_BTN direk GPIO DEGILDIR; U30 (SN74HC165)
 *    shift register'inin E/F pinlerinden okunur (bkz. shift_in_sn74hc165).
 *  - Diot-OR hatti (IO4 WAKEUP) yalnizca 12 dijital tusun OR'lanmis halidir.
 *    Encoder ve SR uzerinden okunan butonlar bu hatta bagli degildir.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * SPI bus (MCP3202 ADC icin donanimsal SPI)
 * Not: SCK (IO6) SN74HC165 ve 74HC595 shift register'lariyla ORTAKTIR.
 *       SR'lar bit-banging ile surulurken ayni SCK pini kullanilir.
 * ========================================================================= */
#define PIN_SPI_SCK          (6)    ///< U35 (ADC) + U29/U30 (dijital tus SR) + U14 (LED SR) ortak clock
#define PIN_SPI_MISO         (2)    ///< U35 ADC DOUT (analog okuma)
#define PIN_SPI_MOSI         (3)    ///< U35 ADC DIN
#define PIN_ADC_CS           (7)    ///< U35 ~CS (MCP3202)

/* =========================================================================
 * Dijital Tus Shift Register (SN74HC165 x2, bit-bang)
 * 24-bit zincirli okuma; ilk 12 bit TCS40DLR durumlaridir.
 * ========================================================================= */
#define PIN_SR_LATCH         (0)    ///< U29/U30 SH/~LD (latch / ornekleme)
#define PIN_SR_DATA          (1)    ///< U29 QH (seri veri cikisi), 24 bit shift-in

/* =========================================================================
 * LED Shift Register (74HC595, bit-bang)
 * Durum/layer LED'leri D8-D11 icin. SCK ortak (PIN_SPI_SCK).
 * ========================================================================= */
#define PIN_LED_SR_RCLK      (30)   ///< U14 RCLK (latch) — RXD0 pininin GPIO kullanimi
#define PIN_LED_SR_SER       (31)   ///< U14 SER (seri veri) — TXD0 pininin GPIO kullanimi

/* =========================================================================
 * Analog Coklayici (CD74HC4067)
 * S0-S3 ile 0-15 arasi kanal secilir. Kullanilan: ANA_KEY_0..11 (kanal 0-11).
 * Kanal 12-15: pil voltaj boleni / /CHG gibi ek sinyaller icin reserve.
 * ========================================================================= */
#define PIN_MUX_S0           (18)
#define PIN_MUX_S1           (19)
#define PIN_MUX_S2           (20)
#define PIN_MUX_S3           (15)
#define MUX_CHANNEL_ANALOG_KEY_COUNT  (12)  ///< ANA_KEY_0..11
#define MUX_CHANNEL_BATTERY           (12)  ///< Pil voltaj boleni (stub; donanim netlesince guncellenecek)
#define MUX_CHANNEL_CHG_STAT          (13)  ///< /CHG sinyali (stub; donanim netlesince guncellenecek)

/* =========================================================================
 * Guc Kontrolu
 * ========================================================================= */
#define PIN_TPS_EN           (14)   ///< U33 boost regulator enable (5V_LED hatti — LED'ler icin)
#define PIN_ANALOG_PWR_EN    (21)   ///< Q1 (P-FET) gate — analog Hall sensörleri + mux gucu (pil tasarrufu)

/* =========================================================================
 * Neopixel (SK6812MINI, 12 LED daisy-chain)
 * ========================================================================= */
#define PIN_NEOPIXEL_DATA    (5)    ///< U34 level shifter uzerinden NEO1-12
#define NEOPIXEL_LED_COUNT   (12)

/* =========================================================================
 * Encoder (donanimsal PCNT)
 * ========================================================================= */
#define PIN_ENC_A            (22)   ///< Filtrelenmis (R43/C9)
#define PIN_ENC_B            (23)   ///< Filtrelenmis (R42/C10)
/* ENC_BTN ve SW2 direk GPIO DEGILDIR; U30 SN74HC165 E/F pinlerinden okunur.
 * Bu sabitler yalnizca SR okuma sonrasi bit konumlarini belirtmek icin kullanilir
 * (bkz. shift_in_sn74hc165.h). */

/* =========================================================================
 * Wake-up (Diot-OR hatti)
 * Yalnizca 12 dijital tusun OR'lanmis halidir. Encoder/SR butonlari haric.
 * ========================================================================= */
#define PIN_WAKEUP_DIOT_OR   (4)    ///< Diot-OR cikisi — GPIO wake (light/deep sleep)

/* =========================================================================
 * Strapping pinler (KULLANMA)
 * ========================================================================= */
#define PIN_BOOT_BTN         (9)    ///< SW1 BOOT butonu — strapping, genel amacli I/O olarak KULLANMA
#define PIN_STRAPPING_IO8    (8)    ///< R39 pull-up, strapping — bosta/kullanma

/* =========================================================================
 * USB (donanimsal, GPIO yapilandirmasi YOK)
 * ========================================================================= */
/* IO12/IO13 USB D+/D- — USB peripheral otomatik yonetir, firmware'de manuel
 * pin ayari yapilmasina gerek yoktur. */

/* =========================================================================
 * Sarj sensörleri (STUB — donanim routing'i henuz netlesmedi)
 * Bu pinler -1 olarak birakilmistir; charger_if bileseni bu durumu soyutlar.
 * Donanim karari netlesince sadece bu sabitler guncellenecektir.
 * ========================================================================= */
#define PIN_VBUS_SENSE       (-1)   ///< STUB: VBUS (USB takili) tespiti, henuz pin atanmadi
#define PIN_CHG_STAT         (-1)   ///< STUB: /CHG (bq253000rter STAT) sinyali, henuz pin atanmadi

/* =========================================================================
 * SPI donanimsal birim secimi
 * ESP32-C6'da 2 adet SPI host var: SPI1 (flash icin rezerve), SPI2 (GPSPI).
 * MCP3202 icin SPI2 (ESP_GSPI_HOST) kullanilir.
 * ========================================================================= */
#define MCP3202_SPI_HOST     (SPI2_HOST)
#define MCP3202_SPI_CLOCK_HZ (1000000)  ///< 1 MHz (MCP3202 max 3.2V'da 1 MHz, 5V'da 2 MHz)

#ifdef __cplusplus
}
#endif
