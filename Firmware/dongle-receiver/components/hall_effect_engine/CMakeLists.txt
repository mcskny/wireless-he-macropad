# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

# Component: hall_effect_engine
# Sorumluluk: Hall Effect motoru - normalizasyon + RT + Snap Tap + Joystick (Bölüm 4)

idf_component_register(
    SRCS "hall_effect_engine.c"
    INCLUDE_DIRS "include"
    REQUIRES system_state
)
