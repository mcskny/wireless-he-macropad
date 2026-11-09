// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define WHE_SENSOR_COUNT 12u
#define WHE_SENSOR_FRAME_SIZE 34u
/* Fixed little-endian frame: magic WHT1, sequence u32, 12 ADC u16, CRC16.
   CRC detects accidental corruption; it does NOT authenticate the sender. */
static inline uint16_t whe_sensor_crc(const uint8_t *p,size_t n) {
    uint16_t c=0xffff;for(size_t i=0;i<n;i++){c^=(uint16_t)p[i]<<8;for(int b=0;b<8;b++)c=(uint16_t)((c&0x8000)?(c<<1)^0x1021:c<<1);}return c;
}
static inline void whe_sensor_encode(uint8_t out[WHE_SENSOR_FRAME_SIZE],uint32_t seq,const uint16_t values[WHE_SENSOR_COUNT]) {
    out[0]='W';out[1]='H';out[2]='T';out[3]='1';for(int i=0;i<4;i++)out[4+i]=(uint8_t)(seq>>(8*i));
    for(size_t i=0;i<WHE_SENSOR_COUNT;i++){out[8+2*i]=(uint8_t)values[i];out[9+2*i]=(uint8_t)(values[i]>>8);}
    uint16_t crc=whe_sensor_crc(out,32);out[32]=(uint8_t)crc;out[33]=(uint8_t)(crc>>8);
}
static inline bool whe_sensor_decode(const uint8_t *data,size_t length,uint32_t *seq,uint16_t values[WHE_SENSOR_COUNT]) {
    if(!data||!seq||!values||length!=WHE_SENSOR_FRAME_SIZE||data[0]!='W'||data[1]!='H'||data[2]!='T'||data[3]!='1')return false;
    uint16_t crc=(uint16_t)data[32]|((uint16_t)data[33]<<8);if(crc!=whe_sensor_crc(data,32))return false;
    uint16_t temp[WHE_SENSOR_COUNT];for(size_t i=0;i<WHE_SENSOR_COUNT;i++){temp[i]=(uint16_t)data[8+2*i]|((uint16_t)data[9+2*i]<<8);if(temp[i]>4095)return false;}
    *seq=(uint32_t)data[4]|((uint32_t)data[5]<<8)|((uint32_t)data[6]<<16)|((uint32_t)data[7]<<24);
    for(size_t i=0;i<WHE_SENSOR_COUNT;i++) { values[i]=temp[i]; }
    return true;
}
