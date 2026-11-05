// SPDX-License-Identifier: MIT
#include "encoder_controller.h"
#include <string.h>
#define WHE_LONG_PRESS_MS 2000u
#define WHE_LAYER_COUNT 4u

void whe_encoder_init(whe_encoder_controller_t *c, uint8_t layer) {
    memset(c, 0, sizeof(*c));
    c->layer = layer < WHE_LAYER_COUNT ? layer : 0;
}
whe_encoder_event_t whe_encoder_update(whe_encoder_controller_t *c,
    bool layer_down, bool encoder_down, int8_t delta, uint32_t now_ms) {
    whe_encoder_event_t out = {0};
    if (c->resync_wait_release) {
        if (!layer_down && !encoder_down) c->resync_wait_release = false;
        out.layer = c->layer;
        return out;
    }
    if (layer_down && !c->layer_down) {
        c->layer_started_ms = now_ms;
        c->layer_consumed = false;
    }
    out.alternate = layer_down;
    out.rotation = delta;
    out.encoder_pressed = encoder_down && !c->encoder_down;
    out.encoder_released = !encoder_down && c->encoder_down;
    if (out.encoder_pressed) c->encoder_alternate = layer_down;
    out.encoder_alternate = c->encoder_alternate;
    if (layer_down && (delta != 0 || out.encoder_pressed)) c->layer_consumed = true;
    if (!layer_down && c->layer_down && !c->layer_consumed) {
        uint8_t before = c->layer;
        c->layer = (uint32_t)(now_ms - c->layer_started_ms) >= WHE_LONG_PRESS_MS
            ? 0 : (uint8_t)((c->layer + 1) % WHE_LAYER_COUNT);
        out.layer_changed = c->layer != before;
    }
    c->layer_down = layer_down;
    c->encoder_down = encoder_down;
    out.layer = c->layer;
    return out;
}
whe_encoder_event_t whe_encoder_disconnect(whe_encoder_controller_t *c) {
    whe_encoder_event_t out = { .layer = c->layer, .encoder_released = c->encoder_down,
                                .encoder_alternate = c->encoder_alternate };
    c->layer_down = false; c->encoder_down = false; c->layer_consumed = false;
    c->resync_wait_release = true;
    return out;
}
