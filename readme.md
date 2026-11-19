# Atari 2600 Emulator (Python)

> **⚠️ Let op:** Dit is een **leerproject** om op laag niveau te begrijpen hoe hardware- en CPU-emulatie werkt. Er is op dit moment nog geen beeld (TIA) of geluid geïmplementeerd, en **geen enkele ROM is momenteel speelbaar**.

## Over dit project

Dit project is een *low-level* Atari 2600-emulator geschreven in Python. Het doel is om stap voor stap de architectuur van de console na te bouwen — te beginnen bij de **MOS 6507 CPU** (een uitgeklede variant van de beroemde 6502-processor met een 13-bits adresbus van maximaal 8 KB).

### Visie op de gebruikerservaring

In tegenstelling tot traditionele emulators met uitgebreide menubalken en instellingenschermen, is het ontwerpdoel een directe, minimalistische ervaring:

* Geen overbodige UI of menu's.
* Direct een ROM openen via bestandsassociatie met `.A26`-bestanden.
* Een subtiele melding bij het opstarten met toegang tot een `F1`-overlay die uitsluitend de keybindings toont.

## Gebruik (ROM inladen)

Zorg ervoor dat je Python 3 geïnstalleerd hebt en dat je ROM-bestand eindigt op `.A26` (standaard 4096 bytes). Je geeft het pad naar het `.A26`-bestand mee als argument in de terminal:

```bash
python main.py pitfall.A26
```

Omdat de CPU nog in ontwikkeling is, zal het programma de instructies uitvoeren totdat het een opcode tegenkomt die nog niet is geïmplementeerd (inmiddels worden er al ruim 1.200 instructies van *Pitfall!* succesvol doorlopen), en vervolgens stoppen met een melding over het betreffende adres en de ontbrekende opcode.

## Huidige Status & Geïmplementeerde Opcodes

De geheugen-mapping (8 KB adresruimte met mirroring, RAM, en 4 KB cartridge ROM op `$1000`–`$1FFF` / `$F000`–`$FFFF`), de Reset Vector (`$FFFC`/`$FFFD`), de registers (`A`, `X`, `Y`, `SP`, `PC`), de belangrijkste addressing modes (`IMM`, `ZP`, `ZPX`, `ABS`, `ABX`, `REL`, `IMP`, `ACC`) en de status-flags zijn opgezet.

### Geïmplementeerde 6507 Instructies

| Opcode | Instructie | Addressing Mode | Beschrijving |
| :--- | :--- | :--- | :--- |
| `0x0A` | `ASL` | Accumulator | *Arithmetic Shift Left* (bits in `A` één plek naar links) |
| `0x10` | `BPL` | Relative | *Branch if Plus* (spring indien `N == 0`) |
| `0x20` | `JSR` | Absolute | *Jump to Subroutine* (bewaart terugkeeradres op stack) |
| `0x29` | `AND` | Immediate | *Logical AND* (bitwise `A & waarde`) |
| `0x30` | `BMI` | Relative | *Branch if Minus* (spring indien `N == 1`) |
| `0x48` | `PHA` | Implied | *Push Accumulator* (duw `A` op de stack) |
| `0x4A` | `LSR` | Accumulator | *Logical Shift Right* (bits in `A` één plek naar rechts) |
| `0x60` | `RTS` | Implied | *Return from Subroutine* (keert terug van subroutine) |
| `0x68` | `PLA` | Implied | *Pull Accumulator* (haal `A` van de stack) |
| `0x78` | `SEI` | Implied | *Set Interrupt Disable* (zet `I`-flag op 1) |
| `0x84` | `STY` | Zero Page | *Store Y Register* (schrijf `Y` naar Zero Page) |
| `0x85` | `STA` | Zero Page | *Store Accumulator* (schrijf `A` naar Zero Page) |
| `0x86` | `STX` | Zero Page | *Store X Register* (schrijf `X` naar Zero Page) |
| `0x90` | `BCC` | Relative | *Branch if Carry Clear* (spring indien `C == 0`) |
| `0x95` | `STA` | Zero Page, X | *Store Accumulator* (schrijf `A` naar `adres + X`) |
| `0x9A` | `TXS` | Implied | *Transfer X to Stack Pointer* |
| `0xA0` | `LDY` | Immediate | *Load Y Register* (laad directe waarde in `Y`) |
| `0xA2` | `LDX` | Immediate | *Load X Register* (laad directe waarde in `X`) |
| `0xA5` | `LDA` | Zero Page | *Load Accumulator* (laad waarde uit Zero Page in `A`) |
| `0xA9` | `LDA` | Immediate | *Load Accumulator* (laad directe waarde in `A`) |
| `0xAD` | `LDA` | Absolute | *Load Accumulator* (laad waarde uit 16-bit adres in `A`) |
| `0xB0` | `BCS` | Relative | *Branch if Carry Set* (spring indien `C == 1`) |
| `0xB5` | `LDA` | Zero Page, X | *Load Accumulator* (laad waarde uit `adres + X` in `A`) |
| `0xBD` | `LDA` | Absolute, X | *Load Accumulator* (laad waarde uit `16-bit adres + X` in `A`) |
| `0xC9` | `CMP` | Immediate | *Compare Accumulator* (vergelijk `A` met waarde en zet flags) |
| `0xCA` | `DEX` | Implied | *Decrement X Register* (`X = X - 1`) |
| `0xD0` | `BNE` | Relative | *Branch if Not Equal* (spring indien `Z == 0`) |
| `0xD8` | `CLD` | Implied | *Clear Decimal Mode* (zet `D`-flag op 0) |
| `0xE8` | `INX` | Implied | *Increment X Register* (`X = X + 1`) |
| `0xF0` | `BEQ` | Relative | *Branch if Equal* (spring indien `Z == 1`) |

*(Eventuele extra toegevoegde geheugen-varianten van instructies zoals `ASL`, `LSR`, `AND`, `CMP`, `STX`, `STY` of `LDY` kunnen hier eenvoudig aan worden toegevoegd.)*

## Roadmap

* [x] Basis 6507 CPU-architectuur, geheugen-mirroring en stack
* [x] CLI ROM-loader (`main.py`) voor `.A26`-bestanden
* [ ] Resterende benodigde 6507 opcodes toevoegen *(in uitvoering — 1.200+ instructies succesvol)*
* [ ] CPU-cycles nauwkeurig bijhouden voor synchronisatie
* [ ] **RIOT-chip** emuleren (timers en joystick/schakelaar-input)
* [ ] **TIA-chip** emuleren (scanline-rendering en NTSC/PAL kleurenpalet)
* [ ] Minimalistisch venster met `F1` keybinding-overlay en `.A26` bestandsassociatie