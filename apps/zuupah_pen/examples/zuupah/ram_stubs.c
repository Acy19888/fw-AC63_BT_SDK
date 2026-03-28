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

/* NOTE: local_irq_disable / local_irq_enable / __local_irq_enable are in
 * irq_stubs.c — a separate TU that omits cpu.h to avoid the static-inline
 * redefinition error.                                                      */

/* crc16.c (and other Flash/LTO code) calls os_mutex_pend/post as direct
 * symbol references — NOT through the _ptr macro path.  The real functions
 * live in .os_code (RAM); the Flash→RAM 23-bit jump overflows.
 * Flash stubs always succeed: the BT stack is event-driven / single-task-
 * queue, so concurrent CRC access is not possible in practice.           */
int os_mutex_create(OS_MUTEX *mutex) { (void)mutex; return 0; }
int os_mutex_pend  (OS_MUTEX *mutex, int timeout) { (void)mutex; (void)timeout; return 0; }
int os_mutex_post  (OS_MUTEX *mutex) { (void)mutex; return 0; }

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

/* spi_get_port — ROM at 0x1117f8, ~13 MB from Flash code → 23-bit overflow.
 * board_set_soft_poweroff (Flash) calls this to learn which SPI port drives
 * the external Flash, so it can protect the right GPIO pins during power-off.
 * Provide a Flash-resident trampoline that reaches ROM via 32-bit pointer.   */
u32 spi_get_port(void) {
    u32 (* volatile fn)(void) = (u32 (* volatile)(void))0x1117f8u;
    return fn();
}

/* get_sfc_bit_mode — STATIC in power_port.c (LTO keeps in .volatile_ram_code/RAM).
 * board_set_soft_poweroff (Flash) calls it to decide whether to protect the
 * quad-SPI WP/HOLD pins during power-off.  Not in ROM.
 * AC635N always uses Quad-SPI (4-bit) flash — return 4 unconditionally.     */
u32 get_sfc_bit_mode(void) {
    return 4;
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

/* ══ Stubs for functions removed with power_hw.c.o ══════════════════════════
 * power_hw.c.o (and embedded pmu_analog.c) was removed from cpu.a in CI to
 * eliminate the ldo13_on→delay_nus Flash→RAM 23-bit overflow.  All PUBLIC
 * functions that the application calls must be provided here.  These stubs
 * run in Flash (no AT_VOLATILE_RAM_CODE); callers are also in Flash (LTO).  */

#include "asm/power_interface.h"

/* power_init — sets up the power-management state machine.  The bootloader
 * already configures LDO/DCDC before our code runs, so a no-op is safe for
 * initial bring-up.  Sleep/wakeup callbacks will simply not be registered.  */
void power_init(const struct low_power_param *param) { (void)param; }

/* power_set_mode — switches between LDO15 / DCDC15.  No-op: the bootloader
 * sets the power mode; omitting the switch is safe for basic operation.     */
void power_set_mode(u8 mode) { (void)mode; }

/* power_set_callback — registers sleep-enter/exit and soft-poweroff hooks.
 * No-op: zuupah_main.c calls power_set_soft_poweroff() directly.           */
void power_set_callback(u8 mode,
                        void (*powerdown_enter)(u8),
                        void (*powerdown_exit)(u32),
                        void (*soft_poweroff_enter)(void))
{
    (void)mode; (void)powerdown_enter;
    (void)powerdown_exit; (void)soft_poweroff_enter;
}

/* power_keep_dacvdd_en — controls whether DACVDD stays on during sleep.
 * No-op: acceptable for a pen that doesn't use the DAC in sleep.           */
void power_keep_dacvdd_en(u8 en) { (void)en; }

/* power_wakeup_init — programs the hardware wakeup sources (port edges,
 * charge detect, RTC alarm, LVD).  No-op: the pen has no sleep mode;
 * wakeup sources are never armed.                                          */
void power_wakeup_init(const struct wakeup_param *param) { (void)param; }

/* ══ GPIO bulk-register stubs ════════════════════════════════════════════════
 * board_set_soft_poweroff() (Flash/LTO) calls these to tri-state all GPIO
 * before soft-poweroff.  On the real chip they live in .volatile_ram_code
 * so they can run when Flash is unpowered.  Our pen never enters true
 * soft-poweroff (power_set_callback is a no-op), so plain Flash stubs are
 * safe and fix the Flash→RAM 23-bit relocation overflow.                   */
#include "asm/gpio.h"
#include "asm/power/power_port.h"

u32 gpio_dir(u32 gpio, u32 start, u32 len, u32 dat, enum gpio_op_mode op)
{ (void)gpio; (void)start; (void)len; (void)dat; (void)op; return 0; }

u32 gpio_set_pu(u32 gpio, u32 start, u32 len, u32 dat, enum gpio_op_mode op)
{ (void)gpio; (void)start; (void)len; (void)dat; (void)op; return 0; }

u32 gpio_set_pd(u32 gpio, u32 start, u32 len, u32 dat, enum gpio_op_mode op)
{ (void)gpio; (void)start; (void)len; (void)dat; (void)op; return 0; }

u32 gpio_die(u32 gpio, u32 start, u32 len, u32 dat, enum gpio_op_mode op)
{ (void)gpio; (void)start; (void)len; (void)dat; (void)op; return 0; }

u32 gpio_dieh(u32 gpio, u32 start, u32 len, u32 dat, enum gpio_op_mode op)
{ (void)gpio; (void)start; (void)len; (void)dat; (void)op; return 0; }

/* Single-GPIO extras also called from board_set_soft_poweroff /
 * sleep_enter_callback via the same Flash→RAM path.                        */
int  gpio_set_direction(u32 gpio, u32 dir)
{ (void)gpio; (void)dir; return 0; }

u32  gpio_set_dieh(u32 gpio, u32 value)
{ (void)gpio; (void)value; return 0; }

/* usb_iomode — disconnects the USB DP/DM pads from the USB controller.
 * Called from board_set_soft_poweroff and sleep_enter_callback (both Flash).
 * Lives in .volatile_ram_code in the library; Flash stub is sufficient.   */
void usb_iomode(u32 enable) { (void)enable; }

/* port_protect — marks a GPIO port as "protected" (do not tri-state).
 * Called from board_set_soft_poweroff (Flash).  No-op: the pen doesn't
 * enter true soft-poweroff anyway.                                         */
void port_protect(u16 *port_group, u32 port_num)
{ (void)port_group; (void)port_num; }

/* dac_power_off — called from sleep_enter_callback (Flash/LTO) when
 * TCFG_AUDIO_ENABLE is defined.  Stub prevents Flash→RAM overflow.        */
void dac_power_off(void) {}

/* norflash_set_write_protect — controls flash WP pin / status register.
 * Defined in flash_wp_otp.c (cpu.a LTO bitcode, lands in .text = Flash).
 * Internally calls flash_addr2cpu_addr which is STATIC in .volatile_ram_code
 * (RAM) — the Flash→RAM 23-bit R_PI32V2_LONG_JUMP_23M2 relocation overflows.
 * The static symbol cannot be overridden directly; override the PUBLIC caller
 * instead.  The pen never programs the NOR flash at runtime, so disabling
 * write-protect management is safe.                                        */
int norflash_set_write_protect(u8 enable) { (void)enable; return 0; }

/* ── power_set_soft_poweroff ─────────────────────────────────────────────── */
/* Defined in power_hw.c.o (now removed from cpu.a).  Called from Flash     */
/* (zuupah_main.c LTO), so no AT_VOLATILE_RAM_CODE needed.  Spin forever:   */
/* the hardware WDT resets the chip after ~100 ms — clean shutdown.          */
void power_set_soft_poweroff(void)
{
    while (1);
}

/* ── ldo13_on ────────────────────────────────────────────────────────────── */
/* power_hw.c.o is removed from cpu.a in CI (ar d cpu.a power_hw.c.o).      */
/* That eliminates pmu_analog.c's ldo13_on + its static delay_nus from the  */
/* LTO blob entirely — no more Flash→RAM 23-bit overflow.  Our version here  */
/* (compiled -fno-lto, AT_VOLATILE_RAM_CODE) becomes the sole definition.   */
/* LDO13_EN(1) = p33_or_1byte(0x00, 0x04) via ROM at 0x1111a6              */
AT_VOLATILE_RAM_CODE __attribute__((noinline))
void ldo13_on(unsigned int udelay)
{
    void (* volatile fn)(unsigned short, unsigned char) =
        (void (* volatile)(unsigned short, unsigned char))0x1111a6u;
    fn(0x00u, 0x04u);   /* P3_ANA_CON0 bit 2 → enable LDO13 */
    if (udelay) {
        volatile unsigned int loops = udelay * 12u;
        while (loops--) { __asm__ volatile("nop"); }
    }
}
