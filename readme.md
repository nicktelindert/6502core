# 6502core

6502core is an Atari emulator written in C. It supports the **Atari 2600** and has experimental **Atari 5200** support. Games and the 5200 BIOS are not included.

## Getting started

On Linux, download the latest `6502core-x86_64.AppImage` from [Releases](https://github.com/nicktelindert/6502core/releases), make it executable and run it:

```sh
chmod +x 6502core-x86_64.AppImage
./6502core-x86_64.AppImage
```

AppImages normally require FUSE. If needed, use `./6502core-x86_64.AppImage --appimage-extract-and-run` instead.

To build from source, install a C compiler, Make and SDL2 development files, then run:

```sh
make 6502core_rel
./6502core_rel
```

You can also pass a ROM file as an argument. On first launch, choose a ROM folder in the browser. Use the arrow keys and Enter to navigate, Backspace to go up, and **S** to save the folder. **F2** opens a game; **F5** changes the folder.

## Controls

| Key | Action |
| --- | --- |
| F1 | Show help |
| F2 | Open a game |
| F3 | Toggle the frame limit |
| F4 | Switch Atari 2600 PAL/NTSC |
| F5 | Choose ROM folder |
| F6 | Choose Atari 5200 BIOS |
| Escape | Close the emulator or browser |

**Atari 2600:** WASD moves, Ctrl fires and Shift resets.

**Atari 5200, player 1:** WASD or arrow keys move; Ctrl and Space are the two fire buttons. Enter starts, P pauses and R resets. Use 0–9 for the keypad and numpad `*` and `/` for `*` and `#`. The stick returns to the center when released. Physical controllers and player 2 are not supported yet.

## Atari 5200

Before opening an `.a52` game, press **F6** and select your own 2048-byte Atari 5200 BIOS. The selection is saved for next time.

5200 support is still experimental: picture, sound, controls and compatibility may not be correct in every game. The loader supports raw 4, 8, 16 and 32 KB games; some cartridge layouts are not supported.

## Video standard

Atari 2600 games with `PAL` in their filename start in PAL mode; other games start in NTSC mode. Use **F4** to switch while playing. PAL colors are approximate; SECAM is not supported.

## Tests and demos

Run `make test` for the regression tests. Two video demos are available without ROMs:

```sh
make 2600-demo 5200-demo
./2600-demo          # Add --pal for PAL
./5200-demo
```

Press Escape to close a demo. Add `--once` to run a single-frame check.

The tests help catch regressions but do not guarantee that every game works correctly. For an optional additional CPU test, see [Klaus Dormann's functional tests](https://github.com/Klaus2m5/6502_65C02_functional_tests) and run `make test-functional FUNCTIONAL_ROM=/path/to/6502_functional_test.bin`.

## Roadmap

- Improve Atari 5200 compatibility and sound.
- Add physical controller support and configurable controls.
- Support more Atari 2600 controllers, such as paddles.
- Explore additional systems, including the Atari 7800 and NES.
