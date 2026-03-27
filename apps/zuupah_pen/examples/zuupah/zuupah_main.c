/**
 * zuupah_main.c
 * Haupt-Logik des Zuupah Pens
 *
 * Verwaltet:
 *  - Batterie-Monitoring + BLE Notify
 *  - Speicher-Monitoring
 *  - Auto-Sleep nach 5 Minuten
 *  - Audio-Wiedergabe
 *  - BLE + SPP Initialisierung
 */

#include "system/app_core.h"
#include "system/includes.h"
#include "app_config.h"
#include "fs/fs.h"
#include "asm/adc_api.h"     /* ADC für Batterie */
#include "asm/timer.h"       /* timer_get_sec(), delay_2ms() */
#include "asm/power_interface.h" /* power_set_soft_poweroff() */
#include "tone_player.h"     /* tone_play_by_path() */
#include "string.h"
#include "btstack/btstack_task.h"
#include "bt_common.h"

#include "zuupah_ble.h"
#include "zuupah_spp.h"
#include "zuupah_ble_profile.h"

#define LOG_TAG "[ZUUPAH]"
#define log_info(x, ...) printf(LOG_TAG x "\n", ##__VA_ARGS__)

/* ─── Konfiguration ──────────────────────────────────────────────────────────*/
#define IDLE_TIMEOUT_SEC        (5 * 60)   /* 5 Minuten */
#define SHUTDOWN_WARNING_SEC    60          /* 60 Sek Warnung vor Power-Off */
#define BATTERY_CHECK_SEC       30          /* Batterie alle 30 Sek prüfen */
#define STORAGE_CHECK_SEC       60          /* Speicher alle 60 Sek prüfen */
#define BOOKS_DIR               "ZUUPAH/BOOKS/"

/* Batterie-Spannungen in mV (für LiPo) */
#define BAT_FULL_MV     4200
#define BAT_EMPTY_MV    3300

/* ─── State ──────────────────────────────────────────────────────────────────*/
static u32  g_last_activity   = 0;
static bool g_warning_sent    = false;
static u32  g_warning_time    = 0;
static char g_current_book[64] = "";

/* ─── Batterie ───────────────────────────────────────────────────────────────*/
u8 zuupah_get_battery_percent(void)
{
    /* ADC Kanal für Batteriespannung lesen */
    u32 mv = adc_get_voltage(AD_CH_VBAT);

    /* Spannung in Prozent umrechnen (linear, LiPo) */
    if (mv >= BAT_FULL_MV)  return 100;
    if (mv <= BAT_EMPTY_MV) return 0;

    return (u8)((mv - BAT_EMPTY_MV) * 100 / (BAT_FULL_MV - BAT_EMPTY_MV));
}

/* ─── Speicher ───────────────────────────────────────────────────────────────*/
u32 zuupah_get_storage_total_mb(void)
{
    /* 8 GB Flash = 8192 MB */
    return 8192;
}

u32 zuupah_get_storage_free_mb(void)
{
    /* Freien Speicher in Bytes abfragen und in MB umrechnen */
    u32 free_bytes = 0;
    fget_free_space("ZUUPAH", &free_bytes);
    return free_bytes / (1024 * 1024);
}

/* ─── Aktuelles Buch ─────────────────────────────────────────────────────────*/
const char* zuupah_get_current_book_id(void)
{
    if (g_current_book[0] == '\0') return NULL;
    return g_current_book;
}

void zuupah_set_current_book(const char *book_id)
{
    strncpy(g_current_book, book_id ? book_id : "", sizeof(g_current_book) - 1);
    zuupah_ble_notify_current_book(g_current_book);
}

/* ─── Aktivität zurücksetzen (bei Button, Audio-Start, SPP-Verbindung) ───────*/
void zuupah_activity_reset(void)
{
    g_last_activity = timer_get_sec();
    g_warning_sent  = false;
    log_info("Aktivität erkannt, Sleep-Timer zurückgesetzt");
}

/* ─── Sounds abspielen ───────────────────────────────────────────────────────*/
void zuupah_play_sound(const char *name)
{
    char path[128];
    /* Wildcard-Erweiterung damit tone_play_by_path MP3/WAV findet */
    snprintf(path, sizeof(path), "ZUUPAH/SOUNDS/%s.*", name);
    tone_play_by_path(path, 1);
}

/* ─── Auto-Sleep Logik (läuft alle 1 Sekunde) ────────────────────────────────*/
static void zuupah_sleep_tick(void)
{
    u32 now     = timer_get_sec();
    u32 elapsed = now - g_last_activity;

    /* 5 Minuten erreicht → Warnung */
    if (!g_warning_sent && elapsed >= IDLE_TIMEOUT_SEC) {
        log_info("Idle Timeout! Sende Shutdown-Warnung...");

        zuupah_play_sound("shutdown_warning"); /* "Ich schalte mich in 60 Sekunden aus" */
        zuupah_ble_notify_event(PEN_EVENT_SHUTDOWN_WARNING);

        g_warning_sent = true;
        g_warning_time = now;
    }

    /* 60 Sekunden nach Warnung → ausschalten */
    if (g_warning_sent && (now - g_warning_time) >= SHUTDOWN_WARNING_SEC) {
        log_info("Power Off!");

        zuupah_play_sound("goodbye");
        zuupah_ble_notify_event(PEN_EVENT_SHUTDOWN_NOW);

        /* Kurz warten damit Audio + BLE Notify noch gesendet werden (1000 * 2ms = 2s) */
        delay_2ms(1000);

        /* JieLi Power-Off */
        power_set_soft_poweroff();
    }
}

/* ─── Batterie + Speicher periodisch melden ──────────────────────────────────*/
static void zuupah_monitor_tick(void)
{
    static u32 bat_timer     = 0;
    static u32 storage_timer = 0;
    static u8  last_bat      = 255;

    u32 now = timer_get_sec();

    /* Batterie alle 30 Sek */
    if (now - bat_timer >= BATTERY_CHECK_SEC) {
        bat_timer = now;
        u8 bat = zuupah_get_battery_percent();
        if (bat != last_bat) {
            zuupah_ble_notify_battery(bat);
            last_bat = bat;
            log_info("Batterie: %d%%", bat);

            /* Warnung bei niedrigem Akkustand */
            if (bat <= 10 && bat != 0) {
                zuupah_play_sound("low_battery"); /* "Akku fast leer" */
            } else if (bat == 0) {
                zuupah_play_sound("battery_empty");
                zuupah_ble_notify_event(PEN_EVENT_SHUTDOWN_NOW);
                delay_2ms(1000);
                power_set_soft_poweroff();
            }
        }
    }

    /* Speicher alle 60 Sek */
    if (now - storage_timer >= STORAGE_CHECK_SEC) {
        storage_timer = now;
        zuupah_ble_notify_storage(
            zuupah_get_storage_free_mb(),
            zuupah_get_storage_total_mb()
        );
    }
}

/* ─── App Init ───────────────────────────────────────────────────────────────*/
static int zuupah_app_init(void)
{
    log_info("=== Zuupah Pen Firmware v1.0 ===");

    /* Ordnerstruktur erstellen */
    fmkdir("ZUUPAH");
    fmkdir("ZUUPAH/BOOKS");
    fmkdir("ZUUPAH/SOUNDS");

    /* BT Stack starten (EDR + BLE) */
#if TCFG_USER_EDR_ENABLE
    btstack_edr_start_before_init(NULL, 0);
#endif
#if TCFG_USER_BLE_ENABLE
    btstack_ble_start_before_init(NULL, 0);
#endif
    btstack_init();

    /* SPP initialisieren */
    zuupah_spp_init();

    /* Aktivitäts-Timer starten */
    g_last_activity = timer_get_sec();

    /* Startsound abspielen */
    zuupah_play_sound("startup");  /* "Hallo! Zuupah Stift bereit" */

    return 0;
}

/* ─── Periodic Timer Callback (every 1 second) ───────────────────────────────*/
static void zuupah_timer_handler(void *priv)
{
    zuupah_sleep_tick();
    zuupah_monitor_tick();
}

/* ─── App State Machine ───────────────────────────────────────────────────────*/
static int zuupah_state_machine(struct application *app, enum app_state state, struct intent *it)
{
    switch (state) {
    case APP_STA_CREATE:
        break;
    case APP_STA_START:
        zuupah_app_init();
        /* Batterie/Sleep-Monitoring alle 1000ms */
        sys_timer_add(NULL, zuupah_timer_handler, 1000);
        break;
    case APP_STA_PAUSE:
        break;
    case APP_STA_RESUME:
        break;
    case APP_STA_STOP:
        break;
    case APP_STA_DESTROY:
        log_info("APP_STA_DESTROY");
        break;
    }
    return 0;
}

static int zuupah_event_handler(struct application *app, struct sys_event *event)
{
    return FALSE;
}

static const struct application_operation zuupah_ops = {
    .state_machine  = zuupah_state_machine,
    .event_handler  = zuupah_event_handler,
};

REGISTER_APPLICATION(zuupah_app) = {
    .name   = "zuupah",
    .action = 0,
    .ops    = &zuupah_ops,
    .state  = APP_STA_DESTROY,
};
