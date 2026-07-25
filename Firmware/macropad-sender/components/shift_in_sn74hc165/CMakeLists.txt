# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: shift_in_sn74hc165
# Sorumluluk: SN74HC165 x2 zincir okuma - dijital tus + SW2/ENC_BTN (Bölüm 3.1.1)

idf_component_register(
    SRCS "shift_in_sn74hc165.c"
    INCLUDE_DIRS "include"
    REQUIRES spi_bus esp_driver_gpio
)
