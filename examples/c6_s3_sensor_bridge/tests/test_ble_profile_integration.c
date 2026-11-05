#include "ble_profile_wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool receive(whe_ble_profile_staging_t *stage,const uint8_t *data,size_t len){
    whe_ble_profile_message_t msg;
    if(!whe_ble_profile_unpack(data,len,&msg))return false;
    if(msg.op==WHE_BLE_PROFILE_BEGIN)return whe_ble_profile_begin(stage,msg.transaction);
    if(msg.op==WHE_BLE_PROFILE_DATA)return whe_ble_profile_chunk(stage,msg.transaction,msg.index,msg.payload,msg.payload_size);
    return whe_ble_profile_ready(stage,msg.transaction,(uint16_t)msg.payload[0]|((uint16_t)msg.payload[1]<<8));
}
int main(void){
    uint8_t original[WHE_PROFILE_BYTES]={0},packet[WHE_BLE_PROFILE_WIRE_MAX];
    whe_ble_profile_staging_t stage={0};whe_hid_engine_t engine;uint16_t readings[12];
    // First key: A, Rapid Trigger. Second key: D. Last-pressed SOCD pair.
    uint8_t *a=original,*d=original+WHE_PROFILE_RECORD_SIZE;
    for(unsigned i=0;i<2;i++){uint8_t *p=i?d:a;p[0]=0x6c;p[1]=7;p[4]=(uint8_t)(i?7:4);p[6]=40;p[7]=5;}
    a[11]=0xc1;a[12]=5;a[13]=5;a[14]=5;a[15]=95;d[11]=0x80;
    assert(whe_profile_validate(original));
    uint16_t crc=whe_sensor_crc(original,sizeof(original));uint8_t crc_bytes[2]={(uint8_t)crc,(uint8_t)(crc>>8)};
    size_t n=whe_ble_profile_pack(packet,WHE_BLE_PROFILE_BEGIN,123,0,NULL,0);assert(receive(&stage,packet,n));
    for(unsigned i=0;i<WHE_BLE_PROFILE_CHUNKS;i++){
        n=whe_ble_profile_pack(packet,WHE_BLE_PROFILE_DATA,123,i,original+i*WHE_BLE_PROFILE_CHUNK,WHE_BLE_PROFILE_CHUNK);
        assert(receive(&stage,packet,n));
    }
    n=whe_ble_profile_pack(packet,WHE_BLE_PROFILE_COMMIT,123,0,crc_bytes,2);assert(receive(&stage,packet,n));
    assert(!memcmp(stage.data,original,sizeof(original)) && whe_profile_load_layer(stage.data,0,&engine));
    for(unsigned i=0;i<12;i++)readings[i]=1900;
    whe_hid_sample(&engine,readings,1000);readings[0]=1000;whe_hid_sample(&engine,readings,2000);
    assert(whe_hid_report(&engine,2000,true).keys[0]==4);
    readings[1]=0;whe_hid_sample(&engine,readings,3000);assert(whe_hid_report(&engine,3000,true).keys[0]==7);
    readings[1]=1900;whe_hid_sample(&engine,readings,4000);assert(whe_hid_report(&engine,4000,true).keys[0]==4);
    readings[0]=1120;whe_hid_sample(&engine,readings,5000);assert(!whe_hid_report(&engine,5000,true).keys[0]);
    puts("BLE profile framing, staging and HID engine integration: ok");
}
