#include "ble_profile_staging.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    whe_ble_profile_staging_t stage;uint8_t profile[WHE_PROFILE_BYTES]={0};
    profile[0]=0x6c;profile[1]=7;profile[4]=4;profile[6]=40;profile[7]=5;
    const uint16_t crc=whe_sensor_crc(profile,sizeof(profile));
    assert(!whe_ble_profile_begin(&stage,0));assert(whe_ble_profile_begin(&stage,42));
    for(unsigned i=0;i<WHE_BLE_PROFILE_CHUNKS;i++){
        assert(whe_ble_profile_chunk(&stage,42,i,profile+i*WHE_BLE_PROFILE_CHUNK,WHE_BLE_PROFILE_CHUNK));
        assert(whe_ble_profile_chunk(&stage,42,i,profile+i*WHE_BLE_PROFILE_CHUNK,WHE_BLE_PROFILE_CHUNK));
        if(i<15)assert(!whe_ble_profile_ready(&stage,42,crc));
    }
    assert(!whe_ble_profile_ready(&stage,43,crc));assert(!whe_ble_profile_ready(&stage,42,crc+1));
    assert(whe_ble_profile_ready(&stage,42,crc));
    profile[0]^=1;assert(!whe_ble_profile_chunk(&stage,42,0,profile,WHE_BLE_PROFILE_CHUNK));
    assert(!whe_ble_profile_ready(&stage,42,crc));
    assert(whe_ble_profile_begin(&stage,43));whe_ble_profile_abort(&stage);
    assert(!whe_ble_profile_chunk(&stage,43,0,profile,WHE_BLE_PROFILE_CHUNK));
    puts("BLE profile staging: ok");
}
