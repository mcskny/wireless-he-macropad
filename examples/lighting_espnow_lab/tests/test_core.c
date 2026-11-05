#include "lighting.h"
#include "protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int sum(light_rgb_t c){return c.r+c.g+c.b;}
int main(void){
    light_profile_t p={EFFECT_STATIC,100,150,200,100,80,0};
    assert(light_profile_valid(&p));
    light_engine_t e;light_engine_init(&e,&p);light_rgb_t a[12],b[12];
    light_engine_render(&e,0,a);assert(a[0].r==100 && a[0].g==150 && a[0].b==200);
    p.effect=EFFECT_OFF;assert(light_engine_set_profile(&e,&p));light_engine_render(&e,100,a);assert(sum(a[11])==0);
    p.effect=EFFECT_BREATHE;p.flags=FLAG_COLORFUL;p.speed=80;assert(light_engine_set_profile(&e,&p));
    /* speed 80 -> one second, trough is 500 ms, including rainbow transition */
    light_engine_render(&e,500,a);assert(sum(a[0])==0);
    light_engine_render(&e,250,a);light_engine_render(&e,1250,b);assert(memcmp(&a[0],&b[0],sizeof(a[0]))!=0);
    p.effect=EFFECT_RAINBOW;p.flags=0;assert(light_engine_set_profile(&e,&p));light_engine_render(&e,0,a);light_engine_render(&e,1000,b);assert(memcmp(&a[0],&b[0],sizeof(a[0]))!=0);
    p.effect=EFFECT_FLOW;p.flags=FLAG_COLORFUL;assert(light_engine_set_profile(&e,&p));
    light_engine_render(&e,500,a);assert(sum(a[0])==0); /* each key changes color at its own dark point */
    p.effect=EFFECT_SWEEP;p.flags=0;assert(light_engine_set_profile(&e,&p));light_engine_render(&e,500,a);assert(sum(a[0])==0);
    p.flags=FLAG_COLORFUL|FLAG_SWEEP_TURN;assert(light_engine_set_profile(&e,&p));
    light_engine_render(&e,100,a);light_engine_render(&e,1100,b);assert(memcmp(&a[0],&b[0],sizeof(a[0]))!=0);
    p.effect=EFFECT_REACTIVE;p.flags=FLAG_DEPTH;p.r=200;p.g=0;p.b=0;assert(light_engine_set_profile(&e,&p));
    light_engine_input(&e,4,0.25f,0);light_engine_render(&e,10,a);light_engine_input(&e,4,0.8f,20);light_engine_render(&e,25,b);assert(b[4].r>a[4].r && sum(b[3])==0);
    p.effect=EFFECT_RIPPLE;p.flags=0;light_engine_init(&e,&p);
    light_engine_input(&e,0,1,1000);light_engine_input(&e,0,0,1010);light_engine_input(&e,0,1,1300);
    light_engine_render(&e,1400,a);
    light_engine_t just_new;light_engine_init(&just_new,&p);light_engine_input(&just_new,0,1,1300);light_engine_render(&just_new,1400,b);
    assert(a[0].r>b[0].r); /* Old wave continues after the new press. */
    p.effect=EFFECT_RIPPLE;p.flags=FLAG_COLORFUL;p.r=255;p.g=255;p.b=255;light_engine_init(&e,&p);
    light_engine_input(&e,0,1,1000);light_engine_input(&e,0,0,1010);light_engine_input(&e,0,1,1300);
    light_engine_render(&e,1400,a);
    light_engine_init(&just_new,&p);light_engine_input(&just_new,0,1,1300);light_engine_render(&just_new,1400,b);
    assert(memcmp(&a[0],&b[0],sizeof(a[0]))!=0); /* overlapping colors mix */
    light_engine_init(&e,&p);light_engine_input(&e,2,1,2000);assert(e.next_wave==1 && e.pressed[2]);
    light_engine_sync_physical_hit(&e,2,8,2020,false);assert(e.next_wave==1 && e.hit_count==8 && e.pressed[2]);
    light_engine_init(&just_new,&p);light_engine_virtual_hit(&just_new,2,8,2000);assert(e.waves[0].hue==just_new.waves[0].hue);
    light_engine_virtual_hit(&e,2,9,2030);assert(e.pressed[2] && e.next_wave==2);light_engine_input(&e,2,1,2040);assert(e.next_wave==2);
    light_engine_input(&e,2,0,2050);light_engine_sync_physical_hit(&e,2,10,2060,true);light_engine_input(&e,2,1,2080);assert(e.hit_count==10 && e.next_wave==3);
    p.flags=FLAG_DEPTH|FLAG_SWEEP_TURN;assert(!light_profile_valid(&p));p.flags=0;
    wire_message_t m={.type=MSG_PROFILE,.len=8,.session=0x12345678,.seq=42};wire_pack_profile(m.payload,2,&p);
    uint8_t wire[WIRE_MAX];size_t n=wire_encode(&m,wire);assert(n==28);
    wire_message_t decoded;assert(wire_decode(wire,n,&decoded));assert(decoded.session==m.session && decoded.seq==42 && decoded.len==8);
    uint8_t layer;light_profile_t out;assert(wire_unpack_profile(decoded.payload,decoded.len,&layer,&out));assert(layer==2 && out.effect==EFFECT_RIPPLE);
    wire[17]^=1;assert(!wire_decode(wire,n,&decoded));wire[17]^=1;assert(!wire_decode(wire,n-1,&decoded));
    puts("lighting and wire protocol tests passed");return 0;
}
