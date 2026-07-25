# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: nvs_store
# Sorumluluk: NVS uzerinde generic (tip-bagimsiz) anahtar-deger saklama (Bölüm 1.1)

idf_component_register(
    SRCS "nvs_store.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash
)
