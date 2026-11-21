OPCODE_SPECS = {
    # --- ADC ---
    0x69: ("ADC", "mode_IMM", 2, False), 0x65: ("ADC", "mode_ZP",  3, False),
    0x75: ("ADC", "mode_ZPX", 4, False), 0x6D: ("ADC", "mode_ABS", 4, False),
    0x7D: ("ADC", "mode_ABX", 4, True),  0x79: ("ADC", "mode_ABY", 4, True),
    0x61: ("ADC", "mode_INX", 6, False), 0x71: ("ADC", "mode_INY", 5, True),

    # --- AND ---
    0x29: ("AND", "mode_IMM", 2, False), 0x25: ("AND", "mode_ZP",  3, False),
    0x35: ("AND", "mode_ZPX", 4, False), 0x2D: ("AND", "mode_ABS", 4, False),
    0x3D: ("AND", "mode_ABX", 4, True),  0x39: ("AND", "mode_ABY", 4, True),
    0x21: ("AND", "mode_INX", 6, False), 0x31: ("AND", "mode_INY", 5, True),

    # --- ASL ---
    0x0A: ("ASL", "mode_ACC", 2, False), 0x06: ("ASL", "mode_ZP",  5, False),
    0x16: ("ASL", "mode_ZPX", 6, False), 0x0E: ("ASL", "mode_ABS", 6, False),
    0x1E: ("ASL", "mode_ABX_W", 7, False),

    # --- BIT ---
    0x24: ("BIT", "mode_ZP",  3, False), 0x2C: ("BIT", "mode_ABS", 4, False),

    # --- Branches ---
    0x10: ("BPL", "mode_REL", 2, False), 0x30: ("BMI", "mode_REL", 2, False),
    0x50: ("BVC", "mode_REL", 2, False), 0x70: ("BVS", "mode_REL", 2, False),
    0x90: ("BCC", "mode_REL", 2, False), 0xB0: ("BCS", "mode_REL", 2, False),
    0xD0: ("BNE", "mode_REL", 2, False), 0xF0: ("BEQ", "mode_REL", 2, False),

    # --- BRK / RTI / RTS / JSR / JMP ---
    0x00: ("BRK", "mode_IMP", 7, False), 0x40: ("RTI", "mode_IMP", 6, False),
    0x60: ("RTS", "mode_IMP", 6, False), 0x20: ("JSR", "mode_ABS", 6, False),
    0x4C: ("JMP", "mode_ABS", 3, False), 0x6C: ("JMP", "mode_IND", 5, False),

    # --- CMP ---
    0xC9: ("CMP", "mode_IMM", 2, False), 0xC5: ("CMP", "mode_ZP",  3, False),
    0xD5: ("CMP", "mode_ZPX", 4, False), 0xCD: ("CMP", "mode_ABS", 4, False),
    0xDD: ("CMP", "mode_ABX", 4, True),  0xD9: ("CMP", "mode_ABY", 4, True),
    0xC1: ("CMP", "mode_INX", 6, False), 0xD1: ("CMP", "mode_INY", 5, True),

    # --- CPX / CPY ---
    0xE0: ("CPX", "mode_IMM", 2, False), 0xE4: ("CPX", "mode_ZP",  3, False),
    0xEC: ("CPX", "mode_ABS", 4, False),
    0xC0: ("CPY", "mode_IMM", 2, False), 0xC4: ("CPY", "mode_ZP",  3, False),
    0xCC: ("CPY", "mode_ABS", 4, False),

    # --- DEC / DEX / DEY ---
    0xC6: ("DEC", "mode_ZP",  5, False), 0xD6: ("DEC", "mode_ZPX", 6, False),
    0xCE: ("DEC", "mode_ABS", 6, False), 0xDE: ("DEC", "mode_ABX_W", 7, False),
    0xCA: ("DEX", "mode_IMP", 2, False), 0x88: ("DEY", "mode_IMP", 2, False),

    # --- EOR ---
    0x49: ("EOR", "mode_IMM", 2, False), 0x45: ("EOR", "mode_ZP",  3, False),
    0x55: ("EOR", "mode_ZPX", 4, False), 0x4D: ("EOR", "mode_ABS", 4, False),
    0x5D: ("EOR", "mode_ABX", 4, True),  0x59: ("EOR", "mode_ABY", 4, True),
    0x41: ("EOR", "mode_INX", 6, False), 0x51: ("EOR", "mode_INY", 5, True),

    # --- Flags ---
    0x18: ("CLC", "mode_IMP", 2, False), 0x38: ("SEC", "mode_IMP", 2, False),
    0x58: ("CLI", "mode_IMP", 2, False), 0x78: ("SEI", "mode_IMP", 2, False),
    0xB8: ("CLV", "mode_IMP", 2, False), 0xD8: ("CLD", "mode_IMP", 2, False),
    0xF8: ("SED", "mode_IMP", 2, False),

    # --- INC / INX / INY ---
    0xE6: ("INC", "mode_ZP",  5, False), 0xF6: ("INC", "mode_ZPX", 6, False),
    0xEE: ("INC", "mode_ABS", 6, False), 0xFE: ("INC", "mode_ABX_W", 7, False),
    0xE8: ("INX", "mode_IMP", 2, False), 0xC8: ("INY", "mode_IMP", 2, False),

    # --- LDA ---
    0xA9: ("LDA", "mode_IMM", 2, False), 0xA5: ("LDA", "mode_ZP",  3, False),
    0xB5: ("LDA", "mode_ZPX", 4, False), 0xAD: ("LDA", "mode_ABS", 4, False),
    0xBD: ("LDA", "mode_ABX", 4, True),  0xB9: ("LDA", "mode_ABY", 4, True),
    0xA1: ("LDA", "mode_INX", 6, False), 0xB1: ("LDA", "mode_INY", 5, True),

    # --- LDX ---
    0xA2: ("LDX", "mode_IMM", 2, False), 0xA6: ("LDX", "mode_ZP",  3, False),
    0xB6: ("LDX", "mode_ZPY", 4, False), 0xAE: ("LDX", "mode_ABS", 4, False),
    0xBE: ("LDX", "mode_ABY", 4, True),

    # --- LDY ---
    0xA0: ("LDY", "mode_IMM", 2, False), 0xA4: ("LDY", "mode_ZP",  3, False),
    0xB4: ("LDY", "mode_ZPX", 4, False), 0xAC: ("LDY", "mode_ABS", 4, False),
    0xBC: ("LDY", "mode_ABX", 4, True),

    # --- LSR ---
    0x4A: ("LSR", "mode_ACC", 2, False), 0x46: ("LSR", "mode_ZP",  5, False),
    0x56: ("LSR", "mode_ZPX", 6, False), 0x4E: ("LSR", "mode_ABS", 6, False),
    0x5E: ("LSR", "mode_ABX_W", 7, False),

    # --- NOP ---
    0xEA: ("NOP", "mode_IMP", 2, False),

    # --- ORA ---
    0x09: ("ORA", "mode_IMM", 2, False), 0x05: ("ORA", "mode_ZP",  3, False),
    0x15: ("ORA", "mode_ZPX", 4, False), 0x0D: ("ORA", "mode_ABS", 4, False),
    0x1D: ("ORA", "mode_ABX", 4, True),  0x19: ("ORA", "mode_ABY", 4, True),
    0x01: ("ORA", "mode_INX", 6, False), 0x11: ("ORA", "mode_INY", 5, True),

    # --- Stack ---
    0x48: ("PHA", "mode_IMP", 3, False), 0x08: ("PHP", "mode_IMP", 3, False),
    0x68: ("PLA", "mode_IMP", 4, False), 0x28: ("PLP", "mode_IMP", 4, False),

    # --- ROL ---
    0x2A: ("ROL", "mode_ACC", 2, False), 0x26: ("ROL", "mode_ZP",  5, False),
    0x36: ("ROL", "mode_ZPX", 6, False), 0x2E: ("ROL", "mode_ABS", 6, False),
    0x3E: ("ROL", "mode_ABX_W", 7, False),

    # --- ROR ---
    0x6A: ("ROR", "mode_ACC", 2, False), 0x66: ("ROR", "mode_ZP",  5, False),
    0x76: ("ROR", "mode_ZPX", 6, False), 0x6E: ("ROR", "mode_ABS", 6, False),
    0x7E: ("ROR", "mode_ABX_W", 7, False),

    # --- SBC ---
    0xE9: ("SBC", "mode_IMM", 2, False), 0xE5: ("SBC", "mode_ZP",  3, False),
    0xF5: ("SBC", "mode_ZPX", 4, False), 0xED: ("SBC", "mode_ABS", 4, False),
    0xFD: ("SBC", "mode_ABX", 4, True),  0xF9: ("SBC", "mode_ABY", 4, True),
    0xE1: ("SBC", "mode_INX", 6, False), 0xF1: ("SBC", "mode_INY", 5, True),

    # --- STA ---
    0x85: ("STA", "mode_ZP",  3, False), 0x95: ("STA", "mode_ZPX", 4, False),
    0x8D: ("STA", "mode_ABS", 4, False), 0x9D: ("STA", "mode_ABX_W", 5, False),
    0x99: ("STA", "mode_ABY_W", 5, False), 0x81: ("STA", "mode_INX", 6, False),
    0x91: ("STA", "mode_INY_W", 6, False),

    # --- STX / STY ---
    0x86: ("STX", "mode_ZP",  3, False), 0x96: ("STX", "mode_ZPY", 4, False),
    0x8E: ("STX", "mode_ABS", 4, False),
    0x84: ("STY", "mode_ZP",  3, False), 0x94: ("STY", "mode_ZPX", 4, False),
    0x8C: ("STY", "mode_ABS", 4, False),

    # --- Transfers ---
    0xAA: ("TAX", "mode_IMP", 2, False), 0xA8: ("TAY", "mode_IMP", 2, False),
    0xBA: ("TSX", "mode_IMP", 2, False), 0x8A: ("TXA", "mode_IMP", 2, False),
    0x9A: ("TXS", "mode_IMP", 2, False), 0x98: ("TYA", "mode_IMP", 2, False),

    # --- Undocumented ---
    0x9E: ("SHX", "mode_ABY_W", 5, False),
    0x1B: ("SLO", "mode_ABY_W", 7, False),
    0x3B: ("RLA", "mode_ABY_W", 7, False),
}


class InstructionsMixin:
    def _build_opcode_table(self):
        return {
            op: (getattr(self, instr), getattr(self, mode), cycles, page_penalty)
            for op, (instr, mode, cycles, page_penalty) in OPCODE_SPECS.items()
        }

    # --- Control Flow & Flags ---
    def JMP(self, addr): self.PC = addr
    def SEI(self, addr): self.flags['I'] = 1
    def CLI(self, addr): self.flags['I'] = 0
    def CLD(self, addr): self.flags['D'] = 0
    def SED(self, addr=None): self.flags['D'] = 1
    def CLV(self, addr): self.flags['V'] = 0
    def CLC(self, addr): self.flags['C'] = 0
    def SEC(self, addr): self.flags['C'] = 1
    def NOP(self, addr): pass

    # --- Loads & Stores ---
    def LDA(self, addr):
        self.A = self.read(addr)
        self.update_zn(self.A)

    def LDX(self, addr):
        self.X = self.read(addr)
        self.update_zn(self.X)

    def LDY(self, addr):
        self.Y = self.read(addr)
        self.update_zn(self.Y)

    def STA(self, addr): self.write(addr, self.A)
    def STX(self, addr): self.write(addr, self.X)
    def STY(self, addr): self.write(addr, self.Y)

    # --- Transfers & Increments ---
    def TXS(self, addr): self.SP = self.X

    def TSX(self, addr):
        self.X = self.SP
        self.update_zn(self.X)

    def TAY(self, addr):
        self.Y = self.A
        self.update_zn(self.Y)

    def TAX(self, addr):
        self.X = self.A
        self.update_zn(self.X)

    def TYA(self, addr):
        self.A = self.Y
        self.update_zn(self.A)

    def TXA(self, addr):
        self.A = self.X
        self.update_zn(self.A)

    def DEX(self, addr):
        self.X = (self.X - 1) & 0xFF
        self.update_zn(self.X)

    def DEY(self, addr):
        self.Y = (self.Y - 1) & 0xFF
        self.update_zn(self.Y)

    def INX(self, addr):
        self.X = (self.X + 1) & 0xFF
        self.update_zn(self.X)

    def INY(self, addr):
        self.Y = (self.Y + 1) & 0xFF
        self.update_zn(self.Y)

    # --- Branches ---
    def _branch(self, condition, addr):
        if condition:
            old_pc = self.PC
            self.PC = addr
            self.bus.tick(1)
            if (old_pc & 0xFF00) != (self.PC & 0xFF00):
                self.bus.tick(1)

    def BNE(self, addr): self._branch(self.flags['Z'] == 0, addr)
    def BEQ(self, addr): self._branch(self.flags['Z'] == 1, addr)
    def BPL(self, addr): self._branch(self.flags['N'] == 0, addr)
    def BMI(self, addr): self._branch(self.flags['N'] == 1, addr)
    def BCC(self, addr): self._branch(self.flags['C'] == 0, addr)
    def BCS(self, addr): self._branch(self.flags['C'] == 1, addr)
    def BVC(self, addr): self._branch(self.flags['V'] == 0, addr)
    def BVS(self, addr): self._branch(self.flags['V'] == 1, addr)

    # --- Memory Inc/Dec ---
    def DEC(self, addr):
        val = (self.read(addr) - 1) & 0xFF
        self.bus.tick(1)
        self.write(addr, val)
        self.update_zn(val)

    def INC(self, addr):
        val = (self.read(addr) + 1) & 0xFF
        self.bus.tick(1)
        self.write(addr, val)
        self.update_zn(val)

    # --- Stack & Subroutines ---
    def PHP(self, addr):
        status = (
            (self.flags['N'] << 7) |
            (self.flags['V'] << 6) |
            (1 << 5) |
            (1 << 4) |
            (self.flags['D'] << 3) |
            (self.flags['I'] << 2) |
            (self.flags['Z'] << 1) |
            self.flags['C']
        )
        self.push_byte(status)

    def PLP(self, addr):
        self.bus.tick(1)
        status = self.pull_byte()
        self.flags['N'] = (status >> 7) & 1
        self.flags['V'] = (status >> 6) & 1
        self.flags['D'] = (status >> 3) & 1
        self.flags['I'] = (status >> 2) & 1
        self.flags['Z'] = (status >> 1) & 1
        self.flags['C'] = status & 1

    def PHA(self, addr): self.push_byte(self.A)

    def PLA(self, addr):
        self.bus.tick(1)
        self.A = self.pull_byte()
        self.update_zn(self.A)

    def BRK(self, addr):
        self.PC = (self.PC + 1) & 0xFFFF
        self.push_byte((self.PC >> 8) & 0xFF)
        self.push_byte(self.PC & 0xFF)
        self.PHP(None)
        self.flags['I'] = 1
        low = self.read(0xFFFE)
        high = self.read(0xFFFF)
        self.PC = (high << 8) | low

    def RTI(self, addr):
        self.PLP(None)
        low = self.pull_byte()
        high = self.pull_byte()
        self.PC = (high << 8) | low

    def JSR(self, addr):
        return_addr = (self.PC - 1) & 0xFFFF
        self.bus.tick(1)
        self.push_byte((return_addr >> 8) & 0xFF)
        self.push_byte(return_addr & 0xFF)
        self.PC = addr

    def RTS(self, addr):
        low = self.pull_byte()
        high = self.pull_byte()
        self.bus.tick(2)
        self.PC = (((high << 8) | low) + 1) & 0xFFFF

    # --- Shifts & Rotates ---
    def ASL(self, addr):
        val = self.A if addr is None else self.read(addr)
        self.flags['C'] = 1 if (val & 0x80) else 0
        result = (val << 1) & 0xFF
        self.update_zn(result)
        if addr is None:
            self.A = result
        else:
            self.bus.tick(1)
            self.write(addr, result)

    def LSR(self, addr):
        val = self.A if addr is None else self.read(addr)
        self.flags['C'] = val & 0x01
        result = val >> 1
        self.update_zn(result)
        if addr is None:
            self.A = result
        else:
            self.bus.tick(1)
            self.write(addr, result)

    def ROL(self, addr):
        val = self.A if addr is None else self.read(addr)
        old_carry = self.flags['C']
        self.flags['C'] = (val >> 7) & 0x01
        result = ((val << 1) | old_carry) & 0xFF
        if addr is None:
            self.A = result
        else:
            self.bus.tick(1)
            self.write(addr, result)
        self.update_zn(result)

    def ROR(self, addr):
        val = self.A if addr is None else self.read(addr)
        old_carry = self.flags['C']
        self.flags['C'] = val & 0x01
        result = ((val >> 1) | (old_carry << 7)) & 0xFF
        if addr is None:
            self.A = result
        else:
            self.bus.tick(1)
            self.write(addr, result)
        self.update_zn(result)

    # --- Logic & Compares ---
    def AND(self, addr):
        self.A &= self.read(addr)
        self.update_zn(self.A)

    def EOR(self, addr):
        self.A ^= self.read(addr)
        self.update_zn(self.A)

    def ORA(self, addr):
        self.A |= self.read(addr)
        self.update_zn(self.A)

    def BIT(self, addr):
        val = self.read(addr)
        self.flags['Z'] = 1 if (self.A & val) == 0 else 0
        self.flags['N'] = (val >> 7) & 0x01
        self.flags['V'] = (val >> 6) & 0x01

    def CMP(self, addr):
        val = self.read(addr)
        result = (self.A - val) & 0xFF
        self.flags['C'] = 1 if self.A >= val else 0
        self.update_zn(result)

    def CPX(self, addr):
        val = self.read(addr)
        result = (self.X - val) & 0xFF
        self.flags['C'] = 1 if self.X >= val else 0
        self.update_zn(result)

    def CPY(self, addr):
        val = self.read(addr)
        result = (self.Y - val) & 0xFF
        self.flags['C'] = 1 if self.Y >= val else 0
        self.update_zn(result)

    # --- Arithmetic ---
    def ADC(self, addr):
        val = self.read(addr)
        carry_in = self.flags['C']
        if self.flags['D']:
            low = (self.A & 0x0F) + (val & 0x0F) + carry_in
            if low > 9:
                low += 6
            high = (self.A >> 4) + (val >> 4) + (1 if low > 0x0F else 0)
            self.flags['V'] = 1 if (~(self.A ^ val) & (self.A ^ (high << 4)) & 0x80) else 0
            if high > 9:
                high += 6
            self.flags['C'] = 1 if high > 0x0F else 0
            self.A = ((high & 0x0F) << 4) | (low & 0x0F)
            self.update_zn(self.A)
            return

        total = self.A + val + carry_in
        result = total & 0xFF
        self.flags['C'] = 1 if total > 0xFF else 0
        self.flags['V'] = 1 if (~(self.A ^ val) & (self.A ^ result) & 0x80) else 0
        self.A = result
        self.update_zn(self.A)

    def SBC(self, addr):
        val = self.read(addr)
        raw_val = val ^ 0xFF
        carry_in = self.flags['C']
        if self.flags['D']:
            low = (self.A & 0x0F) - (val & 0x0F) - (1 - carry_in)
            borrow_low = 0
            if low < 0:
                low -= 6
                borrow_low = 1
            high = (self.A >> 4) - (val >> 4) - borrow_low
            if high < 0:
                high -= 6
            bin_total = self.A + raw_val + carry_in
            self.flags['C'] = 1 if bin_total > 0xFF else 0
            self.flags['V'] = 1 if (~(self.A ^ raw_val) & (self.A ^ (bin_total & 0xFF)) & 0x80) else 0
            self.A = ((high & 0x0F) << 4) | (low & 0x0F)
            self.update_zn(self.A)
            return

        total = self.A + raw_val + carry_in
        result = total & 0xFF
        self.flags['C'] = 1 if total > 0xFF else 0
        self.flags['V'] = 1 if (~(self.A ^ raw_val) & (self.A ^ result) & 0x80) else 0
        self.A = result
        self.update_zn(self.A)

    # --- Undocumented ---
    def SHX(self, addr):
        high = ((addr - self.Y) >> 8) & 0xFF
        self.write(addr, self.X & (high + 1))

    def SLO(self, addr):
        val = self.read(addr)
        self.flags['C'] = 1 if (val & 0x80) else 0
        val = (val << 1) & 0xFF
        self.write(addr, val)
        self.A |= val
        self.update_zn(self.A)

    def RLA(self, addr):
        val = self.read(addr)
        old_c = self.flags['C']
        self.flags['C'] = 1 if (val & 0x80) else 0
        val = ((val << 1) | old_c) & 0xFF
        self.write(addr, val)
        self.A &= val
        self.update_zn(self.A)

    def mode_ABX_W(self):
        low = self.fetch_byte()
        high = self.fetch_byte()
        base = (high << 8) | low
        self.bus.tick(1)
        return (base + self.X) & 0xFFFF

    def mode_ABY_W(self):
        low = self.fetch_byte()
        high = self.fetch_byte()
        base = (high << 8) | low
        self.bus.tick(1)
        return (base + self.Y) & 0xFFFF

    def mode_INY_W(self):
        zp = self.fetch_byte()
        low = self.read(zp)
        high = self.read((zp + 1) & 0xFF)
        base = (high << 8) | low
        self.bus.tick(1)
        return (base + self.Y) & 0xFFFF
