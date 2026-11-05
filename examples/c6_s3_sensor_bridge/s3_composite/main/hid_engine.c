#include "hid_engine.h"
#include <stdlib.h>
#include <string.h>
void whe_hid_reset(whe_hid_engine_t *e){memset(e,0,sizeof(*e));e->socd_last=-1;}
bool whe_hid_set(whe_hid_engine_t *e,unsigned channel,unsigned released,unsigned pressed,unsigned usage,unsigned point,unsigned hysteresis){
    return whe_hid_set_full(e,channel,released,pressed,usage,0,point,hysteresis,0,0,0);
}
static bool valid_usage(unsigned usage){return (usage>=4 && usage<=0x65)||(usage>=0xe0 && usage<=0xe7)||(usage>=0xf0 && usage<=0xfa);}
bool whe_hid_set_full(whe_hid_engine_t *e,unsigned channel,unsigned released,unsigned pressed,unsigned usage,unsigned modifiers,unsigned point,unsigned hysteresis,unsigned usage2,unsigned modifiers2,unsigned point2){
    if(e->enabled || channel>=WHE_HID_KEYS || released>4095 || pressed>4095 || abs((int)released-(int)pressed)<30 ||
       !valid_usage(usage) || modifiers>255 || (usage>=0xf0 && modifiers) || point<1 || point>100 || hysteresis>=point || (!usage2 && (modifiers2||point2)) || (usage2 && (!valid_usage(usage2)||modifiers2>255||(usage2>=0xf0 && modifiers2)||point2<=point||point2>100))) return false;
    e->keys[channel]=(whe_hid_key_t){.released=released,.pressed=pressed,.usage=usage,.modifiers=modifiers,.point=point,.hysteresis=hysteresis,.usage2=usage2,.modifiers2=modifiers2,.point2=point2,.configured=true};
    return true;
}
bool whe_hid_enable(whe_hid_engine_t *e,bool on){
    if(on){bool any=false;for(unsigned i=0;i<WHE_HID_KEYS;i++)any|=e->keys[i].configured;if(!any)return false;}
    e->enabled=on;e->has_sample=false;
    for(unsigned i=0;i<WHE_HID_KEYS;i++){e->keys[i].active=false;e->keys[i].active2=false;}
    return true;
}
void whe_hid_sample(whe_hid_engine_t *e,const uint16_t values[WHE_HID_KEYS],int64_t now_us){
    if(!e->enabled)return;
    bool prev_a=e->socd_enabled && e->keys[e->socd_a].active;
    bool prev_b=e->socd_enabled && e->keys[e->socd_b].active;
    for(unsigned i=0;i<WHE_HID_KEYS;i++){
        whe_hid_key_t *k=&e->keys[i];if(!k->configured)continue;
        float p=((float)values[i]-k->released)/((float)k->pressed-k->released)*100.0f;
        if(p<0)p=0;
        if(p>100)p=100;
        if(k->rapid){
            if(p<=k->rapid_min){k->active=false;k->highest=p;k->lowest=p;}
            else if(p>=k->rapid_max){k->active=true;k->highest=p;k->lowest=p;}
            else if(k->active){if(p>k->highest)k->highest=p;if(p<k->highest-k->rapid_release){k->active=false;k->lowest=p;}}
            else {if(p<k->lowest)k->lowest=p;if(p>k->lowest+k->rapid_press){k->active=true;k->highest=p;}}
        }else if(!k->active && p>=k->point)k->active=true;
        else if(k->active && p<(k->point-k->hysteresis))k->active=false;
        if(k->usage2){if(!k->active2 && p>=k->point2)k->active2=true;else if(k->active2 && p<(k->point2-k->hysteresis))k->active2=false;}
    }
    if(e->socd_enabled){bool a=e->keys[e->socd_a].active,b=e->keys[e->socd_b].active;
        bool new_a=a&&!prev_a,new_b=b&&!prev_b;
        if(new_a && new_b)e->socd_last=-1;else if(new_a)e->socd_last=(int8_t)e->socd_a;else if(new_b)e->socd_last=(int8_t)e->socd_b;
    }
    e->last_valid_us=now_us;e->has_sample=true;
}
whe_hid_report_t whe_hid_report(whe_hid_engine_t *e,int64_t now_us,bool host_ready){
    whe_hid_report_t r={0};
    if(!host_ready || !e->enabled || !e->has_sample || now_us<e->last_valid_us || now_us-e->last_valid_us>WHE_HID_TIMEOUT_US){
        for(unsigned i=0;i<WHE_HID_KEYS;i++){e->keys[i].active=false;e->keys[i].active2=false;}
        e->has_sample=false;e->socd_last=-1;return r;
    }
    unsigned count=0;bool overflow=false;
    for(unsigned i=0;i<WHE_HID_KEYS;i++){
        whe_hid_key_t *k=&e->keys[i];if(!k->configured)continue;
        if(e->socd_enabled && e->keys[e->socd_a].active && e->keys[e->socd_b].active &&
           (i==e->socd_a || i==e->socd_b) && e->socd_last!=(int8_t)i)continue;
        for(unsigned stage=0;stage<2;stage++){
            if(!(stage?k->active2:k->active))continue;
            unsigned usage=stage?k->usage2:k->usage;r.modifier|=stage?k->modifiers2:k->modifiers;
            if(usage>=0xf0){static const uint16_t consumer[]={0x00cd,0x00b6,0x00b5,0x00e9,0x00ea,0x00e2,0x006f,0x0070,0x00b7,0x00b3,0x00b4};if(!r.consumer)r.consumer=consumer[usage-0xf0];continue;}
            if(usage>=0xe0){r.modifier|=1u<<(usage-0xe0);continue;}
            bool exists=false;for(unsigned j=0;j<count;j++)if(r.keys[j]==usage)exists=true;
            if(exists)continue;
            if(count==6){overflow=true;break;}r.keys[count++]=usage;
        }
        if(overflow)break;
    }
    if(overflow)memset(r.keys,1,sizeof(r.keys));
    return r;
}
