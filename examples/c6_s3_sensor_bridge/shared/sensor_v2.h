// Backward-compatible WHT2 telemetry. CRC is corruption detection, not authentication.
#pragma once
#include "sensor_packet.h"
#include <string.h>
#define WHE_SENSOR_V2_SIZE 49u
typedef struct { bool active,app_open,stats_valid,gate_enabled,layer_pressed; uint8_t layer; uint32_t epoch; uint16_t scans,submitted,skipped,replaced,interval_ms; } whe_sensor_status_t;
static inline void whe_put16(uint8_t *p,uint16_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static inline uint16_t whe_get16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static inline void whe_sensor_v2_encode(uint8_t out[WHE_SENSOR_V2_SIZE],uint32_t seq,const uint16_t values[WHE_SENSOR_COUNT],const whe_sensor_status_t *s){
    uint8_t old[WHE_SENSOR_FRAME_SIZE];whe_sensor_encode(old,seq,values);memcpy(out,old,32);out[3]='2';
    out[32]=(uint8_t)((s->active?1:0)|(s->app_open?2:0)|(s->stats_valid?4:0)|(s->gate_enabled?8:0)|(s->layer_pressed?16:0)|((s->layer&3u)<<5));
    for(int i=0;i<4;i++)out[33+i]=(uint8_t)(s->epoch>>(8*i));
    whe_put16(out+37,s->scans);whe_put16(out+39,s->submitted);whe_put16(out+41,s->skipped);whe_put16(out+43,s->replaced);whe_put16(out+45,s->interval_ms);
    whe_put16(out+47,whe_sensor_crc(out,47));
}
static inline bool whe_sensor_v2_decode(const uint8_t *data,size_t length,uint32_t *seq,uint16_t values[WHE_SENSOR_COUNT],whe_sensor_status_t *s){
    if(!data||!seq||!values||!s||length!=WHE_SENSOR_V2_SIZE||data[0]!='W'||data[1]!='H'||data[2]!='T'||data[3]!='2'||(data[32]&0x80)||whe_sensor_crc(data,47)!=whe_get16(data+47))return false;
    uint8_t old[WHE_SENSOR_FRAME_SIZE];memcpy(old,data,32);old[3]='1';whe_put16(old+32,whe_sensor_crc(old,32));
    if(!whe_sensor_decode(old,sizeof(old),seq,values))return false;
    s->active=(data[32]&1)!=0;s->app_open=(data[32]&2)!=0;s->stats_valid=(data[32]&4)!=0;s->gate_enabled=(data[32]&8)!=0;
    s->layer_pressed=(data[32]&16)!=0;s->layer=(data[32]>>5)&3;
    s->epoch=(uint32_t)data[33]|((uint32_t)data[34]<<8)|((uint32_t)data[35]<<16)|((uint32_t)data[36]<<24);
    s->scans=whe_get16(data+37);s->submitted=whe_get16(data+39);s->skipped=whe_get16(data+41);s->replaced=whe_get16(data+43);s->interval_ms=whe_get16(data+45);
    return !s->stats_valid || (s->interval_ms>=100 && s->interval_ms<=10000);
}
