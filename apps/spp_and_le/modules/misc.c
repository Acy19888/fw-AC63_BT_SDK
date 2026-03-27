#include "system/includes.h"
#include "server/server_core.h"
#include "asm/power_interface.h"

/* cpu.a (startup.S) calls exception_analyze() from .volatile_ram_code.
 * A direct RAM→Flash jump exceeds the 23-bit R_PI32V2_LONG_JUMP_23M2 limit.
 * Define exception_analyze itself in .volatile_ram_code so the jump is in-range. */
AT_VOLATILE_RAM_CODE
void exception_analyze(void)
{
    /* No-op stub: keeps the linker happy for the SPP/BLE-only Zuupah build */
}

u16 update_result_get(void)
{
    return 0;
}

void update_mode_api(u8 type, ...)
{

}

void update_loader_download_init(void)
{

}

void (*lmp_ch_update_resume_hdl)(void *priv) = NULL;

u8 update_id_info_fill_content(u8 *p, u16 allow_len)
{
    return 0;
}

void bt_lmp_update_loader_download_init(void)
{
}
