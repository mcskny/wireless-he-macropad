#include "hid_profile.h"
#include <string.h>
bool whe_profile_migrate_v1(uint8_t *out,const uint8_t *old){
 if(!out||!old)return false;
 memset(out,0,WHE_PROFILE_BYTES);
 for(unsigned i=0;i<WHE_PROFILE_LAYERS*WHE_HID_KEYS;i++)memcpy(out+i*WHE_PROFILE_RECORD_SIZE,old+i*WHE_PROFILE_V1_RECORD_SIZE,WHE_PROFILE_V1_RECORD_SIZE);
 return whe_profile_validate(out);
}
bool whe_profile_load_layer(const uint8_t *data,unsigned layer,whe_hid_engine_t *engine){
 if(!data||!engine||layer>=WHE_PROFILE_LAYERS)return false;
 whe_hid_reset(engine);bool any=false;
 for(unsigned i=0;i<WHE_HID_KEYS;i++){
  const uint8_t *p=data+(layer*WHE_HID_KEYS+i)*WHE_PROFILE_RECORD_SIZE;
  if(p[11]&0x30)return false;
  if((p[11]&0x80) && ((p[11]&0x0f)>=WHE_HID_KEYS || (p[11]&0x0f)==i))return false;
  if(!(p[11]&0x80) && (p[11]&0x0f))return false;
  if(!p[4]){for(unsigned j=0;j<WHE_PROFILE_RECORD_SIZE;j++)if(p[j])return false;continue;}
  unsigned free_value=p[0]|((unsigned)p[1]<<8),pressed_value=p[2]|((unsigned)p[3]<<8);
  if(!whe_hid_set_full(engine,i,free_value,pressed_value,p[4],p[5],p[6],p[7],p[8],p[9],p[10]))return false;
  if(p[11]&0x40){if(!p[12]||!p[13]||p[12]>100||p[13]>100||p[14]>=p[15]||p[15]>100)return false;
   whe_hid_key_t *k=&engine->keys[i];k->rapid=true;k->rapid_press=p[12];k->rapid_release=p[13];k->rapid_min=p[14];k->rapid_max=p[15];
  }else if(p[12]||p[13]||p[14]||p[15])return false;
  any=true;
 }
 for(unsigned i=0;i<WHE_HID_KEYS;i++){
  const uint8_t *p=data+(layer*WHE_HID_KEYS+i)*WHE_PROFILE_RECORD_SIZE;
  if(p[11]&0x80){unsigned other=p[11]&0x0f;const uint8_t *q=data+(layer*WHE_HID_KEYS+other)*WHE_PROFILE_RECORD_SIZE;
   if(!p[4] || !q[4] || (q[11]&0x8f)!=(0x80|i) || (engine->socd_enabled && (engine->socd_a!=other || engine->socd_b!=i)))return false;
   if(!engine->socd_enabled){engine->socd_enabled=true;engine->socd_a=i;engine->socd_b=other;}
  }
 }
 if(any)return whe_hid_enable(engine,true);
 return true;
}
bool whe_profile_validate(const uint8_t *data){
 if(!data)return false;
 whe_hid_engine_t engine;
 for(unsigned i=0;i<WHE_PROFILE_LAYERS;i++)if(!whe_profile_load_layer(data,i,&engine))return false;
 return true;
}
