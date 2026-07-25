# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: battery
# Sorumluluk: Pil olcum + flag_50/25/15/5 + kritik kapanma animasyon (Bölüm 4.4)

idf_component_register(
    SRCS "battery.c"
    INCLUDE_DIRS "include"
    REQUIRES system_state neopixel power_ctrl shift_out_74hc595
)
