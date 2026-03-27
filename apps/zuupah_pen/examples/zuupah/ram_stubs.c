/*
 * ram_stubs.c — Volatile-RAM trampolines for functions that cpu.a / system.a
 * call from .volatile_ram_code (RAM, ~0x0000) but that LTO would otherwise
 * place in .text (Flash, ~0x1E00xxxx).  The direct RAM→Flash 23-bit jump
 * (R_PI32V2_LONG_JUMP_23M2) exceeds the ±8 MB reach of the PI32V2 ISA.
 *
 * Placing our own definitions in AT_VOLATILE_RAM_CODE keeps caller and callee
 * in the same address space.  --allow-multiple-definition lets our copy win
 * over the library copies.
 */

#include "system/includes.h"
#include "asm/power_interface.h"
#include "os/os_api.h"

/* ── exception_analyze ────────────────────────────────────────────────────── */
/* Called from startup.S (.volatile_ram_code) in cpu.a.                       */
AT_VOLATILE_RAM_CODE
void exception_analyze(void)
{
    /* Stub: SPP/BLE app does not use hardware-exception analysis */
}

/* ── p33_soft_reset ───────────────────────────────────────────────────────── */
/* Called via inline cpu_reset() which LTO inlines into .text callers.        */
/* cpu.a calls it from .volatile_ram_code; keep it reachable.                 */
AT_VOLATILE_RAM_CODE
void p33_soft_reset(void)
{
    /* We don't have the exact P33 register definitions here.
     * Simply hang in a tight loop. The hardware watchdog (WDT) is enabled
     * in setup.c and will trigger a full system reset automatically. */
    while(1);
}
/* ── os_api.h indirect pointers ─────────────────────────────────────── */
/* LTO otherwise truncates 23-bit jump from .text to .volatile_ram_code       */
#undef os_current_task
#undef os_taskq_post_type
#undef os_mutex_create
#undef os_mutex_pend
#undef os_mutex_post

const char * (* volatile os_current_task_ptr)(void) = os_current_task;
int (* volatile os_taskq_post_type_ptr)(const char *, int, int, int *) = os_taskq_post_type;
int (* volatile os_mutex_create_ptr)(OS_MUTEX *) = os_mutex_create;
int (* volatile os_mutex_pend_ptr)(OS_MUTEX *, int) = os_mutex_pend;
int (* volatile os_mutex_post_ptr)(OS_MUTEX *) = os_mutex_post;

/* ── p33.h indirect pointers ──────────────────────────────────────────────── */
/* LTO otherwise truncates 23-bit jumps from P33 inline functions in .text    */
#undef p33_buf
#undef p33_xor_1byte
#undef p33_and_1byte
#undef p33_or_1byte
#undef p33_tx_1byte
#undef p33_rx_1byte
#undef P33_CON_SET

u8 (* volatile p33_buf_ptr)(u8) = p33_buf;
void (* volatile p33_xor_1byte_ptr)(u16, u8) = p33_xor_1byte;
void (* volatile p33_and_1byte_ptr)(u16, u8) = p33_and_1byte;
void (* volatile p33_or_1byte_ptr)(u16, u8) = p33_or_1byte;
void (* volatile p33_tx_1byte_ptr)(u16, u8) = p33_tx_1byte;
u8 (* volatile p33_rx_1byte_ptr)(u16) = p33_rx_1byte;
void (* volatile P33_CON_SET_ptr)(u16, u8, u8, u8) = P33_CON_SET;
