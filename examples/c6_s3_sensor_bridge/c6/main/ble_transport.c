#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_bt.h"
#include "ble_transport.h"
#include "ble_studio_gatt.h"
#include <stdatomic.h>
#include "esp_timer.h"
#include "esp_hidd.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "host/ble_gap.h"
#include "nimble/hci_common.h"
#include "services/gap/ble_svc_gap.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG="whe_ble_lab";
static esp_hidd_dev_t *device;
static uint16_t connection=BLE_HS_CONN_HANDLE_NONE;
static atomic_bool encrypted;
static uint8_t own_addr_type;
static atomic_llong pairing_until;
static esp_timer_handle_t pairing_timer;
static nvs_handle_t slot_nvs;
static uint8_t selected_slot;
typedef struct {uint8_t valid;ble_addr_t addr;} saved_slot_t;
static saved_slot_t slots[3];
static bool addr_equal(const ble_addr_t *a,const ble_addr_t *b){return a->type==b->type && memcmp(a->val,b->val,6)==0;}
static esp_err_t save_slots(void){esp_err_t e=nvs_set_blob(slot_nvs,"slots",slots,sizeof(slots));return e==ESP_OK?nvs_commit(slot_nvs):e;}


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
 .vendor_id=0,.product_id=0,.version=0x0100,.device_name="Wireless HE",
 .manufacturer_name="Wireless HE",.serial_number="C6-BLE",.report_maps=maps,.report_maps_len=1
};

// Callers must arrange exclusive transport ownership and a neutral report on
// switching. This laboratory currently has no physical-key caller.
esp_err_t whe_ble_keyboard(uint8_t modifiers,const uint8_t keys[6]){
 if(!device||!keys||connection==BLE_HS_CONN_HANDLE_NONE||!encrypted)return ESP_ERR_INVALID_STATE;
 uint8_t report[8]={modifiers,0};memcpy(report+2,keys,6);
 return esp_hidd_dev_input_set(device,0,1,report,sizeof(report));
}
esp_err_t whe_ble_consumer(uint16_t usage){
 if(!device||connection==BLE_HS_CONN_HANDLE_NONE||!encrypted)return ESP_ERR_INVALID_STATE;
 uint8_t report[2]={(uint8_t)usage,(uint8_t)(usage>>8)};
 return esp_hidd_dev_input_set(device,0,2,report,sizeof(report));
}

static void advertise(void);
static int gap_event(struct ble_gap_event *event,void *arg){
 (void)arg;
 if(event->type==BLE_GAP_EVENT_CONNECT){
  ESP_LOGI(TAG,"connect status=%d",event->connect.status);
  if(event->connect.status==0){
   uint16_t new_handle=event->connect.conn_handle;
   bool already_ready=(connection==new_handle && encrypted);
   connection=new_handle;if(!already_ready)encrypted=false;
   struct ble_gap_conn_desc peer;
   if(ble_gap_conn_find(connection,&peer)==0){
    bool wrong_slot=false;
    for(unsigned i=0;i<3;i++)if(i!=selected_slot && slots[i].valid && addr_equal(&slots[i].addr,&peer.peer_id_addr))wrong_slot=true;
    if(wrong_slot){ESP_LOGW(TAG,"rejecting host assigned to another slot");
     int rc=ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);
     if(rc)ESP_LOGE(TAG,"terminate wrong host failed: %d",rc);
     return 0;
    }
   }
   if(!already_ready){int rc=ble_gap_security_initiate(connection);if(rc && rc!=BLE_HS_EALREADY)ESP_LOGW(TAG,"security request %d",rc);}}
  else advertise();
 }else if(event->type==BLE_GAP_EVENT_ENC_CHANGE){
  struct ble_gap_conn_desc desc;
  if(event->enc_change.conn_handle!=connection){
   if(connection!=BLE_HS_CONN_HANDLE_NONE || event->enc_change.status!=0 || ble_gap_conn_find(event->enc_change.conn_handle,&desc)!=0){ESP_LOGW(TAG,"ignoring stale encryption event");return 0;}
   /* Bonded hosts can complete encryption before the application receives CONNECT. */
   connection=event->enc_change.conn_handle;
  }
  ESP_LOGI(TAG,"encryption status=%d",event->enc_change.status);
  bool ok=event->enc_change.status==0 && ble_gap_conn_find(event->enc_change.conn_handle,&desc)==0 && desc.sec_state.encrypted && desc.sec_state.bonded;
  if(ok){
   unsigned slot=selected_slot;
   if(slots[slot].valid && !addr_equal(&slots[slot].addr,&desc.peer_id_addr)){ESP_LOGW(TAG,"host is not selected slot");ok=false;}
   else if(!slots[slot].valid){
    if(esp_timer_get_time()>=atomic_load(&pairing_until)){ESP_LOGW(TAG,"empty slot requires pairing window");ok=false;}
    else {for(unsigned i=0;i<3;i++)if(i!=slot && slots[i].valid && addr_equal(&slots[i].addr,&desc.peer_id_addr)){ok=false;break;}
     if(!ok){ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);encrypted=false;return 0;}
     slots[slot].valid=1;slots[slot].addr=desc.peer_id_addr;
     if(save_slots()!=ESP_OK){slots[slot].valid=0;ok=false;}else ESP_LOGI(TAG,"saved host in slot %u",slot+1);}
   }
  }
  encrypted=ok;
  if(!ok && connection!=BLE_HS_CONN_HANDLE_NONE)ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);
 }else if(event->type==BLE_GAP_EVENT_REPEAT_PAIRING){
  struct ble_gap_conn_desc peer;
  if(esp_timer_get_time()<atomic_load(&pairing_until) &&
     ble_gap_conn_find(event->repeat_pairing.conn_handle,&peer)==0 &&
     slots[selected_slot].valid && addr_equal(&slots[selected_slot].addr,&peer.peer_id_addr) &&
     ble_store_util_delete_peer(&peer.peer_id_addr)==0){
    ESP_LOGI(TAG,"renewing selected host bond during pairing window");return BLE_GAP_REPEAT_PAIRING_RETRY;
  }
  ESP_LOGW(TAG,"repeat pairing denied outside selected host pairing window");return BLE_GAP_REPEAT_PAIRING_IGNORE;
 }else if(event->type==BLE_GAP_EVENT_DISCONNECT){
  if(event->disconnect.conn.conn_handle!=connection){ESP_LOGW(TAG,"ignoring stale disconnect");return 0;}
  ESP_LOGW(TAG,"disconnected reason=%d",event->disconnect.reason);whe_studio_disconnected();connection=BLE_HS_CONN_HANDLE_NONE;encrypted=false;advertise();}
 else if(event->type==BLE_GAP_EVENT_ADV_COMPLETE)advertise();
 return 0;
}
static void advertise(void){
 struct ble_hs_adv_fields fields={0};
 static const ble_uuid16_t hid_uuid=BLE_UUID16_INIT(0x1812);
 const char *name="Wireless HE";
 fields.flags=BLE_HS_ADV_F_BREDR_UNSUP;
 if(esp_timer_get_time()<atomic_load(&pairing_until))fields.flags|=BLE_HS_ADV_F_DISC_GEN;
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
 if(id==ESP_HIDD_START_EVENT){
  if(!slots[0].valid && !slots[1].valid && !slots[2].valid){
   ble_addr_t peers[3];int count=0;
   if(ble_store_util_bonded_peers(peers,&count,3)==0 && count==1){slots[0].valid=1;slots[0].addr=peers[0];if(save_slots()!=ESP_OK)slots[0].valid=0;else ESP_LOGI(TAG,"existing bond assigned to slot 1");}
  }
  int rc=ble_hs_id_infer_auto(0,&own_addr_type);if(!rc)advertise();else ESP_LOGE(TAG,"address %d",rc);
 }
}
static void host_task(void *arg){(void)arg;nimble_port_run();nimble_port_freertos_deinit();}
void ble_store_config_init(void);

void whe_ble_start(void){
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
 ESP_ERROR_CHECK(nvs_open("whe_ble_slots",NVS_READWRITE,&slot_nvs));
 size_t n=sizeof(slots);if(nvs_get_blob(slot_nvs,"slots",slots,&n)!=ESP_OK || n!=sizeof(slots))memset(slots,0,sizeof(slots));
 if(nvs_get_u8(slot_nvs,"selected",&selected_slot)!=ESP_OK || selected_slot>=3)selected_slot=0;
 ESP_ERROR_CHECK(esp_hidd_dev_init(&config,ESP_HID_TRANSPORT_BLE,hid_event,&device));
 if(ble_svc_gap_device_name_set("Wireless HE")!=0) abort();
 if(whe_studio_gatt_init()!=0)abort();
 nimble_port_freertos_init(host_task);
 ESP_LOGI(TAG,"BLE HID started; encrypted Just Works bonding (no MITM authentication)");
 (void)connection;(void)encrypted;
}

bool whe_ble_ready(void){return atomic_load(&encrypted);}

static void pairing_expired(void *arg){
 (void)arg;atomic_store(&pairing_until,0);
 if(connection==BLE_HS_CONN_HANDLE_NONE){if(ble_gap_adv_active())ble_gap_adv_stop();advertise();}
}
void whe_ble_pairing_start(void){
 if(!pairing_timer){esp_timer_create_args_t args={.callback=pairing_expired,.name="ble-pair-window"};if(esp_timer_create(&args,&pairing_timer)!=ESP_OK)return;}
 esp_timer_stop(pairing_timer);esp_timer_start_once(pairing_timer,60000000);
 atomic_store(&pairing_until,esp_timer_get_time()+60000000LL);
 if(connection!=BLE_HS_CONN_HANDLE_NONE){int rc=ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);if(rc)ESP_LOGW(TAG,"pairing disconnect: %d",rc);}
 else {if(ble_gap_adv_active())ble_gap_adv_stop();advertise();}
 ESP_LOGI(TAG,"pairing discovery enabled for 60 seconds");
}

esp_err_t whe_ble_select_slot(unsigned slot){
 if(slot>=3)return ESP_ERR_INVALID_ARG;
 if(slot==selected_slot && slots[slot].valid)return ESP_OK;
 esp_err_t e=nvs_set_u8(slot_nvs,"selected",slot);if(e==ESP_OK)e=nvs_commit(slot_nvs);if(e!=ESP_OK)return e;
 if(connection!=BLE_HS_CONN_HANDLE_NONE && encrypted){uint8_t empty[6]={0};whe_ble_keyboard(0,empty);whe_ble_consumer(0);}
 selected_slot=slot;encrypted=false;
 ESP_LOGI(TAG,"select host slot %u",slot+1);
 if(!slots[slot].valid)whe_ble_pairing_start();
 else if(connection!=BLE_HS_CONN_HANDLE_NONE)ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);
 else {if(ble_gap_adv_active())ble_gap_adv_stop();advertise();}
 return ESP_OK;
}

void whe_ble_slot_snapshot(uint8_t out[3]){
 out[0]=1;out[1]=selected_slot;out[2]=0;
 for(unsigned i=0;i<3;i++)if(slots[i].valid)out[2]|=(uint8_t)(1u<<i);
}
