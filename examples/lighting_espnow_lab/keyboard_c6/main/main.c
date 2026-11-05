#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "spi_bus.h"
#include "mcp3202.h"
#include "mux_cd74hc4067.h"
#include "lab_radio.h"
#include "neopixel.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/* Only this task owns animation state, NVS and LED output. */
typedef struct {uint8_t version,active;light_profile_t layers[4];} stored_t;
_Static_assert(sizeof(light_profile_t)==7 && sizeof(stored_t)==30,"NVS v1 layout changed");
typedef enum { INPUT_DEPTH, INPUT_SHOW, INPUT_RAW, INPUT_FREE, INPUT_FULL } input_kind_t;
typedef struct {uint8_t key,depth;input_kind_t kind;} input_t;
typedef struct {uint16_t free_values[12],full_values[12],free_mask,full_mask;} calibration_t;
static QueueHandle_t rx_queue,input_queue,ack_queue;
static nvs_handle_t store;
static stored_t state;
static light_engine_t engine;
static bool led_ready,radio_ready;
static uint16_t raw_values[12];
static calibration_t calibration;
/* This is a placeholder, not verified PCB LED chain mapping. */
static const uint8_t key_to_led[LIGHT_KEYS]={0,1,2,3,4,5,6,7,8,9,10,11};
static uint64_t millis(void){return (uint64_t)(esp_timer_get_time()/1000);}
static bool state_valid(const stored_t *s){if(s->version!=1||s->active>=4)return false;for(int i=0;i<4;i++)if(!light_profile_valid(&s->layers[i]))return false;return true;}
static esp_err_t save_state(const stored_t *s){esp_err_t e=nvs_set_blob(store,"profiles-v1",s,sizeof(*s));return e==ESP_OK?nvs_commit(store):e;}
static void load_state(void){
    state.version=1;state.active=0;
    for(int i=0;i<4;i++)state.layers[i]=(light_profile_t){EFFECT_STATIC,166,240,202,40,50,0};
    stored_t loaded;size_t n=sizeof(loaded);esp_err_t e=nvs_get_blob(store,"profiles-v1",&loaded,&n);
    if(e==ESP_OK && n==sizeof(loaded) && state_valid(&loaded)) state=loaded;
    else if(e!=ESP_ERR_NVS_NOT_FOUND)printf("No valid saved v1 profile (%s); RAM defaults used.\n",esp_err_to_name(e));
}
static void load_calibration(void){
    calibration_t tmp;size_t size=sizeof(tmp);
    if(nvs_get_blob(store,"cal-v1",&tmp,&size)==ESP_OK && size==sizeof(tmp))calibration=tmp;
}
static void capture_calibration(uint8_t key,bool full){
#if CONFIG_LAB_ANALOG_INPUT
    calibration_t next=calibration;
    if(full){next.full_values[key]=raw_values[key];next.full_mask|=(uint16_t)(1u<<key);}
    else {next.free_values[key]=raw_values[key];next.free_mask|=(uint16_t)(1u<<key);}
    esp_err_t e=nvs_set_blob(store,"cal-v1",&next,sizeof(next));if(e==ESP_OK)e=nvs_commit(store);
    if(e==ESP_OK){calibration=next;printf("CAL key=%u %s=%u\n",key,full?"full":"free",raw_values[key]);}
    else printf("Calibration save failed: %s\n",esp_err_to_name(e));
#else
    (void)key;(void)full;puts("Analog input disabled; calibration not captured.");
#endif
}
static void read_analog(void){
#if CONFIG_LAB_ANALOG_INPUT
    uint64_t now=millis();
    for(uint8_t i=0;i<12;i++){
        (void)mux_cd74hc4067_select(i);esp_rom_delay_us(5);
        (void)mcp3202_read_channel(CONFIG_LAB_ADC_CHANNEL);
        raw_values[i]=mcp3202_read_channel(CONFIG_LAB_ADC_CHANNEL);
        int free_value=calibration.free_values[i],full_value=calibration.full_values[i],difference=free_value-full_value;
        if((calibration.free_mask&(1u<<i)) && (calibration.full_mask&(1u<<i)) && (difference>=10 || difference<=-10)) {
            float depth=(float)(free_value-(int)raw_values[i])/(float)difference;
            light_engine_input(&engine,i,depth,now);
        }
    }
#endif
}
static void console_task(void *arg){
    (void)arg;char line[80],extra;unsigned key,depth;
    puts("Lab: p KEY DEPTH (simulated), raw, free KEY, full KEY, show");
    while(1){
        if(!fgets(line,sizeof(line),stdin)){clearerr(stdin);vTaskDelay(pdMS_TO_TICKS(30));continue;}
        input_t in={0};
        if(sscanf(line,"p %u %u %c",&key,&depth,&extra)==2 && key<12 && depth<=100){in.key=(uint8_t)key;in.depth=(uint8_t)depth;in.kind=INPUT_DEPTH;xQueueSend(input_queue,&in,0);}
        else if(sscanf(line,"free %u %c",&key,&extra)==1 && key<12){in.key=(uint8_t)key;in.kind=INPUT_FREE;xQueueSend(input_queue,&in,0);}
        else if(sscanf(line,"full %u %c",&key,&extra)==1 && key<12){in.key=(uint8_t)key;in.kind=INPUT_FULL;xQueueSend(input_queue,&in,0);}
        else if(!strncmp(line,"raw",3)){in.kind=INPUT_RAW;xQueueSend(input_queue,&in,0);}
        else if(!strncmp(line,"show",4)){in.kind=INPUT_SHOW;xQueueSend(input_queue,&in,0);}else puts("ERR: p KEY DEPTH | raw | free KEY | full KEY | show");
    }
}
static void ack(const wire_message_t *m,uint8_t status){wire_message_t a={.type=MSG_ACK,.len=1,.session=m->session,.seq=m->seq,.payload={status}};if(xQueueSend(ack_queue,&a,0)!=pdTRUE)puts("ACK queue full; sender may retry.");}
static void ack_task(void *arg){(void)arg;wire_message_t a;while(1){if(xQueueReceive(ack_queue,&a,portMAX_DELAY)==pdTRUE){esp_err_t e=radio_send(&a);if(e!=ESP_OK)printf("ACK send failed: %s\n",esp_err_to_name(e));}}}
static void handle(const radio_packet_t *packet){
    static bool have_last;static uint32_t last_session,last_seq;static uint8_t last_status;
    wire_message_t msg;if(!wire_decode(packet->bytes,packet->len,&msg) || msg.type==MSG_ACK)return;
    if(have_last && msg.session==last_session && msg.seq==last_seq){ack(&msg,last_status);return;}
    if(have_last && msg.session==last_session && (int32_t)(msg.seq-last_seq)<=0){ack(&msg,ACK_INVALID);return;}
    stored_t next=state;uint8_t status=ACK_INVALID;
    if(msg.type==MSG_PROFILE){uint8_t layer;light_profile_t p;if(wire_unpack_profile(msg.payload,msg.len,&layer,&p)){next.layers[layer]=p;status=ACK_OK;}}
    else if(msg.type==MSG_ACTIVE && msg.len==1 && msg.payload[0]<4){next.active=msg.payload[0];status=ACK_OK;}
    if(status==ACK_OK){
        esp_err_t e=save_state(&next);
        if(e!=ESP_OK){status=ACK_STORAGE;printf("Profile not applied; NVS: %s\n",esp_err_to_name(e));}
        else {bool affects_active=msg.type==MSG_ACTIVE || (msg.type==MSG_PROFILE && msg.payload[0]==next.active);state=next;if(affects_active)light_engine_set_profile(&engine,&state.layers[state.active]);printf("Applied: layer=%u effect=%u brightness=%u speed=%u flags=%u\n",state.active,state.layers[state.active].effect,state.layers[state.active].brightness,state.layers[state.active].speed,state.layers[state.active].flags);}
    }
    have_last=true;last_session=msg.session;last_seq=msg.seq;last_status=status;ack(&msg,status);
}
void app_main(void){
    setvbuf(stdout,NULL,_IONBF,0);setvbuf(stdin,NULL,_IONBF,0);
    esp_err_t e=nvs_flash_init();if(e!=ESP_OK){printf("NVS error %s (no automatic erase)\n",esp_err_to_name(e));return;}
    ESP_ERROR_CHECK(nvs_open("he_light_lab",NVS_READWRITE,&store));load_state();load_calibration();light_engine_init(&engine,&state.layers[state.active]);
    uint8_t own[6],peer[6];ESP_ERROR_CHECK(esp_read_mac(own,ESP_MAC_WIFI_STA));printf("C6 Wi-Fi STA MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",own[0],own[1],own[2],own[3],own[4],own[5]);
    rx_queue=xQueueCreate(24,sizeof(radio_packet_t));input_queue=xQueueCreate(24,sizeof(input_t));ack_queue=xQueueCreate(16,sizeof(wire_message_t));if(!rx_queue||!input_queue||!ack_queue)return;
    if(radio_parse_mac(CONFIG_LAB_PEER_MAC,peer)){e=radio_init(peer,CONFIG_LAB_WIFI_CHANNEL,rx_queue);radio_ready=e==ESP_OK;if(!radio_ready)printf("Radio error: %s\n",esp_err_to_name(e));}
    else puts("S3 MAC missing/invalid: set LAB_PEER_MAC in menuconfig.");
    if(radio_ready)xTaskCreate(ack_task,"lab_ack",3072,NULL,4,NULL);
#if CONFIG_LAB_LED_OUTPUT
#if CONFIG_LAB_BOOST_CONTROL
    gpio_config_t cfg={.pin_bit_mask=1ULL<<CONFIG_LAB_BOOST_GPIO,.mode=GPIO_MODE_OUTPUT};ESP_ERROR_CHECK(gpio_config(&cfg));ESP_ERROR_CHECK(gpio_set_level(CONFIG_LAB_BOOST_GPIO,CONFIG_LAB_BOOST_ACTIVE_LEVEL));vTaskDelay(pdMS_TO_TICKS(10));
#endif
    e=neopixel_init(CONFIG_LAB_LED_GPIO,LIGHT_KEYS);led_ready=e==ESP_OK;if(led_ready)neopixel_set_brightness(100);else printf("LED init error: %s\n",esp_err_to_name(e));
#else
    puts("LED output disabled in menuconfig; no LED GPIO driven.");
#endif
#if CONFIG_LAB_ANALOG_INPUT
#if CONFIG_LAB_ANALOG_POWER_CONTROL
    gpio_config_t analog_cfg={.pin_bit_mask=1ULL<<CONFIG_LAB_ANALOG_POWER_GPIO,.mode=GPIO_MODE_OUTPUT};ESP_ERROR_CHECK(gpio_config(&analog_cfg));ESP_ERROR_CHECK(gpio_set_level(CONFIG_LAB_ANALOG_POWER_GPIO,CONFIG_LAB_ANALOG_POWER_LEVEL));vTaskDelay(pdMS_TO_TICKS(10));
#endif
    ESP_ERROR_CHECK(spi_bus_init(CONFIG_LAB_ADC_SCK));ESP_ERROR_CHECK(mcp3202_init(CONFIG_LAB_ADC_CS,CONFIG_LAB_ADC_MISO,CONFIG_LAB_ADC_MOSI));ESP_ERROR_CHECK(mux_cd74hc4067_init(CONFIG_LAB_MUX_S0,CONFIG_LAB_MUX_S1,CONFIG_LAB_MUX_S2,CONFIG_LAB_MUX_S3));
    puts("Analog input enabled; use raw, free KEY and full KEY to calibrate.");
#else
    puts("Analog input disabled; p commands simulate pressure only.");
#endif
    xTaskCreate(console_task,"lab_console",4096,NULL,4,NULL);
    TickType_t next=xTaskGetTickCount();
    while(1){
        radio_packet_t packet;while(xQueueReceive(rx_queue,&packet,0)==pdTRUE)handle(&packet);
        read_analog();
        input_t in;while(xQueueReceive(input_queue,&in,0)==pdTRUE){
            if(in.kind==INPUT_SHOW){light_rgb_t frame[LIGHT_KEYS];light_engine_render(&engine,millis(),frame);printf("FRAME layer=%u",state.active);for(int i=0;i<LIGHT_KEYS;i++)printf(" %02X%02X%02X",frame[i].r,frame[i].g,frame[i].b);puts("");}
            else if(in.kind==INPUT_RAW){printf("RAW");for(int i=0;i<12;i++)printf(" %u",raw_values[i]);puts("");}
            else if(in.kind==INPUT_FREE || in.kind==INPUT_FULL)capture_calibration(in.key,in.kind==INPUT_FULL);
            else light_engine_input(&engine,in.key,in.depth/100.0f,millis());
        }
        light_rgb_t frame[LIGHT_KEYS];light_engine_render(&engine,millis(),frame);
        if(led_ready){for(int i=0;i<LIGHT_KEYS;i++)neopixel_set_pixel(key_to_led[i],frame[i].r,frame[i].g,frame[i].b);e=neopixel_refresh();if(e!=ESP_OK)printf("LED refresh error: %s\n",esp_err_to_name(e));}
        vTaskDelayUntil(&next,pdMS_TO_TICKS(16));
    }
}
