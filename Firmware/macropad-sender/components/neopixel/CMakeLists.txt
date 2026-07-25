# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: neopixel
# Sorumluluk: SK6812MINI 12 LED RMT v6 surucu + parlaklik limiti

idf_component_register(
    SRCS "neopixel.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_rmt esp_rom
)
