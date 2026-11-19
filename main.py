import argparse
import sys
from cpu6507 import CPU6507

def main():
    parser = argparse.ArgumentParser(description="Atari 2600 Emulator")
    parser.add_argument("rom", help="Pad naar het ROM-bestand (bijv. pitfall.bin)")
    args = parser.parse_args()

    try:
        with open(args.rom, "rb") as f:
            rom_data = f.read()
    except FileNotFoundError:
        print(f"Fout: Kan het bestand '{args.rom}' niet vinden.")
        sys.exit(1)

    if len(rom_data) != 4096:
        print(f"Waarschuwing: ROM is {len(rom_data)} bytes (standaard Atari 2600 ROM is 4096 bytes).")

    cpu = CPU6507(rom_data)
    print(f"ROM '{args.rom}' geladen. Startadres (PC): {hex(cpu.PC)}")

    stappen = 0
    try:
        while True:
            cpu.step()
            stappen += 1
    except NotImplementedError as e:
        print(f"Gestopt na {stappen} uitgevoerde instructies -> {e}")

if __name__ == "__main__":
    main()
