class Bus:
    def __init__(self, tia, riot, cartridge):
        self.tia = tia
        self.riot = riot
        self.cartridge = cartridge
        self.ram = bytearray(128)
        self.cycles_this_step = 0

    def tick(self, cycles: int = 1) -> None:
        for _ in range(cycles):
            self.cycles_this_step += 1
            self.tia.step(3)
            self.riot.step(1)

    def read(self, address: int) -> int:
        self.tick(1)
        addr = address & 0x1FFF
        if addr & 0x1000:
            return self.cartridge.read(addr) & 0xFF
        if (addr & 0x0080) != 0 and (addr & 0x0200) == 0:
            return self.ram[addr & 0x7F]
        if (addr & 0x0080) != 0 and (addr & 0x0200) != 0:
            return self.riot.read(addr) & 0xFF
        return self.tia.read(addr) & 0xFF

    def write(self, address: int, value: int) -> None:
        self.tick(1)
        addr = address & 0x1FFF
        v = (value or 0) & 0xFF
        if addr & 0x1000:
            self.cartridge.write(addr, v)
        elif (addr & 0x0080) == 0:
            self.tia.write(addr & 0x3F, v)
        elif (addr & 0x0200) == 0:
            self.ram[addr & 0x7F] = v
        else:
            self.riot.write(addr, v)    
    
    def step_frame(self, cpu) -> None:
        self.tia.frame_ready = False
        while not self.tia.frame_ready:
            if self.tia.wsync:
                remaining = 228 - self.tia.clock
                self.tia.step(remaining)
                self.riot.step(max(1, remaining // 3))
            else:
                cpu.step()