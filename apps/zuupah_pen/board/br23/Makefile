# ════════════════════════════════════════════════════════
# Zuupah Pen Firmware — Makefile für AC695X (br23 platform)
# ════════════════════════════════════════════════════════
#
# Build-Befehle:
#   make              → Kompilieren + Firmware erstellen
#   make VERBOSE=1    → Mit detailliertem Output
#   make clean        → Temporäre Dateien löschen
#
# Voraussetzungen:
#   Windows: JieLi Toolchain unter C:/JL/pi32/bin
#   Linux/Mac: JieLi Toolchain unter /opt/jieli/pi32v2/bin
#   Download: http://pkgman.jieliapp.com/doc/all
#

ifeq ($(OS), Windows_NT)
TOOL_DIR        := C:/JL/pi32/bin
CC              := clang.exe
LD              := pi32v2-lto-wrapper.exe
AR              := llvm-ar.exe
MKDIR           := mkdir_win -p
RM              := rm -rf
SYS_LIB_DIR     := C:/JL/pi32/pi32v2-lib/r3
SYS_INC_DIR     := C:/JL/pi32/pi32v2-include
EXT_CFLAGS      :=
export PATH     := $(TOOL_DIR);$(PATH)
FIXBAT          := ..\..\..\..\tools\utils\fixbat.exe
POST_SCRIPT     := ../../../../cpu/br23/tools/download.bat
RUN_POST_SCRIPT := ..\..\..\..\cpu\br23\tools\download.bat
else
TOOL_DIR        := /opt/jieli/pi32v2/bin
CC              := clang
LD              := lto-wrapper
AR              := lto-ar
MKDIR           := mkdir -p
RM              := rm -rf
export OBJDUMP  := $(TOOL_DIR)/objdump
export OBJCOPY  := $(TOOL_DIR)/objcopy
SYS_LIB_DIR     := $(TOOL_DIR)/../lib/r3
SYS_INC_DIR     := $(TOOL_DIR)/../include
EXT_CFLAGS      := -D__SHELL__
export PATH     := $(TOOL_DIR):$(PATH)
FIXBAT          := touch
POST_SCRIPT     := ../../../../cpu/br23/tools/download.sh
RUN_POST_SCRIPT := bash $(POST_SCRIPT)
endif

CC  := $(TOOL_DIR)/$(CC)
LD  := $(TOOL_DIR)/$(LD)
AR  := $(TOOL_DIR)/$(AR)

OUT_ELF     := ../../../../cpu/br23/tools/sdk.elf
OBJ_FILE    := $(OUT_ELF).objs.txt
BUILD_DIR   := objs
ROOT_PREFIX := ../../../..

# ─── Compiler Flags ──────────────────────────────────────────────────────────
CFLAGS := \
	-target pi32v2 \
	-mcpu=r3 \
	-integrated-as \
	-flto \
	-Oz \
	-g \
	-fno-common \
	-fallow-pointer-null \
	-fprefer-gnu-section \
	-fms-extensions \
	-w \

# ─── Defines ─────────────────────────────────────────────────────────────────
DEFINES := \
	-DSUPPORT_MS_EXTENSIONS \
	-DCONFIG_RELEASE_ENABLE \
	-DCONFIG_CPU_BR23 \
	-DCONFIG_FREE_RTOS_ENABLE \
	-DCONFIG_MMU_ENABLE \
	-DCONFIG_AUDIO_ONCHIP \
	-DCONFIG_MEDIA_DEVELOP_ENABLE \
	-D__GCC_PI32V2__ \
	-DVFS_ENABLE=1 \
	-DUSE_SDFILE_NEW=1 \
	-DSDFILE_VERSION=0x020000 \
	-DVM_MAX_SIZE_CONFIG=16*1024 \
	-DCONFIG_SPP_AND_LE_CASE_ENABLE \
	-DCONFIG_TRANSFER_ENABLE \
	-DCONFIG_APP_ZUUPAH=1 \
	-DCONFIG_APP_SPP_LE=1 \
	-DTCFG_USER_EDR_ENABLE=1 \
	-DTCFG_USER_BLE_ENABLE=1 \
	-DUSER_SUPPORT_PROFILE_SPP=1 \
	-DCONFIG_BT_BREDR_ENABLE \
	-DCONFIG_BLE_ENABLE \
	-DCONFIG_LITE_AUDIO \
	$(EXT_CFLAGS) \

# ─── Include Paths ───────────────────────────────────────────────────────────
INCLUDES := \
	-I../../../../include_lib \
	-I../../../../include_lib/driver \
	-I../../../../include_lib/driver/device \
	-I../../../../include_lib/driver/cpu/br23 \
	-I../../../../include_lib/system \
	-I../../../../include_lib/system/generic \
	-I../../../../include_lib/system/device \
	-I../../../../include_lib/system/fs \
	-I../../../../include_lib/system/ui \
	-I../../../../include_lib/system/math/cpu/br23 \
	-I../../../../include_lib/btctrler \
	-I../../../../include_lib/btctrler/port/br23 \
	-I../../../../include_lib/update \
	-I../../../../include_lib/agreement \
	-I../../../../include_lib/btstack \
	-I../../../../include_lib/btstack/le \
	-I../../../../include_lib/btstack/third_party/common \
	-I../../../../include_lib/btstack/third_party/rcsp \
	-I../../../../include_lib/media/media_develop \
	-I../../../../include_lib/media/media_develop/media \
	-I../../../../include_lib/media/media_develop/media/cpu/br23 \
	-I../../../../include_lib/media/media_develop/media/cpu/br23/asm \
	-I../../../../apps/spp_and_le/include \
	-I../../../../apps/spp_and_le/board/br23 \
	-I../../../../apps/common \
	-I../../../../apps/common/audio \
	-I../../../../apps/common/device \
	-I../../../../apps/common/device/usb \
	-I../../../../apps/common/include \
	-I../../../../apps/common/power_manage \
	-I../../../../apps/common/third_party_profile/jieli \
	-I../../../../apps/common/third_party_profile/jieli/gatt_common \
	-I../../../../apps/common/third_party_profile/jieli/JL_rcsp \
	-I../../../../apps/common/third_party_profile/jieli/JL_rcsp/rcsp_updata \
	-I../../../../apps/common/third_party_profile/common \
	-I../../../../include_lib/ai_stack/JL_rcsp \
	-I../../../../cpu/br23 \
	-I../../../../cpu/br23/audio_dec \
	-I../../../../cpu/br23/audio_enc \
	-I../../../../apps/zuupah_pen/examples/zuupah \
	-I$(SYS_INC_DIR) \

# ─── Source Files ─────────────────────────────────────────────────────────────
c_SRC_FILES := \
	../../../../apps/zuupah_pen/examples/zuupah/zuupah_main.c \
	../../../../apps/zuupah_pen/examples/zuupah/ram_stubs.c \
	../../../../apps/zuupah_pen/examples/zuupah/zuupah_ble.c \
	../../../../apps/zuupah_pen/examples/zuupah/zuupah_spp.c \
	../../../../apps/spp_and_le/config/lib_btctrler_config.c \
	../../../../apps/spp_and_le/config/lib_btstack_config.c \
	../../../../apps/spp_and_le/config/lib_driver_config.c \
	../../../../apps/spp_and_le/config/lib_media_config.c \
	../../../../apps/spp_and_le/config/lib_profile_config.c \
	../../../../apps/spp_and_le/config/lib_system_config.c \
	../../../../apps/spp_and_le/config/lib_update_config.c \
	../../../../apps/spp_and_le/config/log_config.c \
	../../../../apps/spp_and_le/board/br23/board_ac635n_demo.c \
	../../../../apps/spp_and_le/modules/bt/app_comm_ble.c \
	../../../../apps/spp_and_le/modules/bt/app_comm_edr.c \
	../../../../apps/spp_and_le/modules/bt/spp_trans.c \
	../../../../apps/spp_and_le/modules/misc.c \
	../../../../apps/spp_and_le/modules/power/app_power_manage.c \
	../../../../apps/spp_and_le/modules/user_cfg.c \
	../../../../apps/spp_and_le/version.c \
	../../../../apps/common/third_party_profile/jieli/gatt_common/le_gatt_common.c \
	../../../../apps/common/third_party_profile/jieli/gatt_common/le_gatt_server.c \
	../../../../apps/common/third_party_profile/common/custom_cfg.c \
	../../../../cpu/br23/adc_api.c \
	../../../../cpu/br23/setup.c \
	../../../../cpu/br23/clock_manager.c \
	../../../../cpu/br23/audio_dec/tone_player.c \
	../../../../cpu/br23/charge.c \
	../../../../cpu/br23/uart_dev.c \

# ─── Linker Flags ────────────────────────────────────────────────────────────
# Aligned with spp_and_le reference app (the correct JieLi SDK flags).
# Key additions vs our previous hacks:
#   --dont-complain-call-overflow  → suppresses PI32V2 23-bit range errors
#   -inline-normal-into-special-section=true → LTO inlines delay_nus INTO
#     ldo13_on (both end up in .volatile_ram_code) → no cross-section call
LFLAGS := \
	--plugin-opt=-pi32v2-always-use-itblock=false \
	--plugin-opt=-enable-ipra=true \
	--plugin-opt=-pi32v2-merge-max-offset=4096 \
	--plugin-opt=-pi32v2-enable-simd=true \
	--plugin-opt=mcpu=r3 \
	--plugin-opt=-global-merge-on-const \
	--plugin-opt=-inline-threshold=5 \
	--plugin-opt=-inline-max-allocated-size=32 \
	--plugin-opt=-inline-normal-into-special-section=true \
	--plugin-opt=-dont-used-symbol-list=malloc,free,sprintf,printf,puts,putchar \
	--plugin-opt=save-temps \
	--plugin-opt=-pi32v2-enable-rep-memop \
	--plugin-opt=-warn-stack-size=256 \
	--sort-common \
	--dont-complain-call-overflow \
	--allow-multiple-definition \
	--plugin-opt=-used-symbol-file=../../../../cpu/br23/sdk_used_list.used \
	--gc-sections \
	--start-group \
	../../../../cpu/br23/liba/cpu.a \
	../../../../cpu/br23/liba/system.a \
	../../../../cpu/br23/liba/btstack.a \
	../../../../cpu/br23/liba/btctrler.a \
	../../../../cpu/br23/liba/rcsp_stack.a \
	../../../../cpu/br23/liba/media.a \
	../../../../cpu/br23/liba/media_app.a \
	../../../../cpu/br23/liba/mp3_dec_lib.a \
	../../../../cpu/br23/liba/wav_dec_lib.a \
	../../../../cpu/br23/liba/update.a \
	--end-group \
	-T../../../../cpu/br23/sdk.ld \
	-M=../../../../cpu/br23/tools/sdk.map \
	--plugin-opt=mcpu=r3 \

LIBPATHS := -L$(SYS_LIB_DIR)
LIBS := \
	$(SYS_LIB_DIR)/libm.a \
	$(SYS_LIB_DIR)/libc.a \
	$(SYS_LIB_DIR)/libcompiler-rt.a \

# ─── Build Rules ─────────────────────────────────────────────────────────────
c_OBJS    := $(c_SRC_FILES:%.c=%.c.o)
OBJS      := $(c_OBJS)
DEP_FILES := $(OBJS:%.o=%.d)
OBJS      := $(addprefix $(BUILD_DIR)/, $(OBJS:$(ROOT_PREFIX)/%=%))
DEP_FILES := $(addprefix $(BUILD_DIR)/, $(DEP_FILES:$(ROOT_PREFIX)/%=%))
# irq_stubs.c.o is compiled without INCLUDES; add it explicitly at the end.
OBJS      += $(BUILD_DIR)/apps/zuupah_pen/examples/zuupah/irq_stubs.c.o

VERBOSE ?= 0
ifeq ($(VERBOSE), 1)
QUITE :=
else
QUITE := @
endif

LINK_AT ?= 1

.PHONY: all clean pre_build
.SUFFIXES:

all: pre_build $(OUT_ELF)
	$(info )
	$(info ✓ Zuupah Firmware erfolgreich kompiliert!)
	$(info )
	$(QUITE) $(OBJCOPY) -O binary -j .text       $(OUT_ELF) ../../../../cpu/br23/tools/text.bin      2>/dev/null || true
	$(QUITE) $(OBJCOPY) -O binary -j .data       $(OUT_ELF) ../../../../cpu/br23/tools/data.bin      2>/dev/null || true
	$(QUITE) $(OBJCOPY) -O binary -j .data_code  $(OUT_ELF) ../../../../cpu/br23/tools/data_code.bin 2>/dev/null || true
	$(QUITE) $(OBJCOPY) -O binary -j .overlay_aec $(OUT_ELF) ../../../../cpu/br23/tools/aec.bin      2>/dev/null || true
	$(QUITE) $(OBJCOPY) -O binary -j .overlay_wav $(OUT_ELF) ../../../../cpu/br23/tools/wav.bin      2>/dev/null || true
	$(QUITE) $(OBJCOPY) -O binary -j .overlay_mp3 $(OUT_ELF) ../../../../cpu/br23/tools/mp3.bin      2>/dev/null || true
	$(QUITE) $(OBJCOPY) -O binary -j .overlay_wma $(OUT_ELF) ../../../../cpu/br23/tools/wma.bin      2>/dev/null || true
	$(QUITE) cat ../../../../cpu/br23/tools/text.bin ../../../../cpu/br23/tools/data.bin ../../../../cpu/br23/tools/data_code.bin ../../../../cpu/br23/tools/aec.bin ../../../../cpu/br23/tools/wav.bin ../../../../cpu/br23/tools/mp3.bin ../../../../cpu/br23/tools/wma.bin > ../../../../cpu/br23/tools/app.bin
	$(info ✓ app.bin erzeugt)
	$(QUITE) $(RUN_POST_SCRIPT) sdk

pre_build:
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P ../../../../cpu/br23/sdk_used_list.c -o ../../../../cpu/br23/sdk_used_list.used
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P ../../../../cpu/br23/sdk_ld.c -o ../../../../cpu/br23/sdk.ld
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P ../../../../cpu/br23/tools/download.c -o $(POST_SCRIPT)
	$(QUITE) $(FIXBAT) $(POST_SCRIPT)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P ../../../../cpu/br23/tools/isd_config_rule.c -o ../../../../cpu/br23/tools/isd_config.ini

clean:
	$(QUITE) $(RM) $(OUT_ELF) $(BUILD_DIR)

ifeq ($(LINK_AT), 1)
$(OUT_ELF): $(OBJS)
	$(info +LINK $@)
	$(shell $(MKDIR) $(@D))
	$(file >$(OBJ_FILE), $(OBJS))
	$(QUITE) $(LD) -o $(OUT_ELF) @$(OBJ_FILE) $(LFLAGS) $(LIBPATHS) $(LIBS)
else
$(OUT_ELF): $(OBJS)
	$(QUITE) $(LD) -o $(OUT_ELF) $(OBJS) $(LFLAGS) $(LIBPATHS) $(LIBS)
endif

# ram_stubs.c must NOT be compiled with LTO (-flto).
# If it were LTO bitcode, the linker might strip section attributes from
# delay_nus during LTO merging, placing it in .text (Flash) instead of RAM.
# Compiling without -flto produces a native object whose .volatile_ram_code
# section is always preserved, making the linker-script alias
#   delay_nus.636 = delay_nus;
# redirect the LTO-promoted delay_nus.636 to a real RAM address.
RAM_STUBS_SRC := $(ROOT_PREFIX)/apps/zuupah_pen/examples/zuupah/ram_stubs.c
RAM_STUBS_OBJ := $(BUILD_DIR)/apps/zuupah_pen/examples/zuupah/ram_stubs.c.o
NOFLTO_CFLAGS := $(filter-out -flto,$(CFLAGS))

$(RAM_STUBS_OBJ) : $(RAM_STUBS_SRC)
	$(info +CC [no-lto] $<)
	$(QUITE) $(MKDIR) $(@D)
	$(QUITE) $(CC) $(NOFLTO_CFLAGS) -fno-lto $(DEFINES) $(INCLUDES) -MM -MT $@ $< -o $(@:.o=.d)
	$(QUITE) $(CC) $(NOFLTO_CFLAGS) -fno-lto $(DEFINES) $(INCLUDES) -c $< -o $@

# irq_stubs.c must also be compiled without LTO and WITHOUT the standard
# INCLUDES (omit cpu.h to avoid static-inline redefinition of local_irq_*).
# Only the bare br23 csfr header path is needed for the IPMASK address.
IRQ_STUBS_SRC := $(ROOT_PREFIX)/apps/zuupah_pen/examples/zuupah/irq_stubs.c
IRQ_STUBS_OBJ := $(BUILD_DIR)/apps/zuupah_pen/examples/zuupah/irq_stubs.c.o

$(IRQ_STUBS_OBJ) : $(IRQ_STUBS_SRC)
	$(info +CC [no-lto,no-cpu-h] $<)
	$(QUITE) $(MKDIR) $(@D)
	$(QUITE) $(CC) $(NOFLTO_CFLAGS) -fno-lto $(DEFINES) -MM -MT $@ $< -o $(@:.o=.d)
	$(QUITE) $(CC) $(NOFLTO_CFLAGS) -fno-lto $(DEFINES) -c $< -o $@

$(BUILD_DIR)/%.c.o : $(ROOT_PREFIX)/%.c
	$(info +CC $<)
	$(QUITE) $(MKDIR) $(@D)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -MM -MT $@ $< -o $(@:.o=.d)
	$(QUITE) $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

-include $(DEP_FILES)
