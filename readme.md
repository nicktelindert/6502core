# Atari 2600 Emulator (MOS 6507)

Een minimalistische, headless Atari 2600-emulator gebouwd in Python als leerproject om de architectuur van de **MOS 6507 CPU** en retro-hardware op instructie- en geheugenniveau te doorgronden.

## Architectuur & Ontwerpdoelen
* **Headless-first:** Geen complexe GUI tijdens de CPU-ontwikkelfase; volledige focus op zuivere instructielogica, registers, status-flags en geheugenadressering.
* **MOS 6507 CPU:** Implementatie van de 6502-variant met een 13-bit adresbus (`8 KB` adresruimte via `& 0x1FFF`).
* **ROM-ondersteuning:** Direct laden van `.A26` ROM-bestanden.
* **Cycle-voorbereiding:** Opcode-tabellen bevatten nu al basis-cyclewaarden ter voorbereiding op de latere integratie met de **TIA** (*Television Interface Adaptor*) en *racing the beam*-timing.

## Voortgang
* **Huidige mijlpaal:** Meer dan **1.305 uitgevoerde instructies** in *Pitfall!* (`pitfall.A26`).
* **Werkwijze:** Iteratieve opbouw waarbij ontbrekende opcodes stapsgewijs worden geanalyseerd en toegevoegd zodra de ROM ze aanroept.

## Geïmplementeerde Onderdelen (`cpu6507.py`)

### Registers & Flags
* **Registers:** `A` (Accumulator), `X`, `Y`, `SP` (Stack Pointer), `PC` (Program Counter)
* **Processor Status Flags:** `N` (Negative), `V` (Overflow), `B` (Break), `D` (Decimal), `I` (Interrupt Disable), `Z` (Zero), `C` (Carry)

### Addressing Modes
* `mode_IMP` (Implied)
* `mode_IMM` (Immediate)
* `mode_ZP` (Zero Page)
* `mode_ZPX` (Zero Page, X)
* `mode_ABS` (Absolute)
* `mode_ABX` (Absolute, X)
* `mode_REL` (Relative — inclusief signed two's complement offset-berekening)

### Geïmplementeerde Opcodes
| Categorie | Instructies | Opcodes (o.a.) |
| :--- | :--- | :--- |
| **Load / Store** | `LDA`, `LDX`, `LDY`, `STA`, `STX`, `STY` | `0xA9`, `0xA5`, `0xAD`, `0xBD`, `0xA2`, `0xA0`, `0x85`, `0x86`, `0x84` |
| **Rekenkundig** | `ADC` | `0x69`, `0x65`, `0x75`, `0x6D` |
| **Logisch & Shifts** | `AND`, `EOR`, `LSR` | `0x29`, `0x45`, `0x4A` |
| **Vergelijkingen** | `CMP`, `CPX`, `CPY` | `0xC9`, `0xE0`, `0xE4`, `0xEC`, `0xC0`, `0xC4`, `0xCC` |
| **Branches** | `BNE`, `BEQ`, `BPL`, `BMI`, `BCS`, `BCC` | `0xD0`, `0xF0`, `0x10`, `0x30`, `0xB0`, `0x90` |
| **Stack** | `PHA`, `PLA` | `0x48`, `0x68` |
| **Flags & Status** | `CLC`, `SEC`, `SEI`, `CLD` | `0x18`, `0x38`, `0x78`, `0xD8` |
| **Control & Overig** | `JMP`, `JSR`, `RTS`, `INX`, `DEX`, `INY`, `DEY`, `TXS`, `NOP` | `0x4C`, `0x20`, `0x60`, `0xE8`, `0xCA`, `0xC8`, `0x88`, `0x9A`, `0xEA` |

## Roadmap
- [x] Basis CPU-structuur, geheugenbus (8 KB) en stack-operaties
- [x] Kernset van load/store-, vergelijkings-, logische en branch-opcodes
- [x] Eerste rekenkundige optelling (`ADC`) en carry-afhandeling
- [ ] Volledige 6507-instructieset afmaken via *Pitfall!*
- [ ] Cycle-teller activeren en koppelen aan scanline-timing
- [ ] TIA-registers en basis beeldopbouw implementeren