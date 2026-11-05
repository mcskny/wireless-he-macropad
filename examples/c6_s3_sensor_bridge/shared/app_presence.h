// Test protocol: source MAC + CRC detect accidental cross-talk, not authentication.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sensor_packet.h"
#define WHE_APP_FRAME_SIZE 8u
#define WHE_APP_TIMEOUT_US 1500000LL
static inline void whe_app_encode(uint8_t out[WHE_APP_FRAME_SIZE],bool app_open){
    out[0]='W';out[1]='H';out[2]='A';out[3]='1';out[4]=1;out[5]=app_open?1:0;
    uint16_t crc=whe_sensor_crc(out,6);out[6]=(uint8_t)crc;out[7]=(uint8_t)(crc>>8);
}
static inline bool whe_app_decode(const uint8_t *p,size_t len,bool *app_open){
    if(!p||!app_open||len!=WHE_APP_FRAME_SIZE||p[0]!='W'||p[1]!='H'||p[2]!='A'||p[3]!='1'||p[4]!=1||p[5]>1)return false;
    if(whe_sensor_crc(p,6)!=((uint16_t)p[6]|((uint16_t)p[7]<<8)))return false;
    *app_open=p[5]!=0;return true;
}
static inline bool whe_app_present(bool reported,int64_t last_us,int64_t now_us){
    return reported && now_us>=last_us && now_us-last_us<=WHE_APP_TIMEOUT_US;
}
