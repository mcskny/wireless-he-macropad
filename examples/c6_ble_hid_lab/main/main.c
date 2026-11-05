// Isolated HOGP transport experiment. No sensor input or mode switching yet.
// Do not flash over the working sensor bridge for normal use.
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_bt.h"
#include "esp_hidd.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG="whe_ble_lab";
static esp_hidd_dev_t *device;
static uint16_t connection=BLE_HS_CONN_HANDLE_NONE;
static bool encrypted;
static uint8_t own_addr_type;

// HID report IDs 1=6KRO keyboard, 2=consumer usage (16 bits).
static const uint8_t report_map[]={
 0x05,0x01,0x09,0x06,0xa1,0x01,0x85,0x01,0x05,0x07,0x19,0xe0,0x29,0xe7,
 0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x08,0x81,0x02,0x95,0x01,0x75,0x08,
 0x81,0x03,0x95,0x06,0x75,0x08,0x15,0x00,0x25,0x65,0x05,0x07,0x19,0x00,
 0x29,0x65,0x81,0x00,0xc0,
 0x05,0x0c,0x09,0x01,0xa1,0x01,0x85,0x02,0x15,0x00,0x26,0xff,0x03,
 0x19,0x00,0x2a,0xff,0x03,0x75,0x10,0x95,0x01,0x81,0x00,0xc0
};
static esp_hid_raw_report_map_t maps[]={{.data=report_map,.len=sizeof(report_map)}};
static const esp_hid_device_config_t config={
 .vendor_id=0,.product_id=0,.version=0x0100,.device_name="Wireless HE Lab",
 .manufacturer_name="Wireless HE",.serial_number="BLE-LAB",.report_maps=maps,.report_maps_len=1
};

// Callers must arrange exclusive transport ownership and a neutral report on
// switching. This laboratory currently has no physical-key caller.
esp_err_t whe_ble_lab_keyboard(uint8_t modifiers,const uint8_t keys[6]){
 if(!device||!keys||connection==BLE_HS_CONN_HANDLE_NONE||!encrypted)return ESP_ERR_INVALID_STATE;
 uint8_t report[8]={modifiers,0};memcpy(report+2,keys,6);
 return esp_hidd_dev_input_set(device,0,1,report,sizeof(report));
}
esp_err_t whe_ble_lab_consumer(uint16_t usage){
 if(!device||connection==BLE_HS_CONN_HANDLE_NONE||!encrypted)return ESP_ERR_INVALID_STATE;
 uint8_t report[2]={(uint8_t)usage,(uint8_t)(usage>>8)};
 return esp_hidd_dev_input_set(device,0,2,report,sizeof(report));
}

static void advertise(void);
static int gap_event(struct ble_gap_event *event,void *arg){
 (void)arg;
 if(event->type==BLE_GAP_EVENT_CONNECT){
  if(event->connect.status==0){connection=event->connect.conn_handle;encrypted=false;
   int rc=ble_gap_security_initiate(connection);if(rc)ESP_LOGW(TAG,"security request %d",rc);}
  else advertise();
 }else if(event->type==BLE_GAP_EVENT_ENC_CHANGE){
  struct ble_gap_conn_desc desc;
  encrypted=event->enc_change.status==0 && ble_gap_conn_find(event->enc_change.conn_handle,&desc)==0 && desc.sec_state.encrypted;
 }else if(event->type==BLE_GAP_EVENT_DISCONNECT){connection=BLE_HS_CONN_HANDLE_NONE;encrypted=false;advertise();}
 else if(event->type==BLE_GAP_EVENT_ADV_COMPLETE)advertise();
 return 0;
}
static void advertise(void){
 struct ble_hs_adv_fields fields={0};
 static const ble_uuid16_t hid_uuid=BLE_UUID16_INIT(0x1812);
 const char *name="Wireless HE Lab";
 fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
 fields.name=(const uint8_t *)name;fields.name_len=(uint8_t)strlen(name);fields.name_is_complete=1;
 fields.uuids16=(ble_uuid16_t *)&hid_uuid;fields.num_uuids16=1;fields.uuids16_is_complete=1;
 fields.appearance=ESP_HID_APPEARANCE_KEYBOARD;fields.appearance_is_present=1;
 int rc=ble_gap_adv_set_fields(&fields);if(rc){ESP_LOGE(TAG,"adv fields %d",rc);return;}
 struct ble_gap_adv_params params={0};params.conn_mode=BLE_GAP_CONN_MODE_UND;params.disc_mode=BLE_GAP_DISC_MODE_GEN;
 params.itvl_min=BLE_GAP_ADV_ITVL_MS(30);params.itvl_max=BLE_GAP_ADV_ITVL_MS(50);
 rc=ble_gap_adv_start(own_addr_type,NULL,BLE_HS_FOREVER,&params,gap_event,NULL);
 if(rc)ESP_LOGE(TAG,"advertise %d",rc);
}
static void hid_event(void *arg,esp_event_base_t base,int32_t id,void *data){
 (void)arg;(void)base;(void)data;
 if(id==ESP_HIDD_START_EVENT){int rc=ble_hs_id_infer_auto(0,&own_addr_type);if(!rc)advertise();else ESP_LOGE(TAG,"address %d",rc);}
}
static void host_task(void *arg){(void)arg;nimble_port_run();nimble_port_freertos_deinit();}
void ble_store_config_init(void);

void app_main(void){
 // Never erase the existing partition on an initialization error.
 ESP_ERROR_CHECK(nvs_flash_init());
 esp_bt_controller_config_t bt=BT_CONTROLLER_INIT_CONFIG_DEFAULT();
 ESP_ERROR_CHECK(esp_bt_controller_init(&bt));
 ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
 ESP_ERROR_CHECK(esp_nimble_init());
 // Prototype: encrypted bonding using Just Works; it does not provide MITM protection.
 ble_hs_cfg.sm_io_cap=BLE_HS_IO_NO_INPUT_OUTPUT;ble_hs_cfg.sm_bonding=1;ble_hs_cfg.sm_sc=1;ble_hs_cfg.sm_mitm=0;
 ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ID|BLE_SM_PAIR_KEY_DIST_ENC;
 ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ID|BLE_SM_PAIR_KEY_DIST_ENC;
 ble_store_config_init();ble_hs_cfg.store_status_cb=ble_store_util_status_rr;
 ESP_ERROR_CHECK(esp_hidd_dev_init(&config,ESP_HID_TRANSPORT_BLE,hid_event,&device));
 nimble_port_freertos_init(host_task);
 ESP_LOGI(TAG,"HOGP lab only: no sensor or profile integration; pairing is not MITM-protected");
 (void)connection;(void)encrypted;
}
