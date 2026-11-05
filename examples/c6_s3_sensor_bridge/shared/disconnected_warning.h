// Non-blocking missing-receiver warning; times are monotonic microseconds.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define WHE_RECEIVER_TIMEOUT_US 1500000LL
#define WHE_WARNING_PHASE_US 150000LL
#define WHE_WARNING_PHASES 4
#define WHE_WARNING_MAX_PENDING 8

typedef struct { bool active; uint8_t pending; int64_t started_us; } whe_disconnected_warning_t;

static inline bool whe_receiver_present(int64_t last_us,int64_t now_us){
    return last_us>0 && now_us>=last_us && now_us-last_us<=WHE_RECEIVER_TIMEOUT_US;
}
/* Returns true while overriding normal lighting; red is false in the off phases. */
static inline bool whe_disconnected_warning_step(whe_disconnected_warning_t *s,bool receiver_present,
                                                 bool new_press,int64_t now_us,bool *red){
    if(!s || !red)return false;
    *red=false;
    if(receiver_present){s->active=false;s->pending=0;return false;}
    if(new_press && s->pending<WHE_WARNING_MAX_PENDING)++s->pending;
    if(s->active && (now_us<s->started_us || now_us-s->started_us>=WHE_WARNING_PHASE_US*WHE_WARNING_PHASES))s->active=false;
    if(!s->active && s->pending){--s->pending;s->active=true;s->started_us=now_us;}
    if(!s->active)return false;
    *red=((now_us-s->started_us)/WHE_WARNING_PHASE_US)%2==0;
    return true;
}
