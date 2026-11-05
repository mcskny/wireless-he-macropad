#include "sensor_packet.h"
#include <assert.h>
#include <stdio.h>
int main(void){uint16_t input[12]={0,1,2,3,4,5,6,7,8,9,4094,4095},out[12]={0};uint8_t frame[WHE_SENSOR_FRAME_SIZE];uint32_t seq=0;
 whe_sensor_encode(frame,0xfedcba98,input);assert(whe_sensor_decode(frame,sizeof(frame),&seq,out));assert(seq==0xfedcba98);for(int i=0;i<12;i++)assert(out[i]==input[i]);
 for(size_t i=0;i<sizeof(frame);i++){frame[i]^=1;assert(!whe_sensor_decode(frame,sizeof(frame),&seq,out));frame[i]^=1;}
 for(size_t n=0;n<sizeof(frame);n++)assert(!whe_sensor_decode(frame,n,&seq,out));
 input[0]=4096;whe_sensor_encode(frame,5,input);seq=0xabc;out[0]=77;assert(!whe_sensor_decode(frame,sizeof(frame),&seq,out));assert(seq==0xabc && out[0]==77);
 assert(!whe_sensor_decode(NULL,sizeof(frame),&seq,out));assert(!whe_sensor_decode(frame,sizeof(frame),NULL,out));assert(!whe_sensor_decode(frame,sizeof(frame),&seq,NULL));
 input[0]=0;whe_sensor_encode(frame,UINT32_MAX,input);assert(whe_sensor_decode(frame,sizeof(frame),&seq,out) && seq==UINT32_MAX);puts("sensor packet: ok");}
