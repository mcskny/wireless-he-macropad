# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

# Component: espnow_rx
# Sorumluluk: ESP-NOW RX - C6 paket alici, MAC/size filter, queue (Bölüm 3)

idf_component_register(
    SRCS "espnow_rx.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_netif
)
