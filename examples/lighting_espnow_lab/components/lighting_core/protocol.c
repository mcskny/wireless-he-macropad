#include "protocol.h"
#include <string.h>
static uint32_t crc32(const uint8_t *p,size_t n) {uint32_t c=0xffffffffu;while(n--){c^=*p++;for(int i=0;i<8;i++) c=(c>>1)^(0xedb88320u & (0u-(c&1u)));}return ~c;}
static void put32(uint8_t *p,uint32_t n){for(int i=0;i<4;i++) p[i]=(uint8_t)(n>>(8*i));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
size_t wire_encode(const wire_message_t *m,uint8_t out[WIRE_MAX]) {
    if(!m || !out || m->len>sizeof(m->payload) || m->type<MSG_PROFILE || m->type>MSG_ACK) return 0;
    memcpy(out,"HELA",4);out[4]=1;out[5]=m->type;out[6]=m->len;out[7]=0;put32(out+8,m->session);put32(out+12,m->seq);memcpy(out+16,m->payload,m->len);put32(out+16+m->len,crc32(out,16+m->len));return 20+m->len;
}
bool wire_decode(const uint8_t *in,size_t n,wire_message_t *m) {
    if(!in || !m || n<20 || n>WIRE_MAX || memcmp(in,"HELA",4) || in[4]!=1 || in[5]<MSG_PROFILE || in[5]>MSG_ACK || in[7]!=0 || in[6]>sizeof(m->payload) || n!=(size_t)(20+in[6]) || crc32(in,n-4)!=get32(in+n-4)) return false;
    m->type=in[5];m->len=in[6];m->session=get32(in+8);m->seq=get32(in+12);memcpy(m->payload,in+16,m->len);return true;
}
void wire_pack_profile(uint8_t d[8],uint8_t layer,const light_profile_t *p){d[0]=layer;d[1]=p->effect;d[2]=p->r;d[3]=p->g;d[4]=p->b;d[5]=p->brightness;d[6]=p->speed;d[7]=p->flags;}
bool wire_unpack_profile(const uint8_t *d,size_t n,uint8_t *layer,light_profile_t *p){if(!d||n!=8||!layer||!p||d[0]>=4) return false;*layer=d[0];*p=(light_profile_t){d[1],d[2],d[3],d[4],d[5],d[6],d[7]};return light_profile_valid(p);}
