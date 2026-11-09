// SPDX-License-Identifier: GPL-3.0-only
#include "encoder_controller.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    whe_encoder_controller_t c; whe_encoder_event_t e;
    whe_encoder_init(&c,0);
    whe_encoder_update(&c,true,false,0,100);
    e=whe_encoder_update(&c,false,false,0,200); assert(e.layer==1 && e.layer_changed);
    whe_encoder_update(&c,true,false,0,300);
    e=whe_encoder_update(&c,true,false,-1,310); assert(e.alternate && e.rotation==-1);
    e=whe_encoder_update(&c,false,false,0,320); assert(e.layer==1 && !e.layer_changed);
    whe_encoder_update(&c,true,false,0,400);
    e=whe_encoder_update(&c,true,true,0,410); assert(e.alternate && e.encoder_pressed);
    e=whe_encoder_update(&c,false,false,0,420); assert(e.layer==1 && !e.layer_changed && e.encoder_released && e.encoder_alternate);
    whe_encoder_update(&c,true,false,0,500);
    e=whe_encoder_update(&c,false,false,0,2500); assert(e.layer==0 && e.layer_changed);
    whe_encoder_init(&c,2);whe_encoder_update(&c,true,false,0,UINT32_MAX-100);
    e=whe_encoder_update(&c,false,false,0,50);assert(e.layer==3);
    whe_encoder_update(&c,true,true,0,200);e=whe_encoder_disconnect(&c);assert(e.encoder_released && e.layer==3);
    e=whe_encoder_update(&c,true,true,1,250);assert(!e.encoder_pressed && e.rotation==0);
    e=whe_encoder_update(&c,false,false,0,300);assert(!e.layer_changed && e.layer==3);
    whe_encoder_init(&c,99);assert(c.layer==0);
    puts("encoder controller: all tests passed");
}
