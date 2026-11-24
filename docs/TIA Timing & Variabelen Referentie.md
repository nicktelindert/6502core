# Atari 2600 TIA — Beknopte Technische Referentie

De **TIA** (*Television Interface Adapter*) heeft geen framebuffer zoals een moderne videokaart. Hij tekent **live mee** met de elektronenstraal van de oude beeldbuis, één pixel per keer. Jouw C-code is dus in feite een **klokgestuurde catch-up renderer** die bijhoudt welke pixels al getekend zijn op de huidige scanline.

## 1. De Kernvariabelen (Wat je in je `struct TIA` nodig hebt)

### Timing, Sync & Blanking
| Variabele / Register | Type | Wat het is in gewone programmeertaal |
| :--- | :--- | :--- |
| **`color_clock`** | `int` (`0–227`) | De X-positie op de huidige lijn. **1 color clock = 1 pixel-tik**. Bij 228 springt hij terug naar 0. |
| **`rendered_clock`** | `int` (`0–228`) | Tot welke color clock de scanline al is uitgerekend in de framebuffer (voor *catch-up rendering*). |
| **`scanline`** | `int` (`0–261`) | De Y-positie (welke horizontale lijn de straal tekent). NTSC heeft **262 lijnen**, PAL **312**. |
| **`WSYNC`** (`$02`) | *Strobe* | Een "pauzeknop" voor de CPU. Schrijven naar adres `$02` zet de CPU stil tot `color_clock == 0` van de volgende lijn. |
| **`VSYNC`** (`$00`) | `uint8_t` | Bit 1 (`0x02`) zet het signaal "ga terug naar linksboven (nieuw frame)" aan of uit. |
| **`VBLANK`** (`$01`) | `uint8_t` | Bit 1 (`0x02`) zet de elektronenstraal uit (**zwart beeld**) tijdens de boven- en ondermarge. *(Let op: `$02` is het adres van `WSYNC`, terwijl `0x02` als bitmasker op `vblank` bit D1 test!)* |

### Objecten, Positie & Vertraging (Vertical Delay)
| Variabele / Register | Type | Wat het is in gewone programmeertaal |
| :--- | :--- | :--- |
| **`RESP0` / `RESP1`** | *Strobe* | Geen waarde, maar een **trigger**. Zodra de CPU hierheen schrijft, wordt de X-positie van Player 0 of 1 vastgepind op de huidige `color_clock`. |
| **`HMP0` + `HMOVE`** | `int8_t` + *Strobe* | Fijnafstelling van Player X-positie ($-8$ t/m $+7$ pixels). `HMP0` slaat het getal op, `HMOVE` voert de verschuiving uit. |
| **`PF0`, `PF1`, `PF2`** | `uint8_t` | 20 bits samen voor de achtergrond (Playfield). Elke bit is **4 pixels breed** ($20 \times 4 = 80$ pixels = linkerhelft scherm; rechterhelft is kopie of spiegelbeeld). |
| **`GRP0`, `GRP1`** | `uint8_t` | 8-bits actieve bitmap voor de vorm van Player 0 en Player 1 (1 bit = 1 pixel, tenzij uitgerekt via `NUSIZ`). |
| **`grp0_old`, `grp1_old`** | `uint8_t` | Schaduwkopieën van de vorige `GRP0`/`GRP1`-waarde voor de hardwarematige *Vertical Delay*. |
| **`VDELP0` (`$25`), `VDELP1` (`$26`)** | `uint8_t` | Als bit 0 (`0x01`) aanstaat, gebruikt de TIA tijdens het tekenen `grp0_old` (of `grp1_old`) in plaats van de nieuwe bitmap. |

> **Tip voor C:** Een *strobe-register* (`WSYNC`, `RESP0`, `HMOVE`, `CXCLR`) slaat niets op. In je `tia_write(uint16_t addr, uint8_t val)` vang je simpelweg `case 0x10:` (`RESP0`) af en voer je direct een functie uit.

## 2. Hoe één Scanline en de Catch-Up Renderer werken

De **6502 CPU** en de **TIA** lopen synchroon, maar de TIA is **3x zo snel**:

$$\text{1 CPU-cyclus} = \text{3 TIA color clocks}$$

```
|<------------------------ 1 SCANLINE = 228 COLOR CLOCKS (76 CPU CYCLES) ------------------------>|
|                                                                                                 |
|<---- HBLANK (68 clocks / 22.67 cycles) ---->|<------- ZICHTBAAR BEELD (160 pixels) ------------>|
|  0                                       67 | 68                                           227  |
|  [ Straal keert terug, geen beeld ]         | [ Pixels worden naar framebuffer geschreven ]     |
|                                             |                                                   |
^                                             ^                                                   |
`-- WSYNC laat CPU hier weer los              `-- x = 0 in framebuffer (clock - 68)              |
```

### Stap voor stap in je catch-up renderfunctie (`rendered_clock` $\rightarrow$ `target_clock`)

1. **Afknippen op het zichtbare venster (`68..228`):**
   * Alles onder color clock `68` valt in HBLANK en levert geen zichtbare pixels op.
   * Reken het zichtbare bereik om naar framebuffer-coördinaten (`0..160`):
     ```c
     int start = (tia->rendered_clock > 68) ? tia->rendered_clock : 68;
     int end   = (target_clock < 228) ? target_clock : 228;

     if (start >= end) {
         tia->rendered_clock = target_clock;
         return; // Viel volledig binnen HBLANK of leeg bereik
     }

     int x_start = start - 68; // 0..159
     int x_end   = end - 68;   // 1..160
     ```
2. **Check of `VBLANK` actief is (`tia->vblank & 0x02u`):**
   * Staat bit 1 van `VBLANK` aan? Vul het segment `[x_start, x_end)` in de framebuffer met `0` (zwart), werk `tia->rendered_clock = target_clock` bij en keer direct terug.
3. **Pixels tekenen volgens prioriteit (wanneer `VBLANK` uitstaat):**
   * Bereken voor elke pixel tussen `x_start` en `x_end` de kleur op basis van de prioriteitsvolgorde:
     $$\text{Player 0 / Missile 0} > \text{Player 1 / Missile 1} > \text{Playfield / Ball} > \text{Achtergrond (COLUBK)}$$
4. **Botsingen (Collisions) bijwerken:**
   * Raken twee actieve objecten elkaar op dezelfde pixel? Zet het juiste bit met `|=` aan.
5. **Scanline afronden bij `color_clock >= 228`:**
   ```c
   tia->color_clock++;
   if (tia->color_clock >= 228) {
       tia->color_clock = 0;
       tia->rendered_clock = 0;
       tia->scanline++;
       tia->wsync_halt = false; // CPU mag weer verder!
   }
   ```

## 3. De kruislingse werking van `VDELP0` en `VDELP1`

Op de TIA heb je per scanline weinig tijd om alle registers bij te werken. Daarom werkt de hardware met een kruislingse delay-buffer:

* **Schrijven naar `GRP0` (`$1B`):** zet `tia->grp0 = v`, én bewaart de *vorige* `grp1` in `tia->grp1_old`.
* **Schrijven naar `GRP1` (`$1C`):** zet `tia->grp1 = v`, én bewaart de *vorige* `grp0` in `tia->grp0_old`.

```c
case 0x1Bu: /* GRP0 */
    tia->grp0 = v;
    tia->grp1_old = tia->grp1;
    break;

case 0x1Cu: /* GRP1 */
    tia->grp1 = v;
    tia->grp0_old = tia->grp0;
    break;
```

* **Bij het tekenen van Player 0:**
  * `(tia->vdelp0 & 0x01u)` is `1` $\rightarrow$ teken `tia->grp0_old`
  * `(tia->vdelp0 & 0x01u)` is `0` $\rightarrow$ teken `tia->grp0`
* *(En voor Player 1 precies hetzelfde met `vdelp1`, `grp1_old` en `grp1`.)*

## 4. De 8 Collision-registers (`CX*`)

De TIA detecteert botsingen op pixelniveau tijdens het tekenen. Bits worden **alleen aangezet (`|=`)** en blijven staan totdat de CPU naar **`CXCLR` (`$2C`)** schrijft om ze allemaal terug te zetten naar `0`.

| Register (Leesadres) | Variabele | Bit 7 (`0x80`) | Bit 6 (`0x40`) |
| :--- | :--- | :--- | :--- |
| **`CXM0P`** (`$00`) | `cxm0p` | Missile 0 $\leftrightarrow$ Player 1 | Missile 0 $\leftrightarrow$ Player 0 |
| **`CXM1P`** (`$01`) | `cxm1p` | Missile 1 $\leftrightarrow$ Player 0 | Missile 1 $\leftrightarrow$ Player 1 |
| **`CXP0FB`** (`$02`) | `cxp0fb` | Player 0 $\leftrightarrow$ Playfield | Player 0 $\leftrightarrow$ Ball |
| **`CXP1FB`** (`$03`) | `cxp1fb` | Player 1 $\leftrightarrow$ Playfield | Player 1 $\leftrightarrow$ Ball |
| **`CXM0FB`** (`$04`) | `cxm0fb` | Missile 0 $\leftrightarrow$ Playfield | Missile 0 $\leftrightarrow$ Ball |
| **`CXM1FB`** (`$05`) | `cxm1fb` | Missile 1 $\leftrightarrow$ Playfield | Missile 1 $\leftrightarrow$ Ball |
| **`CXBLPF`** (`$06`) | `cxblpf` | Ball $\leftrightarrow$ Playfield | *(niet gebruikt)* |
| **`CXPPMM`** (`$07`) | `cxppmm` | Player 0 $\leftrightarrow$ Player 1 | Missile 0 $\leftrightarrow$ Missile 1 |

## 5. Opbouw van een volledig Frame (NTSC — 262 lijnen)

De TIA telt zelf niet tot 262; de game-code regelt dit via `VSYNC` en `VBLANK`:

```
Scanline
  0  +---------------------------------------+
     |  VSYNC (3 lijnen)                     | -> Bit 1 van VSYNC wordt 1: start nieuw frame!
  3  +---------------------------------------+
     |  VBLANK (37 lijnen)                   | -> Game berekent AI, input, posities (onzichtbaar)
 40  +=======================================+
     |                                       |
     |  ZICHTBAAR SCHERM (192 lijnen)        | -> VBLANK = 0. Hier racet de CPU per lijn
     |  (160 x 192 pixels)                   |    om registers aan te passen ("racing the beam")
     |                                       |
232  +=======================================+
     |  OVERSCAN (30 lijnen)                 | -> VBLANK weer 1. Extra rekentijd voor de game
262  +---------------------------------------+
```

## 6. `RESP0`-offset en Synchronisatie-loop

### Waarom `RESP0` +5 pixels verschuift
Wanneer de CPU `STA RESP0` uitvoert, eindigt die instructie op het laatste moment van de laatste CPU-cyclus. In de TIA duurt het nog even voordat het reset-signaal door de interne logica loopt:
* Schrijft de CPU tijdens **HBLANK** (`color_clock < 68`) $\rightarrow$ Player 0 start op pixel `0` (`clock 68`).
* Schrijft de CPU tijdens **zichtbaar beeld** (`color_clock >= 68`) $\rightarrow$ Player 0 verschijnt vanaf de **volgende scanline** op `(color_clock - 68) + 5` (of `+ 6` bij meerdere sprite-kopieën via `NUSIZ0`).

### Minimale C-loop om CPU en TIA synchroon te houden
```c
// Voor elke instructie die de 6502 uitvoert:
int cpu_cycles = cpu_step(&cpu);

// Laat de TIA 3x zoveel tikken inhalen:
for (int i = 0; i < cpu_cycles * 3; i++) {
    tia_tick(&tia);
}

// Als WSYNC actief is, tik de TIA door tot het einde van de scanline:
while (tia.wsync_halt) {
    tia_tick(&tia);
}
```