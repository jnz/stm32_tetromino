# Tetromino desktop ornament (STM32F429I-DISC1)

![Tetromino on the STM32F429I-DISC1](tetromino.gif)

Play the browser version: <https://zwiener.org/tetromino.html>

AI port (Claude Code) of my browser Tetromino
(<https://zwiener.org/tetromino.html>, 2011-2016) to the
STM32F429I-DISC1 with its 240x320 panel. The AI plays, and when it loses, the
game over screen shows the score next to the all-time best for 8 s before the
next game starts. The best score and the number of games are kept in flash.

```
game/        platform independent: game core + AI, renderer, high score store,
             ornament flow (app.c), generated artwork (assets.c)
stm32f429/   firmware: HAL, BSP, display and flash glue, its own Makefile
             (called from the top level one)
sim/         host simulator, writes frames as PPM
tests/       host tests
tools/       gen_assets.py (artwork from art/ -> game/assets.c)
art/         tile sheet and background of the browser version, input of
             gen_assets.py
```

## Flash a prebuilt release

No toolchain needed: download `firmware.bin` from the
[releases page](https://github.com/jnz/stm32_tetromino/releases), connect
the board through its ST-LINK USB port and copy the file onto the drive
that shows up (DIS_F429ZI). The board restarts with the game.

Alternatively flash `firmware.hex` with STM32CubeProgrammer, which only
erases the sectors it writes, so a high score from an earlier version
survives the update.

## Build and flash

Everything from the repository root:

```
make flash           # build the firmware and flash it over the on-board ST-LINK
make firmware        # build only (stm32f429/firmware.elf)
make test            # host tests: game rules, AI, flash store, renderer
make shots           # host simulator, renders frames to build/shots
```

The firmware needs the arm-none-eabi toolchain: put its path into
`stm32f429/config.mk`, e.g. `TOOLCHAIN_ROOT=C:/ST/STM32CubeCLT_1.21.0/GNU-tools-for-STM32/bin/`.
The host targets need any C11 gcc.

Options for the firmware go on the same command line:

```
make flash ROTATE=1              # picture turned by 180 degrees (USB cable at the bottom)
make flash INFOTEXT=zwiener.org  # a line of text bottom right in the side panel
make flash AI_BLUNDER=50         # the AI places 5 % of its pieces at random
make flash DEBUG=1               # probe can attach while running, see below
```

## Behaviour

- **Port faithfulness.** Field 10x22 (2 hidden spawn rows), 7-bag randomizer,
  wall kick tables, scoring (40/100/300/1200 x level), level every 10 lines up
  to 20, line flash, ghost piece, and the AI (2-ply search) moving the piece
  one key press per 50 ms tick.
- **Exact evaluation.** The AI's weights are integers, not floats. Equally good
  placements then tie exactly and the first one found wins, so the host
  simulator and the board make the same moves from the same position, whatever
  the compiler.
- **The AI plays perfectly by default** (`AI_BLUNDER=0`) and practically
  does not lose. To see game overs, a share of pieces can be placed at random.
- **High score in flash.** `game/hiscore.c` appends a 32 byte record (CRC32,
  sequence number) per save into a two sector log in bank 2, sectors 14/15
  at 0x08108000. A save happens at every game over and every 20 min while a
  game is ahead of the record. An erase happens once per 512 saves.
- **256 colours, no SDRAM.** The framebuffers hold one palette index per
  pixel (LTDC format L8), the LTDC turns them into colours through a 256
  entry CLUT. That makes a frame 75 KB, so both buffers fit into the
  internal SRAM, and everything else (variables, stack) moves to the CCM.
  The SDRAM is never initialised and is held in power-down. The palette is
  built by `tools/gen_assets.py` from the colours the renderer produces,
  blending still happens in RGB and maps back through lookup tables.
- **Partial redraw.** Each framebuffer has a cache of what it shows, a frame
  only draws the cells and values that differ. `make test` checks every
  partial frame pixel by pixel against a full one.
- **Unattended.** Independent watchdog (~4 s), faults reset the board.
- **User button** (B1, blue). Short press: fast drop on or off, the AI
  drops each piece as soon as it is in place (`AI>` in the title bar). A
  hard drop scores a point per row, so fast games score a little more.
  Long press (1 s): picture turned by 180 degrees, or back. The turn is
  relative to the build's `ROTATE`. Both settings are kept in flash, 5 s
  after the last change.
- **LEDs.** Green LD3 blinks once per cleared line (four times for a
  tetromino clear). Red LD4 blinks 6 times when the running game overtakes
  the all-time best, along with "NEW RECORD!" on a record game over, and is
  on for a second after any other game over. `make LEDS=0` keeps them dark.

## Live debugging

The core sleeps in `__WFI()` between game steps, which most of the time
keeps a probe from attaching to the running board. `make DEBUG=1 flash`
sets `DBGMCU_CR.DBG_SLEEP`, then `STM32_Programmer_CLI -c port=SWD
mode=HOTPLUG` attaches without a reset. It keeps the core clocked in sleep,
so it is not the default. Flashing works either way.

## Regenerating the artwork

`python tools/gen_assets.py` (Pillow, DejaVu Sans Bold from matplotlib or the
system). Tiles come from `art/tetromino_blocks.png` scaled from 25 to
15 px, the background from `art/basi.png`.

