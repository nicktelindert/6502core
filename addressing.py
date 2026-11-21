class AddressingMixin:
    def mode_ACC(self):
        self.bus.tick(1)
        return None

    def mode_IMP(self):
        self.bus.tick(1)
        return None

    def mode_IMM(self):
        addr = self.PC
        self.PC = (self.PC + 1) & 0xFFFF
        return addr

    def mode_ZP(self):
        return self.fetch_byte()

    def mode_ZPX(self):
        zp = self.fetch_byte()
        self.bus.tick(1)
        return (zp + self.X) & 0xFF

    def mode_ZPY(self):
        zp = self.fetch_byte()
        self.bus.tick(1)
        return (zp + self.Y) & 0xFF

    def mode_ABS(self):
        low = self.fetch_byte()
        high = self.fetch_byte()
        return (high << 8) | low

    def mode_ABX(self):
        low = self.fetch_byte()
        high = self.fetch_byte()
        base_addr = (high << 8) | low
        addr = (base_addr + self.X) & 0xFFFF
        if (base_addr & 0xFF00) != (addr & 0xFF00):
            self._page_crossed = True
        return addr

    def mode_ABY(self):
        low = self.fetch_byte()
        high = self.fetch_byte()
        base_addr = (high << 8) | low
        addr = (base_addr + self.Y) & 0xFFFF
        if (base_addr & 0xFF00) != (addr & 0xFF00):
            self._page_crossed = True
        return addr

    def mode_REL(self):
        offset = self.fetch_byte()
        if offset & 0x80:
            offset -= 0x100
        return (self.PC + offset) & 0xFFFF

    def mode_INX(self):
        zp = (self.fetch_byte() + self.X) & 0xFF
        self.bus.tick(1)
        low = self.read(zp)
        high = self.read((zp + 1) & 0xFF)
        return (high << 8) | low

    def mode_INY(self):
        zp_addr = self.fetch_byte()
        low = self.read(zp_addr)
        high = self.read((zp_addr + 1) & 0xFF)
        base_addr = (high << 8) | low
        addr = (base_addr + self.Y) & 0xFFFF
        if (base_addr & 0xFF00) != (addr & 0xFF00):
            self._page_crossed = True
        return addr

    def mode_IND(self):
        low_ptr = self.read(self.PC)
        high_ptr = self.read((self.PC + 1) & 0xFFFF)
        self.PC = (self.PC + 2) & 0xFFFF
        ptr = (high_ptr << 8) | low_ptr

        low = self.read(ptr)
        high_addr = (ptr & 0xFF00) | ((ptr + 1) & 0x00FF)
        high = self.read(high_addr)
        return (high << 8) | low