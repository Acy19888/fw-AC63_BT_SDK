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
#include "asm/p33.h"

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
