# Atari 2600 Emulator (Python)

Een cycle-accurate Atari 2600 (VCS) emulator geschreven in Python met Pygame, gebouwd vanaf scratch om de architectuur van de MOS 6507, TIA en RIOT (6532) te doorgronden.

## Features

- **MOS 6507 CPU (`cpu6507.py`)**: Volledige 6502-instructieset (13-bit adresbus) inclusief cycle-accurate uitvoering en page-crossing timing.
- **TIA Graphics & Timing (`tia.py`)**: "Racing the beam" rendering op color-clock niveau (228 color clocks per scanline, 262 NTSC scanlines) met ondersteuning voor `WSYNC`, `HMOVE`, `RESPx`, Playfield (`PF0`–`PF2`), Players (`P0`/`P1`), Missiles (`M0`/`M1`) en Ball (`BL`).
- **TIA Audio**: 2-kanaals geluidsweergave via 4-bit, 5-bit en 9-bit Linear Feedback Shift Registers (LFSR).
- **RIOT 6532**: 128 bytes RAM, interval-timers en joystick/console-schakelaars.
- **Debug Overlay**: Ingebouwde live inspector (`F1`) voor scanline, color clock en sprite-posities.

## Geteste Games

- *Pac-Man*
- *Pitfall!*
- *Jungle Hunt*
- *Donkey Kong* (speelbaar; kleine mid-scanline playfield-offset in bewerking)

## Installatie & Gebruik

1. **Vereisten installeren**:
   ```bash
   pip install pygame
   ```
   *(Op Linux / Distrobox met PipeWire/PulseAudio eventueel `pulseaudio-libs` installeren en `SDL_AUDIODRIVER=pulseaudio` gebruiken.)*

2. **Emulator starten**:
   ```bash
   python main.py pad/naar/rom.bin
   ```

## Besturing

| Toets | Actie |
| :--- | :--- |
| **W / A / S / D** of **Pijltjes** | Joystick beweging |
| **Ctrl / Shift / Spatie** | Fire-knop |
| **F1** | Debug overlay tonen/verbergen |
| **ESC** | Afsluiten |

## BUGS
- Het geluid is nog een beetje om te janken, maar heb er een basis in zitten
- Er zitten nog grafische bugs in, met name Donkey Kong gaat niet zo soepel...

## Roadmap

- [ ] Mid-scanline `PF1`/`PF2` latching verfijnen voor *Donkey Kong*
- [ ] Audio-output balanceren en resampling vloeiender maken
- [ ] Ondersteuning voor extra bankswitching-schemes (bijv. F8 / F6)
- [ ] Code opschonen/refactoring(veel spaghetti code!)
