#pragma once
#include "hid_engine.h"
#include <stdbool.h>
#include <stdint.h>
#define WHE_PROFILE_LAYERS 4
#define WHE_PROFILE_RECORD_SIZE 16
#define WHE_PROFILE_V1_RECORD_SIZE 12
#define WHE_PROFILE_V1_BYTES (WHE_PROFILE_LAYERS*WHE_HID_KEYS*WHE_PROFILE_V1_RECORD_SIZE)
#define WHE_PROFILE_BYTES (WHE_PROFILE_LAYERS*WHE_HID_KEYS*WHE_PROFILE_RECORD_SIZE)
/* Each record is little-endian: free16, pressed16, usage, modifiers,
   point, hysteresis, second usage, modifiers, point, reserved. Indexed
   by layer then raw ADC channel. Zero usage means unassigned. */
bool whe_profile_validate(const uint8_t *data);
bool whe_profile_load_layer(const uint8_t *data,unsigned layer,whe_hid_engine_t *engine);
bool whe_profile_migrate_v1(uint8_t *out,const uint8_t *old);
