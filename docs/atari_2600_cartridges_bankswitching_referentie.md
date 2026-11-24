# Atari 2600 Cartridges & Bankswitching — Technische Referentie

De 6507-processor in de Atari 2600 heeft maar **13 adreslijnen** ($A_0$ t/m $A_{12}$), waardoor hij direct maximaal **8 KB** (`0x0000` t/m `0x1FFF`) kan adresseren. Omdat de eerste 4 KB gereserveerd is voor de TIA, RIOT-RAM en I/O-registers, blijft er voor de cartridge zelf maar **4 KB (`4096` bytes)** over op het adresbereik **`0x1000` t/m `0x1FFF`**.

Om toch spellen van 8 KB, 16 KB, 32 KB of meer te draaien, gebruiken cartridges **bankswitching**: slimme hardware op de cartridge-printplaat kijkt mee op de adresbus en schakelt een ander blok geheugen (een *bank*) in het `0x1000`–`0x1FFF` venster.

---

## 1. De Geheugenmap van de 6507 (13-bit adresbus)

In de CPU-emulator zie je vaak 16-bit adressen (zoals `0xF000`–`0xFFFF` voor de reset-vector), omdat de bovenste 3 bits ($A_{13}, A_{14}, A_{15}$) niet zijn aangesloten op de 6507. Maskeer elk adres daarom altijd eerst met **`addr & 0x1FFF`**:

| Adresbereik (gemaskeerd) | Grootte | Onderdeel | Opmerking |
| :--- | :--- | :--- | :--- |
| `0x0000` – `0x007F` | 128 B | **TIA** | Schrijf- en leesregisters (spiegelt elke `0x0080` bytes) |
| `0x0080` – `0x00FF` | 128 B | **RIOT RAM** | Werkgeheugen & Stack (spiegelt ook op `0x0180`–`0x01FF`) |
| `0x0280` – `0x029F` | 32 B | **RIOT I/O & Timer** | Joysticks, console-switches en interval-timer |
| **`0x1000` – `0x1FFF`** | **4096 B** | **Cartridge ROM (en evt. extra RAM)** | Hier kijkt de cartridge mee op de adresbus |

---

## 2. Overzicht van de belangrijkste Cartridge-types

### A. Standaard zonder Bankswitching

| Type | ROM-grootte | Werking |
| :--- | :--- | :--- |
| **`2K`** | 2048 bytes (`2 KB`) | Geen bankswitching. De 2 KB wordt twee keer gespiegeld in `0x1000`–`0x1FFF` (lees met `rom[addr & 0x07FF]`). |
| **`4K`** | 4096 bytes (`4 KB`) | Geen bankswitching. Eén-op-één gekoppeld aan `0x1000`–`0x1FFF` (lees met `rom[addr & 0x0FFF]`). |

---

### B. Standaard Atari Bankswitching (`F8`, `F6`, `F4`)

Dit zijn veruit de meest voorkomende formats. Zodra de CPU een **lees- of schrijfactie** doet op een specifiek *hotspot-adres*, schakelt de cartridge direct de actieve 4 KB-bank om.

| Type | ROM-grootte | Aantal banken (4 KB) | Hotspot-adressen |
| :--- | :--- | :--- | :--- |
| **`F8`** | `8 KB` | 2 banken (`0`, `1`) | `0x1FF8` = Bank 0, `0x1FF9` = Bank 1 |
| **`F6`** | `16 KB` | 4 banken (`0` t/m `3`) | `0x1FF6` = Bank 0, `0x1FF7` = Bank 1, `0x1FF8` = Bank 2, `0x1FF9` = Bank 3 |
| **`F4`** | `32 KB` | 8 banken (`0` t/m `7`) | `0x1FF4` t/m `0x1FFB` = Bank 0 t/m 7 |

> **Belangrijk detail:** Ook bij het **lezen** van een hotspot (bijv. `LDA 0x1FF9`) wisselt de bank *eerst*, en geeft de cartridge daarna de byte op dat adres uit de *nieuwe* bank terug.

---

### C. Superchip (`F8SC`, `F6SC`, `F4SC` — +128 bytes extra RAM)

Sommige `F8`, `F6` en `F4` cartridges hebben een extra **128 bytes RAM-chip** (de *Atari Superchip*) ingebouwd aan het begin van de cartridge-ruimte:

* **Schrijven naar extra RAM:** `0x1000` t/m `0x107F` (128 bytes)
* **Lezen van extra RAM:** `0x1080` t/m `0x10FF` (dezelfde 128 bytes!)
* **Normale ROM + hotspots:** `0x1100` t/m `0x1FFF`

*(Waarom aparte read/write-adressen? De 6507 heeft geen `R/W`-pin op de cartridge-poort! Daarom gebruikt de Superchip adreslijn $A_7$ (`0x0080`) om het verschil tussen schrijven (`0`) en lezen (`1`) te zien.)*

---

### D. Overige veelgebruikte mappers (`FE`, `E0`, `3F`, `FA`)

| Type | ROM-grootte | Bekend van / Werking |
| :--- | :--- | :--- |
| **`FE`** | `8 KB` (2 × 4 KB) | **Activision** (o.a. *Decathlon*, *Robot Tank*). Kijkt naar stack-toegang rond `0x01FE` en bit `0x20` van het teruggekeerde adres om tussen bank 0 en 1 te schakelen. |
| **`E0`** | `8 KB` (8 × 1 KB) | **Parker Brothers** (o.a. *Frogger*, *Star Wars*). Verdeelt `0x1000`–`0x1FFF` in **4 slots van 1 KB**:<br>• Slot 0 (`0x1000`–`0x13FF`): hotspots `0x1FE0`–`0x1FE7` (kiest slice `0..7`)<br>• Slot 1 (`0x1400`–`0x17FF`): hotspots `0x1FE8`–`0x1FEF` (kiest slice `0..7`)<br>• Slot 2 (`0x1800`–`0x1BFF`): hotspots `0x1FF0`–`0x1FF7` (kiest slice `0..7`)<br>• Slot 3 (`0x1C00`–`0x1FFF`): **altijd vast** op de laatste 1 KB slice (`slice 7`) |
| **`3F`** | `8 KB` – `512 KB` | **Tigervision** (o.a. *Miner 2049er*). Verdeelt het bereik in **2 slots van 2 KB**:<br>• Slot 0 (`0x1000`–`0x17FF`): wisselbaar door een banknummer te **schrijven** naar adres **`0x003F`**<br>• Slot 1 (`0x1800`–`0x1FFF`): **altijd vast** op de laatste 2 KB bank van de ROM |
| **`FA`** | `12 KB` (3 × 4 KB) + 256 B RAM | **CBS RAM Plus** (o.a. *Omega Race*). Hotspots `0x1FF8`, `0x1FF9`, `0x1FFA` voor 3 banken; `0x1000`–`0x10FF` (write) en `0x1100`–`0x11FF` (read) voor 256 bytes extra RAM. |

---

## 3. Hoe detecteer je het Cartridge-type bij het laden?

Een standaard `.bin` of `.a26` ROM-bestand heeft **geen header** (het zijn alleen ruwe ROM-bytes). Je emulator bepaalt het type daarom meestal op basis van de **bestandsgrootte** in combinatie met een **handtekening-scan** op de bytecode:

```
┌──────────────────────────────────────────────────────────────────────┐
│ Bestandsgrootte checken                                              │
├──────────────┬───────────────────────────────────────────────────────┤
│ 2048 bytes   │ -> CART_2K                                            │
│ 4096 bytes   │ -> CART_4K                                            │
│ 8192 bytes   │ -> Standaard CART_F8 (check bytecode voor FE / E0 / 3F)│
│ 12288 bytes  │ -> CART_FA (CBS RAM Plus)                             │
│ 16384 bytes  │ -> CART_F6                                            │
│ 32768 bytes  │ -> CART_F4                                            │
└──────────────┴───────────────────────────────────────────────────────┘
```

### Veelgebruikte heuristieken in C bij 8 KB / 16 KB / 32 KB:
1. **Superchip (`SC`) detecteren:** Staat in de eerste 256 bytes van elke bank overal dezelfde waarde (`0x00` of `0xFF`), dan is die ruimte gereserveerd voor de 128 bytes Superchip RAM.
2. **`FE` (Activision) herkennen:** Zoek in de ROM naar opcode-patronen die op `0x01FE` werken (zoals `0x20, 0x00, 0xD0` of `JSR` naar een andere bank).
3. **`E0` (Parker Bros) herkennen:** Zoek naar instructies die de hotspots `0x1FE0`–`0x1FF7` aanspreken (zoals `LDA 0x1FE0`, bytes `0xAD, 0xE0, 0x1F` of `0xE0, 0xFF`).
4. **`3F` (Tigervision) herkennen:** Zoek naar `STA 0x3F` (bytes `0x85, 0x3F`) in de ROM.

---

## 4. Voorbeeld-implementatie voor uitlezen & schrijven in C

Hieronder zie je een strakke opzet voor het uitlezen (`cart_read`) en schrijven (`cart_write`) van de meest gebruikte types (`2K`, `4K`, `F8`, `F6`, `F4` + Superchip):

```c
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    CART_2K,
    CART_4K,
    CART_F8,
    CART_F6,
    CART_F4,
    CART_E0,
    CART_FE,
    CART_3F
} CartType;

typedef struct {
    uint8_t *rom;
    size_t   rom_size;
    CartType type;
    int      current_bank;   // Actieve 4K bank (of slot-index)
    uint8_t  ram[256];       // Optioneel extra cartridge RAM (Superchip / CBS)
    bool     has_superchip;  // 128 bytes RAM op 0x1000..0x10FF
} Cartridge;

// Hulpfunctie: update actieve bank als een hotspot geraakt wordt
static void cart_check_hotspots(Cartridge *cart, uint16_t addr) {
    switch (cart->type) {
        case CART_F8:
            if (addr == 0x1FF8) cart->current_bank = 0;
            else if (addr == 0x1FF9) cart->current_bank = 1;
            break;

        case CART_F6:
            if (addr >= 0x1FF6 && addr <= 0x1FF9) {
                cart->current_bank = addr - 0x1FF6; // Bank 0..3
            }
            break;

        case CART_F4:
            if (addr >= 0x1FF4 && addr <= 0x1FFB) {
                cart->current_bank = addr - 0x1FF4; // Bank 0..7
            }
            break;

        default:
            break;
    }
}

uint8_t cart_read(Cartridge *cart, uint16_t addr) {
    addr &= 0x1FFF; // 6507 heeft 13 adreslijnen

    // 1. Eerst checken of dit adres een bankswitch triggert!
    cart_check_hotspots(cart, addr);

    // 2. Superchip RAM uitlezen (0x1080 .. 0x10FF)
    if (cart->has_superchip && addr >= 0x1080 && addr <= 0x10FF) {
        return cart->ram[addr & 0x007F];
    }

    // 3. ROM uitlezen op basis van cartridge-type
    switch (cart->type) {
        case CART_2K:
            return cart->rom[addr & 0x07FF];

        case CART_4K:
            return cart->rom[addr & 0x0FFF];

        case CART_F8:
        case CART_F6:
        case CART_F4: {
            size_t offset = (size_t)cart->current_bank * 0x1000u + (addr & 0x0FFFu);
            return cart->rom[offset];
        }

        default:
            return cart->rom[addr & 0x0FFF];
    }
}

void cart_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    addr &= 0x1FFF;

    // 1. Ook een schrijfactie naar een hotspot (bijv. STA 0x1FF9) triggert een bankswitch
    cart_check_hotspots(cart, addr);

    // 2. Superchip RAM beschrijven (0x1000 .. 0x107F)
    if (cart->has_superchip && addr >= 0x1000 && addr <= 0x107F) {
        cart->ram[addr & 0x007F] = val;
    }
}
```

### Twee veelgemaakte fouten bij Cartridge-emulatie
1. **Vergeten dat `STA` op een hotspot ook bankswitcht:** Sommige games gebruiken `BIT 0x1FF8` of `LDA 0x1FF8` (lezen), andere gebruiken `STA 0x1FF8` of `NOP 0x1FF8` (schrijven / dummy-write). Roep je hotspot-check dus in **zowel `cart_read` als `cart_write`** aan.
2. **Startbank bij opstarten:** Bij `F8`, `F6` en `F4` staat de startbank op echte hardware niet altijd gegarandeerd op `0` — goed geschreven ROMs hebben in élke bank op `0x1FFC`–`0x1FFD` een geldige reset-vector staan die direct naar de gewenste startbank springt. Beginnen op `current_bank = 0` werkt daardoor vrijwel altijd direct.