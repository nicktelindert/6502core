# cartridge.py
import zlib



class Cartridge:  
    def __init__(self, rom_data: bytes, name: str = "Unknown"):
        self.name = f"{name} {hex(zlib.crc32(rom_data) & 0xFFFFFFFF)}"

    def read(self, address: int) -> int:
        raise NotImplementedError

    def write(self, address: int, value: int) -> None:
        pass


class Cartridge2K4K(Cartridge):
    """Standaard 2K (gespiegeld) of 4K cartridge zonder bankswitching."""
    def __init__(self, rom_data: bytes, name):
        super().__init__(rom_data,name)
        self.rom = rom_data
        self.mask = 0x07FF if len(rom_data) <= 0x0800 else 0x0FFF

    def read(self, address: int) -> int:
        return self.rom[address & self.mask]


class CartridgeF8(Cartridge):
    """8K Atari bankswitching (2 banks van 4K, hotspots $1FF8-$1FF9)."""
    def __init__(self, rom_data: bytes, name):
        super().__init__(rom_data,name)
        self.banks = [rom_data[0x0000:0x1000], rom_data[0x1000:0x2000]]
        vec0 = self.banks[0][0xFFC] | (self.banks[0][0xFFD] << 8)
        self.current_bank = 0 if (vec0 & 0x1000) else 1

    def _check_hotspot(self, address: int) -> None:
        addr = address & 0x1FFF
        if addr == 0x1FF8:
            self.current_bank = 0
        elif addr == 0x1FF9:
            self.current_bank = 1

    def read(self, address: int) -> int:
        self._check_hotspot(address)
        return self.banks[self.current_bank][address & 0x0FFF]

    def write(self, address: int, value: int) -> None:
        self._check_hotspot(address)


class CartridgeF6(Cartridge):
    """16K Atari bankswitching (4 banks van 4K, hotspots $1FF6-$1FF9)."""
    def __init__(self, rom_data: bytes, name):
        super().__init__(rom_data,name)
        self.banks = [rom_data[i:i + 0x1000] for i in range(0, 0x4000, 0x1000)]
        self.current_bank = 0
        for idx, bank in enumerate(self.banks):
            vec = bank[0xFFC] | (bank[0xFFD] << 8)
            if vec & 0x1000:
                self.current_bank = idx
                break

    def _check_hotspot(self, address: int) -> None:
        addr = address & 0x1FFF
        if 0x1FF6 <= addr <= 0x1FF9:
            self.current_bank = addr - 0x1FF6

    def read(self, address: int) -> int:
        self._check_hotspot(address)
        return self.banks[self.current_bank][address & 0x0FFF]

    def write(self, address: int, value: int) -> None:
        self._check_hotspot(address)


class CartridgeF6SC(CartridgeF6):
    """16K + 128 bytes Superchip RAM (o.a. Dig Dug, Jr. Pac-Man, Crystal Castles)."""
    def __init__(self, rom_data: bytes, name):
        super().__init__(rom_data, name)
        self.ram = bytearray(128)

    def read(self, address: int) -> int:
        addr = address & 0x1FFF
        if 0x1080 <= addr <= 0x10FF:
            return self.ram[addr & 0x007F]
        return super().read(addr)

    def write(self, address: int, value: int) -> None:
        addr = address & 0x1FFF
        if 0x1000 <= addr <= 0x107F:
            self.ram[addr & 0x007F] = value & 0xFF
            return
        super().write(addr, value)

F6SC_CRC32 = {
    0xee7b80d1, #Another Dig Dug (USA)
    0xB629D904,  # Dig Dug (USA)
    0xD352052E,  # Dig Dug (Europe)
    0x5A27B58C,  # Dig Dug (Rev 1)
}

def _is_f6sc(rom_data: bytes) -> bool:
    if (zlib.crc32(rom_data) & 0xFFFFFFFF) in F6SC_CRC32:
        return True
    for b in range(4):
        page = rom_data[b * 0x1000 : b * 0x1000 + 0x100]
        if len(set(page)) == 1 and page[0] in (0x00, 0xFF):
            return True
    return False


def create_cartridge(rom_data: bytes, name) -> Cartridge:
    size = len(rom_data)
    if size <= 0x1000:
        return Cartridge2K4K(rom_data, name)
    if size == 0x2000:
        return CartridgeF8(rom_data, f"{name} 8K")
    if size == 0x4000:
        if _is_f6sc(rom_data):
            return CartridgeF6SC(rom_data, f"{name} 16K SC")
        return CartridgeF6(rom_data, f"{name} 16K")
    raise ValueError(f"Niet-ondersteunde ROM-grootte: {size} bytes")