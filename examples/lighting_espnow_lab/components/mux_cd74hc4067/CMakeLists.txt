# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: mux_cd74hc4067
# Sorumluluk: CD74HC4067 16-kanal analog mux kanal secimi (Bölüm 3.1.3)

idf_component_register(
    SRCS "mux_cd74hc4067.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio
)
