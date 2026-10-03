# Tetromino desktop ornament (STM32F429I-DISC1)

AI port (Claude Code) of my browser Tetromino
(<https://zwiener.org/tetromino.html>, 2011-2016) to the
STM32F429I-DISC1 with its 240x320 panel. The AI plays, and when it loses, the
game over screen shows the score next to the all-time best for 8 s before the
next game starts. The best score and the number of games are kept in flash.

Display, flash and touch code come from the INSLIB sensor firmware for the
same board.

```
game/        platform independent: game core + AI, renderer, high score store,
             ornament flow (app.c), generated artwork (assets.c)
stm32f429/   firmware: HAL, BSP, display + flash glue, Makefile
sim/         host simulator, writes frames as PPM
tests/       host tests
tools/       gen_assets.py (artwork from art/ -> game/assets.c)
art/         tile sheet and background of the browser version, input of
             gen_assets.py
```

## Build and flash

```
cd stm32f429
make                 # needs TOOLCHAIN_ROOT in stm32f429/config.mk
make flash           # STM32_Programmer_CLI over the on-board ST-LINK
make ROTATE=1        # picture turned by 180 degrees (USB cable at the bottom)
```

Host side, from the repository root (any C11 gcc):

```
make test            # game rules, AI, flash store incl. torn writes, renderer
make shots           # renders frames and game over screens to build/shots
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
  at 0x08108000. Sectors 12/13 belong to the INSLIB configuration store, so
  the board can switch between both firmwares without either losing its
  data, and `make flash` does not erase bank 2. A save happens at every game
  over and every 20 min while a game is ahead of the record. An erase happens
  once per 512 saves.
- **Unattended.** Independent watchdog (~4 s), faults reset the board.
- **LEDs.** Green LD3 blinks once per cleared line (four times for a
  tetromino clear). Red LD4 blinks 6 times when the running game overtakes
  the all-time best, along with "NEW RECORD!" on a record game over, and is
  on for a second after any other game over. `make LEDS=0` keeps them dark.

## Live debugging

The core sleeps in `__WFI()` between game steps, which most of the time
keeps a probe from attaching to the running board. `make DEBUG=1 flash`
sets `DBGMCU_CR.DBG_SLEEP`, then `STM32_Programmer_CLI -c port=SWD
mode=HOTPLUG` attaches without a reset. It keeps the core clocked in sleep,
so it is not the default. Flashing works either way. Useful symbols
(addresses from `arm-none-eabi-nm firmware.elf`):

- `g_timing`: worst game step and frame in CPU cycles (180 per us). Measured
  on the board: AI step about 15 ms, frame 8.5 ms, against a 50 ms tick.
- `g_touch`: last touch state (present, pressed, x, y, presses), only with
  `TOUCH=1`.
- The visible frame: `-u 0xD0000000 0x4B000 fb.bin`, ARGB8888 (layer 0, or
  `0xD004B000` when the LTDC's layer 2 is the one enabled).

## Regenerating the artwork

`python tools/gen_assets.py` (Pillow, DejaVu Sans Bold from matplotlib or the
system). Tiles come from `art/tetromino_blocks.png` scaled from 25 to
15 px, the background from `art/basi.png`.

