#include "lab_radio.h"
#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "freertos/semphr.h"
#include "esp_idf_version.h"
static QueueHandle_t packets;
static SemaphoreHandle_t tx_done;
static uint8_t expected[6];
static volatile esp_now_send_status_t tx_status;
static bool tx_pending;
bool radio_parse_mac(const char *s,uint8_t out[6]) {
    unsigned x[6]; char tail;
    if(!s || !out || strlen(s)!=17 || sscanf(s,"%2x:%2x:%2x:%2x:%2x:%2x%c",&x[0],&x[1],&x[2],&x[3],&x[4],&x[5],&tail)!=6) return false;
    for(int i=0;i<6;i++) {if(x[i]>255)return false;out[i]=(uint8_t)x[i];}
    if((out[0]&1) || !memcmp(out,"\0\0\0\0\0\0",6)) return false;
    return true;
}
static void receive_cb(const esp_now_recv_info_t *info,const uint8_t *data,int len) {
    if(!info || !data || len<0 || len>WIRE_MAX || memcmp(expected,info->src_addr,6)) return;
    radio_packet_t p={.len=(size_t)len};memcpy(p.mac,info->src_addr,6);memcpy(p.bytes,data,(size_t)len);
    (void)xQueueSend(packets,&p,0);
}
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
static void sent_cb(const esp_now_send_info_t *info,esp_now_send_status_t status)
#else
static void sent_cb(const uint8_t *info,esp_now_send_status_t status)
#endif
{(void)info;tx_status=status;xSemaphoreGive(tx_done);}
esp_err_t radio_init(const uint8_t peer[6],uint8_t channel,QueueHandle_t queue) {
    if(!peer || !queue || channel<1 || channel>13) return ESP_ERR_INVALID_ARG;
    memcpy(expected,peer,6);packets=queue;tx_done=xSemaphoreCreateBinary();if(!tx_done)return ESP_ERR_NO_MEM;
    ESP_ERROR_CHECK(esp_netif_init());esp_err_t e=esp_event_loop_create_default();if(e!=ESP_OK && e!=ESP_ERR_INVALID_STATE)return e;
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&cfg));ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_start());ESP_ERROR_CHECK(esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE));ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(receive_cb));ESP_ERROR_CHECK(esp_now_register_send_cb(sent_cb));
    esp_now_peer_info_t p={0};memcpy(p.peer_addr,peer,6);p.channel=channel;p.ifidx=WIFI_IF_STA;p.encrypt=false;
    return esp_now_add_peer(&p);
}
esp_err_t radio_send(const wire_message_t *m) {
    uint8_t bytes[WIRE_MAX];size_t n=wire_encode(m,bytes);if(!n || !tx_done)return ESP_ERR_INVALID_ARG;
    /* Never start another send while a previous callback is still outstanding. */
    if(tx_pending) {if(xSemaphoreTake(tx_done,0)!=pdTRUE)return ESP_ERR_INVALID_STATE;tx_pending=false;}
    while(xSemaphoreTake(tx_done,0)==pdTRUE){};
    esp_err_t e=esp_now_send(expected,bytes,n);if(e!=ESP_OK)return e;
    tx_pending=true;
    if(xSemaphoreTake(tx_done,pdMS_TO_TICKS(1000))!=pdTRUE)return ESP_ERR_TIMEOUT;
    tx_pending=false;
    return tx_status==ESP_NOW_SEND_SUCCESS ? ESP_OK : ESP_FAIL;
}
