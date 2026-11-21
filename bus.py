class Bus:
    def __init__(self, tia, riot, cartridge):
        self.tia = tia
        self.riot = riot
        self.cartridge = cartridge
        self.ram = bytearray(128)
        self.cycles_this_step = 0

        # Directe method-referenties schelen miljoenen attribute-lookups per seconde
        self._tia_step = tia.step
        self._riot_step = riot.step
        self._cart_read = cartridge.read
        self._cart_write = cartridge.write
        self._tia_read = tia.read
        self._tia_write = tia.write
        self._riot_read = riot.read
        self._riot_write = riot.write

        # In Bus.__init__:
        self._fast_rom = None
        self._fast_banks = None
        self._fast_mask = 0x0FFF
        self._superchip_ram = None

        rom = getattr(cartridge, "rom", None)
        banks = getattr(cartridge, "banks", None)

        if isinstance(rom, (bytes, bytearray, memoryview)):
            self._fast_rom = rom
            self._fast_mask = getattr(cartridge, "mask", len(rom) - 1)
        elif isinstance(banks, (list, tuple)) and len(banks) > 0:
            self._fast_banks = banks
            self._fast_mask = 0x0FFF
            sc_ram = getattr(cartridge, "ram", None)
            if isinstance(sc_ram, bytearray) and len(sc_ram) == 128:
                self._superchip_ram = sc_ram

    def _get_fast_rom_base(self) -> int:
        attr = self._bank_attr
        if attr is None:
            return 0
        bank = getattr(self.cartridge, attr, 0)
        return (bank * 4096) & self._fast_rom_mask

    def tick(self, cycles: int = 1) -> None:
        self.cycles_this_step += cycles
        self._tia_step(cycles * 3)
        self._riot_step(cycles)

    def read(self, address: int) -> int:
        self.cycles_this_step += 1
        self._tia_step(3)
        self._riot_step(1)

        addr = address & 0x1FFF
        if addr & 0x1000:
            if self._superchip_ram is not None and 0x1080 <= addr <= 0x10FF:
                return self._superchip_ram[addr & 0x007F]

            if 0x1FF6 <= addr <= 0x1FF9:
                return self._cart_read(addr)

            fast_rom = self._fast_rom
            if fast_rom is not None:
                return fast_rom[addr & self._fast_mask]

            fast_banks = self._fast_banks
            if fast_banks is not None:
                return fast_banks[self.cartridge.current_bank][addr & 0x0FFF]

            return self._cart_read(addr)

        if addr & 0x0080:
            if addr & 0x0200:
                return self._riot_read(addr)
            return self.ram[addr & 0x7F]
        return self._tia_read(addr)

    def write(self, address: int, value: int) -> None:
        self.cycles_this_step += 1
        self._tia_step(3)
        self._riot_step(1)

        addr = address & 0x1FFF
        v = value & 0xFF if value else 0
        if addr & 0x1000:
            self._cart_write(addr, v)
        elif (addr & 0x0080) == 0:
            self._tia_write(addr & 0x3F, v)
        elif (addr & 0x0200) == 0:
            self.ram[addr & 0x7F] = v
        else:
            self._riot_write(addr, v)

    def step_frame(self, cpu) -> None:
        tia = self.tia
        tia_step = self._tia_step
        riot_step = self._riot_step
        cpu_step = cpu.step

        tia.frame_ready = False
        while not tia.frame_ready:
            if tia.wsync:
                remaining = 228 - tia.clock
                tia_step(remaining)
                riot_step(remaining // 3 if remaining >= 3 else 1)
            else:
                cpu_step()