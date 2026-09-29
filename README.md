# Game Boy DMG Emulator

A compact C11 emulator for original Game Boy ROMs. The core includes the LR35902 CPU, MBC1 cartridge banking, memory-mapped input and timer registers, interrupts, and scanline-based background/window/sprite rendering.

## Source Layout

- `src/main.c` - SDL application and command-line options
- `src/gb.c` - machine initialization and cycle scheduling
- `src/cpu.c` - LR35902 instruction execution and interrupts
- `src/memory.c` - address bus and memory-mapped registers
- `src/cartridge.c` - ROM loading and MBC1 banking
- `src/timer.c` - divider and programmable timer
- `src/ppu.c` - LCD timing and background/window/sprite rendering
- `src/input.c` - joypad state and interrupts
- `src/gb_internal.h` - private interfaces shared between core modules
- `tests/test_core.c` - focused hardware regression tests

## License

The emulator source code is licensed under the MIT License; see [LICENSE](LICENSE). ROM files in `roms/` are provided separately and are not covered by this license. Use ROMs only when you have the rights to do so.

## Build

Requires CMake, a C11 compiler, and SDL2.

```cmd
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

## Run

```cmd
build\gameboy.exe "roms\True Lies (USA, Europe).gb"
```

Use arrows for the D-pad, Z for A, X for B, Backspace for Select, Enter for Start, and Escape to close the window.

## Headless ROM Check

Run a ROM for a finite number of frames without opening a window. The command exits nonzero if an illegal CPU opcode is reached.

```cmd
build\gameboy.exe "roms\True Lies (USA, Europe).gb" --headless --frames 120
build\gameboy.exe "roms\Mortal Kombat 3 (USA).gb" --headless --frames 120
```
