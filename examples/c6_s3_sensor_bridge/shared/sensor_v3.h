#pragma once
#include "sensor_v2.h"
#define WHE_SENSOR_V3_SIZE 53u
/* V3 adds commanded rail-on time, in microseconds, for the last interval. */
static inline void whe_sensor_v3_encode(uint8_t out[WHE_SENSOR_V3_SIZE],uint32_t seq,const uint16_t values[WHE_SENSOR_COUNT],const whe_sensor_status_t *s,uint32_t gate_on_us){
 uint8_t v2[WHE_SENSOR_V2_SIZE];whe_sensor_v2_encode(v2,seq,values,s);memcpy(out,v2,47);out[3]='3';
 for(unsigned i=0;i<4;i++)out[47+i]=(uint8_t)(gate_on_us>>(8*i));
 whe_put16(out+51,whe_sensor_crc(out,51));
}
static inline bool whe_sensor_v3_decode(const uint8_t *data,size_t len,uint32_t *seq,uint16_t values[WHE_SENSOR_COUNT],whe_sensor_status_t *s,uint32_t *gate_on_us){
 if(!data||!gate_on_us||len!=WHE_SENSOR_V3_SIZE||memcmp(data,"WHT3",4)||whe_sensor_crc(data,51)!=whe_get16(data+51))return false;
 uint8_t v2[WHE_SENSOR_V2_SIZE];memcpy(v2,data,47);v2[3]='2';whe_put16(v2+47,whe_sensor_crc(v2,47));
 if(!whe_sensor_v2_decode(v2,sizeof(v2),seq,values,s))return false;
 *gate_on_us=(uint32_t)data[47]|((uint32_t)data[48]<<8)|((uint32_t)data[49]<<16)|((uint32_t)data[50]<<24);
 return !s->stats_valid || *gate_on_us<=(uint32_t)s->interval_ms*1000u+2000u;
}
