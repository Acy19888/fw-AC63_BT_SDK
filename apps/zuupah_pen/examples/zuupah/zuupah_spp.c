/**
 * zuupah_spp.c
 * SPP (BR/EDR Classic Bluetooth) Dateiempfang für den Zuupah Pen
 * Empfängt Audio-Bücher von der Zuupah App und speichert sie auf Flash
 *
 * Protokoll:
 *   App → Pen: [Header 16+N Bytes] [Rohdaten Stream]
 *   Pen → App: "OK\n", "DONE\n", "ERROR:...\n"
 */

#include "system/app_core.h"
#include "system/includes.h"
#include "app_config.h"
#include "spp_user.h"
#include "bt_common.h"
#include "fs/fs.h"
#include "string.h"
#include "stdlib.h"

#include "zuupah_spp.h"
#include "zuupah_ble.h"

#define LOG_TAG "[ZUUPAH_SPP]"
#define log_info(x, ...)  printf(LOG_TAG x "\n", ##__VA_ARGS__)

/* ─── Konfiguration ──────────────────────────────────────────────────────────*/
#define BOOKS_DIR           "ZUUPAH/BOOKS/"
#define MAX_FILENAME        128
#define WRITE_BUF_SIZE      4096
#define MAX_FILE_SIZE       (50 * 1024 * 1024)   /* 50 MB */
#define MAGIC               0x5A555550            /* "ZUUP" */

/* ─── Header Struktur (muss mit App übereinstimmen) ──────────────────────────*/
typedef struct __attribute__((packed)) {
    u32 magic;
    u32 total_bytes;
    u32 checksum;       /* CRC32 */
    u32 filename_len;
    /* Danach: filename_len Bytes Dateiname */
    /* Danach: Dateidaten */
} ZuupahHeader;

/* ─── State ──────────────────────────────────────────────────────────────────*/
typedef enum {
    SPP_IDLE,
    SPP_WAIT_HEADER,
    SPP_RECEIVING,
} SppState;

static SppState g_state       = SPP_IDLE;
static FILE    *g_file        = NULL;
static u32      g_total_bytes = 0;
static u32      g_expect_crc  = 0;
static u32      g_received    = 0;
static char     g_filename[MAX_FILENAME];
static char     g_filepath[256];

/* Header-Sammelpuffer */
static u8  g_hdr_buf[sizeof(ZuupahHeader) + MAX_FILENAME];
static u32 g_hdr_pos   = 0;
static u32 g_hdr_total = 0;

/* Write-Buffer (RAM → Flash) */
static u8  g_write_buf[WRITE_BUF_SIZE];
static u32 g_write_pos = 0;

static struct spp_operation_t *spp_api = NULL;

/* ─── Hilfsfunktionen ────────────────────────────────────────────────────────*/
static void spp_send(const char *msg)
{
    if (spp_api) {
        spp_api->send_data(NULL, (u8*)msg, strlen(msg));
    }
}

static void flush_write_buf(void)
{
    if (g_file && g_write_pos > 0) {
        fwrite(g_write_buf, 1, g_write_pos, g_file);
        g_write_pos = 0;
    }
}

static void cleanup(int delete_file)
{
    flush_write_buf();
    if (g_file) {
        fclose(g_file);
        g_file = NULL;
    }
    if (delete_file && g_filepath[0]) {
        fdelete_by_name(g_filepath);
        log_info("Unvollständige Datei gelöscht: %s", g_filepath);
    }
    g_state      = SPP_IDLE;
    g_received   = 0;
    g_hdr_pos    = 0;
    g_hdr_total  = 0;
    g_write_pos  = 0;
    memset(g_filename, 0, sizeof(g_filename));
    memset(g_filepath, 0, sizeof(g_filepath));
}

/* ─── CRC32 ──────────────────────────────────────────────────────────────────*/
static u32 crc32_byte(u32 crc, u8 byte)
{
    crc ^= byte;
    for (int i = 0; i < 8; i++) {
        crc = (crc & 1) ? (0xEDB88320 ^ (crc >> 1)) : (crc >> 1);
    }
    return crc;
}

static u32 file_crc32(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    u32 crc = 0xFFFFFFFF;
    u8 buf[512];
    int n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (int i = 0; i < n; i++) crc = crc32_byte(crc, buf[i]);
    }
    fclose(f);
    return crc ^ 0xFFFFFFFF;
}

/* ─── SPP State Callback ─────────────────────────────────────────────────────*/
static void zuupah_spp_state_cbk(u8 state)
{
    if (state == SPP_USER_ST_CONNECT) {
        log_info("SPP verbunden — bereit für Dateitransfer");
        cleanup(0);
        g_state = SPP_WAIT_HEADER;
        spp_send("OK\n");
    } else if (state == SPP_USER_ST_DISCONN) {
        log_info("SPP getrennt");
        if (g_state == SPP_RECEIVING) {
            cleanup(1);  /* Unvollständige Datei löschen */
        } else {
            cleanup(0);
        }
    }
}

/* ─── SPP Daten empfangen ────────────────────────────────────────────────────*/
static void zuupah_spp_receive_cbk(void *priv, u8 *data, u16 len)
{
    if (g_state == SPP_IDLE) return;

    /* ABORT prüfen */
    if (len == 5 && memcmp(data, "ABORT", 5) == 0) {
        log_info("Transfer abgebrochen");
        cleanup(1);
        return;
    }

    /* ── Header sammeln ──────────────────────────────────────────────────── */
    if (g_state == SPP_WAIT_HEADER) {
        /* Erst die 16 Fix-Bytes lesen */
        if (g_hdr_pos < sizeof(ZuupahHeader)) {
            u32 n = sizeof(ZuupahHeader) - g_hdr_pos;
            if (n > len) n = len;
            memcpy(g_hdr_buf + g_hdr_pos, data, n);
            g_hdr_pos += n;
            data += n; len -= n;
        }

        /* Fix-Header vollständig → filename_len auslesen */
        if (g_hdr_pos >= sizeof(ZuupahHeader) && g_hdr_total == 0) {
            ZuupahHeader *hdr = (ZuupahHeader*)g_hdr_buf;
            if (hdr->magic != MAGIC) {
                log_info("FEHLER: Falscher Magic! 0x%08lx", hdr->magic);
                spp_send("ERROR:bad_magic\n");
                cleanup(0);
                return;
            }
            if (hdr->filename_len == 0 || hdr->filename_len >= MAX_FILENAME) {
                spp_send("ERROR:bad_header\n");
                cleanup(0);
                return;
            }
            if (hdr->total_bytes > MAX_FILE_SIZE) {
                spp_send("ERROR:file_too_large\n");
                cleanup(0);
                return;
            }
            g_hdr_total = sizeof(ZuupahHeader) + hdr->filename_len;
        }

        /* Dateiname sammeln */
        if (g_hdr_total > 0 && g_hdr_pos < g_hdr_total && len > 0) {
            u32 n = g_hdr_total - g_hdr_pos;
            if (n > len) n = len;
            memcpy(g_hdr_buf + g_hdr_pos, data, n);
            g_hdr_pos += n;
            data += n; len -= n;
        }

        /* Header vollständig? */
        if (g_hdr_total > 0 && g_hdr_pos >= g_hdr_total) {
            ZuupahHeader *hdr = (ZuupahHeader*)g_hdr_buf;
            memcpy(g_filename, g_hdr_buf + sizeof(ZuupahHeader), hdr->filename_len);
            g_filename[hdr->filename_len] = '\0';
            g_total_bytes = hdr->total_bytes;
            g_expect_crc  = hdr->checksum;

            log_info("Transfer startet: %s (%lu Bytes)", g_filename, g_total_bytes);

            /* fopen erstellt Ordner automatisch */
            snprintf(g_filepath, sizeof(g_filepath), "%s%s", BOOKS_DIR, g_filename);
            g_file = fopen(g_filepath, "wb");
            if (!g_file) {
                log_info("FEHLER: Kann Datei nicht erstellen");
                spp_send("ERROR:cannot_create_file\n");
                cleanup(0);
                return;
            }
            g_state    = SPP_RECEIVING;
            g_received = 0;
        }

        if (len == 0) return;
    }

    /* ── Dateidaten schreiben ────────────────────────────────────────────── */
    if (g_state == SPP_RECEIVING && len > 0) {
        u32 to_write = len;
        if (g_received + to_write > g_total_bytes) {
            to_write = g_total_bytes - g_received;
        }

        /* In Write-Buffer kopieren, bei Bedarf auf Flash flushen */
        u32 done = 0;
        while (done < to_write) {
            u32 space = WRITE_BUF_SIZE - g_write_pos;
            u32 chunk = to_write - done;
            if (chunk > space) chunk = space;
            memcpy(g_write_buf + g_write_pos, data + done, chunk);
            g_write_pos += chunk;
            done += chunk;
            if (g_write_pos >= WRITE_BUF_SIZE) {
                flush_write_buf();
            }
        }
        g_received += to_write;

        /* Fertig? */
        if (g_received >= g_total_bytes) {
            flush_write_buf();
            fclose(g_file);
            g_file = NULL;

            log_info("Empfang abgeschlossen, prüfe CRC32...");

            u32 actual_crc = file_crc32(g_filepath);
            if (actual_crc != g_expect_crc) {
                log_info("FEHLER: CRC32 stimmt nicht! got=0x%08lx want=0x%08lx",
                         actual_crc, g_expect_crc);
                fdelete_by_name(g_filepath);
                cleanup(0);
                spp_send("ERROR:checksum_fail\n");
                return;
            }

            log_info("CRC32 OK! Buch gespeichert: %s", g_filepath);

            /* Meta-Datei schreiben */
            char meta[256];
            snprintf(meta, sizeof(meta), "%s%s.meta", BOOKS_DIR, g_filename);
            FILE *mf = fopen(meta, "w");
            if (mf) {
                fprintf(mf, "file=%s\n", g_filename);
                fclose(mf);
            }

            cleanup(0);
            spp_send("DONE\n");

            /* Erfolgssound abspielen */
            extern void zuupah_play_sound(const char *name);
            zuupah_play_sound("book_received");

            /* BLE-Status updaten */
            extern u32 zuupah_get_storage_free_mb(void);
            extern u32 zuupah_get_storage_total_mb(void);
            zuupah_ble_notify_storage(
                zuupah_get_storage_free_mb(),
                zuupah_get_storage_total_mb()
            );
        }
    }
}

/* ─── SPP Init ───────────────────────────────────────────────────────────────*/
void zuupah_spp_init(void)
{
    log_info("Initialisiere Zuupah SPP...");
    spp_get_operation_table(&spp_api);
    if (spp_api) {
        spp_api->regist_recieve_cbk(0, zuupah_spp_receive_cbk);
        spp_api->regist_state_cbk(0, zuupah_spp_state_cbk);
    }
}
