#include "sensor_v2.h"
#include <assert.h>
#include <stdio.h>
int main(void){uint16_t a[12],b[12];for(int i=0;i<12;i++)a[i]=100+i;
 whe_sensor_status_t s={.active=true,.app_open=true,.stats_valid=true,.gate_enabled=false,.layer=3,.layer_pressed=true,.epoch=123456,.scans=980,.submitted=970,.skipped=20,.replaced=3,.interval_ms=1001},z;
 uint8_t frame[WHE_SENSOR_V2_SIZE];uint32_t seq;whe_sensor_v2_encode(frame,123,a,&s);
 assert(whe_sensor_v2_decode(frame,sizeof(frame),&seq,b,&z));assert(seq==123&&b[11]==111&&z.active&&z.app_open&&z.layer==3&&z.layer_pressed&&z.epoch==123456&&z.scans==980&&z.interval_ms==1001);
 frame[20]^=1;assert(!whe_sensor_v2_decode(frame,sizeof(frame),&seq,b,&z));assert(!whe_sensor_v2_decode(frame,sizeof(frame)-1,&seq,b,&z));puts("sensor v2: ok");}
