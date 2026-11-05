#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "lab_radio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static QueueHandle_t rx_queue;
static uint32_t session, sequence;
static bool ready;
static const char *names[]={"off","static","breathe","flow","rainbow","sweep","reactive","ripple"};
static int effect_id(const char *s){for(int i=0;i<EFFECT_COUNT;i++)if(!strcmp(s,names[i]))return i;return -1;}
static void transmit(wire_message_t *msg) {
    msg->session=session;msg->seq=++sequence;
    if(!ready){printf("RESULT %" PRIu32 " RADIO_NOT_CONFIGURED\n",msg->seq);return;}
    for(int attempt=1;attempt<=3;attempt++) {
        esp_err_t err=radio_send(msg);
        if(err!=ESP_OK){printf("Tentativa %d: MAC send %s\n",attempt,esp_err_to_name(err));vTaskDelay(pdMS_TO_TICKS(100));continue;}
        TickType_t end=xTaskGetTickCount()+pdMS_TO_TICKS(1500);
        while((int32_t)(end-xTaskGetTickCount())>0) {
            radio_packet_t packet;wire_message_t reply;
            TickType_t remaining=end-xTaskGetTickCount();
            if(xQueueReceive(rx_queue,&packet,remaining)!=pdTRUE)break;
            if(wire_decode(packet.bytes,packet.len,&reply) && reply.type==MSG_ACK && reply.len==1 && reply.session==msg->session && reply.seq==msg->seq) {
                printf("RESULT %" PRIu32 " %s\n",msg->seq,reply.payload[0]==ACK_OK?"OK":reply.payload[0]==ACK_INVALID?"INVALID":"STORAGE_ERROR");return;
            }
        }
        printf("Tentativa %d: uygulama ACK bekleme suresi doldu\n",attempt);
    }
    printf("RESULT %" PRIu32 " TIMEOUT\n",msg->seq);
}
void app_main(void) {
    setvbuf(stdout,NULL,_IONBF,0);setvbuf(stdin,NULL,_IONBF,0);
    esp_err_t e=nvs_flash_init();if(e!=ESP_OK){printf("NVS error %s (no automatic erase)\n",esp_err_to_name(e));return;}
    uint8_t own[6],peer[6];ESP_ERROR_CHECK(esp_read_mac(own,ESP_MAC_WIFI_STA));
    printf("S3 Wi-Fi STA MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",own[0],own[1],own[2],own[3],own[4],own[5]);
    rx_queue=xQueueCreate(16,sizeof(radio_packet_t));if(!rx_queue)return;
    session=esp_random();if(!session)session=1;
    if(radio_parse_mac(CONFIG_LAB_PEER_MAC,peer)){e=radio_init(peer,CONFIG_LAB_WIFI_CHANNEL,rx_queue);ready=e==ESP_OK;if(!ready)printf("Radio error: %s\n",esp_err_to_name(e));}
    else printf("C6 MAC missing/invalid: set LAB_PEER_MAC in menuconfig.\n");
    printf("Komut: set layer effect R G B brightness speed flags | active layer\n");
    printf("effect: off static breathe flow rainbow sweep reactive ripple; flags: 1=colorful 2=depth 4=sweep turn\n");
    char line[160],effect[24],extra;unsigned layer,r,g,b,bright,speed,flags;
    while(1) {
        if(!fgets(line,sizeof(line),stdin)){clearerr(stdin);vTaskDelay(pdMS_TO_TICKS(30));continue;}
        wire_message_t m={0};
        if(sscanf(line,"set %u %23s %u %u %u %u %u %u %c",&layer,effect,&r,&g,&b,&bright,&speed,&flags,&extra)==8) {
            int id=effect_id(effect);
            if(layer>=4 || id<0 || r>255||g>255||b>255||bright>100||speed<1||speed>100||flags>7){puts("ERR invalid args");continue;}
            light_profile_t profile={(uint8_t)id,(uint8_t)r,(uint8_t)g,(uint8_t)b,(uint8_t)bright,(uint8_t)speed,(uint8_t)flags};
            if(!light_profile_valid(&profile)){puts("ERR incompatible flags");continue;}
            m.type=MSG_PROFILE;m.len=8;wire_pack_profile(m.payload,(uint8_t)layer,&profile);transmit(&m);
        } else if(sscanf(line,"active %u %c",&layer,&extra)==1 && layer<4) {m.type=MSG_ACTIVE;m.len=1;m.payload[0]=(uint8_t)layer;transmit(&m);}
        else puts("ERR invalid command");
    }
}
