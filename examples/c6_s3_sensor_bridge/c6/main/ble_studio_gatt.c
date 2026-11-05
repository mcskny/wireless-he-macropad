#include "ble_studio_gatt.h"
#include "ble_transport.h"
#include "host/ble_gatt.h"
#include "host/ble_att.h"
#include "host/ble_hs.h"
#include "os/os_mbuf.h"
/* Service: C49B0001-6A38-46AF-A8F1-2996DF5A00A1; command: ...0002... */
static const ble_uuid128_t service_uuid=BLE_UUID128_INIT(0xa1,0x00,0x5a,0xdf,0x96,0x29,0xf1,0xa8,0xaf,0x46,0x38,0x6a,0x01,0x00,0x9b,0xc4);
static const ble_uuid128_t command_uuid=BLE_UUID128_INIT(0xa1,0x00,0x5a,0xdf,0x96,0x29,0xf1,0xa8,0xaf,0x46,0x38,0x6a,0x02,0x00,0x9b,0xc4);
static const ble_uuid128_t slots_uuid=BLE_UUID128_INIT(0xa1,0x00,0x5a,0xdf,0x96,0x29,0xf1,0xa8,0xaf,0x46,0x38,0x6a,0x03,0x00,0x9b,0xc4);
static int command_access(uint16_t connection,uint16_t attr,struct ble_gatt_access_ctxt *ctxt,void *arg){
 (void)connection;(void)attr;(void)arg;
 if(ctxt->op!=BLE_GATT_ACCESS_OP_WRITE_CHR || !whe_ble_ready())return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
 uint8_t frame[20];uint16_t n=0;
 if(ble_hs_mbuf_to_flat(ctxt->om,frame,sizeof(frame),&n)!=0)return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
 return whe_studio_apply(frame,n)?0:BLE_ATT_ERR_UNLIKELY;
}
static int slots_access(uint16_t conn,uint16_t attr,struct ble_gatt_access_ctxt *ctxt,void *arg){
 (void)conn;(void)attr;(void)arg;if(ctxt->op!=BLE_GATT_ACCESS_OP_READ_CHR || !whe_ble_ready())return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
 uint8_t snapshot[4];whe_ble_slot_snapshot(snapshot);snapshot[3]=whe_studio_current_layer();return os_mbuf_append(ctxt->om,snapshot,sizeof(snapshot))==0?0:BLE_ATT_ERR_INSUFFICIENT_RES;
}
static const struct ble_gatt_svc_def services[]={
 {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=&service_uuid.u,.characteristics=(struct ble_gatt_chr_def[]){
 {.uuid=&command_uuid.u,.access_cb=command_access,.flags=BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_WRITE_ENC},
 {.uuid=&slots_uuid.u,.access_cb=slots_access,.flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_READ_ENC},{0}}},
 {0}
};
int whe_studio_gatt_init(void){int rc=ble_gatts_count_cfg(services);return rc?rc:ble_gatts_add_svcs(services);}
