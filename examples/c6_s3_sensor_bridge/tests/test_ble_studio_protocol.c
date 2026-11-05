#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ble_studio_protocol.h"
static void seal(uint8_t *p){uint16_t crc=whe_sensor_crc(p,13);p[13]=crc;p[14]=crc>>8;}
int main(void){
 uint8_t p[15]={'W',1,WHE_BLE_STUDIO_LIGHT_PROFILE,2,1,0,1,12,34,56,60,50,0};
 whe_ble_studio_command_t c;seal(p);assert(whe_ble_studio_decode(p,sizeof p,&c));assert(c.layer==2&&c.profile[2]==34);
 p[6]^=1;assert(!whe_ble_studio_decode(p,sizeof p,&c));p[6]^=1;
 p[3]=4;seal(p);assert(!whe_ble_studio_decode(p,sizeof p,&c));p[3]=1;
 p[12]=2;seal(p);assert(!whe_ble_studio_decode(p,sizeof p,&c));p[12]=0;
 memset(p+6,0,7);p[2]=WHE_BLE_STUDIO_SELECT_LAYER;seal(p);assert(whe_ble_studio_decode(p,sizeof p,&c));
 p[6]=1;seal(p);assert(!whe_ble_studio_decode(p,sizeof p,&c));
 puts("BLE Studio protocol validation passed");
}
