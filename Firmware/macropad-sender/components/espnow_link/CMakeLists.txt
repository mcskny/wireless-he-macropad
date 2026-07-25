# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: espnow_link
# Sorumluluk: ESP-NOW S3 TX + callback + fail-safe arama modu (Bölüm 2.2)

idf_component_register(
    SRCS "espnow_link.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_netif
)
