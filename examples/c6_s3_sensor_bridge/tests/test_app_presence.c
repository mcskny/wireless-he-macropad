#include "app_presence.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 uint8_t frame[WHE_APP_FRAME_SIZE];bool open=false;
 whe_app_encode(frame,true);assert(whe_app_decode(frame,sizeof(frame),&open)&&open);
 frame[3]^=1;assert(!whe_app_decode(frame,sizeof(frame),&open));frame[3]^=1;
 assert(!whe_app_decode(frame,sizeof(frame)-1,&open));frame[5]=2;assert(!whe_app_decode(frame,sizeof(frame),&open));
 whe_app_encode(frame,false);assert(whe_app_decode(frame,sizeof(frame),&open)&&!open);
 assert(whe_app_present(true,100,1500100));assert(!whe_app_present(true,100,1500101));
 assert(!whe_app_present(true,100,99));assert(!whe_app_present(false,100,100));
 puts("app presence: ok");
}
