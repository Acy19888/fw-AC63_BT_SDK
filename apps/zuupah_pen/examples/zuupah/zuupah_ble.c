/**
 * zuupah_ble.c
 * BLE GATT Server für den Zuupah Pen
 * Basiert auf dem JieLi AC63 SDK ble_trans Beispiel
 */

#include "system/app_core.h"
#include "system/includes.h"
#include "app_config.h"
#include "btstack/bluetooth.h"
#include "bt_common.h"
#include "le_common.h"
#include "gatt_common/le_gatt_common.h"
#include "string.h"

#include "zuupah_ble.h"
#include "zuupah_ble_profile.h"

#define LOG_TAG "[ZUUPAH_BLE]"
#define log_info(x, ...)  printf(LOG_TAG x "\n", ##__VA_ARGS__)

/* ─── Advertising Data ──────────────────────────────────────────────────────── */
#define ADV_INTERVAL_MS   200   /* ms */

static u8 zuupah_adv_data[31];
static u8 zuupah_scan_rsp[31];
static u16 zuupah_con_handle = 0;
static u8  zuupah_connected  = 0;

/* Gerätename: "Zuupah-XXXX" (letzten 2 Bytes der BT-Adresse als HEX) */
static char zuupah_device_name[16] = "Zuupah-0000";

static void build_adv_data(void)
{
    u8 pos = 0;

    /* Flags: LE General Discoverable, BR/EDR Not Supported (für BLE-Only Advertising) */
    zuupah_adv_data[pos++] = 2;
    zuupah_adv_data[pos++] = 0x01;
    zuupah_adv_data[pos++] = 0x06;

    /* Complete List of 16-bit Service UUIDs: 0xAA00 */
    zuupah_adv_data[pos++] = 3;
    zuupah_adv_data[pos++] = 0x03;
    zuupah_adv_data[pos++] = 0x00;
    zuupah_adv_data[pos++] = 0xAA;

    /* Scan Response: Complete Local Name */
    u8 name_len = strlen(zuupah_device_name);
    zuupah_scan_rsp[0] = name_len + 1;
    zuupah_scan_rsp[1] = 0x09;  /* Complete Local Name */
    memcpy(&zuupah_scan_rsp[2], zuupah_device_name, name_len);
}

/* ─── GATT Read/Write Handler ────────────────────────────────────────────────
   Wird vom BLE-Stack aufgerufen wenn App liest oder schreibt
*/
static int zuupah_gatt_callback(le_att_handle_t handle, u8 type, u8 *data, u16 len)
{
    switch (type) {
    case ATT_READ_REQ:
        switch (handle) {
        case ZUUPAH_ATT_BATTERY_VALUE_HANDLE: {
            extern u8 zuupah_get_battery_percent(void);
            u8 bat = zuupah_get_battery_percent();
            att_server_set_read_value(handle, &bat, 1);
            break;
        }
        case ZUUPAH_ATT_STORAGE_FREE_VALUE_HANDLE: {
            extern u32 zuupah_get_storage_free_mb(void);
            u32 free_mb = zuupah_get_storage_free_mb();
            att_server_set_read_value(handle, (u8*)&free_mb, 4);
            break;
        }
        case ZUUPAH_ATT_STORAGE_TOTAL_VALUE_HANDLE: {
            extern u32 zuupah_get_storage_total_mb(void);
            u32 total_mb = zuupah_get_storage_total_mb();
            att_server_set_read_value(handle, (u8*)&total_mb, 4);
            break;
        }
        case ZUUPAH_ATT_CURRENT_BOOK_VALUE_HANDLE: {
            extern const char* zuupah_get_current_book_id(void);
            const char *bid = zuupah_get_current_book_id();
            if (bid) {
                att_server_set_read_value(handle, (u8*)bid, strlen(bid));
            } else {
                att_server_set_read_value(handle, (u8*)"", 0);
            }
            break;
        }
        }
        break;

    case ATT_WRITE_REQ:
        /* Keine schreibbaren Charakteristiken im BLE Service */
        break;
    }
    return 0;
}

/* ─── Verbindungs-Events ─────────────────────────────────────────────────────*/
static void zuupah_ble_event_handler(struct bt_event *bt)
{
    switch (bt->event) {
    case BT_STATUS_INIT_OK:
        log_info("BT init OK, starte BLE Advertising");
        build_adv_data();
        le_controller_set_cmd(LE_CONTROLLER_SET_SCAN_RSP_DATA, zuupah_scan_rsp, sizeof(zuupah_scan_rsp));
        le_controller_set_cmd(LE_CONTROLLER_SET_ADV_DATA, zuupah_adv_data, sizeof(zuupah_adv_data));
        le_controller_set_cmd(LE_CONTROLLER_SET_ADV_INTERVAL, (void*)(ADV_INTERVAL_MS * 8 / 5), 0);
        le_controller_set_cmd(LE_CONTROLLER_START_ADV, NULL, 0);
        break;

    case BT_STATUS_LINK_KEY_MISS:
    case BT_STATUS_PAGE_TIMEOUT:
        break;
    }
}

static void zuupah_ble_connect_handler(u16 con_handle, u8 *addr)
{
    log_info("BLE verbunden, handle=0x%04x", con_handle);
    zuupah_con_handle = con_handle;
    zuupah_connected  = 1;

    /* 2M PHY aktivieren für schnellere Übertragung */
    le_controller_set_cmd(LE_CONTROLLER_SET_PHY, (void*)(u32)con_handle, (void*)BIT(1)); /* 2M PHY */
}

static void zuupah_ble_disconnect_handler(u16 con_handle, u8 reason)
{
    log_info("BLE getrennt, reason=0x%02x", reason);
    zuupah_con_handle = 0;
    zuupah_connected  = 0;

    /* Advertising neu starten */
    le_controller_set_cmd(LE_CONTROLLER_START_ADV, NULL, 0);
}

/* ─── Öffentliche API ────────────────────────────────────────────────────────*/

void zuupah_ble_init(void)
{
    log_info("Initialisiere Zuupah BLE...");

    /* GATT Profile registrieren */
    att_server_init(zuupah_profile_data, sizeof(zuupah_profile_data));
    att_server_register_callback(zuupah_gatt_callback);

    /* BLE Event Callbacks */
    ble_module_enable(1);
}

u8 zuupah_ble_is_connected(void)
{
    return zuupah_connected;
}

void zuupah_ble_notify_battery(u8 percent)
{
    if (!zuupah_connected) return;
    att_server_notify(zuupah_con_handle,
                      ZUUPAH_ATT_BATTERY_VALUE_HANDLE,
                      &percent, 1);
    log_info("Notify Batterie: %d%%", percent);
}

void zuupah_ble_notify_storage(u32 free_mb, u32 total_mb)
{
    if (!zuupah_connected) return;
    att_server_notify(zuupah_con_handle,
                      ZUUPAH_ATT_STORAGE_FREE_VALUE_HANDLE,
                      (u8*)&free_mb, 4);
    log_info("Notify Speicher: %lu MB frei von %lu MB", free_mb, total_mb);
}

void zuupah_ble_notify_current_book(const char *book_id)
{
    if (!zuupah_connected || !book_id) return;
    att_server_notify(zuupah_con_handle,
                      ZUUPAH_ATT_CURRENT_BOOK_VALUE_HANDLE,
                      (u8*)book_id, strlen(book_id));
}

void zuupah_ble_notify_event(u8 event_type)
{
    if (!zuupah_connected) return;
    att_server_notify(zuupah_con_handle,
                      ZUUPAH_ATT_PEN_EVENT_VALUE_HANDLE,
                      &event_type, 1);
    log_info("Notify Event: 0x%02x", event_type);
}
