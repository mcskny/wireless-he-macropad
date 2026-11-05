#include "sensor_v3.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 uint16_t in[12]={0},out[12];uint32_t seq,on;whe_sensor_status_t source={.stats_valid=true,.interval_ms=1000,.layer=2},result;
 uint8_t frame[WHE_SENSOR_V3_SIZE];whe_sensor_v3_encode(frame,42,in,&source,117000);
 assert(whe_sensor_v3_decode(frame,sizeof(frame),&seq,out,&result,&on)&&seq==42&&on==117000&&result.layer==2);
 frame[48]^=1;assert(!whe_sensor_v3_decode(frame,sizeof(frame),&seq,out,&result,&on));
 whe_sensor_v3_encode(frame,42,in,&source,1003000);assert(!whe_sensor_v3_decode(frame,sizeof(frame),&seq,out,&result,&on));
 puts("sensor v3 duty telemetry: ok");
}
