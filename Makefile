# Host build of the Tetromino ornament: tests and the simulator.
# The firmware has its own Makefile in stm32f429/.
#
#   make test      build and run the host tests
#   make sim       build build/sim (see sim/sim.c for its options)
#   make shots     render a few frames and game over screens to build/shots
#   make assets    regenerate game/assets.[ch] from art/ (needs Pillow)

# make's built-in default for CC is cc, which MinGW does not have.
ifeq ($(origin CC),default)
CC      := gcc
endif
CFLAGS  ?= -std=c11 -O2 -g -Wall -Wextra -Wvla -Wpointer-arith -Wwrite-strings
CFLAGS  += -Igame -Isim

BUILD   := build
GAME    := game/tetris.c game/render.c game/assets.c game/hiscore.c game/app.c
HDRS    := $(wildcard game/*.h sim/*.h)

.PHONY: all test sim shots assets clean

all: test sim

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
