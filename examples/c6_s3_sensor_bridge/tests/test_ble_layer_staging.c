#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ble_layer_staging.h"
int main(void){
 uint8_t original[WHE_PROFILE_BYTES]={0},layer[WHE_BLE_LAYER_BYTES]={0};
 /* Valid key record in one of the untouched layers. */
 original[0]=0x70;original[1]=0x07;original[2]=0x20;original[3]=0x03;original[4]=4;original[6]=50;original[7]=5;
 assert(whe_profile_validate(original));
 layer[0]=0x70;layer[1]=0x07;layer[2]=0x20;layer[3]=0x03;layer[4]=5;layer[6]=50;layer[7]=5;
 whe_ble_layer_staging_t s={0};assert(whe_ble_layer_begin(&s,original,2,12));
 for(unsigned i=0;i<WHE_BLE_LAYER_CHUNKS;i++)assert(whe_ble_layer_chunk(&s,12,i,layer+i*8,8));
 assert(whe_ble_layer_ready(&s,12,whe_sensor_crc(layer,sizeof layer)));
 assert(!memcmp(s.candidate,original,WHE_BLE_LAYER_BYTES));
 assert(!memcmp(original+2*WHE_BLE_LAYER_BYTES,(uint8_t[WHE_BLE_LAYER_BYTES]){0},WHE_BLE_LAYER_BYTES));
 assert(!whe_ble_layer_ready(&s,12,0));
 assert(whe_ble_layer_begin(&s,original,2,13));
 assert(whe_ble_layer_chunk(&s,13,0,layer,8));layer[4]=6;assert(!whe_ble_layer_chunk(&s,13,0,layer,8));assert(!s.active);
 puts("BLE layer staging: ok");
}
