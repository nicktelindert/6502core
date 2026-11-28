# 6502core Emulator (C)

Originally, I built this project in Python and was quite happy with how it turned out. However, Python eventually hits a performance ceiling when emulating hardware at low-level cycle accuracy. Because of that—and with the goal of supporting heavier 6502-based consoles down the road—I decided to rewrite the entire codebase in C. Porting everything over is quite a task, and there are still a few bugs left to iron out, but the foundation is solid.

## Architecture & Goal
The goal of **6502core** is to provide a clean, cycle-oriented MOS 6502 CPU core (including variants like the 6507) that can drive multiple classic consoles. Right now, the focus is on the Atari lineup—starting with the Atari 2600—before expanding to other 6502-based systems.

The `CPU6502` core in `cpu6502.c` accesses memory and timing through the `CpuBus` interface in `cpu6502.h`. `cpu_init_with_bus` accepts a system-specific context and callbacks for reads, writes, idle cycles and per-instruction cycle accounting; callbacks and their context must remain valid while the CPU is used. Reads and writes each account for one CPU cycle, and addresses are passed through as full 16-bit values. The existing `cpu_init` connects the Atari 2600 bus, which applies its own address mirroring and advances TIA, RIOT and cartridge timing. Optional JSR/RTS callbacks preserve the current 2600 FE cartridge behavior; they are not required for other buses.

This separation is a foundation, not Atari 5200 or 7800 support. Those systems still need their own memory maps, chips, interrupt and timing behavior, and cartridge handling. The current CPU is not yet a complete or cycle-exact implementation for those machines.

The shared core models NMOS decimal ADC/SBC and exposes `cpu_set_irq` and `cpu_set_nmi` with `true` meaning an asserted active-low input. IRQ is level-sensitive; NMI latches an assertion edge. Interrupt entry uses the 6502 vectors and stack sequence, and CLI, SEI and PLP use the preceding interrupt-mask state for the next boundary. Input changes are serviced at instruction boundaries; exact interrupt sampling during individual cycles, interrupt hijacking, and 5200-specific DMA/HALT/RDY behavior are not yet modeled. The Atari 2600 does not connect these interrupt inputs.

## Rewritten TIA (Color-Clock Accuracy)
After researching and documenting the hardware behavior, I rewrote the TIA to render at the **color-clock level**. This provides more accurate video and audio timing than the earlier scanline-based approach. Scanline tracking remains useful for frame bookkeeping, but it does not replace color-clock rendering.

## PAL and NTSC
ROMs whose filename contains `PAL` (case-insensitive) start in PAL mode; all others start in NTSC mode. The directory name is not used. Press **F4** to switch between PAL and NTSC while a ROM is running. Loading another ROM selects its standard again from its filename. The current standard is shown next to the FPS counter.

PAL uses a 50 Hz frame limit and NTSC uses 60 Hz. The TIA audio resampling and DPC oscillator use the corresponding CPU clock. Frame height is determined by the ROM's VSYNC rather than forced to 312 or 262 lines. F3 toggles the frame limit. PAL colors are an approximation using the PAL hue ordering and the existing RGB model, not an analog PAL signal simulation; SECAM is not supported.

## Tests
Run `make test` for the CPU, Atari 2600 system, TIA, RIOT and synthetic cartridge regression tests. The cartridge tests cover bank-switch hotspots, 6507 address mirrors, shared Superchip RAM, switching banks with a `BIT` instruction, and indexed and branch dummy reads.

The standalone CPU bus test links only the CPU and addressing code and checks a full 16-bit test memory without Atari 2600 hardware.

It also tests interrupt entry, priority, masking and stack behavior, representative NMOS decimal flags, and all 40,000 valid BCD ADC/SBC combinations with both carry inputs. For an additional independent test, supply the separately obtained `bin_files/6502_functional_test.bin` from [Klaus Dormann's 6502 functional tests](https://github.com/Klaus2m5/6502_65C02_functional_tests) and run `make test-functional FUNCTIONAL_ROM=/path/to/6502_functional_test.bin`. The runner expects the 64 KB image from commit `7954e2dbb49c469ea286070bf46cdd71aeb29e4b` (SHA-256 `fa12bfc761e6f9057e4cc01a665a7b800ff01ae91f598af1e39a1201d01953fd`); that test covers official instructions but not all interrupt, undocumented-opcode or cycle-level behavior.

The Atari 2600 system tests check all 65,536 CPU addresses against the TIA, RIOT, RAM and cartridge bus decoding and 13-bit mirrors. A synthetic cartridge exercises reset, instructions, RAM, TIA and RIOT together; another checks a WSYNC stall and RIOT timing. Cartridge tests cover 2K/4K ROM mirrors, F8, F6/F6SC, F4/F4SC and EF bank hotspots, E0's switchable slices, F8SC/F6SC/F4SC RAM, and FA's 256-byte RAM. They also cover 3F's 64 KB layout and writes through the TIA address space, including a CPU instruction; 3F ROMs larger than 64 KB are not supported. DPC tests cover bank switching, fetcher registers, oscillator timing for both video standards and a startup timer-polling loop. RIOT tests cover timer intervals and underflow, I/O and timer write mirrors. TIA tests cover player/missile copies, collisions, priority, reflection, vertical delay, WSYNC and PAL color/audio timing. A reload test verifies TIA state is reset between cartridges, video selection and DPC startup after a reload.

Mapper hotspot expectations were checked against Stella's implementations of [F4](https://github.com/stella-emu/stella/blob/master/src/emucore/CartF4.cxx), [E0](https://github.com/stella-emu/stella/blob/master/src/emucore/CartE0.cxx), [EF](https://github.com/stella-emu/stella/blob/master/src/emucore/CartEF.cxx) and [3F](https://github.com/stella-emu/stella/blob/master/src/emucore/Cart3F.hxx). Superchip write-port reads use the emulator's last modeled bus value as an approximation of the floating bus.

These are regression tests, not complete Atari 2600 certification. They do not cover every cartridge mapper, TIA mid-scanline and audio behavior, all CPU dummy bus accesses, or all RIOT I/O behavior. Passing the mapper tests does not establish mid-scanline timing accuracy. Timer phase follows Stella's implementation and has not been independently verified on hardware. At power-on the timer uses a deterministic `$80` value with a 1024-cycle divider; Stella randomizes the value, so this is not a guarantee of actual hardware power-on state. The separate 6502 functional suite runs on flat 64 KB memory, not on the 2600's mirrored bus.

To prevent a program that never produces VSYNC from hanging or growing the framebuffer without limit, the emulator stops the loaded cartridge and reports an error after 32,768 scanlines without a frame. Unusually long intentional startup periods may hit this limit.

## Bugs
- [ ] Dig Dug sometimes misses enemies? (timing issues?)

## Roadmap
- [x] Rewrite core emulator from Python to C
- [x] Rewrite TIA to operate at color-clock level
- [x] Stabilize low-latency SDL audio queue
- [x] Fix remaining top-scanline flicker in *Jungle Hunt* (NTSC)
- [x] Set rendering to 4:3 ratio
- [x] Add PAL and NTSC selection
- [x] Borrowed the Cartridge detection from Stella
- [x] Added GPL V2 license to be compatible with Stella
- [x] Add DPC support
- [ ] Expand Atari support (e.g., Atari 5200 / 7800)
- [ ] Add support for the NES
- [ ] Consider other consoles using the MOS 6502