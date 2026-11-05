/* Transport-independent Easy Switch slot selection. Bond management and host
 * authorization must be implemented by the BLE transport, not this helper. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define WHE_BLE_SLOT_COUNT 3
/* Bottom row: KEY 3, KEY 7, KEY 11; measured raw ADC channels. */
static const uint8_t whe_ble_slot_channels[WHE_BLE_SLOT_COUNT]={9,10,11};
typedef struct { bool consumed; uint8_t previous; } whe_ble_slot_chord_t;
/* Inputs are debounced; keys already down when layer is pressed are ignored. */
static inline int whe_ble_slot_chord_step(whe_ble_slot_chord_t *state, bool layer,
                                         uint8_t held_mask){
    if(!state)return -1;
    held_mask &= 7u;
    uint8_t rising=(uint8_t)(held_mask & (uint8_t)~state->previous);
    int slot=-1;
    if(layer && !state->consumed && rising && !(rising & (rising-1))){
        for(unsigned i=0;i<WHE_BLE_SLOT_COUNT;i++)if(rising==(1u<<i))slot=(int)i;
        state->consumed=true;
    }
    if(!layer)state->consumed=false;
    state->previous=held_mask;
    return slot;
}
