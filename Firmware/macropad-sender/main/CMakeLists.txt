# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# main/ bileseni - ana döngü (Bölüm 2) ve global baslangic state'i.
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "include"
    REQUIRES
        system_state
        esp_rom
        esp_timer
        nvs_store
        spi_bus
        mcp3202
        mux_cd74hc4067
        shift_in_sn74hc165
        shift_out_74hc595
        encoder
        neopixel
        power_ctrl
        battery
        charger_if
        packet
        espnow_link
        wakeup_manager
)
