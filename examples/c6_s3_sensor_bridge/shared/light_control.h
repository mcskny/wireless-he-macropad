#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "sensor_packet.h"
#define WHE_CONTROL_SIZE 13u
static inline void whe_control_pack(uint8_t b[WHE_CONTROL_SIZE],uint8_t op,uint8_t value,uint8_t layer,uint32_t id){
 memcpy(b,"WHC1",4);b[4]=op;b[5]=value;b[6]=layer;for(unsigned i=0;i<4;i++)b[7+i]=(uint8_t)(id>>(8*i));uint16_t c=whe_sensor_crc(b,11);b[11]=(uint8_t)c;b[12]=(uint8_t)(c>>8);
}
static inline bool whe_control_unpack(const uint8_t *b,size_t n,uint8_t *op,uint8_t *value,uint8_t *layer,uint32_t *id){
 if(!b||n!=WHE_CONTROL_SIZE||memcmp(b,"WHC1",4)||whe_sensor_crc(b,11)!=((uint16_t)b[11]|((uint16_t)b[12]<<8))||b[6]>=4||!((b[4]==1&&b[5]<4)||((b[4]==2||b[4]==3)&&b[5]<12)))return false;
 *op=b[4];*value=b[5];*layer=b[6];*id=(uint32_t)b[7]|((uint32_t)b[8]<<8)|((uint32_t)b[9]<<16)|((uint32_t)b[10]<<24);return true;
}
