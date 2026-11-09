// SPDX-License-Identifier: GPL-3.0-only
#include "power_policy.h"
#include <stdlib.h>
#include <string.h>
#define IDLE_AFTER_US 2000000LL
void whe_power_policy_init(whe_power_policy_t *p){if(p){memset(p,0,sizeof(*p));p->wake_delta=20;p->release_delta=10;}}
bool whe_power_policy_configure(whe_power_policy_t *p,uint16_t wake,uint16_t release){
    if(!p || wake<2 || wake>4095 || release>=wake)return false;
    p->wake_delta=wake;p->release_delta=release;return true;
}
bool whe_power_policy_update(whe_power_policy_t *p,const uint16_t v[WHE_POWER_CHANNELS],int64_t now) {
    if(!p || !v)return false;
    if(!p->initialized){memcpy(p->baseline,v,sizeof(p->baseline));p->initialized=true;return false;}
    bool wake=false,released=true;
    for(unsigned i=0;i<WHE_POWER_CHANNELS;i++) {
        int delta=abs((int)v[i]-(int)p->baseline[i]);
        if(delta>=p->wake_delta)wake=true;
        if(delta>p->release_delta)released=false;
    }
    if(!p->active){if(wake){p->active=true;p->release_timing=false;}}
    else if(!released){p->release_timing=false;}
    else if(!p->release_timing){p->released_since_us=now;p->release_timing=true;}
    else if(now>=p->released_since_us && now-p->released_since_us>=IDLE_AFTER_US){p->active=false;p->release_timing=false;}
    return p->active;
}
bool whe_tx_policy_should_send(whe_tx_policy_t *p,bool was_active,bool is_active,bool app_open,int64_t now){
    if(!p)return false;
    bool send=was_active || is_active || app_open || !p->initialized || now<p->last_attempt_us || now-p->last_attempt_us>=100000;
    if(send){p->initialized=true;p->last_attempt_us=now;}
    return send;
}
