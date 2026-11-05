#include "transport_mode.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    whe_transport_mode_t s={0};whe_transport_action_t a;
    a=whe_transport_input(&s,true,false);assert(!a.layer_tap&&!a.switch_requested);
    a=whe_transport_input(&s,false,false);assert(a.layer_tap&&!a.switch_requested);
    whe_transport_input(&s,true,false);
    a=whe_transport_input(&s,true,true);assert(a.switch_requested&&a.target==WHE_TRANSPORT_BLE&&a.suppress_key);
    a=whe_transport_input(&s,true,true);assert(!a.switch_requested&&a.suppress_key);
    a=whe_transport_input(&s,false,true);assert(!a.layer_tap&&a.suppress_key);
    a=whe_transport_input(&s,false,false);assert(a.suppress_key);
    a=whe_transport_input(&s,false,false);assert(!a.suppress_key);
    // Failure to commit leaves the original mode intact.
    assert(s.mode==WHE_TRANSPORT_ESPNOW);whe_transport_commit(&s,WHE_TRANSPORT_BLE);
    whe_transport_input(&s,true,false);a=whe_transport_input(&s,true,true);
    assert(a.switch_requested&&a.target==WHE_TRANSPORT_ESPNOW);
    whe_transport_input(&s,false,false);whe_transport_commit(&s,WHE_TRANSPORT_ESPNOW);
    // Pressing Layer after KEY 1 was already down must not switch modes.
    whe_transport_input(&s,false,true);a=whe_transport_input(&s,true,true);assert(!a.switch_requested);
    puts("transport chord coordinator: ok");
}
