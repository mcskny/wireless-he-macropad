#include "disconnected_warning.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    whe_disconnected_warning_t w={0};bool red;
    assert(!whe_receiver_present(0,100000));
    assert(whe_receiver_present(1000000,2400000));
    assert(!whe_receiver_present(1000000,2600000));
    assert(!whe_disconnected_warning_step(&w,true,true,0,&red));
    assert(whe_disconnected_warning_step(&w,false,true,1000000,&red) && red);
    assert(whe_disconnected_warning_step(&w,false,false,1100000,&red) && !red);
    assert(whe_disconnected_warning_step(&w,false,false,1200000,&red) && red);
    assert(whe_disconnected_warning_step(&w,false,false,1300000,&red) && !red);
    assert(!whe_disconnected_warning_step(&w,false,false,1400000,&red));
    assert(whe_disconnected_warning_step(&w,false,true,1500000,&red) && red);
    assert(whe_disconnected_warning_step(&w,false,true,1550000,&red) && red);
    assert(whe_disconnected_warning_step(&w,false,false,1900000,&red) && red);
    assert(!whe_disconnected_warning_step(&w,true,false,1950000,&red));
    assert(!whe_disconnected_warning_step(&w,false,false,2000000,&red));
    puts("disconnected receiver warning: ok");
}
