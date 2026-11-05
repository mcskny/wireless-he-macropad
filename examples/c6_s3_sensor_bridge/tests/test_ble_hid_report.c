#include "ble_hid_report.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    whe_hid_report_t report={.modifier=2,.keys={4,5,0,0,0,0},.consumer=0x00e9};
    uint8_t k[WHE_BLE_KEYBOARD_REPORT_LEN],c[WHE_BLE_CONSUMER_REPORT_LEN];
    whe_ble_encode_reports(&report,true,k,c);
    assert(k[0]==2 && k[1]==0 && k[2]==4 && k[3]==5 && k[7]==0);
    assert(c[0]==0xe9 && c[1]==0);
    whe_ble_encode_reports(&report,false,k,c);
    for(unsigned i=0;i<sizeof(k);i++)assert(k[i]==0);
    for(unsigned i=0;i<sizeof(c);i++)assert(c[i]==0);
    puts("BLE HID report encoding: ok");
}
