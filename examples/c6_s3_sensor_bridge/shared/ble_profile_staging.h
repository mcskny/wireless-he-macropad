// BLE profile-transfer staging. No radio, flash writes or acknowledgements here.
// The caller must transport chunks, authenticate the sender and commit to NVS.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "hid_profile.h"
#include "sensor_packet.h"

#define WHE_BLE_PROFILE_CHUNK 48u
#define WHE_BLE_PROFILE_CHUNKS (WHE_PROFILE_BYTES/WHE_BLE_PROFILE_CHUNK)
_Static_assert(WHE_PROFILE_BYTES%WHE_BLE_PROFILE_CHUNK==0 && WHE_BLE_PROFILE_CHUNKS==16,"profile layout");

typedef struct {
    uint8_t data[WHE_PROFILE_BYTES];
    uint32_t transaction;
    uint16_t received;
    bool active;
} whe_ble_profile_staging_t;

static inline bool whe_ble_profile_begin(whe_ble_profile_staging_t *s,uint32_t transaction){
    if(!s || !transaction)return false;
    memset(s,0,sizeof(*s));s->active=true;s->transaction=transaction;return true;
}
static inline bool whe_ble_profile_chunk(whe_ble_profile_staging_t *s,uint32_t transaction,
                                          unsigned index,const uint8_t *bytes,size_t length){
    if(!s||!s->active||s->transaction!=transaction||index>=WHE_BLE_PROFILE_CHUNKS||!bytes||length!=WHE_BLE_PROFILE_CHUNK)return false;
    uint16_t bit=(uint16_t)(1u<<index);uint8_t *dest=s->data+index*WHE_BLE_PROFILE_CHUNK;
    // A retry is idempotent, but inconsistent duplicate chunks invalidate staging.
    if(s->received&bit){if(memcmp(dest,bytes,length)){s->active=false;return false;}return true;}
    memcpy(dest,bytes,length);s->received|=bit;return true;
}
static inline bool whe_ble_profile_ready(const whe_ble_profile_staging_t *s,uint32_t transaction,uint16_t crc){
    return s && s->active && s->transaction==transaction && s->received==UINT16_MAX &&
           whe_sensor_crc(s->data,WHE_PROFILE_BYTES)==crc && whe_profile_validate(s->data);
}
static inline void whe_ble_profile_abort(whe_ble_profile_staging_t *s){if(s)s->active=false;}
