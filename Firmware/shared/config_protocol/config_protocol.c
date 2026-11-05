// SPDX-License-Identifier: MIT
#include "config_protocol.h"
#include <string.h>

static void put32(uint8_t *p,uint32_t v) {for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t get32(const uint8_t *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint32_t crc32(const uint8_t *data,size_t n) {
    uint32_t c=0xffffffffu;
    for(size_t i=0;i<n;i++) {c^=data[i];for(int bit=0;bit<8;bit++)c=(c>>1)^(0xedb88320u & (0u-(c&1u)));}
    return ~c;
}
static bool valid_type(uint8_t type) {return (type>=WHE_CFG_GET_CAPABILITIES && type<=WHE_CFG_ABORT) || type==WHE_CFG_REPLY;}
size_t whe_cfg_encode(uint8_t *out,size_t cap,uint8_t type,uint32_t id,const uint8_t *payload,size_t n) {
    if(!out || !valid_type(type) || !id || n>WHE_CFG_MAX_PAYLOAD || (n && !payload) || cap<WHE_CFG_HEADER_SIZE+n+WHE_CFG_TRAILER_SIZE) return 0;
    memcpy(out,"WHEC",4);out[4]=1;out[5]=type;put32(out+6,id);out[10]=(uint8_t)n;out[11]=(uint8_t)(n>>8);
    if(n)memcpy(out+WHE_CFG_HEADER_SIZE,payload,n);
    put32(out+WHE_CFG_HEADER_SIZE+n,crc32(out,WHE_CFG_HEADER_SIZE+n));
    return WHE_CFG_HEADER_SIZE+n+WHE_CFG_TRAILER_SIZE;
}
bool whe_cfg_decode(const uint8_t *data,size_t n,whe_cfg_frame_t *out) {
    if(!data || !out || n<WHE_CFG_HEADER_SIZE+WHE_CFG_TRAILER_SIZE || memcmp(data,"WHEC",4)!=0 || data[4]!=1 || !valid_type(data[5]) || !get32(data+6))return false;
    size_t len=(size_t)data[10]|((size_t)data[11]<<8);
    if(len>WHE_CFG_MAX_PAYLOAD || n!=WHE_CFG_HEADER_SIZE+len+WHE_CFG_TRAILER_SIZE || get32(data+WHE_CFG_HEADER_SIZE+len)!=crc32(data,WHE_CFG_HEADER_SIZE+len))return false;
    *out=(whe_cfg_frame_t){.type=data[5],.request_id=get32(data+6),.length=(uint16_t)len,.payload=data+WHE_CFG_HEADER_SIZE};return true;
}
