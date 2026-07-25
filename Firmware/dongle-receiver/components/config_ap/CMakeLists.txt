# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

# Component: config_ap
# Sorumluluk: Wi-Fi AP + WebSocket server, kalibrasyon (Bölüm 2 CONFIG_AP_MODE)

idf_component_register(
    SRCS "config_ap.c"
    INCLUDE_DIRS "include"
    REQUIRES system_state esp_wifi esp_event esp_netif esp_http_server
    EMBED_FILES "web/index.html"
)
