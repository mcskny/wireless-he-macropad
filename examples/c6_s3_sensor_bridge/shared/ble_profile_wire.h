// Framing for profile replication; CRC detects corruption, not authentication.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_profile_staging.h"

#define WHE_BLE_PROFILE_WIRE_MAX (12u+WHE_BLE_PROFILE_CHUNK)
typedef enum {WHE_BLE_PROFILE_BEGIN=1,WHE_BLE_PROFILE_DATA=2,WHE_BLE_PROFILE_COMMIT=3} whe_ble_profile_op_t;
typedef struct {whe_ble_profile_op_t op;uint32_t transaction;uint8_t index;const uint8_t *payload;size_t payload_size;} whe_ble_profile_message_t;

static inline size_t whe_ble_profile_pack(uint8_t out[WHE_BLE_PROFILE_WIRE_MAX],whe_ble_profile_op_t op,
                                          uint32_t tx,uint8_t index,const uint8_t *payload,size_t length){
    if(!out||!tx||!((op==WHE_BLE_PROFILE_BEGIN&&index==0&&length==0)||
       (op==WHE_BLE_PROFILE_DATA&&index<WHE_BLE_PROFILE_CHUNKS&&length==WHE_BLE_PROFILE_CHUNK&&payload)||
       (op==WHE_BLE_PROFILE_COMMIT&&index==0&&length==2&&payload)))return 0;
    memcpy(out,"WBP1",4);out[4]=(uint8_t)op;for(unsigned i=0;i<4;i++)out[5+i]=(uint8_t)(tx>>(8*i));out[9]=index;
    if(length)memcpy(out+10,payload,length);
    uint16_t crc=whe_sensor_crc(out,10+length);out[10+length]=(uint8_t)crc;out[11+length]=(uint8_t)(crc>>8);return 12+length;
}
static inline bool whe_ble_profile_unpack(const uint8_t *buf,size_t length,whe_ble_profile_message_t *out){
    if(!buf||!out||length<12||length>WHE_BLE_PROFILE_WIRE_MAX||memcmp(buf,"WBP1",4))return false;
    uint16_t crc=(uint16_t)buf[length-2]|((uint16_t)buf[length-1]<<8);
    if(whe_sensor_crc(buf,length-2)!=crc)return false;
    uint32_t tx=(uint32_t)buf[5]|((uint32_t)buf[6]<<8)|((uint32_t)buf[7]<<16)|((uint32_t)buf[8]<<24);
    size_t payload=length-12;uint8_t op=buf[4],index=buf[9];
    if(!tx||!((op==WHE_BLE_PROFILE_BEGIN&&index==0&&payload==0)||
       (op==WHE_BLE_PROFILE_DATA&&index<WHE_BLE_PROFILE_CHUNKS&&payload==WHE_BLE_PROFILE_CHUNK)||
       (op==WHE_BLE_PROFILE_COMMIT&&index==0&&payload==2)))return false;
    *out=(whe_ble_profile_message_t){.op=(whe_ble_profile_op_t)op,.transaction=tx,.index=index,.payload=buf+10,.payload_size=payload};return true;
}
