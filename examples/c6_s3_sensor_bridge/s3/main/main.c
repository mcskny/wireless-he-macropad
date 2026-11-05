// USB serial telemetry bridge for bench testing; no HID or configuration writes.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "sensor_packet.h"

typedef struct {uint32_t seq;uint16_t values[WHE_SENSOR_COUNT];} sample_t;
static QueueHandle_t sample_queue;
static void on_receive(const esp_now_recv_info_t *info,const uint8_t *data,int len) {
    (void)info;sample_t sample;
    if(len >= 0 && whe_sensor_decode(data,(size_t)len,&sample.seq,sample.values))
        (void)xQueueOverwrite(sample_queue,&sample);
}
void app_main(void) {
    sample_queue=xQueueCreate(1,sizeof(sample_t));assert(sample_queue);
    ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t wifi=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&wifi));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());ESP_ERROR_CHECK(esp_wifi_set_channel(6,WIFI_SECOND_CHAN_NONE));ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_receive));setvbuf(stdout,NULL,_IONBF,0);
    sample_t sample;while(1){if(xQueueReceive(sample_queue,&sample,pdMS_TO_TICKS(500))==pdTRUE){
        char line[128];int used=snprintf(line,sizeof(line),"K,%lu",(unsigned long)sample.seq);
        for(size_t i=0;i<WHE_SENSOR_COUNT && used>0 && (size_t)used<sizeof(line);i++) {
            int n=snprintf(line+used,sizeof(line)-(size_t)used,",%u",(unsigned)sample.values[i]);
            if(n<0 || (size_t)n>=sizeof(line)-(size_t)used){used=-1;break;}used+=n;
        }
        if(used>0 && (size_t)used+1<sizeof(line)){line[used++]='\n';fwrite(line,1,(size_t)used,stdout);}
    }}
}
