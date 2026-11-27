# 6502core Emulator (C)

Originally, I built this project in Python and was quite happy with how it turned out. However, Python eventually hits a performance ceiling when emulating hardware at low-level cycle accuracy. Because of that—and with the goal of supporting heavier 6502-based consoles down the road—I decided to rewrite the entire codebase in C. Porting everything over is quite a task, and there are still a few bugs left to iron out, but the foundation is solid.

## Architecture & Goal
The goal of **6502core** is to provide a clean, cycle-oriented MOS 6502 CPU core (including variants like the 6507) that can drive multiple classic consoles. Right now, the focus is on the Atari lineup—starting with the Atari 2600—before expanding to other 6502-based systems.

## Rewritten TIA (Color-Clock Accuracy)
After researching and documenting the hardware behavior, I rewrote the TIA to render at the **color-clock level**. This provides more accurate video and audio timing than the earlier scanline-based approach. Scanline tracking remains useful for frame bookkeeping, but it does not replace color-clock rendering.


## Bugs
- [ ] Donkey Kong Jr start rendering way too late

## Roadmap
- [x] Rewrite core emulator from Python to C
- [x] Rewrite TIA to operate at color-clock level
- [x] Stabilize low-latency SDL audio queue
- [x] Fix remaining top-scanline flicker in *Jungle Hunt* (NTSC)
- [x] Set rendering to 4:3 ratio and add overscan option(only height)
- [x] Borrowed the Cartridge detection from Stella
- [x] Added GPL V2 license to be compatible with Stella
- [x] Add DPC support
- [ ] Expand Atari support (e.g., Atari 5200 / 7800)
- [ ] Add support for the NES
- [ ] Consider other consoles using the MOS 6502