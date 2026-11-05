#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "sensor_packet.h"
#define WHE_LIGHT_SIZE 42u
/* Four 7-byte profiles and the host reference-clock time in milliseconds. */
static inline void whe_light_pack(uint8_t out[WHE_LIGHT_SIZE],const uint8_t profiles[28],uint64_t ms){
    memcpy(out,"WHL1",4);memcpy(out+4,profiles,28);
    for(unsigned i=0;i<8;i++)out[32+i]=(uint8_t)(ms>>(8*i));
    uint16_t crc=whe_sensor_crc(out,40);out[40]=(uint8_t)crc;out[41]=(uint8_t)(crc>>8);
}
static inline bool whe_light_unpack(const uint8_t *p,size_t len,uint8_t profiles[28],uint64_t *ms){
    if(!p||!profiles||!ms||len!=WHE_LIGHT_SIZE||memcmp(p,"WHL1",4)||whe_sensor_crc(p,40)!=((uint16_t)p[40]|((uint16_t)p[41]<<8)))return false;
    for(unsigned i=0;i<4;i++){const uint8_t *q=p+4+i*7;if(q[0]>=10||q[4]>100||q[5]<1||q[5]>100||(q[6]&~5u))return false;}
    memcpy(profiles,p+4,28);*ms=0;for(unsigned i=0;i<8;i++)*ms|=(uint64_t)p[32+i]<<(8*i);return true;
}
