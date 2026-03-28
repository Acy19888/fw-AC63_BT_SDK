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
#include "asm/power/p33.h"

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

/* ── LTO C-trampolines for p33 (ROM) functions ────────────────────────── */
/* Precompiled LLVM IR in cpu.a contains direct calls to p33_or_1byte etc.*/
/* These jumps fail the 23-bit range check from .text.                    */
/* By declaring these functions strongly in .text, LTO prefers them over  */
/* the ones in cpu.a. We then manually perform a 32-bit indirect call     */
/* to the true hardware ROM address (found in cpu/br23/tools/rom.lst).    */

u8 p33_buf(u8 buf) {
    u8 (* volatile real_func)(u8) = (u8 (* volatile)(u8))0x1110b0;
    return real_func(buf);
}

void p33_xor_1byte(u16 addr, u8 data0) {
    void (* volatile real_func)(u16, u8) = (void (* volatile)(u16, u8))0x1123cc;
    real_func(addr, data0);
}

void p33_and_1byte(u16 addr, u8 data0) {
    void (* volatile real_func)(u16, u8) = (void (* volatile)(u16, u8))0x1111f0;
    real_func(addr, data0);
}

void p33_or_1byte(u16 addr, u8 data0) {
    void (* volatile real_func)(u16, u8) = (void (* volatile)(u16, u8))0x1111a6;
    real_func(addr, data0);
}

void p33_tx_1byte(u16 addr, u8 data0) {
    void (* volatile real_func)(u16, u8) = (void (* volatile)(u16, u8))0x1110c8;
    real_func(addr, data0);
}

u8 p33_rx_1byte(u16 addr) {
    u8 (* volatile real_func)(u16) = (u8 (* volatile)(u16))0x11111c;
    return real_func(addr);
}

void P33_CON_SET(u16 addr, u8 start, u8 len, u8 data) {
    void (* volatile real_func)(u16, u8, u8, u8) = (void (* volatile)(u16, u8, u8, u8))0x112418;
    real_func(addr, start, len, data);
}

/* ── LTO C-trampolines for GPIO (RAM) functions ───────────────────────── */
/* Precompiled LLVM IR in cpu.a/system.a contains direct calls to gpio_   */
/* Since these functions reside in .volatile_ram_code, jumping to them    */
/* from .text exceeds 23 bits. We override them and use their __gpio_     */
/* internal aliases via 32-bit volatile pointers.                         */

#include "generic/gpio.h"

int gpio_direction_input(unsigned int gpio) {
    int (* volatile real_func)(unsigned int) = __gpio_direction_input;
    return real_func(gpio);
}

int gpio_direction_output(unsigned int gpio, int value) {
    int (* volatile real_func)(unsigned int, int) = __gpio_direction_output;
    return real_func(gpio, value);
}

int gpio_set_pull_up(unsigned int gpio, int value) {
    int (* volatile real_func)(unsigned int, int) = __gpio_set_pull_up;
    return real_func(gpio, value);
}

int gpio_set_pull_down(unsigned int gpio, int value) {
    int (* volatile real_func)(unsigned int, int) = __gpio_set_pull_down;
    return real_func(gpio, value);
}

int gpio_set_hd(unsigned int gpio, int value) {
    int (* volatile real_func)(unsigned int, int) = __gpio_set_hd;
    return real_func(gpio, value);
}

int gpio_set_die(unsigned int gpio, int value) {
    int (* volatile real_func)(unsigned int, int) = __gpio_set_die;
    return real_func(gpio, value);
}

int gpio_read(unsigned int gpio) {
    int (* volatile real_func)(unsigned int) = __gpio_read;
    return real_func(gpio);
}
