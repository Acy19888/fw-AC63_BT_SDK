/*
 * irq_stubs.c — Flash-resident external symbols for local_irq_disable/enable.
 *
 * Problem: cpu.h (included via system/includes.h) declares these as
 * "static inline" under #if 1, making it impossible to also define
 * external symbols with the same name in the same translation unit
 * (clang: "error: redefinition of 'local_irq_disable'").
 *
 * Solution: compile this file WITHOUT system/includes.h and WITHOUT cpu.h,
 * so the static-inline conflict never arises.  The external symbols here
 * are picked up by --allow-multiple-definition and win over the RAM-resident
 * copies in system.a, fixing the Flash→RAM R_PI32V2_LONG_JUMP_23M2 overflow
 * from callers like vir_read_sys_time (virtual_rtc.c).
 *
 * Hardware details (BR23 / PI32V2):
 *   csfr_base        = 0x100000
 *   q32DSP_sfr_base  = csfr_base + 0xf000  = 0x10f000
 *   q32DSP(0) base   = 0x10f000
 *   IPMASK field     = /* 6a * / in JL_TypeDef_q32DSP
 *                    → byte offset 0x6a*4 = 0x1a8
 *                    → absolute address  = 0x10f1a8
 */

/* irq_lock_cnt[]: nesting counter for nested interrupt-disable calls.
 * Declared in cpu.h; re-declared here without including cpu.h.
 * BR23 is single-core so only index [0] is used.                          */
extern volatile int irq_lock_cnt[2];

/* IPMASK hardware register — interrupt priority mask.
 * Setting to 7 masks all interrupts; 0 enables all.                       */
#define IPMASK_REG   (*(volatile unsigned int *)(0x10f000u + 0x6au * 4u))

void local_irq_disable(void)
{
    __builtin_pi32v2_cli();
    irq_lock_cnt[0]++;          /* cpu_id always 0 on single-core BR23 */
    IPMASK_REG = 7u;
    __builtin_pi32v2_sti();
}

void local_irq_enable(void)
{
    if (--irq_lock_cnt[0] == 0) {
        IPMASK_REG = 0u;
    }
}

/* __local_irq_enable — unconditional re-enable; resets nesting count.     */
void __local_irq_enable(void)
{
    irq_lock_cnt[0] = 0;
    IPMASK_REG = 0u;
}
