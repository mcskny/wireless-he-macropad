# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: power_ctrl
# Sorumluluk: MOSFET guc kontrolu + parlaklik hesaplama + mod LED geri bildirimi (Bölüm 4.5)

idf_component_register(
    SRCS "power_ctrl.c"
    INCLUDE_DIRS "include"
    REQUIRES system_state neopixel esp_driver_gpio
)
