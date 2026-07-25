# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: system_state
# Sorumluluk: Sistem geneli runtime state tipleri (power_mode_t, battery_flags_t,
# link_state_t, system_state_t, persistent_config_t). Header-only tip saglayici.

idf_component_register(
    SRCS "system_state.c"
    INCLUDE_DIRS "include"
)
