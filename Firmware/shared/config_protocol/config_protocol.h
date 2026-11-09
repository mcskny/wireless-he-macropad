// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Candidate USB configuration frame; not yet attached to any transport.
   CRC detects corruption only; it does not authenticate the sender. */
#define WHE_CFG_MAX_PAYLOAD 512u
#define WHE_CFG_HEADER_SIZE 12u
#define WHE_CFG_TRAILER_SIZE 4u
#define WHE_CFG_MAX_FRAME (WHE_CFG_HEADER_SIZE + WHE_CFG_MAX_PAYLOAD + WHE_CFG_TRAILER_SIZE)
typedef enum {
    WHE_CFG_GET_CAPABILITIES=1, WHE_CFG_GET_STATE=2,
    WHE_CFG_STAGE=3, WHE_CFG_COMMIT=4, WHE_CFG_ABORT=5,
    WHE_CFG_REPLY=128
} whe_cfg_type_t;
typedef struct { uint8_t type; uint32_t request_id; uint16_t length; const uint8_t *payload; } whe_cfg_frame_t;
/* Returns complete frame length or zero on invalid input / insufficient space. */
size_t whe_cfg_encode(uint8_t *out, size_t capacity, uint8_t type,
                      uint32_t request_id, const uint8_t *payload, size_t length);
/* Decodes exactly one complete frame; payload points into the caller's buffer. */
bool whe_cfg_decode(const uint8_t *data, size_t length, whe_cfg_frame_t *out);
