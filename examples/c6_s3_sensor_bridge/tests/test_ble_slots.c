#include <assert.h>
#include <stdio.h>
#include "ble_slots.h"
int main(void){
 whe_ble_slot_chord_t s={0};
 assert(whe_ble_slot_chord_step(&s,false,1)==-1);
 assert(whe_ble_slot_chord_step(&s,true,1)==-1);
 assert(whe_ble_slot_chord_step(&s,true,0)==-1);
 assert(whe_ble_slot_chord_step(&s,true,2)==1);
 assert(whe_ble_slot_chord_step(&s,true,6)==-1);
 assert(whe_ble_slot_chord_step(&s,false,0)==-1);
 assert(whe_ble_slot_chord_step(&s,true,4)==2);
 assert(whe_ble_slot_chord_step(&s,false,0)==-1);
 assert(whe_ble_slot_chord_step(&s,true,3)==-1);
 assert(whe_ble_slot_channels[0]==9 && whe_ble_slot_channels[1]==10 && whe_ble_slot_channels[2]==11);
 puts("BLE slot chord tests passed");
}
