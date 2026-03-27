#ifndef _ZUUPAH_BLE_H
#define _ZUUPAH_BLE_H

#include "generic/typedef.h"

/* BLE initialisieren und Advertising starten */
void zuupah_ble_init(void);

/* Werte an verbundene App senden (Notify) */
void zuupah_ble_notify_battery(u8 percent);
void zuupah_ble_notify_storage(u32 free_mb, u32 total_mb);
void zuupah_ble_notify_current_book(const char *book_id);
void zuupah_ble_notify_event(u8 event_type);

/* Verbindungsstatus */
u8 zuupah_ble_is_connected(void);

#endif
