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