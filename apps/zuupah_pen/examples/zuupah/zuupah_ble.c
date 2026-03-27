/**
 * zuupah_ble.c
 * BLE GATT Server für den Zuupah Pen
 * Basiert direkt auf dem JieLi SDK ble_trans.c Pattern
 */

#include "system/app_core.h"
#include "system/includes.h"
#include "app_config.h"
#include "app_action.h"
#include "btstack/btstack_task.h"
#include "btstack/bluetooth.h"
#include "user_cfg.h"
#include "vm.h"
#include "btcontroller_modules.h"
#include "bt_common.h"
#include "3th_profile_api.h"
#include "le_common.h"
#include "rcsp_bluetooth.h"
#include "JL_rcsp_api.h"
#include "custom_cfg.h"
#include "btstack/btstack_event.h"
#include "gatt_common/le_gatt_common.h"
#include "string.h"

#include "zuupah_ble.h"
#include "zuupah_ble_profile.h"

#define LOG_TAG "[ZUUPAH_BLE]"
#define log_info(x, ...)  printf(LOG_TAG x "\n", ##__VA_ARGS__)

/* ─── Advertising Data ───────────────────────────────────────────────────────*/
#define ADV_INTERVAL_MIN    (160 * 1)   /* 100ms (unit: 0.625ms) */

static u8  zuupah_adv_data[ADV_RSP_PACKET_MAX];
static u8  zuupah_scan_rsp_data[ADV_RSP_PACKET_MAX];
static u16 zuupah_con_handle = 0;
static adv_cfg_t zuupah_server_adv_config;

/* ─── Forward Declarations ───────────────────────────────────────────────────*/
static uint16_t zuupah_att_read_callback(hci_con_handle_t connection_handle,
                                          uint16_t att_handle, uint16_t offset,
                                          uint8_t *buffer, uint16_t buffer_size);
static int zuupah_att_write_callback(hci_con_handle_t connection_handle,
                                      uint16_t att_handle, uint16_t transaction_mode,
                                      uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
static int zuupah_event_packet_handler(int event, u8 *packet, u16 size, u8 *ext_param);

/* ─── GATT Server Config ─────────────────────────────────────────────────────*/
const gatt_server_cfg_t zuupah_server_init_cfg = {
    .att_read_cb        = &zuupah_att_read_callback,
    .att_write_cb       = &zuupah_att_write_callback,
    .event_packet_handler = &zuupah_event_packet_handler,
};

static gatt_ctrl_t zuupah_gatt_control_block = {
    .mtu_size       = 247,
    .cbuffer_size   = 2 * (ATT_PACKET_HEAD_SIZE + 247),
    .multi_dev_flag = 0,
    .server_config  = &zuupah_server_init_cfg,
    .client_config  = NULL,
    .sm_config      = NULL,
    .hci_cb_packet_handler = NULL,
};

/* ─── ATT Read Callback ──────────────────────────────────────────────────────
   buffer==NULL: nur Länge zurückgeben
   buffer!=NULL: Daten kopieren, Länge zurückgeben
*/
static uint16_t zuupah_att_read_callback(hci_con_handle_t connection_handle,
                                          uint16_t att_handle, uint16_t offset,
                                          uint8_t *buffer, uint16_t buffer_size)
{
    uint16_t att_value_len = 0;

    switch (att_handle) {

    case ZUUPAH_ATT_BATTERY_VALUE_HANDLE: {
        extern u8 zuupah_get_battery_percent(void);
        static u8 bat_val;
        bat_val = zuupah_get_battery_percent();
        att_value_len = 1;
        if (buffer) {
            buffer[0] = bat_val;
        }
        break;
    }

    case ZUUPAH_ATT_STORAGE_FREE_VALUE_HANDLE: {
        extern u32 zuupah_get_storage_free_mb(void);
        static u32 free_mb;
        free_mb = zuupah_get_storage_free_mb();
        att_value_len = 4;
        if (buffer) {
            memcpy(buffer, &free_mb, 4);
        }
        break;
    }

    case ZUUPAH_ATT_STORAGE_TOTAL_VALUE_HANDLE: {
        extern u32 zuupah_get_storage_total_mb(void);
        static u32 total_mb;
        total_mb = zuupah_get_storage_total_mb();
        att_value_len = 4;
        if (buffer) {
            memcpy(buffer, &total_mb, 4);
        }
        break;
    }

    case ZUUPAH_ATT_CURRENT_BOOK_VALUE_HANDLE: {
        extern const char *zuupah_get_current_book_id(void);
        const char *bid = zuupah_get_current_book_id();
        if (!bid) { bid = ""; }
        att_value_len = strlen(bid);
        if (buffer && att_value_len) {
            memcpy(buffer, bid, att_value_len);
        }
        break;
    }

    /* CCC handles */
    case ZUUPAH_ATT_BATTERY_CCC_HANDLE:
    case ZUUPAH_ATT_STORAGE_FREE_CCC_HANDLE:
    case ZUUPAH_ATT_CURRENT_BOOK_CCC_HANDLE:
    case ZUUPAH_ATT_PEN_EVENT_CCC_HANDLE:
        att_value_len = 2;
        if (buffer) {
            buffer[0] = ble_gatt_server_characteristic_ccc_get(connection_handle, att_handle);
            buffer[1] = 0;
        }
        break;

    default:
        break;
    }

    return att_value_len;
}

/* ─── ATT Write Callback ─────────────────────────────────────────────────────*/
static int zuupah_att_write_callback(hci_con_handle_t connection_handle,
                                      uint16_t att_handle, uint16_t transaction_mode,
                                      uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    switch (att_handle) {

    case ZUUPAH_ATT_BATTERY_CCC_HANDLE:
    case ZUUPAH_ATT_STORAGE_FREE_CCC_HANDLE:
    case ZUUPAH_ATT_CURRENT_BOOK_CCC_HANDLE:
    case ZUUPAH_ATT_PEN_EVENT_CCC_HANDLE:
        ble_gatt_server_characteristic_ccc_set(connection_handle, att_handle, buffer[0]);
        break;

    default:
        break;
    }
    return 0;
}

/* ─── Event Packet Handler ───────────────────────────────────────────────────*/
static int zuupah_event_packet_handler(int event, u8 *packet, u16 size, u8 *ext_param)
{
    switch (event) {

    case GATT_COMM_EVENT_CONNECTION_COMPLETE:
        zuupah_con_handle = little_endian_read_16(packet, 0);
        log_info("BLE verbunden, handle=0x%04x", zuupah_con_handle);
        break;

    case GATT_COMM_EVENT_DISCONNECT_COMPLETE:
        log_info("BLE getrennt, reason=0x%02x", packet[2]);
        if (zuupah_con_handle == little_endian_read_16(packet, 0)) {
            zuupah_con_handle = 0;
        }
        break;

    case GATT_COMM_EVENT_CAN_SEND_NOW:
        break;

    case GATT_COMM_EVENT_MTU_EXCHANGE_COMPLETE:
        log_info("MTU=%u", little_endian_read_16(packet, 2));
        break;

    default:
        break;
    }
    return 0;
}

/* ─── Advertising Setup ──────────────────────────────────────────────────────*/
static void zuupah_adv_config_set(void)
{
    u8 offset = 0;
    u8 *buf = zuupah_adv_data;

    /* Flags: LE General Discoverable, BR/EDR Not Supported */
    offset += make_eir_packet_val(&buf[offset], offset,
                                   HCI_EIR_DATATYPE_FLAGS,
                                   FLAGS_GENERAL_DISCOVERABLE_MODE | FLAGS_EDR_NOT_SUPPORTED, 1);

    /* Complete List of 16-bit Service UUIDs: 0xAA00 */
    offset += make_eir_packet_val(&buf[offset], offset,
                                   HCI_EIR_DATATYPE_COMPLETE_16BIT_SERVICE_UUIDS,
                                   0xAA00, 2);

    zuupah_server_adv_config.adv_data     = zuupah_adv_data;
    zuupah_server_adv_config.adv_data_len = offset;

    /* Scan Response: Complete Local Name */
    offset = 0;
    buf = zuupah_scan_rsp_data;
    char *gap_name = ble_comm_get_gap_name();
    u8 name_len = strlen(gap_name);
    offset += make_eir_packet_data(&buf[offset], offset,
                                    HCI_EIR_DATATYPE_COMPLETE_LOCAL_NAME,
                                    (void *)gap_name, name_len);
    zuupah_server_adv_config.rsp_data     = zuupah_scan_rsp_data;
    zuupah_server_adv_config.rsp_data_len = offset;

    zuupah_server_adv_config.adv_interval = ADV_INTERVAL_MIN;
    zuupah_server_adv_config.adv_auto_do  = 1;
    zuupah_server_adv_config.adv_type     = ADV_IND;
    zuupah_server_adv_config.adv_channel  = ADV_CHANNEL_ALL;
    memset(zuupah_server_adv_config.direct_address_info, 0, 7);

    ble_gatt_server_set_adv_config(&zuupah_server_adv_config);
}

/* ─── SDK-Required Fixed-Name Functions ──────────────────────────────────────
   bt_ble_before_start_init: called by btstack during btstack_init()
   bt_ble_init:              called by btstack_ble_start_after_init() after BT ready
   bt_ble_exit:              called when BT stack exits
*/
void bt_ble_before_start_init(void)
{
    log_info("bt_ble_before_start_init");
    ble_comm_init(&zuupah_gatt_control_block);
}

void bt_ble_init(void)
{
    log_info("bt_ble_init: Zuupah BLE Server starten");
    ble_comm_set_config_name(bt_get_local_name(), 1);
    zuupah_con_handle = 0;
    ble_gatt_server_set_profile(zuupah_profile_data, sizeof(zuupah_profile_data));
    zuupah_adv_config_set();
}

void bt_ble_exit(void)
{
    log_info("bt_ble_exit");
    zuupah_con_handle = 0;
}

/* ─── Öffentliche API ────────────────────────────────────────────────────────*/

u8 zuupah_ble_is_connected(void)
{
    return (zuupah_con_handle != 0) ? 1 : 0;
}

void zuupah_ble_notify_battery(u8 percent)
{
    if (!zuupah_con_handle) { return; }
    if (!ble_comm_att_check_send(zuupah_con_handle, 1)) { return; }
    ble_comm_att_send_data(zuupah_con_handle, ZUUPAH_ATT_BATTERY_VALUE_HANDLE,
                           &percent, 1, ATT_OP_AUTO_READ_CCC);
    log_info("Notify Batterie: %d%%", percent);
}

void zuupah_ble_notify_storage(u32 free_mb, u32 total_mb)
{
    if (!zuupah_con_handle) { return; }
    if (!ble_comm_att_check_send(zuupah_con_handle, 4)) { return; }
    ble_comm_att_send_data(zuupah_con_handle, ZUUPAH_ATT_STORAGE_FREE_VALUE_HANDLE,
                           (u8 *)&free_mb, 4, ATT_OP_AUTO_READ_CCC);
    log_info("Notify Speicher: %u MB frei", (unsigned)free_mb);
}

void zuupah_ble_notify_current_book(const char *book_id)
{
    if (!zuupah_con_handle || !book_id) { return; }
    u16 len = strlen(book_id);
    if (!ble_comm_att_check_send(zuupah_con_handle, len)) { return; }
    ble_comm_att_send_data(zuupah_con_handle, ZUUPAH_ATT_CURRENT_BOOK_VALUE_HANDLE,
                           (u8 *)book_id, len, ATT_OP_AUTO_READ_CCC);
}

void zuupah_ble_notify_event(u8 event_type)
{
    if (!zuupah_con_handle) { return; }
    if (!ble_comm_att_check_send(zuupah_con_handle, 1)) { return; }
    ble_comm_att_send_data(zuupah_con_handle, ZUUPAH_ATT_PEN_EVENT_VALUE_HANDLE,
                           &event_type, 1, ATT_OP_AUTO_READ_CCC);
    log_info("Notify Event: 0x%02x", event_type);
}
