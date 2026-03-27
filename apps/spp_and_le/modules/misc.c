#include "system/includes.h"
#include "server/server_core.h"
#include "asm/power_interface.h"

/* startup.S in cpu.a calls exception_analyze() from .volatile_ram_code (RAM).
 * The library defines it in .text (Flash ~0x1E00xxx), which is out of range
 * for a 23-bit jump from RAM. Define it in .volatile_ram_code so both ends
 * are in RAM. --allow-multiple-definition lets this override the library copy. */
AT_VOLATILE_RAM_CODE
void exception_analyze(void)
{
    /* Stub: normal app code does not trigger hardware exceptions via this path */
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
