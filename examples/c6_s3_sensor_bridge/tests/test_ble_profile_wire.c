#include "ble_profile_wire.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    uint8_t packet[WHE_BLE_PROFILE_WIRE_MAX],chunk[WHE_BLE_PROFILE_CHUNK]={0};whe_ble_profile_message_t m;
    size_t n=whe_ble_profile_pack(packet,WHE_BLE_PROFILE_BEGIN,0x12345678,0,NULL,0);
    assert(n==12&&whe_ble_profile_unpack(packet,n,&m)&&m.transaction==0x12345678&&m.op==WHE_BLE_PROFILE_BEGIN);
    n=whe_ble_profile_pack(packet,WHE_BLE_PROFILE_DATA,5,15,chunk,sizeof(chunk));
    assert(n==WHE_BLE_PROFILE_WIRE_MAX&&whe_ble_profile_unpack(packet,n,&m)&&m.index==15&&m.payload_size==sizeof(chunk));
    packet[20]^=1;assert(!whe_ble_profile_unpack(packet,n,&m));
    assert(!whe_ble_profile_pack(packet,WHE_BLE_PROFILE_DATA,5,16,chunk,sizeof(chunk)));
    assert(!whe_ble_profile_pack(packet,WHE_BLE_PROFILE_COMMIT,0,0,chunk,2));
    puts("BLE profile wire framing: ok");
}
