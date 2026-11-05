#include "release_filter.h"
#include "power_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 whe_release_filter_t f={0};whe_power_policy_t p;whe_power_policy_init(&p);assert(whe_power_policy_configure(&p,20,17));
 uint16_t v[12],filtered[12];for(int i=0;i<12;i++)v[i]=1900;
 whe_release_filter_update(&f,v,filtered,false);assert(!whe_power_policy_update(&p,filtered,0));
 v[0]=1800;whe_release_filter_update(&f,v,filtered,false);assert(whe_power_policy_update(&p,filtered,10000));
 for(int i=0;i<4000;i++){v[0]=1800;whe_release_filter_update(&f,v,filtered,true);assert(whe_power_policy_update(&p,filtered,11000+i*1000));}
 for(int i=0;i<2200;i++){v[0]=(i%20<2)?1930:1900;whe_release_filter_update(&f,v,filtered,true);whe_power_policy_update(&p,filtered,4011000+i*1000);}
 assert(!p.active);
 v[0]=1879;whe_release_filter_update(&f,v,filtered,false);assert(whe_power_policy_update(&p,filtered,6300000));
 puts("release filter: noise rejection, held key and unfiltered wake ok");
}
