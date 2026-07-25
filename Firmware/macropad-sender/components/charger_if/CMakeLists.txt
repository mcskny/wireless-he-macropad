# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: charger_if
# Sorumluluk: VBUS//CHG stub soyutlama + sarj animasyonlari (Bölüm 5)

idf_component_register(
    SRCS "charger_if.c"
    INCLUDE_DIRS "include"
    REQUIRES system_state neopixel power_ctrl esp_driver_gpio
)
