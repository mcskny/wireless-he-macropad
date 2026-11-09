// SPDX-License-Identifier: GPL-3.0-only
#include "config_stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { unsigned count; uint32_t last; size_t length; } seen_t;
static void receive(const whe_cfg_frame_t *f, void *context) {
    seen_t *seen=context; ++seen->count; seen->last=f->request_id; seen->length=f->length;
}
int main(void) {
    whe_cfg_stream_t s; seen_t seen={0}; whe_cfg_stream_init(&s);
    uint8_t frame[WHE_CFG_MAX_FRAME],payload[WHE_CFG_MAX_PAYLOAD];memset(payload,0xa5,sizeof(payload));
    size_t n=whe_cfg_encode(frame,sizeof(frame),WHE_CFG_STAGE,0x1234,payload,sizeof(payload)); assert(n==WHE_CFG_MAX_FRAME);
    const uint8_t noise[]={0,1,'W','H','X','W','H'};
    assert(whe_cfg_stream_feed(&s,noise,sizeof(noise),receive,&seen)==0);
    /* Noise ends with WH; reset models an explicit disconnect. */
    whe_cfg_stream_reset(&s);
    for(size_t i=0;i<n;i++) assert(whe_cfg_stream_feed(&s,frame+i,1,receive,&seen)==(i==n-1));
    assert(seen.count==1 && seen.last==0x1234 && seen.length==sizeof(payload));
    uint8_t shortframe[32];size_t k=whe_cfg_encode(shortframe,sizeof(shortframe),WHE_CFG_GET_STATE,7,NULL,0);assert(k);
    /* A magic prefix split across chunks must be retained. */
    assert(whe_cfg_stream_feed(&s,shortframe,2,receive,&seen)==0);
    assert(whe_cfg_stream_feed(&s,shortframe+2,k-2,receive,&seen)==1 && seen.count==2);
    uint8_t corrupt[32];memcpy(corrupt,shortframe,k);corrupt[k-1]^=1;
    assert(whe_cfg_stream_feed(&s,corrupt,k,receive,&seen)==0 && s.rejected>0);
    uint8_t joined[64];memcpy(joined,shortframe,k);memcpy(joined+k,shortframe,k);
    assert(whe_cfg_stream_feed(&s,joined,2*k,receive,&seen)==2 && seen.count==4 && seen.last==7);
    assert(whe_cfg_stream_feed(NULL,joined,k,receive,&seen)==0);
    assert(whe_cfg_stream_feed(&s,NULL,1,receive,&seen)==0);
    assert(whe_cfg_stream_feed(&s,shortframe,5,receive,&seen)==0);whe_cfg_stream_reset(&s);
    assert(whe_cfg_stream_feed(&s,shortframe+5,k-5,receive,&seen)==0);
    assert(whe_cfg_stream_feed(&s,shortframe,k,receive,&seen)==1 && seen.count==5);
    puts("config stream: all tests passed");
}
