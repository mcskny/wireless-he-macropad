/* Atomic, transport-independent staging for one BLE Studio layer.
 * BLE GATT authorization, reliable acknowledgement, NVS commit, and timeouts
 * belong to the caller. No active state is changed before validation. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "hid_profile.h"
#include "sensor_packet.h"
#define WHE_BLE_LAYER_BYTES (WHE_HID_KEYS*WHE_PROFILE_RECORD_SIZE)
#define WHE_BLE_LAYER_CHUNK 8u
#define WHE_BLE_LAYER_CHUNKS (WHE_BLE_LAYER_BYTES/WHE_BLE_LAYER_CHUNK)
_Static_assert(WHE_BLE_LAYER_CHUNKS==24,"unexpected profile layout");
typedef struct {
    uint8_t candidate[WHE_PROFILE_BYTES];
    uint32_t received;
    uint16_t transaction;
    uint8_t layer;
    bool active;
} whe_ble_layer_staging_t;
static inline bool whe_ble_layer_begin(whe_ble_layer_staging_t *s,const uint8_t *current,unsigned layer,uint16_t tx){
    if(!s || !current || layer>=WHE_PROFILE_LAYERS || !tx || !whe_profile_validate(current))return false;
    memcpy(s->candidate,current,WHE_PROFILE_BYTES);s->received=0;s->transaction=tx;s->layer=layer;s->active=true;return true;
}
static inline bool whe_ble_layer_chunk(whe_ble_layer_staging_t *s,uint16_t tx,unsigned index,const uint8_t *data,size_t length){
    if(!s||!s->active||s->transaction!=tx||index>=WHE_BLE_LAYER_CHUNKS||!data||length!=WHE_BLE_LAYER_CHUNK)return false;
    uint32_t bit=1u<<index;uint8_t *p=s->candidate+s->layer*WHE_BLE_LAYER_BYTES+index*WHE_BLE_LAYER_CHUNK;
    if(s->received&bit){if(memcmp(p,data,length)){s->active=false;return false;}return true;}
    memcpy(p,data,length);s->received|=bit;return true;
}
static inline bool whe_ble_layer_ready(const whe_ble_layer_staging_t *s,uint16_t tx,uint16_t crc){
    return s && s->active && s->transaction==tx && s->received==((1u<<WHE_BLE_LAYER_CHUNKS)-1u) &&
      whe_sensor_crc(s->candidate+s->layer*WHE_BLE_LAYER_BYTES,WHE_BLE_LAYER_BYTES)==crc && whe_profile_validate(s->candidate);
}
