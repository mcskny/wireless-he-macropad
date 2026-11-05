#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#define A 25
#define B 26
#define BTN 27
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static int32_t position, transitions, presses, invalid;
static int a_level,b_level,button_down;
static int64_t started;
extern const unsigned char page[] asm("_binary_index_html_start");
static void read_encoder(void *arg){
 static const int8_t table[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
 int prev=(gpio_get_level(A)<<1)|gpio_get_level(B),accum=0,candidate=gpio_get_level(BTN),stable=candidate,count=0;
 for(;;){int a=gpio_get_level(A),b=gpio_get_level(B),cur=(a<<1)|b,s=gpio_get_level(BTN);
 portENTER_CRITICAL(&lock);a_level=a;b_level=b;
 if(cur!=prev){int d=table[(prev<<2)|cur];if(!d){invalid++;accum=0;}else{transitions++;accum+=d;if(accum>=4){position++;accum=0;}else if(accum<=-4){position--;accum=0;}}prev=cur;}
 if(s!=candidate){candidate=s;count=0;}else if(count<20)count++;
 if(count==20&&stable!=candidate){stable=candidate;button_down=!stable;if(!stable)presses++;}
 portEXIT_CRITICAL(&lock);vTaskDelay(pdMS_TO_TICKS(1));}}
static esp_err_t root(httpd_req_t *r){httpd_resp_set_type(r,"text/html; charset=utf-8");return httpd_resp_sendstr(r,(const char*)page);}
static esp_err_t status(httpd_req_t *r){int32_t p,t,n,i;int a,b,btn;portENTER_CRITICAL(&lock);p=position;t=transitions;n=presses;i=invalid;a=a_level;b=b_level;btn=button_down;portEXIT_CRITICAL(&lock);char buf[256];snprintf(buf,sizeof(buf),"{\"position\":%" PRId32 ",\"transitions\":%" PRId32 ",\"presses\":%" PRId32 ",\"invalid\":%" PRId32 ",\"a\":%d,\"b\":%d,\"button\":%d,\"uptime\":%lld}",p,t,n,i,a,b,btn,(long long)((esp_timer_get_time()-started)/1000000));httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_sendstr(r,buf);}
static esp_err_t reset(httpd_req_t *r){portENTER_CRITICAL(&lock);position=transitions=presses=invalid=0;portEXIT_CRITICAL(&lock);return httpd_resp_sendstr(r,"OK");}
void app_main(void){started=esp_timer_get_time();gpio_config_t cfg={.pin_bit_mask=(1ULL<<A)|(1ULL<<B)|(1ULL<<BTN),.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};ESP_ERROR_CHECK(gpio_config(&cfg));a_level=gpio_get_level(A);b_level=gpio_get_level(B);button_down=!gpio_get_level(BTN);xTaskCreate(read_encoder,"encoder",3072,NULL,5,NULL);
 esp_err_t e=nvs_flash_init();if(e==ESP_ERR_NVS_NO_FREE_PAGES||e==ESP_ERR_NVS_NEW_VERSION_FOUND){ESP_ERROR_CHECK(nvs_flash_erase());e=nvs_flash_init();}ESP_ERROR_CHECK(e);ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());esp_netif_create_default_wifi_ap();wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&init));wifi_config_t wifi={.ap={.ssid="ESP32-Encoder",.password="encoder123",.ssid_len=13,.channel=1,.max_connection=4,.authmode=WIFI_AUTH_WPA2_PSK}};ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&wifi));ESP_ERROR_CHECK(esp_wifi_start());httpd_handle_t server=NULL;httpd_config_t c=HTTPD_DEFAULT_CONFIG();ESP_ERROR_CHECK(httpd_start(&server,&c));httpd_uri_t u[]={ {.uri="/",.method=HTTP_GET,.handler=root},{.uri="/api/status",.method=HTTP_GET,.handler=status},{.uri="/api/reset",.method=HTTP_POST,.handler=reset} };for(int j=0;j<3;j++)ESP_ERROR_CHECK(httpd_register_uri_handler(server,&u[j]));}
