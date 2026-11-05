#include "power_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void){whe_power_policy_t p;uint16_t v[12];for(int i=0;i<12;i++)v[i]=2000;whe_power_policy_init(&p);
 assert(!whe_power_policy_configure(&p,20,20));assert(!whe_power_policy_configure(&p,1,0));assert(whe_power_policy_configure(&p,80,40));
 assert(!whe_power_policy_update(&p,v,0));v[3]=1921;assert(!whe_power_policy_update(&p,v,10000));v[3]=1920;assert(whe_power_policy_update(&p,v,20000));
 v[3]=1960;assert(whe_power_policy_update(&p,v,30000));assert(whe_power_policy_update(&p,v,2029999));assert(!whe_power_policy_update(&p,v,2030000));
 v[8]=2100;assert(whe_power_policy_update(&p,v,2040000));v[8]=2000;assert(whe_power_policy_update(&p,v,2050000));v[8]=2041;assert(whe_power_policy_update(&p,v,4050000));
 v[8]=2000;assert(whe_power_policy_update(&p,v,4060000));assert(!whe_power_policy_update(&p,v,6060000));
 /* Long held press remains active; a release bounce restarts the timer. */
 v[0]=1800;assert(whe_power_policy_update(&p,v,7000000));assert(whe_power_policy_update(&p,v,17000000));
 v[0]=2000;assert(whe_power_policy_update(&p,v,18000000));v[0]=1940;assert(whe_power_policy_update(&p,v,19900000));
 v[0]=2000;assert(whe_power_policy_update(&p,v,20000000));assert(whe_power_policy_update(&p,v,21999999));assert(!whe_power_policy_update(&p,v,22000000));
 assert(!whe_power_policy_update(NULL,v,0));assert(!whe_power_policy_update(&p,NULL,0));
 whe_power_policy_init(&p);assert(!whe_power_policy_update(&p,v,0));v[0]=1981;assert(!whe_power_policy_update(&p,v,10000));v[0]=1980;assert(whe_power_policy_update(&p,v,20000));
 whe_tx_policy_t tx={0};assert(whe_tx_policy_should_send(&tx,false,false,false,0));assert(!whe_tx_policy_should_send(&tx,false,false,false,99999));assert(whe_tx_policy_should_send(&tx,false,false,false,100000));
 assert(!whe_tx_policy_should_send(&tx,false,false,false,110000));assert(whe_tx_policy_should_send(&tx,false,true,false,120000));
 assert(whe_tx_policy_should_send(&tx,true,true,false,121000));assert(whe_tx_policy_should_send(&tx,true,false,false,122000));assert(!whe_tx_policy_should_send(&tx,false,false,false,130000));assert(whe_tx_policy_should_send(&tx,false,false,false,222000));
 assert(!whe_tx_policy_should_send(NULL,false,false,false,0));
 whe_tx_policy_t live={0}; assert(whe_tx_policy_should_send(&live,false,false,true,0));
 assert(whe_tx_policy_should_send(&live,false,false,true,9999));
 assert(whe_tx_policy_should_send(&live,false,false,true,10000));
 assert(!whe_tx_policy_should_send(&live,false,false,false,109999));
 assert(whe_tx_policy_should_send(&live,false,false,false,110000));
 assert(whe_tx_policy_should_send(&live,false,true,false,111000));puts("power policy: ok");}
