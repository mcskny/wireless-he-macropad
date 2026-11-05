#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
static int keyboard_calls, consumer_calls;
static bool endpoint_busy, fail_keyboard;
static uint8_t last_modifier;
bool tud_mounted(void) { return true; }
bool tud_hid_keyboard_report(uint8_t id,uint8_t mod,const uint8_t *keys) {
    (void)id;(void)keys;keyboard_calls++;
    if (endpoint_busy || fail_keyboard) return false;
    last_modifier=mod;endpoint_busy=true;return true;
}
bool tud_hid_report(uint8_t id,const void *payload,uint16_t len) {
    (void)payload;(void)len;if(id==2)consumer_calls++;
    if(endpoint_busy)return false;endpoint_busy=true;return true;
}
#include "usb_hid.c"
int main(void) {
    bool keys[12]={0};uint16_t map[4][12]={{0}};
    for(int i=0;i<6;i++){keys[i]=true;map[0][i]=(uint16_t)(4+i);}
    keys[8]=true;map[0][8]=0xe1;
    keys[9]=true;map[0][9]=0x1e9;
    assert(usb_hid_send_keyboard(keys,(const uint16_t (*)[12])map,0)==0);
    assert(last_modifier==2 && keyboard_calls==1 && consumer_calls==1);
    endpoint_busy=false;
    assert(usb_hid_send_keyboard(keys,(const uint16_t (*)[12])map,0)==0);
    assert(keyboard_calls==1 && consumer_calls==2);
    endpoint_busy=false;fail_keyboard=true;
    assert(usb_hid_release_all()==0);
    assert(s_prev_modifier==2);
    endpoint_busy=false;fail_keyboard=false;
    assert(usb_hid_release_all()==0);
    assert(s_prev_modifier==0);
    return 0;
}
