# Tetromino desktop ornament: one entry point for everything.
#
# Firmware (cross compiler, see stm32f429/Makefile for TOOLCHAIN_ROOT):
#   make firmware        build stm32f429/firmware.elf
#   make flash           build and flash over the on-board ST-LINK
#   make clean-firmware  remove the firmware build
#   Options on the command line are passed through, e.g.
#   make flash ROTATE=1 INFOTEXT=  (no info text)
#
# Host (any C11 gcc):
#   make test            build and run the host tests
#   make sim             build build/sim (see sim/sim.c for its options)
#   make shots           render a few frames and game over screens to build/shots
#   make assets          regenerate game/assets.[ch] from art/ (needs Pillow)
#   make clean           remove the host build
#
# Plain "make" runs the host tests and builds the simulator.

# make's built-in default for CC is cc, which MinGW does not have.
ifeq ($(origin CC),default)
CC      := gcc
endif
CFLAGS  ?= -std=c11 -O2 -g -Wall -Wextra -Wvla -Wpointer-arith -Wwrite-strings
CFLAGS  += -Igame -Isim

BUILD   := build
GAME    := game/tetris.c game/render.c game/assets.c game/hiscore.c game/app.c game/fx.c
HDRS    := $(wildcard game/*.h sim/*.h)

.PHONY: all test sim shots assets clean firmware flash clean-firmware

all: test sim

# --- firmware, built by stm32f429/Makefile ---------------------------------

firmware:
	$(MAKE) -C stm32f429

flash:
	$(MAKE) -C stm32f429 flash

clean-firmware:
	$(MAKE) -C stm32f429 clean

# --- host ------------------------------------------------------------------

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_tetris: tests/test_tetris.c sim/flash_ram.c $(GAME) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_tetris.c sim/flash_ram.c $(GAME)

$(BUILD)/sim: sim/sim.c sim/flash_ram.c $(GAME) $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ sim/sim.c sim/flash_ram.c $(GAME)

test: $(BUILD)/test_tetris
	./$(BUILD)/test_tetris

sim: $(BUILD)/sim

shots: $(BUILD)/sim
	mkdir -p $(BUILD)/shots
	./$(BUILD)/sim -t 60000 -e 2000 -g -o $(BUILD)/shots

assets:
	python3 tools/gen_assets.py

clean:
	rm -rf $(BUILD)
