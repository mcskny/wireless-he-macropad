// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Pure state machine: independent of USB, radio, NVS and test firmware. */
typedef struct {
    uint8_t layer;
    bool layer_down;
    bool encoder_down;
    bool layer_consumed;
    bool encoder_alternate;
    bool resync_wait_release;
    uint32_t layer_started_ms;
} whe_encoder_controller_t;

typedef struct {
    uint8_t layer;
    bool layer_changed;
    bool alternate;             /* rotation uses current layer state */
    bool encoder_alternate;     /* latched at encoder press until release */
    int8_t rotation;
    bool encoder_pressed;
    bool encoder_released;
} whe_encoder_event_t;

void whe_encoder_init(whe_encoder_controller_t *controller, uint8_t layer);
/* Inputs must already be debounced. Signed elapsed arithmetic is wrap-safe. */
whe_encoder_event_t whe_encoder_update(whe_encoder_controller_t *controller,
    bool layer_down, bool encoder_down, int8_t delta, uint32_t now_ms);
/* On link loss cancel held states; never interpret loss as a tap. */
whe_encoder_event_t whe_encoder_disconnect(whe_encoder_controller_t *controller);
