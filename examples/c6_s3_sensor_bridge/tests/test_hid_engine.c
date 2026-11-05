#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hid_engine.h"
static bool empty(whe_hid_report_t r){whe_hid_report_t zero={0};return memcmp(&r,&zero,sizeof(r))==0;}
int main(void){
    whe_hid_engine_t e;whe_hid_reset(&e);uint16_t v[12];for(int i=0;i<12;i++)v[i]=1900;
    assert(!whe_hid_enable(&e,true));assert(!whe_hid_set(&e,1,1900,1890,4,74,5));
    assert(whe_hid_set(&e,1,1900,0,4,74,5));assert(whe_hid_enable(&e,true));
    assert(empty(whe_hid_report(&e,100,true)));whe_hid_sample(&e,v,1000);assert(empty(whe_hid_report(&e,1000,true)));
    v[1]=400;whe_hid_sample(&e,v,2000);assert(whe_hid_report(&e,2000,true).keys[0]==4);
    v[1]=500;whe_hid_sample(&e,v,3000);assert(whe_hid_report(&e,3000,true).keys[0]==4);
    v[1]=1000;whe_hid_sample(&e,v,4000);assert(empty(whe_hid_report(&e,4000,true)));
    v[1]=0;whe_hid_sample(&e,v,5000);assert(empty(whe_hid_report(&e,155001,true)));
    whe_hid_sample(&e,v,160000);assert(empty(whe_hid_report(&e,160000,false)));
    assert(!whe_hid_set(&e,2,1900,0,5,74,5));assert(whe_hid_enable(&e,false));
    for(unsigned i=0;i<7;i++)assert(whe_hid_set(&e,i,1900,0,4+i,50,5));
    assert(whe_hid_enable(&e,true));for(int i=0;i<7;i++)v[i]=0;whe_hid_sample(&e,v,200000);
    whe_hid_report_t r=whe_hid_report(&e,200000,true);for(int i=0;i<6;i++)assert(r.keys[i]==1);
    assert(empty(whe_hid_report(&e,199999,true)));puts("HID engine: ok");
}
