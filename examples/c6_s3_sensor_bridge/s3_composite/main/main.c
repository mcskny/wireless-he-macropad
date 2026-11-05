// USB CDC sensor bridge and explicitly armed RAM-only keyboard HID bench test.
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
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "sensor_packet.h"
#include "sensor_v2.h"
#include "sensor_v3.h"
#include "usb_composite.h"
#include "hid_engine.h"
#include "hid_profile.h"
#include "app_presence.h"
#include "light_sync.h"
#include "light_control.h"
#include "ble_profile_wire.h"

typedef struct { uint32_t seq; uint16_t values[WHE_SENSOR_COUNT]; bool expected_source,has_status,has_gate_time; whe_sensor_status_t status; uint32_t gate_on_us; int64_t received_us; } sample_t;
typedef struct {
    uint32_t received, invalid, queue_pending, usb_accepted, usb_dropped;
    uint32_t sequence_gaps, sequence_discontinuities;
} diagnostics_t;
static QueueHandle_t sample_queue;
static portMUX_TYPE diag_mux = portMUX_INITIALIZER_UNLOCKED;
static diagnostics_t stats;
static bool have_seq;
static uint32_t last_seq;
static int64_t app_heartbeat_us;
static bool app_heartbeat_seen;
static nvs_handle_t profile_store;
static uint8_t profile_active[WHE_PROFILE_BYTES],profile_staging[WHE_PROFILE_BYTES];
static bool profile_available,profile_apply_pending,profile_transaction;
static uint32_t profile_tx,profile_chunks;static uint8_t wanted_layer;
static uint32_t profile_expected_chunks;static unsigned profile_crc_offset,profile_crc_length;
#define PROFILE_CHUNK 48u
#define PROFILE_CHUNKS (WHE_PROFILE_BYTES/PROFILE_CHUNK)
_Static_assert(WHE_PROFILE_BYTES%PROFILE_CHUNK==0 && PROFILE_CHUNKS<32,"profile chunks");
static void profile_response(char stage,unsigned long tx,unsigned index,bool ok){char line[80];int n=snprintf(line,sizeof(line),"R,CFG,%c,%lu,%u,%s\n",stage,tx,index,ok?"OK":"ERR");if(n>0 && (size_t)n<sizeof(line))(void)whe_usb_write_line(line,(size_t)n);}
static bool profile_command(const char *line){
 if(strncmp(line,"CFG,",4))return false;
 unsigned long tx=0;unsigned index=0,crc=0;int end=0;char hex[2*PROFILE_CHUNK+1]={0};bool ok=false;
 if(sscanf(line,"CFG,B2,%lu%n",&tx,&end)==1 && !line[end] && tx<=UINT32_MAX){profile_transaction=true;profile_tx=(uint32_t)tx;profile_chunks=0;profile_expected_chunks=(1u<<PROFILE_CHUNKS)-1u;profile_crc_offset=0;profile_crc_length=WHE_PROFILE_BYTES;memset(profile_staging,0,sizeof(profile_staging));profile_response('B',tx,0,true);return true;}
 end=0;
 if(sscanf(line,"CFG,L2,%lu,%u%n",&tx,&index,&end)==2 && !line[end] && tx<=UINT32_MAX && index<4){
  profile_transaction=true;profile_tx=(uint32_t)tx;profile_chunks=0;profile_expected_chunks=15u<<(index*4);profile_crc_offset=index*WHE_HID_KEYS*WHE_PROFILE_RECORD_SIZE;profile_crc_length=WHE_HID_KEYS*WHE_PROFILE_RECORD_SIZE;
  if(profile_available)memcpy(profile_staging,profile_active,sizeof(profile_staging));else memset(profile_staging,0,sizeof(profile_staging));
  profile_response('L',tx,index,true);return true;
 }
 end=0;
 if(sscanf(line,"CFG,D,%lu,%u,%96[0-9a-fA-F]%n",&tx,&index,hex,&end)==3 && !line[end]){
  ok=profile_transaction && tx==profile_tx && index<PROFILE_CHUNKS && (profile_expected_chunks & (1u<<index)) && strlen(hex)==2*PROFILE_CHUNK;
  if(ok){for(unsigned i=0;i<PROFILE_CHUNK;i++){unsigned v=0;if(sscanf(hex+2*i,"%2x",&v)!=1){ok=false;break;}profile_staging[index*PROFILE_CHUNK+i]=(uint8_t)v;}if(ok)profile_chunks|=1u<<index;}
  profile_response('D',tx,index,ok);return true;
 }
 end=0;
 if(sscanf(line,"CFG,C,%lu,%u%n",&tx,&crc,&end)==2 && !line[end]){
  if(!profile_transaction && profile_available && tx==profile_tx && crc==whe_sensor_crc(profile_active+profile_crc_offset,profile_crc_length)){profile_response('C',tx,0,true);return true;}
  ok=profile_transaction && tx==profile_tx && crc<=65535 && profile_chunks==profile_expected_chunks && whe_sensor_crc(profile_staging+profile_crc_offset,profile_crc_length)==crc && whe_profile_validate(profile_staging);
  if(ok){esp_err_t e=nvs_set_blob(profile_store,"hid_v2",profile_staging,sizeof(profile_staging));if(e==ESP_OK)e=nvs_commit(profile_store);ok=e==ESP_OK;}
  if(ok){memcpy(profile_active,profile_staging,sizeof(profile_active));profile_available=true;profile_apply_pending=true;profile_transaction=false;}
  profile_response('C',tx,0,ok);return true;
 }
 profile_response('?',tx,index,false);return true;
}
static portMUX_TYPE status_mux=portMUX_INITIALIZER_UNLOCKED;
static bool status_inflight;
static void status_sent(const esp_now_send_info_t *info,esp_now_send_status_t result) {
    (void)info;(void)result;portENTER_CRITICAL(&status_mux);status_inflight=false;portEXIT_CRITICAL(&status_mux);
}
static void send_app_status(bool open) {
    static const uint8_t broadcast[6]={0xff,0xff,0xff,0xff,0xff,0xff};
    uint8_t frame[WHE_APP_FRAME_SIZE];whe_app_encode(frame,open);
    portENTER_CRITICAL(&status_mux);bool available=!status_inflight;if(available)status_inflight=true;portEXIT_CRITICAL(&status_mux);
    if(available && esp_now_send(broadcast,frame,sizeof(frame))!=ESP_OK){portENTER_CRITICAL(&status_mux);status_inflight=false;portEXIT_CRITICAL(&status_mux);}
}

/* Background broadcast: complete snapshots repeat so a dropped packet is retried.
   CRC and source address detect errors, but do not authenticate the sender. */
static void profile_broadcast_step(int64_t now){
    static int64_t next;static unsigned step;static uint32_t tx;
    static uint8_t snapshot[WHE_PROFILE_BYTES];
    static const uint8_t broadcast[6]={255,255,255,255,255,255};
    if(!profile_available || now<next)return;
    if(!step){memcpy(snapshot,profile_active,sizeof(snapshot));if(!++tx)++tx;}
    uint8_t frame[WHE_BLE_PROFILE_WIRE_MAX],crcbytes[2];size_t n;
    if(!step)n=whe_ble_profile_pack(frame,WHE_BLE_PROFILE_BEGIN,tx,0,NULL,0);
    else if(step<=WHE_BLE_PROFILE_CHUNKS)n=whe_ble_profile_pack(frame,WHE_BLE_PROFILE_DATA,tx,step-1,snapshot+(step-1)*WHE_BLE_PROFILE_CHUNK,WHE_BLE_PROFILE_CHUNK);
    else {uint16_t crc=whe_sensor_crc(snapshot,sizeof(snapshot));crcbytes[0]=crc;crcbytes[1]=crc>>8;n=whe_ble_profile_pack(frame,WHE_BLE_PROFILE_COMMIT,tx,0,crcbytes,2);}
    portENTER_CRITICAL(&status_mux);bool available=!status_inflight;if(available)status_inflight=true;portEXIT_CRITICAL(&status_mux);
    if(!available){next=now+20000;return;}
    if(esp_now_send(broadcast,frame,n)!=ESP_OK){portENTER_CRITICAL(&status_mux);status_inflight=false;portEXIT_CRITICAL(&status_mux);next=now+20000;return;}
    if(++step>WHE_BLE_PROFILE_CHUNKS+1){step=0;next=now+5000000;}else next=now+20000;
}


static void on_receive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    sample_t sample;
    memset(&sample,0,sizeof(sample));
    if(len>=0 && (size_t)len==WHE_SENSOR_V3_SIZE){sample.has_gate_time=whe_sensor_v3_decode(data,(size_t)len,&sample.seq,sample.values,&sample.status,&sample.gate_on_us);sample.has_status=sample.has_gate_time;}
    if(len>=0 && (size_t)len==WHE_SENSOR_V2_SIZE) sample.has_status=whe_sensor_v2_decode(data,(size_t)len,&sample.seq,sample.values,&sample.status);
    bool valid=sample.has_status || (len>=0 && whe_sensor_decode(data,(size_t)len,&sample.seq,sample.values));
    if (!valid) {
        portENTER_CRITICAL(&diag_mux); ++stats.invalid; portEXIT_CRITICAL(&diag_mux);
        return;
    }
    static const uint8_t expected[6]={0xb4,0x3a,0x45,0x80,0xcf,0x00};
    sample.expected_source=info && info->src_addr && memcmp(info->src_addr,expected,sizeof(expected))==0;
    sample.received_us=esp_timer_get_time();
    /* This counts receptions while a prior sample is still queued. The reader
       may race between the check and overwrite, so it is an upper estimate. */
    bool pending = uxQueueMessagesWaiting(sample_queue) != 0;
    portENTER_CRITICAL(&diag_mux);
    ++stats.received;
    if (pending) ++stats.queue_pending;
    if (have_seq) {
        uint32_t delta = sample.seq - last_seq;
        if (delta > 1 && delta < 100000) stats.sequence_gaps += delta - 1;
        else if (delta == 0 || delta >= 100000) ++stats.sequence_discontinuities;
    }
    last_seq = sample.seq; have_seq = true;
    portEXIT_CRITICAL(&diag_mux);
    (void)xQueueOverwrite(sample_queue, &sample);
}

static void report_diagnostics(void) {
    diagnostics_t snapshot;
    portENTER_CRITICAL(&diag_mux);
    snapshot = stats;
    memset(&stats, 0, sizeof(stats));
    portEXIT_CRITICAL(&diag_mux);
    char line[180];
    int n = snprintf(line, sizeof(line),
        "D,rx=%lu,invalid=%lu,sequence_gaps=%lu,restarts=%lu,pending=%lu,usb=%lu,usb_drop=%lu\n",
        (unsigned long)snapshot.received, (unsigned long)snapshot.invalid,
        (unsigned long)snapshot.sequence_gaps, (unsigned long)snapshot.sequence_discontinuities,
        (unsigned long)snapshot.queue_pending, (unsigned long)snapshot.usb_accepted,
        (unsigned long)snapshot.usb_dropped);
    if (n > 0 && (size_t)n < sizeof(line)) (void)whe_usb_write_line(line, (size_t)n);
}

/* Bench configuration over the USB CDC interface. HID starts disabled and
   settings are RAM-only; no packet may turn keyboard output on. */
static void hid_command(whe_hid_engine_t *engine,const char *line) {
    if(strcmp(line,"APP,1")==0){app_heartbeat_us=esp_timer_get_time();app_heartbeat_seen=true;return;}
    if(profile_command(line))return;
    if(strncmp(line,"CTRL,",5)==0){
        unsigned op,value,layer;unsigned long id;int end=0;
        if(sscanf(line,"CTRL,%u,%u,%u,%lu%n",&op,&value,&layer,&id,&end)!=4||line[end]||op<1||op>3||layer>3||id>UINT32_MAX||(op==1&&value>3)||(op>=2&&value>11))return;
        uint8_t frame[WHE_CONTROL_SIZE];whe_control_pack(frame,(uint8_t)op,(uint8_t)value,(uint8_t)layer,(uint32_t)id);
        static const uint8_t broadcast[6]={0xff,0xff,0xff,0xff,0xff,0xff};
        portENTER_CRITICAL(&status_mux);bool available=!status_inflight;if(available)status_inflight=true;portEXIT_CRITICAL(&status_mux);
        if(available && esp_now_send(broadcast,frame,sizeof(frame))!=ESP_OK){portENTER_CRITICAL(&status_mux);status_inflight=false;portEXIT_CRITICAL(&status_mux);}return;
    }
    if(strncmp(line,"LIGHT,",6)==0){
        char hex[57]={0};unsigned long long ms=0;int n=0;uint8_t profiles[28],frame[WHE_LIGHT_SIZE],checked[28];uint64_t checked_ms;
        if(sscanf(line,"LIGHT,%56[0-9a-fA-F],%llu%n",hex,&ms,&n)!=2 || line[n] || strlen(hex)!=56)return;
        for(unsigned i=0;i<28;i++){unsigned x=0;if(sscanf(hex+2*i,"%2x",&x)!=1)return;profiles[i]=(uint8_t)x;}
        whe_light_pack(frame,profiles,(uint64_t)ms);if(!whe_light_unpack(frame,sizeof(frame),checked,&checked_ms))return;
        static const uint8_t broadcast[6]={0xff,0xff,0xff,0xff,0xff,0xff};
        portENTER_CRITICAL(&status_mux);bool available=!status_inflight;if(available)status_inflight=true;portEXIT_CRITICAL(&status_mux);
        if(available && esp_now_send(broadcast,frame,sizeof(frame))!=ESP_OK){portENTER_CRITICAL(&status_mux);status_inflight=false;portEXIT_CRITICAL(&status_mux);}return;
    }
    unsigned ch,free_v,down_v,usage,point,hyst;int end=0;bool ok=false;
    if (sscanf(line,"HID,SET,%u,%u,%u,%u,%u,%u%n",&ch,&free_v,&down_v,&usage,&point,&hyst,&end)==6 && line[end]=='\0')
        ok=whe_hid_set(engine,ch,free_v,down_v,usage,point,hyst);
    else if(strcmp(line,"HID,ON")==0) ok=whe_hid_enable(engine,true);
    else if(strcmp(line,"HID,OFF")==0) ok=whe_hid_enable(engine,false);
    else if(strcmp(line,"HID,CLEAR")==0){whe_hid_reset(engine);ok=true;}
    const char *response=ok ? "R,HID,OK\n" : "R,HID,ERROR\n";
    (void)whe_usb_write_line(response,strlen(response));
}
static void hid_commands_poll(whe_hid_engine_t *engine) {
    static char line[128];static size_t used;static bool overflow;
    for(int n=0;n<128;n++) {
        int c=whe_usb_read_byte();if(c<0)break;
        if(c=='\n'){
            if(!overflow){if(used && line[used-1]=='\r')--used;line[used]=0;hid_command(engine,line);}
            used=0;overflow=false;
        } else if(!overflow){if(used+1<sizeof(line))line[used++]=(char)c;else overflow=true;}
    }
    if(!whe_usb_connected()){used=0;overflow=false;app_heartbeat_seen=false;if(!profile_available && engine->enabled)whe_hid_enable(engine,false);}
}
void app_main(void) {
    ESP_ERROR_CHECK(whe_usb_init());
    sample_queue = xQueueCreate(1, sizeof(sample_t)); assert(sample_queue);
    ESP_ERROR_CHECK(nvs_flash_init()); ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(nvs_open("whe_hid",NVS_READWRITE,&profile_store));
    size_t profile_size=sizeof(profile_active);
    if(nvs_get_blob(profile_store,"hid_v2",profile_active,&profile_size)==ESP_OK && profile_size==sizeof(profile_active) && whe_profile_validate(profile_active)){profile_available=true;profile_apply_pending=true;}
    else {uint8_t old[WHE_PROFILE_V1_BYTES];size_t old_size=sizeof(old);
        if(nvs_get_blob(profile_store,"hid_v1",old,&old_size)==ESP_OK && old_size==sizeof(old) && whe_profile_migrate_v1(profile_active,old)){profile_available=true;profile_apply_pending=true;}}
    wifi_init_config_t wifi = WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&wifi));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM)); ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start()); ESP_ERROR_CHECK(esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE)); ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_receive));
    ESP_ERROR_CHECK(esp_now_register_send_cb(status_sent));
    esp_now_peer_info_t peer={.channel=6,.ifidx=WIFI_IF_STA,.encrypt=false};memset(peer.peer_addr,0xff,6);ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    int64_t next_report = esp_timer_get_time() + 1000000;
    int64_t next_status = 0;
    sample_t sample; whe_hid_engine_t hid; whe_hid_reset(&hid);
    whe_hid_report_t previous={0}; bool sent_initial=false,sent_consumer_initial=false,neutral_keyboard_sent=false;
    bool have_hid_seq=false; uint32_t hid_seq=0;
    bool have_status=false;uint32_t status_epoch=0;uint8_t status_flags=0;
    bool have_layer=false;uint8_t last_layer=0;bool last_pressed=false;int64_t next_layer_report=0;
    for (;;) {
        hid_commands_poll(&hid);
        if (xQueueReceive(sample_queue, &sample, pdMS_TO_TICKS(5)) == pdTRUE) {
            if(sample.expected_source && sample.has_status && profile_available && sample.status.layer!=wanted_layer){wanted_layer=sample.status.layer;profile_apply_pending=true;}
            if(profile_apply_pending && profile_available){
                static const uint8_t empty[6]={0};
                if(!neutral_keyboard_sent){if(whe_usb_keyboard(empty,0))neutral_keyboard_sent=true;}
                else if(whe_usb_consumer(0)){whe_profile_load_layer(profile_active,wanted_layer,&hid);memset(&previous,0,sizeof(previous));sent_initial=true;sent_consumer_initial=true;have_hid_seq=false;profile_apply_pending=false;neutral_keyboard_sent=false;}
            }
            if (sample.expected_source && hid.enabled && !profile_apply_pending) {
                uint32_t delta=sample.seq-hid_seq;
                if (!have_hid_seq || !hid.has_sample || (delta>0 && delta<0x80000000u)) {
                    whe_hid_sample(&hid,sample.values,sample.received_us);
                    hid_seq=sample.seq;have_hid_seq=true;
                }
            }
            if(sample.expected_source && sample.has_status){
                whe_sensor_status_t *st=&sample.status;
                if(!have_layer || st->layer!=last_layer || st->layer_pressed!=last_pressed || sample.received_us>=next_layer_report){
                    char line[32];int n=snprintf(line,sizeof(line),"L,%u,%u\n",(unsigned)st->layer,(unsigned)st->layer_pressed);
                    if(n>0 && (size_t)n<sizeof(line) && whe_usb_write_line(line,(size_t)n)){have_layer=true;last_layer=st->layer;last_pressed=st->layer_pressed;next_layer_report=sample.received_us+1000000;}
                }
                uint8_t flags=(st->active?1:0)|(st->app_open?2:0)|(st->stats_valid?4:0)|(st->gate_enabled?8:0);
                if(!have_status || status_epoch!=st->epoch || status_flags!=flags){
                    char metadata[160];int n=snprintf(metadata,sizeof(metadata),"P,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u",(unsigned long)st->epoch,st->active,st->app_open,st->gate_enabled,st->stats_valid,st->scans,st->submitted,st->skipped,st->replaced,st->interval_ms);
                    if(n>0 && (size_t)n<sizeof(metadata)){int extra=sample.has_gate_time?snprintf(metadata+n,sizeof(metadata)-(size_t)n,",%lu\n",(unsigned long)sample.gate_on_us):snprintf(metadata+n,sizeof(metadata)-(size_t)n,"\n");if(extra<0 || (size_t)extra>=sizeof(metadata)-(size_t)n)n=-1;else n+=extra;}
                    if(n>0 && (size_t)n<sizeof(metadata) && whe_usb_write_line(metadata,(size_t)n)){have_status=true;status_epoch=st->epoch;status_flags=flags;}
                }
            }
            char line[128]; int used = snprintf(line, sizeof(line), "K,%lu", (unsigned long)sample.seq);
            for (size_t i = 0; i < WHE_SENSOR_COUNT && used > 0 && (size_t)used < sizeof(line); i++) {
                int n = snprintf(line + used, sizeof(line) - (size_t)used, ",%u", (unsigned)sample.values[i]);
                if (n < 0 || (size_t)n >= sizeof(line) - (size_t)used) { used = -1; break; }
                used += n;
            }
            bool accepted = false;
            if (used > 0 && (size_t)used + 1 < sizeof(line)) {
                line[used++] = '\n'; accepted = whe_usb_write_line(line, (size_t)used);
            }
            portENTER_CRITICAL(&diag_mux);
            if (accepted) ++stats.usb_accepted; else ++stats.usb_dropped;
            portEXIT_CRITICAL(&diag_mux);
        }
        int64_t now = esp_timer_get_time();
        whe_hid_report_t report=profile_apply_pending?(whe_hid_report_t){0}:whe_hid_report(&hid,now,whe_usb_host_mounted());
        if(!profile_apply_pending){
            if ((!sent_initial || report.modifier!=previous.modifier || memcmp(report.keys,previous.keys,sizeof(report.keys))!=0) && whe_usb_keyboard(report.keys,report.modifier)) {previous.modifier=report.modifier;memcpy(previous.keys,report.keys,sizeof(report.keys));sent_initial=true;}
            if ((!sent_consumer_initial || report.consumer!=previous.consumer) && whe_usb_consumer(report.consumer)) {previous.consumer=report.consumer;sent_consumer_initial=true;}
        }
        if(now>=next_status){bool open=whe_usb_connected() && app_heartbeat_seen && now>=app_heartbeat_us && now-app_heartbeat_us<=1000000;send_app_status(open);next_status=now+200000;}
        profile_broadcast_step(now);
        if (now >= next_report) { report_diagnostics(); next_report = now + 1000000; }
    }
}
