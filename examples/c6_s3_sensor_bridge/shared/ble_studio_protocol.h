/* BLE Studio lighting control wire format, v1. Not a GATT implementation.
 * Messages fit the default 20-byte ATT write payload. CRC detects corruption;
 * callers MUST enforce an encrypted, authorized connection and acknowledge only
 * after applying a valid command. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sensor_packet.h"
#define WHE_BLE_STUDIO_FRAME_SIZE 15u
#define WHE_BLE_STUDIO_SELECT_LAYER 1u
#define WHE_BLE_STUDIO_LIGHT_PROFILE 2u

typedef struct {
    uint8_t operation, layer, profile[7];
    uint16_t transaction;
} whe_ble_studio_command_t;

static inline bool whe_ble_studio_decode(const uint8_t *frame,size_t length,whe_ble_studio_command_t *out){
    if(!frame || !out || length!=WHE_BLE_STUDIO_FRAME_SIZE || frame[0]!='W' || frame[1]!=1 || frame[3]>=4)return false;
    uint16_t expected=(uint16_t)frame[13]|((uint16_t)frame[14]<<8);
    if(whe_sensor_crc(frame,13)!=expected)return false;
    uint8_t op=frame[2];const uint8_t *p=frame+6;
    if(op==WHE_BLE_STUDIO_SELECT_LAYER){for(unsigned i=0;i<7;i++)if(p[i])return false;}
    else if(op==WHE_BLE_STUDIO_LIGHT_PROFILE){
        if(p[0]>=10 || p[4]>100 || p[5]<1 || p[5]>100 || (p[6]&~5u))return false;
    }else return false;
    uint16_t tx=(uint16_t)frame[4]|((uint16_t)frame[5]<<8);if(!tx)return false;
    out->operation=op;out->layer=frame[3];out->transaction=tx;
    for(unsigned i=0;i<7;i++)out->profile[i]=p[i];
    return true;
}
