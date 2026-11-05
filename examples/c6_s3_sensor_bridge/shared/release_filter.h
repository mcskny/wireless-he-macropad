#pragma once
#include <stdint.h>
#include <stdbool.h>
#define WHE_RELEASE_FILTER_CHANNELS 12
typedef struct {int32_t q8[WHE_RELEASE_FILTER_CHANNELS];bool initialized;} whe_release_filter_t;
/* Only the idle-return decision is filtered; wake and live sensor data stay raw. */
static inline void whe_release_filter_update(whe_release_filter_t *f,const uint16_t in[12],uint16_t out[12],bool active){
 for(unsigned i=0;i<12;i++){
  int32_t target=(int32_t)in[i]*256;
  if(!f->initialized||!active)f->q8[i]=target;
  else f->q8[i]+=(target-f->q8[i])/32;
  out[i]=active?(uint16_t)((f->q8[i]+128)/256):in[i];
 }
 f->initialized=true;
}
