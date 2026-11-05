#include "light_sync.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 uint8_t p[28]={1,2,3,4,50,20,0, 7,10,20,30,10,90,1, 5,0,255,1,100,50,5, 9,255,0,0,80,1,1},q[28],f[WHE_LIGHT_SIZE];
 uint64_t t=1234567890123ULL,result=0;whe_light_pack(f,p,t);
 assert(whe_light_unpack(f,sizeof f,q,&result)&&result==t&&!memcmp(p,q,28));
 f[10]^=1;assert(!whe_light_unpack(f,sizeof f,q,&result));
 p[0]=10;whe_light_pack(f,p,t);assert(!whe_light_unpack(f,sizeof f,q,&result));
 puts("lighting sync frame: ok");
}
