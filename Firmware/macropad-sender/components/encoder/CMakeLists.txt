# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: encoder
# Sorumluluk: PCNT v2 quadrature encoder decode (IO22/IO23)

idf_component_register(
    SRCS "encoder.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_pcnt
)
