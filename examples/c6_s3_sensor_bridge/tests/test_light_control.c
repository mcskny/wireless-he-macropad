#include "light_control.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 uint8_t f[WHE_CONTROL_SIZE],op,value,layer;uint32_t id;
 whe_control_pack(f,1,3,3,123);assert(whe_control_unpack(f,sizeof f,&op,&value,&layer,&id)&&op==1&&value==3&&layer==3&&id==123);
 whe_control_pack(f,2,11,2,456);assert(whe_control_unpack(f,sizeof f,&op,&value,&layer,&id)&&op==2&&value==11&&id==456);
 whe_control_pack(f,3,4,1,457);assert(whe_control_unpack(f,sizeof f,&op,&value,&layer,&id)&&op==3&&value==4&&id==457);
 f[5]^=1;assert(!whe_control_unpack(f,sizeof f,&op,&value,&layer,&id));
 whe_control_pack(f,2,12,0,1);assert(!whe_control_unpack(f,sizeof f,&op,&value,&layer,&id));
 puts("light control: ok");
}
