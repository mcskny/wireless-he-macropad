// Transport-mode chord coordinator. Inputs must already be debounced.
// It does not start or stop a radio or write configuration by itself.
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { WHE_TRANSPORT_ESPNOW=0, WHE_TRANSPORT_BLE=1 } whe_transport_t;
typedef struct {
    whe_transport_t mode;
    bool layer_down, key_down, chord_used, consume_key;
} whe_transport_mode_t;
typedef struct {
    bool layer_tap, switch_requested, suppress_key;
    whe_transport_t target;
} whe_transport_action_t;

static inline whe_transport_action_t whe_transport_input(whe_transport_mode_t *s,bool layer,bool key){
    whe_transport_action_t out={0};
    if(!s)return out;
    // A key already held before Layer does not constitute the requested chord.
    if(layer && key && !s->key_down && !s->chord_used){
        s->chord_used=true;s->consume_key=true;out.switch_requested=true;
        out.target=s->mode==WHE_TRANSPORT_ESPNOW ? WHE_TRANSPORT_BLE : WHE_TRANSPORT_ESPNOW;
    }
    if(s->layer_down && !layer){if(!s->chord_used)out.layer_tap=true;s->chord_used=false;}
    out.suppress_key=s->consume_key;
    if(!key)s->consume_key=false;
    s->layer_down=layer;s->key_down=key;
    return out;
}
// Call only after releasing held outputs and successfully starting target radio.
static inline void whe_transport_commit(whe_transport_mode_t *s,whe_transport_t mode){
    if(s && (mode==WHE_TRANSPORT_ESPNOW || mode==WHE_TRANSPORT_BLE))s->mode=mode;
}
