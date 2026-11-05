#include "hid_profile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void record(uint8_t *p,uint8_t key,uint8_t modifier,uint8_t second){
 p[0]=0x6c;p[1]=7; /* released 1900 */
 p[2]=0;p[3]=0;p[4]=key;p[5]=modifier;p[6]=40;p[7]=5;
 p[8]=second;p[9]=0;p[10]=second?75:0;p[11]=0;
}
int main(void){
 uint8_t blob[WHE_PROFILE_BYTES]={0};whe_hid_engine_t engine;uint16_t values[12];
 for(unsigned i=0;i<12;i++)values[i]=1900;
 record(blob+1*WHE_PROFILE_RECORD_SIZE,4,2,5);
 record(blob+(12+1)*WHE_PROFILE_RECORD_SIZE,6,0,0);
 assert(whe_profile_validate(blob));assert(whe_profile_load_layer(blob,0,&engine));
 values[1]=1000;whe_hid_sample(&engine,values,500);whe_hid_report_t first=whe_hid_report(&engine,500,true);
 assert(first.modifier==2 && first.keys[0]==4 && first.keys[1]==0);
 values[1]=0;whe_hid_sample(&engine,values,1000);whe_hid_report_t report=whe_hid_report(&engine,1000,true);
 assert(report.modifier==2 && report.keys[0]==4 && report.keys[1]==5);
 values[1]=1000;whe_hid_sample(&engine,values,1500);report=whe_hid_report(&engine,1500,true);assert(report.keys[0]==4 && report.keys[1]==0);
 values[1]=1900;whe_hid_sample(&engine,values,1600);report=whe_hid_report(&engine,1600,true);assert(report.modifier==0 && !report.keys[0]);
 values[1]=0;
 assert(whe_profile_load_layer(blob,1,&engine));assert(!engine.has_sample);whe_hid_sample(&engine,values,2000);
 report=whe_hid_report(&engine,2000,true);assert(report.modifier==0 && report.keys[0]==6 && report.keys[1]==0);
 assert(whe_profile_load_layer(blob,2,&engine)&&!engine.enabled);
 record(blob+(2*12+1)*WHE_PROFILE_RECORD_SIZE,0xf0,0,0);
 assert(whe_profile_validate(blob)&&whe_profile_load_layer(blob,2,&engine));
 whe_hid_sample(&engine,values,3000);report=whe_hid_report(&engine,3000,true);assert(report.consumer==0x00cd && report.keys[0]==0);
 values[1]=1900;whe_hid_sample(&engine,values,4000);assert(whe_hid_report(&engine,4000,true).consumer==0);
 blob[(2*12+1)*WHE_PROFILE_RECORD_SIZE+5]=1;assert(!whe_profile_validate(blob));blob[(2*12+1)*WHE_PROFILE_RECORD_SIZE+5]=0;
 blob[11]=1;assert(!whe_profile_validate(blob));blob[11]=0;
 memset(blob,0,sizeof(blob));record(blob,4,0,0);record(blob+WHE_PROFILE_RECORD_SIZE,5,0,0);
 blob[11]=0x81;blob[WHE_PROFILE_RECORD_SIZE+11]=0x80;
 assert(whe_profile_validate(blob));assert(whe_profile_load_layer(blob,0,&engine));
 for(unsigned i=0;i<12;i++)values[i]=1900;
 values[0]=0;whe_hid_sample(&engine,values,5000);report=whe_hid_report(&engine,5000,true);assert(report.keys[0]==4 && !report.keys[1]);
 values[1]=0;whe_hid_sample(&engine,values,6000);report=whe_hid_report(&engine,6000,true);assert(report.keys[0]==5 && !report.keys[1]);
 values[1]=1900;whe_hid_sample(&engine,values,7000);report=whe_hid_report(&engine,7000,true);assert(report.keys[0]==4 && !report.keys[1]);
 whe_profile_load_layer(blob,0,&engine);values[1]=0;whe_hid_sample(&engine,values,8000);report=whe_hid_report(&engine,8000,true);assert(!report.keys[0]);
 blob[WHE_PROFILE_RECORD_SIZE+11]=0;assert(!whe_profile_validate(blob));
 uint8_t old[WHE_PROFILE_V1_BYTES]={0};old[0]=0x6c;old[1]=7;old[4]=4;old[6]=40;old[7]=5;
 assert(whe_profile_migrate_v1(blob,old));assert(whe_profile_load_layer(blob,0,&engine));assert(engine.keys[0].usage==4 && !engine.keys[0].rapid);
 blob[11]=0x40;blob[12]=5;blob[13]=5;blob[14]=5;blob[15]=95;
 assert(whe_profile_validate(blob));assert(whe_profile_load_layer(blob,0,&engine));
 for(unsigned i=0;i<12;i++)values[i]=1900;
 whe_hid_sample(&engine,values,9000);assert(!whe_hid_report(&engine,9000,true).keys[0]);
 values[0]=1400;whe_hid_sample(&engine,values,10000);assert(whe_hid_report(&engine,10000,true).keys[0]==4);
 values[0]=1000;whe_hid_sample(&engine,values,11000);assert(whe_hid_report(&engine,11000,true).keys[0]==4);
 values[0]=1120;whe_hid_sample(&engine,values,12000);assert(!whe_hid_report(&engine,12000,true).keys[0]);
 values[0]=1000;whe_hid_sample(&engine,values,13000);assert(whe_hid_report(&engine,13000,true).keys[0]==4);
 values[0]=1900;whe_hid_sample(&engine,values,14000);assert(!whe_hid_report(&engine,14000,true).keys[0]);
 puts("four-layer HID profile: validation, modifiers, two stages and switching ok");
}
