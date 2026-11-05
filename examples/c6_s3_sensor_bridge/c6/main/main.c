#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "sensor_packet.h"
#include "sensor_v2.h"
#include "sensor_v3.h"
#include "power_policy.h"
#include "release_filter.h"
#include "app_presence.h"
#include "neopixel.h"
#include "lighting.h"
#include "light_sync.h"
#include "light_control.h"
#include "disconnected_warning.h"
#include "ble_transport.h"
#include "transport_mode.h"
#include "ble_slots.h"
#include "ble_profile_wire.h"
#include "nvs.h"
#include "esp_system.h"
_Static_assert(sizeof(light_profile_t)==7,"lighting wire profile size");

#define PIN_SCK 19
#define PIN_MISO 20
#define PIN_MOSI 18
#define PIN_CS 22
#define PIN_ANALOG_GATE 23 /* XIAO D5; experimental active-low P-MOSFET gate */
#define PIN_LAYER 17 /* XIAO D7: switch to GND, internal pull-up */
static const gpio_num_t mux_pins[] = {GPIO_NUM_0, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_21};

static void mux_select(unsigned channel) {
    for (unsigned bit = 0; bit < 4; bit++)
        gpio_set_level(mux_pins[bit], (channel >> bit) & 1);
    // Allow the mux and ADC input to settle after switching channels.
    esp_rom_delay_us(10);
}

static uint16_t adc_read(spi_device_handle_t adc) {
    // MCP3202: start, single-ended, CH0, MSB first; mode 0, 24 clocks.
    spi_transaction_t t = { .length = 24, .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA };
    t.tx_data[0] = 0x01;
    t.tx_data[1] = 0xA0;
    t.tx_data[2] = 0x00;
    ESP_ERROR_CHECK(spi_device_polling_transmit(adc, &t));
    return ((t.rx_data[1] & 0x0f) << 8) | t.rx_data[2];
}

#define SEND_WAKE_SAMPLE (1u << 0)
#define SEND_WAKE_COMPLETE (1u << 1)
#define MAX_SAMPLE_AGE_US 4000

typedef struct { uint8_t frame[WHE_SENSOR_V3_SIZE]; int64_t captured_us; } pending_sample_t;
static TaskHandle_t sampling_task, sender_task;
static QueueHandle_t latest_sample;
static portMUX_TYPE send_mux=portMUX_INITIALIZER_UNLOCKED;
static uint32_t scans, missed_ticks, send_busy, send_failed, send_submitted, max_scan_us, stale_samples;
static uint16_t diag_baseline,diag_value,diag_delta;static unsigned diag_channel;
static whe_sensor_status_t rate_snapshot;
static uint32_t gate_on_snapshot_us;
#if CONFIG_WHE_POWER_GATE_EXPERIMENT
static bool gate_command_on;static int64_t gate_start_us;static uint64_t gate_elapsed_us;
static void analog_gate_set(bool on){
    ESP_ERROR_CHECK(gpio_set_level(PIN_ANALOG_GATE,on?0:1));int64_t now=esp_timer_get_time();
    portENTER_CRITICAL(&send_mux);
    if(on!=gate_command_on){if(gate_command_on && now>=gate_start_us)gate_elapsed_us+=(uint64_t)(now-gate_start_us);gate_command_on=on;gate_start_us=now;}
    portEXIT_CRITICAL(&send_mux);
}
#endif
static bool diagnostic_active=false, reported_app_open=false;
static int64_t reported_app_at_us=0;
static uint16_t lighting_raw[12],lighting_baseline[12];
static bool lighting_sample_ready;
static light_profile_t synced_profiles[4];static bool synced_valid;static uint8_t lighting_layer;
static uint32_t layer_press_count;
static bool layer_held, transition_animation;
static int64_t pairing_animation_us,slot_animation_us;
static uint8_t slot_animation_index;
static int64_t transition_requested_us;
static whe_transport_t transition_target;
static uint64_t lighting_host_ms;static int64_t lighting_host_received_us;
typedef struct {uint32_t id;uint8_t key,layer,op;} light_event_t;
static QueueHandle_t light_events;static bool layer_command_pending;static uint8_t layer_command_value;
/* Physical positions use column*4+row. ADC channels were measured on the
   assembled board; LED order follows NEO1..NEO12 in the PCB layout. */
static const uint8_t adc_to_light[12]={4,0,8,1,5,9,2,6,10,3,7,11};
static const uint8_t light_to_led[12]={0,3,6,9,1,4,7,10,2,5,8,11};
/* KEY 0: top-left key, ADC 1 on the measured test board. */
#define MODE_KEY_ADC 1
static whe_transport_mode_t transport;
static nvs_handle_t local_store;
static uint8_t local_profile[WHE_PROFILE_BYTES];
static bool local_profile_valid;
static QueueHandle_t profile_queue;
typedef struct {uint8_t bytes[WHE_BLE_PROFILE_WIRE_MAX];size_t length;} profile_frame_t;
static void profile_worker(void *arg){
    (void)arg;whe_ble_profile_staging_t staging={0};profile_frame_t f;
    for(;;){
        if(xQueueReceive(profile_queue,&f,portMAX_DELAY)!=pdTRUE)continue;
        whe_ble_profile_message_t m;if(!whe_ble_profile_unpack(f.bytes,f.length,&m))continue;
        if(m.op==WHE_BLE_PROFILE_BEGIN)whe_ble_profile_begin(&staging,m.transaction);
        else if(m.op==WHE_BLE_PROFILE_DATA)whe_ble_profile_chunk(&staging,m.transaction,m.index,m.payload,m.payload_size);
        else if(whe_ble_profile_ready(&staging,m.transaction,whe_get16(m.payload))){
            portENTER_CRITICAL(&send_mux);
            bool changed=!local_profile_valid||memcmp(local_profile,staging.data,WHE_PROFILE_BYTES)!=0;
            portEXIT_CRITICAL(&send_mux);
            if(changed){
                esp_err_t e=nvs_set_blob(local_store,"hid_v2",staging.data,WHE_PROFILE_BYTES);
                if(e==ESP_OK)e=nvs_commit(local_store);
                if(e==ESP_OK){portENTER_CRITICAL(&send_mux);memcpy(local_profile,staging.data,WHE_PROFILE_BYTES);local_profile_valid=true;portEXIT_CRITICAL(&send_mux);}
                printf("BLE profile persistence: %s\n",esp_err_to_name(e));
            }
            whe_ble_profile_abort(&staging);
        }
    }
}
static void sample_tick(void *arg) {(void)arg;xTaskNotifyGive(sampling_task);}
/* ESP-NOW broadcast completion is a local driver notification, not a receiver ACK. */
static void radio_sent(const esp_now_send_info_t *info,esp_now_send_status_t status) {
    (void)info; (void)status;
    if(sender_task) xTaskNotify(sender_task,SEND_WAKE_COMPLETE,eSetBits);
}
static void sender_loop(void *arg) {
    (void)arg;
    bool in_flight=false;
    const uint8_t broadcast[6]={0xff,0xff,0xff,0xff,0xff,0xff};
    for (;;) {
        uint32_t events=0;
        xTaskNotifyWait(0,UINT32_MAX,&events,portMAX_DELAY);
        if(events & SEND_WAKE_COMPLETE) in_flight=false;
        if(in_flight) continue;
        pending_sample_t candidate;
        if(xQueueReceive(latest_sample,&candidate,0)!=pdTRUE) continue;
        int64_t age=esp_timer_get_time()-candidate.captured_us;
        if(age<0 || age>MAX_SAMPLE_AGE_US) {
            portENTER_CRITICAL(&send_mux);++stale_samples;portEXIT_CRITICAL(&send_mux);
            continue;
        }
        in_flight=true;
        esp_err_t result=esp_now_send(broadcast,candidate.frame,sizeof(candidate.frame));
        portENTER_CRITICAL(&send_mux);
        if(result!=ESP_OK){in_flight=false;++send_failed;}else ++send_submitted;
        portEXIT_CRITICAL(&send_mux);
        /* A failed submission must not consume a later queue notification. */
        if(result!=ESP_OK && uxQueueMessagesWaiting(latest_sample))
            xTaskNotify(sender_task,SEND_WAKE_SAMPLE,eSetBits);
    }
}
static void on_app_status(const esp_now_recv_info_t *info,const uint8_t *data,int len) {
    static const uint8_t s3_mac[6]={0x90,0x70,0x69,0x11,0x84,0x18};
    bool open=false;
    if(!info || !info->src_addr || memcmp(info->src_addr,s3_mac,6)!=0 || len<0)return;
    if(len>=12 && len<=WHE_BLE_PROFILE_WIRE_MAX && memcmp(data,"WBP1",4)==0){
        profile_frame_t f={.length=(size_t)len};memcpy(f.bytes,data,(size_t)len);
        (void)xQueueSend(profile_queue,&f,0);return;
    }
    int64_t now=esp_timer_get_time();
    uint8_t op,value,command_layer;uint32_t id;
    if(whe_control_unpack(data,(size_t)len,&op,&value,&command_layer,&id)){
        if(op==1){portENTER_CRITICAL(&send_mux);layer_command_value=value;layer_command_pending=true;portEXIT_CRITICAL(&send_mux);}
        else {light_event_t event={.id=id,.key=value,.layer=command_layer,.op=op};(void)xQueueSend(light_events,&event,0);}return;
    }
    uint8_t profiles[28];uint64_t host_ms;
    if(whe_light_unpack(data,(size_t)len,profiles,&host_ms)){
        light_profile_t candidate[4];memcpy(candidate,profiles,sizeof(candidate));
        for(unsigned i=0;i<4;i++)if(!light_profile_valid(&candidate[i]))return;
        portENTER_CRITICAL(&send_mux);memcpy(synced_profiles,candidate,sizeof(candidate));synced_valid=true;lighting_host_ms=host_ms;lighting_host_received_us=now;portEXIT_CRITICAL(&send_mux);return;
    }
    if(!whe_app_decode(data,(size_t)len,&open))return;
    portENTER_CRITICAL(&send_mux);reported_app_open=open;reported_app_at_us=now;portEXIT_CRITICAL(&send_mux);
}
static void diagnostic_task(void *arg) {
    (void)arg;
    int64_t previous_us=esp_timer_get_time();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        portENTER_CRITICAL(&send_mux);
        int64_t now_us=esp_timer_get_time();
        uint32_t interval=(uint32_t)((now_us-previous_us)/1000);previous_us=now_us;
#if CONFIG_WHE_POWER_GATE_EXPERIMENT
        if(gate_command_on && now_us>=gate_start_us){gate_elapsed_us+=(uint64_t)(now_us-gate_start_us);gate_start_us=now_us;}
        gate_on_snapshot_us=(uint32_t)(gate_elapsed_us>UINT32_MAX?UINT32_MAX:gate_elapsed_us);gate_elapsed_us=0;
#else
        gate_on_snapshot_us=0;
#endif
        uint32_t a=scans,b=missed_ticks,c=send_busy,d=send_failed,e=send_submitted,f=max_scan_us,g=stale_samples;bool active=diagnostic_active;bool app=reported_app_open;int64_t seen=reported_app_at_us;uint16_t base=diag_baseline,value=diag_value,delta=diag_delta;unsigned channel=diag_channel;
        rate_snapshot.epoch++;
        rate_snapshot.stats_valid=interval>=100 && interval<=10000;
        rate_snapshot.scans=(uint16_t)(a>65535?65535:a);
        rate_snapshot.submitted=(uint16_t)(e>65535?65535:e);
        rate_snapshot.skipped=(uint16_t)(b>65535?65535:b);
        rate_snapshot.replaced=(uint16_t)(c>65535?65535:c);
        rate_snapshot.interval_ms=(uint16_t)(interval>65535?65535:interval);
        scans=missed_ticks=send_busy=send_failed=send_submitted=max_scan_us=stale_samples=0;
        portEXIT_CRITICAL(&send_mux);
        /* Sampling never waits for the USB host; this lower-priority task can block. */
        printf("STAT mode=%s app=%s scans=%lu skipped_ticks=%lu replaced_est=%lu send_error=%lu submitted=%lu scan_max_us=%lu stale=%lu maxdelta=%u ch=%u baseline=%u value=%u\n",
               active ? "active" : "idle",whe_app_present(app,seen,esp_timer_get_time()) ? "yes" : "no",(unsigned long)a,(unsigned long)b,(unsigned long)c,(unsigned long)d,(unsigned long)e,(unsigned long)f,(unsigned long)g,(unsigned)delta,channel,(unsigned)base,(unsigned)value);
    }
}
/* Low-priority visual wiring test. D6 is free on the XIAO sensor setup;
   D5 remains reserved for the optional analog power gate. */
static void led_test_task(void *arg) {
    (void)arg;
    if (neopixel_init(16, 12) != ESP_OK) { printf("LED init failed\n"); vTaskDelete(NULL); return; }
    neopixel_set_brightness(100);
    light_profile_t profile={.effect=EFFECT_RIPPLE,.r=70,.g=255,.b=140,.brightness=15,.speed=50,.flags=FLAG_COLORFUL};
    static light_engine_t engine;light_engine_init(&engine,&profile);
    uint32_t last_event=0;bool have_event=false;
    bool physical_pressed[12]={0},candidate_pressed[12]={0};
    int64_t candidate_since[12]={0};
    whe_disconnected_warning_t warning={0};uint32_t last_layer_press_count=0;
    for (;;) {
        int64_t local_us=esp_timer_get_time();uint64_t now=(uint64_t)local_us/1000;
        light_profile_t selected;bool valid;uint8_t layer;uint64_t host_ms;int64_t host_received,receiver_seen;uint32_t layer_presses;bool held,transition_show;int64_t transition_start,pair_start,slot_start;uint8_t slot_index;whe_transport_t target;
        portENTER_CRITICAL(&send_mux);valid=synced_valid;layer=lighting_layer;selected=synced_profiles[layer];host_ms=lighting_host_ms;host_received=lighting_host_received_us;receiver_seen=reported_app_at_us;layer_presses=layer_press_count;held=layer_held;transition_show=transition_animation;transition_start=transition_requested_us;target=transition_target;pair_start=pairing_animation_us;slot_start=slot_animation_us;slot_index=slot_animation_index;portEXIT_CRITICAL(&send_mux);
        bool new_press=layer_presses!=last_layer_press_count;last_layer_press_count=layer_presses;
        if(valid){if(memcmp(&engine.profile,&selected,sizeof(selected))){light_engine_set_profile(&engine,&selected);printf("LIGHT applied layer=%u effect=%u rgb=%u,%u,%u brightness=%u speed=%u flags=%u\n",(unsigned)(layer+1),(unsigned)selected.effect,(unsigned)selected.r,(unsigned)selected.g,(unsigned)selected.b,(unsigned)selected.brightness,(unsigned)selected.speed,(unsigned)selected.flags);}if(local_us>=host_received)now=host_ms+(uint64_t)(local_us-host_received)/1000;}
        uint16_t raw[12],base[12];bool ready;
        portENTER_CRITICAL(&send_mux);ready=lighting_sample_ready;memcpy(raw,lighting_raw,sizeof(raw));memcpy(base,lighting_baseline,sizeof(base));portEXIT_CRITICAL(&send_mux);
        if(ready)for(unsigned i=0;i<12;i++){
            int delta=(int)base[i]-(int)raw[i];
            bool candidate=physical_pressed[i] ? delta>30 : delta>=60;
            if(candidate!=candidate_pressed[i]){candidate_pressed[i]=candidate;candidate_since[i]=local_us;}
            if(candidate!=physical_pressed[i] && local_us-candidate_since[i]>=20000){physical_pressed[i]=candidate;if(candidate && !held)new_press=true;}
            light_engine_input(&engine,adc_to_light[i],physical_pressed[i]?1.0f:0.0f,now);
        }
        light_event_t event;while(xQueueReceive(light_events,&event,0)==pdTRUE){
            if(event.layer!=layer || (have_event && event.id==last_event))continue;
            have_event=true;last_event=event.id;
            if(event.op==3){bool pending=false;for(unsigned i=0;i<12;i++)if(adc_to_light[i]==event.key){pending=candidate_pressed[i]&&!physical_pressed[i];break;}
                light_engine_sync_physical_hit(&engine,event.key,event.id,now,pending);
            }else light_engine_virtual_hit(&engine,event.key,event.id,now);
        }
        light_rgb_t colors[12];light_engine_render(&engine,now,colors);
        bool receiver_ok=whe_receiver_present(receiver_seen,local_us);
        bool disconnected=transport.mode==WHE_TRANSPORT_ESPNOW ? !receiver_ok : !whe_ble_ready();
        bool warning_red=false;
        bool warning_active=whe_disconnected_warning_step(&warning,!disconnected,new_press,local_us,&warning_red);
        if(disconnected){
            unsigned level=0;
            if(warning_active){int64_t phase=(local_us-warning.started_us)%300000;
                if(phase<240000){int64_t rise=phase<120000?phase:240000-phase;
                    unsigned t=(unsigned)(rise*1000/120000);
                    level=(unsigned)((uint64_t)t*t*(3000-2*t)*100/1000000000ULL);}}
            for(unsigned i=0;i<12;i++)colors[i]=(light_rgb_t){.r=level,.g=transport.mode==WHE_TRANSPORT_BLE?level*38/100:0,.b=0};
        }
        // Blue breathing confirmation when the pairing window opens.
        int64_t pairing_elapsed=local_us-pair_start;
        if(pair_start>0 && pairing_elapsed>=0 && pairing_elapsed<1200000){
            int64_t phase=pairing_elapsed%600000;
            int64_t ramp=phase<300000?phase:600000-phase;
            unsigned level=(unsigned)(ramp*100/300000);
            for(unsigned i=0;i<12;i++)colors[i]=(light_rgb_t){.r=0,.g=level*20/100,.b=level};
        }
        // Selection confirmation stays visible while the BLE host reconnects.
        int64_t slot_elapsed=local_us-slot_start;
        if(slot_start>0 && slot_elapsed>=0 && slot_elapsed<900000){
            int64_t phase=slot_elapsed%450000;
            int64_t ramp=phase<225000?phase:450000-phase;
            unsigned t=(unsigned)(ramp*1000/225000);
            unsigned level=(unsigned)((uint64_t)t*t*(3000-2*t)*100/1000000000ULL);
            for(unsigned i=0;i<12;i++)colors[i]=(light_rgb_t){.r=0,.g=(i/4==slot_index)?level*20/100:0,.b=(i/4==slot_index)?level:0};
        }
        // Mode change always overrides the other effects.
        int64_t elapsed=local_us-transition_start;
        if(transition_show){
            int phase=elapsed>=0 && elapsed<800000?(int)(elapsed%400000)*400/400000-50:-999;
            for(unsigned i=0;i<12;i++){
                int distance=abs((int)(i/4)*100-phase);
                unsigned level=distance<100?(unsigned)(100-distance):0;
                colors[i]=(light_rgb_t){.r=0,.g=target==WHE_TRANSPORT_ESPNOW?level:0,.b=target==WHE_TRANSPORT_BLE?level:0};
            }
        }
        for(int i=0;i<12;i++)neopixel_set_pixel(light_to_led[i],colors[i].r,colors[i].g,colors[i].b);
        esp_err_t e = neopixel_refresh();
        if (e != ESP_OK) printf("LED refresh failed: %s\n", esp_err_to_name(e));
        vTaskDelay(pdMS_TO_TICKS((transition_show || warning_active || (pair_start>0 && pairing_elapsed<1200000) || (slot_start>0 && slot_elapsed<900000))?16:33));
    }
}
static void radio_init(void) {
    ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t wifi=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&wifi));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));ESP_ERROR_CHECK(esp_wifi_set_channel(6,WIFI_SECOND_CHAN_NONE));ESP_ERROR_CHECK(esp_now_init());ESP_ERROR_CHECK(esp_now_register_send_cb(radio_sent));ESP_ERROR_CHECK(esp_now_register_recv_cb(on_app_status));
    esp_now_peer_info_t peer={.channel=6,.ifidx=WIFI_IF_STA,.encrypt=false};memset(peer.peer_addr,0xff,6);ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}
void app_main(void) {
    light_events=xQueueCreate(16,sizeof(light_event_t));if(!light_events)abort();
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_open("whe_c6",NVS_READWRITE,&local_store));
    uint8_t mode=0;(void)nvs_get_u8(local_store,"transport",&mode);
    transport.mode=mode==1?WHE_TRANSPORT_BLE:WHE_TRANSPORT_ESPNOW;
    
    size_t size=sizeof(local_profile);
    local_profile_valid=nvs_get_blob(local_store,"hid_v2",local_profile,&size)==ESP_OK && size==sizeof(local_profile) && whe_profile_validate(local_profile);
    size=sizeof(synced_profiles);
    if(nvs_get_blob(local_store,"lights",synced_profiles,&size)==ESP_OK && size==sizeof(synced_profiles)){
        synced_valid=true;for(unsigned i=0;i<4;i++)if(!light_profile_valid(&synced_profiles[i]))synced_valid=false;
    }
    profile_queue=xQueueCreate(20,sizeof(profile_frame_t));assert(profile_queue);
    if(transport.mode==WHE_TRANSPORT_ESPNOW){radio_init();assert(xTaskCreate(profile_worker,"profile-store",4096,NULL,1,NULL)==pdPASS);}
    else whe_ble_start();
    printf("TRANSPORT %s profile=%s\n",mode==1?"BLE":"ESP-NOW",local_profile_valid?"saved":"missing");
    gpio_config_t mux = { .pin_bit_mask = (1ULL<<0)|(1ULL<<1)|(1ULL<<2)|(1ULL<<21), .mode = GPIO_MODE_OUTPUT };
    ESP_ERROR_CHECK(gpio_config(&mux));
    gpio_config_t layer_gpio={.pin_bit_mask=1ULL<<PIN_LAYER,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&layer_gpio));
    mux_select(0); /* Address pins LOW before an unpowered mux is isolated. */
#if CONFIG_WHE_POWER_GATE_EXPERIMENT
    /* Gate pull-up keeps the P-MOSFET off until explicitly driven LOW. */
    ESP_ERROR_CHECK(gpio_set_level(PIN_ANALOG_GATE,1));
    gpio_config_t gate={.pin_bit_mask=(1ULL<<PIN_ANALOG_GATE),.mode=GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&gate));ESP_ERROR_CHECK(gpio_set_level(PIN_ANALOG_GATE,1));
#endif
    whe_power_policy_t policy;whe_power_policy_init(&policy);
    whe_release_filter_t release_filter={0};
    if(!whe_power_policy_configure(&policy,CONFIG_WHE_WAKE_DELTA_ADC,CONFIG_WHE_RELEASE_DELTA_ADC)) abort();
    whe_tx_policy_t tx_policy={0};
    bool active=false;
    portENTER_CRITICAL(&send_mux);diagnostic_active=false;portEXIT_CRITICAL(&send_mux);
    spi_bus_config_t bus = { .sclk_io_num = PIN_SCK, .miso_io_num = PIN_MISO, .mosi_io_num = PIN_MOSI, .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 3 };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED));
    spi_device_interface_config_t config = { .clock_speed_hz = 1000000, .mode = 0, .spics_io_num = PIN_CS, .queue_size = 1 };
    spi_device_handle_t adc;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &config, &adc));
    latest_sample=xQueueCreate(1,sizeof(pending_sample_t)); if(!latest_sample) abort();
    if(transport.mode==WHE_TRANSPORT_ESPNOW && xTaskCreate(sender_loop,"latest-sender",3072,NULL,8,&sender_task)!=pdPASS) abort();
    if(xTaskCreate(diagnostic_task,"diagnostic",3072,NULL,1,NULL)!=pdPASS) abort();
    if(xTaskCreate(led_test_task,"led-test",3072,NULL,1,NULL)!=pdPASS) abort();
    sampling_task=xTaskGetCurrentTaskHandle();
    const esp_timer_create_args_t timer_args={.callback=sample_tick,.dispatch_method=ESP_TIMER_TASK,.name="sample-1khz",.skip_unhandled_events=false};
    esp_timer_handle_t timer;ESP_ERROR_CHECK(esp_timer_create(&timer_args,&timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer,10000));
    uint32_t sequence = 0;
    bool button_raw=false,button_stable=false;int64_t button_changed_us=esp_timer_get_time();uint8_t layer=0;
    whe_hid_engine_t ble_engine;whe_hid_reset(&ble_engine);
    uint8_t ble_layer=255;bool ble_connected=false,ble_armed=false;
    whe_hid_report_t previous_ble={0};bool ble_sent=false;int64_t next_ble=0;
    whe_ble_slot_chord_t slot_chord={0};bool slot_stable[3]={0},slot_candidate[3]={0};int64_t slot_changed[3]={0};uint8_t slot_consumed=0;
    bool pairing_cancelled=false,pairing_fired=false,pairing_candidate=false;int64_t pairing_down_us=0,pairing_candidate_us=0;
    bool switch_pending=false,mode_key=false,mode_candidate=false;int64_t mode_candidate_since=0;whe_transport_t switch_target=WHE_TRANSPORT_ESPNOW;
    int64_t released_since=0;
    while (1) {
        uint32_t ticks=ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        if(ticks>1 && active) sequence += ticks-1; /* idle scans are intentionally not all sent */
        int64_t scan_started=esp_timer_get_time();
        portENTER_CRITICAL(&send_mux);bool has_layer_command=layer_command_pending;uint8_t requested_layer=layer_command_value;layer_command_pending=false;portEXIT_CRITICAL(&send_mux);
        if(has_layer_command && requested_layer!=layer){layer=requested_layer;portENTER_CRITICAL(&send_mux);lighting_layer=layer;portEXIT_CRITICAL(&send_mux);}
        bool current_button=gpio_get_level(PIN_LAYER)==0,button_event=false;
        if(current_button!=button_raw){button_raw=current_button;button_changed_us=scan_started;}
        if(button_raw!=button_stable && scan_started-button_changed_us>=20000){button_stable=button_raw;button_event=true;portENTER_CRITICAL(&send_mux);layer_held=button_stable;portEXIT_CRITICAL(&send_mux);}
#if CONFIG_WHE_POWER_GATE_EXPERIMENT
        if(!active) {
            analog_gate_set(true);
            esp_rom_delay_us(CONFIG_WHE_SENSOR_STARTUP_US);
        }
#endif
        uint16_t readings[12];
        for (unsigned ch = 0; ch < 12; ch++) {
            mux_select(ch);
            // Sample once after the mux settling delay. Cross-channel settling
            // must also be checked while switches move on the physical board.
            readings[ch] = adc_read(adc);
        }
        /* Confirm only candidate wake channels inside the same 10 ms scan.
           Median rejects a single ADC spike without waiting for the next scan. */
        if(!active && policy.initialized) {
            for(unsigned ch=0;ch<12;ch++) {
                if(abs((int)readings[ch]-(int)policy.baseline[ch]) >= (int)policy.wake_delta) {
                    uint16_t a=readings[ch];mux_select(ch);uint16_t b=adc_read(adc);uint16_t c=adc_read(adc);
                    readings[ch]=(a>b) ? ((b>c)?b:((a>c)?c:a)) : ((a>c)?a:((b>c)?c:b));
                }
            }
        }
        uint16_t policy_readings[12];whe_release_filter_update(&release_filter,readings,policy_readings,active);
        bool next_active=whe_power_policy_update(&policy,policy_readings,esp_timer_get_time());
        portENTER_CRITICAL(&send_mux);memcpy(lighting_raw,readings,sizeof(readings));memcpy(lighting_baseline,policy.baseline,sizeof(policy.baseline));lighting_sample_ready=policy.initialized;portEXIT_CRITICAL(&send_mux);
        int key_delta=abs((int)readings[MODE_KEY_ADC]-(int)policy.baseline[MODE_KEY_ADC]);
        bool candidate=policy.initialized && key_delta>(mode_key?80:120);
        if(candidate!=mode_candidate){mode_candidate=candidate;mode_candidate_since=scan_started;}
        if(candidate!=mode_key && scan_started-mode_candidate_since>=20000)mode_key=candidate;
        if(transport.mode==WHE_TRANSPORT_BLE){
            if(button_event && button_stable){pairing_down_us=scan_started;pairing_cancelled=false;pairing_fired=false;pairing_candidate=false;}
            if(button_stable){
                bool motion=false;for(unsigned i=0;i<12;i++)if(abs((int)readings[i]-(int)policy.baseline[i])>120)motion=true;
                if(motion!=pairing_candidate){pairing_candidate=motion;pairing_candidate_us=scan_started;}
                if(pairing_candidate && scan_started-pairing_candidate_us>=20000)pairing_cancelled=true;
                if(!pairing_cancelled && !pairing_fired && scan_started-pairing_down_us>=3000000){
                    pairing_fired=true;transport.chord_used=true;portENTER_CRITICAL(&send_mux);pairing_animation_us=scan_started;portEXIT_CRITICAL(&send_mux);whe_ble_pairing_start();
                }
            }
        }
        if(transport.mode==WHE_TRANSPORT_BLE){
            uint8_t held_mask=0;
            for(unsigned i=0;i<3;i++){
                unsigned ch=whe_ble_slot_channels[i];int d=abs((int)readings[ch]-(int)policy.baseline[ch]);
                bool v=policy.initialized && d>(slot_stable[i]?80:120);
                if(v!=slot_candidate[i]){slot_candidate[i]=v;slot_changed[i]=scan_started;}
                if(v!=slot_stable[i] && scan_started-slot_changed[i]>=20000)slot_stable[i]=v;
                if(slot_stable[i])held_mask|=1u<<i;else slot_consumed&=(uint8_t)~(1u<<i);
            }
            int chosen=whe_ble_slot_chord_step(&slot_chord,button_stable,held_mask);
            if(chosen>=0 && !switch_pending){
                slot_consumed|=1u<<chosen;transport.chord_used=true;pairing_cancelled=true;
                portENTER_CRITICAL(&send_mux);slot_animation_us=scan_started;slot_animation_index=(uint8_t)chosen;portEXIT_CRITICAL(&send_mux);
                esp_err_t e=whe_ble_select_slot((unsigned)chosen);if(e!=ESP_OK)printf("BLE slot error: %s\n",esp_err_to_name(e));
            }
        }
        whe_transport_action_t action=whe_transport_input(&transport,button_stable,mode_key);
        if(action.layer_tap && !switch_pending){layer=(layer+1u)%4u;button_event=true;portENTER_CRITICAL(&send_mux);lighting_layer=layer;++layer_press_count;portEXIT_CRITICAL(&send_mux);}
        if(action.switch_requested && !switch_pending){
            if(action.target==WHE_TRANSPORT_ESPNOW || local_profile_valid){switch_pending=true;switch_target=action.target;portENTER_CRITICAL(&send_mux);transition_animation=true;transition_requested_us=scan_started;transition_target=action.target;portEXIT_CRITICAL(&send_mux);printf("MODE requested %u; release all keys\n",(unsigned)switch_target);}
            else printf("BLE switch refused: save a profile through S3 first\n");
        }
        uint16_t output_readings[12];memcpy(output_readings,readings,sizeof(readings));
        if(button_stable||action.suppress_key||switch_pending||slot_consumed){
            for(unsigned i=0;i<12;i++)if(switch_pending||(i==MODE_KEY_ADC && (button_stable||action.suppress_key)) || ((button_stable || slot_consumed) && ((i==whe_ble_slot_channels[0] && (button_stable || (slot_consumed&1))) || (i==whe_ble_slot_channels[1] && (button_stable || (slot_consumed&2))) || (i==whe_ble_slot_channels[2] && (button_stable || (slot_consumed&4)))))){
                const uint8_t *record=local_profile+(layer*12+i)*WHE_PROFILE_RECORD_SIZE;
                output_readings[i]=local_profile_valid&&record[4]?whe_get16(record):policy.baseline[i];
            }
        }
        if(transport.mode==WHE_TRANSPORT_BLE){
            bool connected=whe_ble_ready();
            if(connected!=ble_connected){printf("BLE encrypted=%u\n",connected);ble_connected=connected;ble_armed=false;ble_sent=false;ble_layer=255;}
            if(ble_layer!=layer){
                // Deliver neutral reports before changing a layer; retry on congestion.
                uint8_t empty[6]={0};
                if(!connected || (whe_ble_keyboard(0,empty)==ESP_OK && whe_ble_consumer(0)==ESP_OK)){
                    if(local_profile_valid)whe_profile_load_layer(local_profile,layer,&ble_engine);
                    ble_layer=layer;ble_armed=false;ble_sent=false;
                }
            }
            bool all_up=true;for(unsigned i=0;i<12;i++)if(abs((int)readings[i]-(int)policy.baseline[i])>150)all_up=false;
            if(connected && all_up && !ble_armed){ble_armed=true;printf("BLE armed layer=%u\n",layer);}
            whe_hid_sample(&ble_engine,output_readings,scan_started);
            whe_hid_report_t report=whe_hid_report(&ble_engine,scan_started,connected&&ble_armed&&ble_layer==layer&&!switch_pending);
            if(connected && scan_started>=next_ble){
                bool ok=true;
                if(!ble_sent||report.modifier!=previous_ble.modifier||memcmp(report.keys,previous_ble.keys,6)){
                    if(whe_ble_keyboard(report.modifier,report.keys)==ESP_OK){previous_ble.modifier=report.modifier;memcpy(previous_ble.keys,report.keys,6);}else ok=false;
                }
                if(!ble_sent||report.consumer!=previous_ble.consumer){if(whe_ble_consumer(report.consumer)==ESP_OK)previous_ble.consumer=report.consumer;else ok=false;}
                ble_sent=ok;next_ble=scan_started+7500;
            }
        }
        if(switch_pending){
            bool all_up=!button_stable;for(unsigned i=0;i<12;i++)if(abs((int)readings[i]-(int)policy.baseline[i])>150)all_up=false;
            if(!all_up)released_since=0;else if(!released_since)released_since=scan_started;
            if(released_since && scan_started-released_since>300000 && scan_started-transition_requested_us>=800000){
                // NVS offsets are preserved by the enlarged factory partition.
                esp_err_t e=nvs_set_u8(local_store,"transport",(uint8_t)switch_target);
                if(e==ESP_OK && synced_valid)e=nvs_set_blob(local_store,"lights",synced_profiles,sizeof(synced_profiles));
                if(e==ESP_OK)e=nvs_commit(local_store);
                if(e==ESP_OK)esp_restart();
                printf("MODE save failed: %s\n",esp_err_to_name(e));switch_pending=false;portENTER_CRITICAL(&send_mux);transition_animation=false;portEXIT_CRITICAL(&send_mux);
            }
        }
        uint16_t max_delta=0;unsigned max_ch=0;
        for(unsigned i=0;i<12;i++){unsigned delta=(unsigned)abs((int)readings[i]-(int)policy.baseline[i]);if(delta>max_delta){max_delta=delta;max_ch=i;}}
        portENTER_CRITICAL(&send_mux);diag_delta=max_delta;diag_channel=max_ch;diag_baseline=policy.baseline[max_ch];diag_value=readings[max_ch];portEXIT_CRITICAL(&send_mux);
#if CONFIG_WHE_POWER_GATE_EXPERIMENT
        if(!next_active) {
            mux_select(0); /* Never leave address outputs HIGH while mux VCC is off. */
            analog_gate_set(false);
        }
#endif
        portENTER_CRITICAL(&send_mux);bool app_open=reported_app_open;int64_t app_seen=reported_app_at_us;portEXIT_CRITICAL(&send_mux);
        bool send_now=whe_tx_policy_should_send(&tx_policy,active,next_active,whe_app_present(app_open,app_seen,esp_timer_get_time()),esp_timer_get_time());
        send_now=send_now || button_event;
        uint32_t elapsed=(uint32_t)(esp_timer_get_time()-scan_started);
        portENTER_CRITICAL(&send_mux);
        ++scans;if(ticks>1)missed_ticks+=ticks-1;if(elapsed>max_scan_us)max_scan_us=elapsed;
        portEXIT_CRITICAL(&send_mux);
        if(send_now && transport.mode==WHE_TRANSPORT_ESPNOW) {
            pending_sample_t sample={.captured_us=scan_started};
            portENTER_CRITICAL(&send_mux);whe_sensor_status_t status=rate_snapshot;uint32_t gate_on_us=gate_on_snapshot_us;portEXIT_CRITICAL(&send_mux);
            status.active=next_active;status.app_open=whe_app_present(app_open,app_seen,esp_timer_get_time());
            status.layer=layer;status.layer_pressed=button_stable;
#if CONFIG_WHE_POWER_GATE_EXPERIMENT
            status.gate_enabled=true;
#else
            status.gate_enabled=false;
#endif
            whe_sensor_v3_encode(sample.frame,sequence,output_readings,&status,gate_on_us);
            /* Approximate count: the sender may consume between this check and overwrite. */
            if(uxQueueMessagesWaiting(latest_sample)) {
                portENTER_CRITICAL(&send_mux);++send_busy;portEXIT_CRITICAL(&send_mux);
            }
            (void)xQueueOverwrite(latest_sample,&sample);
            xTaskNotify(sender_task,SEND_WAKE_SAMPLE,eSetBits);
        }
        // Sequence counts radio opportunities, not the intentionally unsent idle scans.
        if(send_now)++sequence;
        if(active!=next_active) {
            bool was_active=active;active=next_active;
            ESP_ERROR_CHECK(esp_timer_stop(timer));
            uint32_t pending=ulTaskNotifyTake(pdTRUE,0);
            if(was_active)sequence+=pending;
            portENTER_CRITICAL(&send_mux);diagnostic_active=active;missed_ticks+=pending;portEXIT_CRITICAL(&send_mux);
            ESP_ERROR_CHECK(esp_timer_start_periodic(timer,active ? 1000 : 10000));
        }
    }
}
