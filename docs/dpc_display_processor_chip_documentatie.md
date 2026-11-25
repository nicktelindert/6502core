# Atari 2600 DPC (Display Processor Chip) Documentatie

De **DPC (Display Processor Chip)**, ontworpen door David Crane voor *Pitfall II: Lost Caverns*, is een custom co-processor op de cartridge die de 6507-CPU ontlast tijdens de krappe horizontal blanking (HBlank)-tijd.

---

## 1. Geheugenindeling (Cartridge Space)

De DPC-cartridge bevat **10 KB** aan gegevens, verdeeld in twee delen, plus de geheugengekoppelde DPC-registers:

| Adresbereik | Grootte | Beschrijving |
| :--- | :--- | :--- |
| `$1000 – $103F` | 64 bytes | **DPC Read Registers** (Random, Muziek, Data-fetchers) |
| `$1040 – $107F` | 64 bytes | **DPC Write Registers** (Top/Bottom grenzen, Counters, Reset) |
| `$1080 – $1FF7` | ~4 KB | **Programma ROM** (Bank 0 of Bank 1) |
| `$1FF8` | 1 byte | **Bankswitch Hotspot:** Selecteert Bank 0 (`$0000 – $0FFF` in ROM) |
| `$1FF9` | 1 byte | **Bankswitch Hotspot:** Selecteert Bank 1 (`$1000 – $1FFF` in ROM) |
| `$1FFA – $1FFF` | 6 bytes | Interrupt- en resetvectoren (`NMI`, `RESET`, `IRQ`) |
| **Display Data ROM** | 2 KB | Aparte 2 KB ROM (`$2000 – $27FF` in het `.bin`-bestand) die uitsluitend via de 8 data-fetchers wordt uitgelezen |

> **Let op:** Een standaard *Pitfall II*-ROM is **10.495** of **10.240** bytes groot (2 × 4 KB programma-ROM + 2 KB display-ROM, eventueel gevolgd door 255 bytes padding/chip-identificatie).

---

## 2. De 8 Data-Fetchers

De kern van de DPC bestaat uit **8 onafhankelijke data-fetchers** (genummerd `0` t/m `7`). Elke fetcher heeft:
* **11-bit Counter (`0x000 – 0x7FF`):** Wijst naar een byte in de 2 KB Display Data ROM.
* **8-bit `Top` register:** Bovenste grens van het actieve venster (vergeleken met de lage byte van de counter).
* **8-bit `Bottom` register:** Onderste grens van het actieve venster (vergeleken met de lage byte van de counter).
* **8-bit `Flag` (`0x00` of `0xFF`):** Geeft aan of de fetcher zich binnen het actieve bereik bevindt.

### Werking van een Fetcher-Read
Wanneer de 6507 een leesactie uitvoert op een fetcher-adres (`$1008 – $103F`):
1. De DPC haalt de byte op uit de 2 KB Display Data ROM op index `counter & 0x7FF`.
2. De `Flag` wordt bijgewerkt op basis van `top` en `bottom`:
   * Als `(counter & 0xFF) == top`: `flag = 0xFF`
   * Als `(counter & 0xFF) == bottom`: `flag = 0x00`
3. Afhankelijk van het gekozen adresvenster wordt de data al dan niet gemaskeerd met `flag` (`data & flag`) en/of bit-getransformeerd.
4. Na de leesactie wordt de 11-bit `counter` **automatisch met 1 verlaagd** (`counter = (counter - 1) & 0x7FF`), klaar voor de volgende scanline.

---

## 3. Overzicht van DPC-Registers

### Read Registers (`$1000 – $103F`)
Voor elk bereik geldt: `fetcher = address & 0x07` (voor `$1008 – $103F`).

| Adresbereik | Functie | Maskering met `Flag`? | Bewerking op gelezen byte (`D`) |
| :--- | :--- | :--- | :--- |
| `$1000 – $1003` | **Random Number Generator** | N.v.t. | Huidige waarde van 8-bit LFSR (klokt bij elke read) |
| `$1004 – $1007` | **Music / Audio Output** | N.v.t. | Gemixte 4-bit amplitude van fetchers 5, 6 en 7 (voor `AUDV0`) |
| `$1008 – $100F` | **Data Fetcher (Raw)** | Nee | Ongewijzigde byte `D` |
| `$1010 – $1017` | **Data Fetcher (Masked)** | Ja (`D & flag`) | Gemaskeerde byte |
| `$1018 – $101F` | **Data Fetcher (Bit-Reversed)** | Ja (`D & flag`) | Bits gespiegeld (`b0↔b7`, `b1↔b6`, `b2↔b5`, `b3↔b4`) |
| `$1020 – $1027` | **Data Fetcher (Nibble-Swapped)** | Ja (`D & flag`) | Bovenste en onderste 4 bits omgewisseld (`(D << 4) \| (D >> 4)`) |
| `$1028 – $102F` | **Data Fetcher (Shift Right)** | Ja (`D & flag`) | 1 bit naar rechts geschoven (`D >> 1`) |
| `$1030 – $1037` | **Data Fetcher (Shift Left)** | Ja (`D & flag`) | 1 bit naar links geschoven (`D << 1`) |
| `$1038 – $103F` | **Data Fetcher Flag** | N.v.t. | Geeft direct `flag` terug (`0x00` of `0xFF`) |

### Write Registers (`$1040 – $107F`)
Voor elk bereik geldt: `fetcher = address & 0x07`.

| Adresbereik | Functie | Werking bij schrijfactie (`val`) |
| :--- | :--- | :--- |
| `$1040 – $1047` | **Set Top Register** | `top[f] = val` |
| `$1048 – $104F` | **Set Bottom Register** | `bottom[f] = val` |
| `$1050 – $1057` | **Set Counter Low** | `counter[f] = (counter[f] & 0x0700) \| val` |
| `$1058 – $105F` | **Set Counter High / Control** | `counter[f] = ((val & 0x07) << 8) \| (counter[f] & 0x00FF)`<br>Voor fetchers 5–7: bit 4 (`0x10`) schakelt de muziekmodus in/uit |
| `$1060 – $1067` | *(Ongebruikt)* | Geen effect |
| `$1068 – $106F` | *(Ongebruikt)* | Geen effect |
| `$1070 – $1077` | **Reset Random Generator** | Zet de 8-bit LFSR terug naar zijn startwaarde (meestal `0x01`) |
| `$1078 – $107F` | *(Ongebruikt)* | Geen effect |

---

## 4. Random Number Generator (LFSR)

De DPC beschikt over een 8-bit Linear Feedback Shift Register (LFSR):
* **Startwaarde bij reset / `$1070–$1077`:** `0x01`
* **Bij elke leesactie op `$1000 – $1003`:**
  ```c
  uint8_t bit = ((lfsr >> 7) ^ (lfsr >> 5) ^ (lfsr >> 4) ^ (lfsr >> 3)) & 1;
  lfsr = (lfsr << 1) | bit;
  ```
* Geeft daarna de nieuwe `lfsr`-waarde terug.

---

## 5. Muziek- en Geluidsgenerator (Fetchers 5, 6 en 7)

Fetchers `5`, `6` en `7` kunnen worden ingezet als onafhankelijke blokgolf-oscillatoren op een interne DPC-klok van circa **20 kHz** (ongeveer 1 DPC-audio-tick per 59–60 CPU-cycles):

1. **Activatie:** Wanneer bit 4 (`0x10`) is gezet bij een write naar `$105D`, `$105E` of `$105F`, draait de betreffende fetcher in muziekmodus.
2. **Klok- en tellerlogica (per ~20 kHz tick):**
   * De teller van de actieve muziekfetcher telt af (`counter--`).
   * Wanneer `(counter & 0xFF) == bottom[f]`, wordt de lage byte herladen met `top[f]`.
   * Zolang `(counter & 0xFF)` boven het omslagpunt ligt (of `flag == 0xFF`), levert het kanaal een actieve puls op.
3. **Output op `$1004 – $1007`:**
   * De drie kanalen leveren elk een gewogen bijdrage (`4`, `5` en `6`) die samen worden opgeteld tot een waarde tussen `0` en `15` (`0x00 – 0x0F`).
   * De 6507 leest `$1004` periodiek (vaak één keer per scanline) en schrijft deze waarde rechtstreeks naar TIA-register `AUDV0`.

---

## 6. Aandachtspunten voor Emulatie

* **Geen directe TIA-aanpassing nodig:** De TIA zelf werkt identiek aan een standaard 2600-cartridge. Alle DPC-communicatie loopt via normale 6507-lees- en schrijfacties op `$1000 – $107F`.
* **Flag-evaluatie bij writes én reads:** Zorg ervoor dat `flag[f]` direct wordt bijgewerkt wanneer `top[f]`, `bottom[f]` of `counter[f]` wordt geschreven (`$1040 – $105F`), én tijdens een data-fetcher read (`$1008 – $103F`). Als `flag[f]` onterecht op `0x00` blijft hangen, maskeert `D & flag` alle sprite- en kleurdata op `0x00`.
* **Programma-ROM vs. Display-ROM:** Bankswitching via `$1FF8` / `$1FF9` wisselt alleen het adresbereik `$1080 – $1FFF` tussen de eerste en tweede 4 KB van de ROM. De 2 KB Display Data ROM (offset `0x2000` in het ROM-bestand) blijft altijd direct toegankelijk voor de 8 data-fetchers, ongeacht de actieve programmabank.
* **Bus-reflectie ($1000 vs. $3000/$5000/$7000/$9000/$B000/$D000/$F000):** Omdat de 6507 een 13-bit adresbus heeft (`A0–A12`), moeten alle cartridge-adressen vooraf worden gemaskeerd met `address & 0x1FFF`.