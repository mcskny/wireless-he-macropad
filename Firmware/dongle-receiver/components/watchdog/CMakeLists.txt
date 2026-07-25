# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

# Component: watchdog
# Sorumluluk: C6 baglanti watchdog 1000ms, tuş birakma tetik (Bölüm 3)

idf_component_register(
    SRCS "watchdog.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer
)
