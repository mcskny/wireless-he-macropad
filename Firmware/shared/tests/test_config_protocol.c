// SPDX-License-Identifier: GPL-3.0-only
#include "config_protocol.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    uint8_t bytes[WHE_CFG_MAX_FRAME],payload[WHE_CFG_MAX_PAYLOAD];memset(payload,0xa5,sizeof(payload));
    assert(!whe_cfg_encode(bytes,sizeof(bytes),WHE_CFG_STAGE,0,payload,1));
    assert(!whe_cfg_encode(bytes,sizeof(bytes),WHE_CFG_STAGE,1,payload,sizeof(payload)+1));
    size_t n=whe_cfg_encode(bytes,sizeof(bytes),WHE_CFG_STAGE,0x12345678,payload,sizeof(payload));
    assert(n==sizeof(bytes));whe_cfg_frame_t f={0};assert(whe_cfg_decode(bytes,n,&f));
    assert(f.type==WHE_CFG_STAGE && f.request_id==0x12345678 && f.length==sizeof(payload) && !memcmp(f.payload,payload,sizeof(payload)));
    for(size_t i=0;i<n;i++) assert(!whe_cfg_decode(bytes,i,&f));
    bytes[20]^=1;assert(!whe_cfg_decode(bytes,n,&f));bytes[20]^=1;
    bytes[4]=2;assert(!whe_cfg_decode(bytes,n,&f));
    puts("config frame: all tests passed");
}
