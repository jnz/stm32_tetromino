# Makefile for the Tetromino ornament on the STM32F429I-DISC1.
#
#   make                  build firmware.elf/.bin/.hex
#   make flash            build and flash over the on-board ST-LINK
#   make ROTATE=1         picture turned by 180 degrees (display.h)
#   make AI_BLUNDER=n     per mille of random AI moves, 0 = perfect (main.c)
#   make TOUCH=1          touch control (off by default, not reliable yet)
#   make LEDS=0           user LEDs off (default: green per line, red records)
#   make DEBUG=1          debug port stays up in sleep, for attaching to the
#                         running board (costs most of the WFI saving)
#   make Q=               show the full command lines
#
# TOOLCHAIN_ROOT must point at the arm-none-eabi tools. Put it in a
# config.mk next to this file, e.g.
#
#	TOOLCHAIN_ROOT=C:/ST/STM32CubeCLT_1.21.0/GNU-tools-for-STM32/bin/
#
# Build structure taken from the INSLIB sensor firmware.

-include config.mk

BUILD_CONFIG := $(MAKEFILE_LIST)

ifeq ($(TOOLCHAIN_ROOT),)
	$(error TOOLCHAIN_ROOT is not defined. Please set TOOLCHAIN_ROOT in config.mk)
endif

Q ?= @

NPROC ?= $(or $(NUMBER_OF_PROCESSORS),$(shell nproc 2>/dev/null),4)
MAKEFLAGS += -j$(NPROC)

OPTIMIZE_LEVEL ?= 2
ROTATE         ?= 0
AI_BLUNDER     ?= 0
TOUCH          ?= 0
DEBUG          ?= 0
LEDS           ?= 1

APPNAME  := firmware
OBJDIR   := build
GAME     := ../game

CROSS_COMPILE ?= arm-none-eabi-
ARCH_FLAGS    := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mlittle-endian --specs=nosys.specs
LINKER_SCRIPT := STM32F429ZITX_FLASH.ld

APP_CPP_FLAGS += -DUSE_HAL_DRIVER -DSTM32F4xx -DSTM32F429xx
APP_CPP_FLAGS += -DUSE_STM32F429I_DISCOVERY_REVD
APP_CPP_FLAGS += -DDISPLAY_ROTATE_180=$(ROTATE) -DAI_BLUNDER=$(AI_BLUNDER) -DTETRIS_TOUCH=$(TOUCH) -DTETRIS_DEBUG=$(DEBUG) -DTETRIS_LEDS=$(LEDS)
APP_CPP_FLAGS += -fno-strict-aliasing -fno-math-errno
# Debug information lives in the ELF only, objcopy strips it from the
# .bin/.hex that gets flashed.
APP_CPP_FLAGS += -g3

WARNING_CHECKS := -Wall -Wextra -Wframe-larger-than=4096 -Wstack-usage=4096
WARNING_CHECKS += -Wpointer-arith -Wwrite-strings -Wvla -Wno-unknown-pragmas

default: all

APP_SUBDIRS += \
	./Core/Src \
	$(GAME) \
	./Drivers/BSP/STM32F429I-Discovery \
	./Drivers/BSP/Components/ili9341 \
	./Drivers/BSP/Components/stmpe811 \
	./Drivers/STM32F4xx_HAL_Driver/Src \

APP_INCLUDE_PATH += \
	-I"Core/Inc/" \
	-I"$(GAME)/" \
	-I"Drivers/BSP/Components/Common/" \
	-I"Drivers/BSP/Components/ili9341/" \
	-I"Drivers/BSP/Components/stmpe811/" \
	-I"Drivers/BSP/STM32F429I-Discovery/" \
	-I"Drivers/STM32F4xx_HAL_Driver/Inc/" \
	-I"Drivers/CMSIS/Include/" \
	-I"Drivers/CMSIS/Device/ST/STM32F4xx/Include/" \
	-I"Utilities/Fonts/" \

S_STARTUP := startup_stm32f429zitx
S_SRC     := Core/Startup/$(S_STARTUP).s

LIBRARIES := -lm

# =======================================================================

CC       := $(TOOLCHAIN_ROOT)$(CROSS_COMPILE)gcc
OC       := $(TOOLCHAIN_ROOT)$(CROSS_COMPILE)objcopy
OD       := $(TOOLCHAIN_ROOT)$(CROSS_COMPILE)objdump
SIZEINFO := $(TOOLCHAIN_ROOT)$(CROSS_COMPILE)size

COMPILER_FLAGS  = -O$(OPTIMIZE_LEVEL) $(WARNING_CHECKS) -c
COMPILER_FLAGS += -ffunction-sections -fdata-sections -MMD $(APP_CPP_FLAGS) $(ARCH_FLAGS)
ASM_FLAGS       = -x assembler-with-cpp $(COMPILER_FLAGS)
LINK_FLAGS      = -Wl,--gc-sections -Wl,-Map=$(APPNAME).map -T $(LINKER_SCRIPT) $(ARCH_FLAGS)

# The font tables are #included by stm32f429i_discovery_lcd.c, so
# Utilities is an include path only. The game renders its own fonts
# (game/assets.c) and does not use them.
C_SRCS    = $(foreach dir,$(APP_SUBDIRS),$(wildcard $(dir)/*.c))
VPATH     = $(APP_SUBDIRS)
OBJS      = $(addprefix $(OBJDIR)/,$(notdir $(C_SRCS:%.c=%.o))) $(OBJDIR)/$(S_STARTUP).o
C_DEPS    = $(OBJS:%.o=%.d)

COMPILER_CMDLINE = -std=c11 $(COMPILER_FLAGS) $(APP_INCLUDE_PATH)

# Vendor code (ST HAL and BSP) gets relaxed warnings, so that -Wall -Wextra
# stays meaningful for Core/Src and the game.
VENDOR_OBJ_DIRS := ./Drivers/STM32F4xx_HAL_Driver/Src \
                   ./Drivers/BSP/STM32F429I-Discovery \
                   ./Drivers/BSP/Components/ili9341 \
                   ./Drivers/BSP/Components/stmpe811
VENDOR_SRCS     := $(foreach dir,$(VENDOR_OBJ_DIRS),$(wildcard $(dir)/*.c))
VENDOR_OBJS     := $(addprefix $(OBJDIR)/,$(notdir $(VENDOR_SRCS:%.c=%.o)))
$(VENDOR_OBJS): WARNING_CHECKS := -Wall -Wno-unused-parameter -Wno-unknown-pragmas

$(OBJDIR):
	$(Q)mkdir -p $(OBJDIR)

# Rebuild on a changed command line (ROTATE=1, AI_BLUNDER=...), not only on
# changed sources: the stamp holds the flags and is rewritten only when
# they differ.
FLAGS_STAMP := $(OBJDIR)/build-flags

.PHONY: FORCE
FORCE:

$(FLAGS_STAMP): FORCE | $(OBJDIR)
	$(Q)echo '$(COMPILER_CMDLINE) $(LINK_FLAGS)' | cmp -s - $@ \
	    || echo '$(COMPILER_CMDLINE) $(LINK_FLAGS)' > $@

$(OBJDIR)/$(S_STARTUP).o: $(S_SRC) $(BUILD_CONFIG) $(FLAGS_STAMP) | $(OBJDIR)
	@echo 'ASM: $<'
	$(Q)$(CC) $(ASM_FLAGS) $(APP_INCLUDE_PATH) -o "$@" "$<"

$(OBJDIR)/%.o: %.c $(BUILD_CONFIG) $(FLAGS_STAMP) | $(OBJDIR)
	@echo 'CC: $<'
	$(Q)$(CC) $(COMPILER_CMDLINE) -o "$@" "$<"

all: $(APPNAME).elf

-include $(C_DEPS)

$(APPNAME).elf: $(OBJS) $(LINKER_SCRIPT) $(BUILD_CONFIG) $(FLAGS_STAMP)
	@echo 'LD: $@'
	$(Q)$(CC) $(LINK_FLAGS) -o "$@" $(OBJS) $(LIBRARIES)
	$(Q)$(OC) -O binary "$@" "$(APPNAME).bin"
	$(Q)$(OC) -O ihex "$@" "$(APPNAME).hex"
	$(Q)$(SIZEINFO) "$@"

clean:
	rm -rf $(OBJDIR) $(APPNAME).elf $(APPNAME).bin $(APPNAME).hex $(APPNAME).map

# STM32_Programmer_CLI ships next to the compiler in STM32CubeCLT.
PROGRAMMER ?= $(TOOLCHAIN_ROOT)../../STM32CubeProgrammer/bin/STM32_Programmer_CLI

# mode=UR (connect under reset) and no full chip erase: only the sectors
# being written are erased, so the high score in bank 2 survives a reflash.
# Never add "-e all" here.
flash: all
	@echo "Flashing $(APPNAME).elf"
	$(Q)"$(PROGRAMMER)" -c port=SWD mode=UR -w $(APPNAME).elf -v -rst

.PHONY: all clean flash default
