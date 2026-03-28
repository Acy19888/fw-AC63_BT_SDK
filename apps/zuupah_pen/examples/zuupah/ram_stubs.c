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
/* cpu_reset() is a static inline in cpu.h that LTO inlines into .text        */
/* (Flash) callers. Flash→RAM 23-bit jump fails, so p33_soft_reset must       */
/* live in .text (Flash) too. The while(1) stub is safe either way: the WDT   */
/* triggers a full hardware reset after ~100 ms regardless of location.       */
void p33_soft_reset(void)
{
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

/* ── Custom C implementations for gpio / pmu_analog RAM stubs ───────── */
/*                                                                         */
/* Problem: pmu_analog.c (inside cpu.a/btctrler.a) has functions in      */
/* .volatile_ram_code (RAM) that call helpers compiled as static          */
/* functions. LTO promotes these static helpers to global names like      */
/* "delay_nus.636" — but the section attribute is lost, so they land in   */
/* .text (Flash). A RAM→Flash 23-bit jump then fails with:               */
/*   R_PI32V2_LONG_JUMP_23M2 relocation truncated to fit                  */
/*                                                                         */
/* Fix: override the RAM-resident callers (e.g. ldo13_on) entirely with  */
/* our own AT_VOLATILE_RAM_CODE implementations. Because ours are global  */
/* symbols and the linker uses --allow-multiple-definition, our version   */
/* wins. We never call delay_nus.636 — we call our own delay_nus         */
/* (also in RAM) or use direct ROM function pointers.                     */
/*                                                                         */
/* gpio_* stay in .text (Flash): their callers (e.g. DCDC13_EN) are      */
/* in Flash — a Flash→RAM jump would also fail.                           */

#include "asm/br23.h"

static JL_PORT_FLASH_TypeDef *get_gpio_port(unsigned int gpio) {
    switch (gpio / 16) {
        case 0: return JL_PORTA;
        case 1: return JL_PORTB;
        case 2: return JL_PORTC;
        case 3: return JL_PORTD;
        default: return (JL_PORT_FLASH_TypeDef *)0;
    }
}

int gpio_direction_input(unsigned int gpio) {
    JL_PORT_FLASH_TypeDef *port = get_gpio_port(gpio);
    if (!port) return -1;
    port->DIR |= (1 << (gpio % 16));
    return 0;
}

int gpio_direction_output(unsigned int gpio, int value) {
    JL_PORT_FLASH_TypeDef *port = get_gpio_port(gpio);
    if (!port) return -1;
    if (value) port->OUT |= (1 << (gpio % 16));
    else       port->OUT &= ~(1 << (gpio % 16));
    port->DIR &= ~(1 << (gpio % 16));
    return 0;
}

int gpio_set_pull_up(unsigned int gpio, int value) {
    JL_PORT_FLASH_TypeDef *port = get_gpio_port(gpio);
    if (!port) return -1;
    if (value) port->PU |= (1 << (gpio % 16));
    else       port->PU &= ~(1 << (gpio % 16));
    return 0;
}

int gpio_set_pull_down(unsigned int gpio, int value) {
    JL_PORT_FLASH_TypeDef *port = get_gpio_port(gpio);
    if (!port) return -1;
    if (value) port->PD |= (1 << (gpio % 16));
    else       port->PD &= ~(1 << (gpio % 16));
    return 0;
}

int gpio_set_hd(unsigned int gpio, int value) {
    JL_PORT_FLASH_TypeDef *port = get_gpio_port(gpio);
    if (!port) return -1;
    if (value) port->HD |= (1 << (gpio % 16));
    else       port->HD &= ~(1 << (gpio % 16));
    return 0;
}

int gpio_set_die(unsigned int gpio, int value) {
    JL_PORT_FLASH_TypeDef *port = get_gpio_port(gpio);
    if (!port) return -1;
    if (value) port->DIE |= (1 << (gpio % 16));
    else       port->DIE &= ~(1 << (gpio % 16));
    return 0;
}

int gpio_read(unsigned int gpio) {
    JL_PORT_FLASH_TypeDef *port = get_gpio_port(gpio);
    if (!port) return 0;
    return !!(port->IN & (1 << (gpio % 16)));
}

/* ── ldo13_on — override the library version with a RAM-resident copy ─── */
/* JieLi LTO strips .volatile_ram_code from PUBLIC (extern) symbols during  */
/* native code generation: ldo13_on lands in .text (Flash) even though its  */
/* bitcode has the section attribute.  Its static delay_nus helper stays in  */
/* RAM (internal symbols keep the attribute), producing a Flash→RAM 23-bit   */
/* overflow.                                                                  */
/*                                                                            */
/* Fix: define ldo13_on ourselves in this non-LTO translation unit           */
/* (ram_stubs.c is compiled with -fno-lto).  The section attribute is        */
/* honoured by the regular compiler, so the function is placed in            */
/* .volatile_ram_code (RAM).  --allow-multiple-definition makes our copy win */
/* over the library version.  The inline busy-wait avoids any call to the    */
/* library's private delay_nus (unreachable from outside the LTO blob).      */
/*                                                                            */
/* LDO13_EN(1) = P33_TX_NBIT(P3_ANA_CON0, BIT(2), 1)                        */
/*             = p33_or_1byte(0x00, 0x04)  [ROM at 0x1111a6]                 */
AT_VOLATILE_RAM_CODE __attribute__((noinline))
void ldo13_on(unsigned int udelay)
{
    /* Call p33_or_1byte from ROM (always reachable, even when Flash is off) */
    void (* volatile fn)(unsigned short, unsigned char) =
        (void (* volatile)(unsigned short, unsigned char))0x1111a6u;
    fn(0x00u, 0x04u);   /* set P3_ANA_CON0 bit 2 → enable LDO13 */

    if (udelay) {
        /* Inline busy-wait: no external call across the RAM/Flash boundary. */
        volatile unsigned int loops = udelay * 12u;
        while (loops--) { __asm__ volatile("nop"); }
    }
}
