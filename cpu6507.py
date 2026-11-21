from tia import TIA
from riot import RIOT


class CPU6507:
    def __init__(self, rom_data):
        self.tia = TIA()
        self.riot = RIOT()

        self.memory = bytearray(0x2000)
        self.rom_banks = [rom_data[i:i + 0x1000] for i in range(0, len(rom_data), 0x1000)]
        self.current_bank = 0
        if len(self.rom_banks) == 2:
            vec0 = self.rom_banks[0][0xFFC] | (self.rom_banks[0][0xFFD] << 8)
            self.current_bank = 0 if (vec0 & 0x1000) else 1

        print("ROM grootte:", len(rom_data), "bytes")
        print("Reset vector:", hex(self.read(0xFFFC) | (self.read(0xFFFD) << 8)))

        self.extra_cycles = 0
        self.pending_tia_clocks = 0
        self.A = 0x00
        self.X = 0x00
        self.Y = 0x00
        self.SP = 0xFD

        self.flags = {'N': 0, 'V': 0, 'B': 0, 'D': 0, 'I': 1, 'Z': 0, 'C': 0}

        low = self.read(0xFFFC)
        high = self.read(0xFFFD)
        self.PC = (high << 8) | low

        self._hist = []

        # Alle 151 officiële 6502/6507 opcodes + gebruikte undocumented opcodes
        self.opcodes = {
            # --- ADC ---
            0x69: (self.ADC, self.mode_IMM, 2),
            0x65: (self.ADC, self.mode_ZP,  3),
            0x75: (self.ADC, self.mode_ZPX, 4),
            0x6D: (self.ADC, self.mode_ABS, 4),
            0x7D: (self.ADC, self.mode_ABX, 4),
            0x79: (self.ADC, self.mode_ABY, 4),
            0x61: (self.ADC, self.mode_INX, 6),
            0x71: (self.ADC, self.mode_INY, 5),

            # --- AND ---
            0x29: (self.AND, self.mode_IMM, 2),
            0x25: (self.AND, self.mode_ZP,  3),
            0x35: (self.AND, self.mode_ZPX, 4),
            0x2D: (self.AND, self.mode_ABS, 4),
            0x3D: (self.AND, self.mode_ABX, 4),
            0x39: (self.AND, self.mode_ABY, 4),
            0x21: (self.AND, self.mode_INX, 6),
            0x31: (self.AND, self.mode_INY, 5),

            # --- ASL ---
            0x0A: (self.ASL, self.mode_ACC, 2),
            0x06: (self.ASL, self.mode_ZP,  5),
            0x16: (self.ASL, self.mode_ZPX, 6),
            0x0E: (self.ASL, self.mode_ABS, 6),
            0x1E: (self.ASL, self.mode_ABX, 7),

            # --- BIT ---
            0x24: (self.BIT, self.mode_ZP,  3),
            0x2C: (self.BIT, self.mode_ABS, 4),

            # --- Branches ---
            0x10: (self.BPL, self.mode_REL, 2),
            0x30: (self.BMI, self.mode_REL, 2),
            0x50: (self.BVC, self.mode_REL, 2),
            0x70: (self.BVS, self.mode_REL, 2),
            0x90: (self.BCC, self.mode_REL, 2),
            0xB0: (self.BCS, self.mode_REL, 2),
            0xD0: (self.BNE, self.mode_REL, 2),
            0xF0: (self.BEQ, self.mode_REL, 2),

            # --- BRK / RTI / RTS / JSR / JMP ---
            0x00: (self.BRK, self.mode_IMP, 7),
            0x40: (self.RTI, self.mode_IMP, 6),
            0x60: (self.RTS, self.mode_IMP, 6),
            0x20: (self.JSR, self.mode_ABS, 6),
            0x4C: (self.JMP, self.mode_ABS, 3),
            0x6C: (self.JMP, self.mode_IND, 5),

            # --- CMP ---
            0xC9: (self.CMP, self.mode_IMM, 2),
            0xC5: (self.CMP, self.mode_ZP,  3),
            0xD5: (self.CMP, self.mode_ZPX, 4),
            0xCD: (self.CMP, self.mode_ABS, 4),
            0xDD: (self.CMP, self.mode_ABX, 4),
            0xD9: (self.CMP, self.mode_ABY, 4),
            0xC1: (self.CMP, self.mode_INX, 6),
            0xD1: (self.CMP, self.mode_INY, 5),

            # --- CPX ---
            0xE0: (self.CPX, self.mode_IMM, 2),
            0xE4: (self.CPX, self.mode_ZP,  3),
            0xEC: (self.CPX, self.mode_ABS, 4),

            # --- CPY ---
            0xC0: (self.CPY, self.mode_IMM, 2),
            0xC4: (self.CPY, self.mode_ZP,  3),
            0xCC: (self.CPY, self.mode_ABS, 4),

            # --- DEC / DEX / DEY ---
            0xC6: (self.DEC, self.mode_ZP,  5),
            0xD6: (self.DEC, self.mode_ZPX, 6),
            0xCE: (self.DEC, self.mode_ABS, 6),
            0xDE: (self.DEC, self.mode_ABX, 7),
            0xCA: (self.DEX, self.mode_IMP, 2),
            0x88: (self.DEY, self.mode_IMP, 2),

            # --- EOR ---
            0x49: (self.EOR, self.mode_IMM, 2),
            0x45: (self.EOR, self.mode_ZP,  3),
            0x55: (self.EOR, self.mode_ZPX, 4),
            0x4D: (self.EOR, self.mode_ABS, 4),
            0x5D: (self.EOR, self.mode_ABX, 4),
            0x59: (self.EOR, self.mode_ABY, 4),
            0x41: (self.EOR, self.mode_INX, 6),
            0x51: (self.EOR, self.mode_INY, 5),

            # --- Flags ---
            0x18: (self.CLC, self.mode_IMP, 2),
            0x38: (self.SEC, self.mode_IMP, 2),
            0x58: (self.CLI, self.mode_IMP, 2),
            0x78: (self.SEI, self.mode_IMP, 2),
            0xB8: (self.CLV, self.mode_IMP, 2),
            0xD8: (self.CLD, self.mode_IMP, 2),
            0xF8: (self.SED, self.mode_IMP, 2),

            # --- INC / INX / INY ---
            0xE6: (self.INC, self.mode_ZP,  5),
            0xF6: (self.INC, self.mode_ZPX, 6),
            0xEE: (self.INC, self.mode_ABS, 6),
            0xFE: (self.INC, self.mode_ABX, 7),
            0xE8: (self.INX, self.mode_IMP, 2),
            0xC8: (self.INY, self.mode_IMP, 2),

            # --- LDA ---
            0xA9: (self.LDA, self.mode_IMM, 2),
            0xA5: (self.LDA, self.mode_ZP,  3),
            0xB5: (self.LDA, self.mode_ZPX, 4),
            0xAD: (self.LDA, self.mode_ABS, 4),
            0xBD: (self.LDA, self.mode_ABX, 4),
            0xB9: (self.LDA, self.mode_ABY, 4),
            0xA1: (self.LDA, self.mode_INX, 6),
            0xB1: (self.LDA, self.mode_INY, 5),

            # --- LDX ---
            0xA2: (self.LDX, self.mode_IMM, 2),
            0xA6: (self.LDX, self.mode_ZP,  3),
            0xB6: (self.LDX, self.mode_ZPY, 4),
            0xAE: (self.LDX, self.mode_ABS, 4),
            0xBE: (self.LDX, self.mode_ABY, 4),

            # --- LDY ---
            0xA0: (self.LDY, self.mode_IMM, 2),
            0xA4: (self.LDY, self.mode_ZP,  3),
            0xB4: (self.LDY, self.mode_ZPX, 4),
            0xAC: (self.LDY, self.mode_ABS, 4),
            0xBC: (self.LDY, self.mode_ABX, 4),

            # --- LSR ---
            0x4A: (self.LSR, self.mode_ACC, 2),
            0x46: (self.LSR, self.mode_ZP,  5),
            0x56: (self.LSR, self.mode_ZPX, 6),
            0x4E: (self.LSR, self.mode_ABS, 6),
            0x5E: (self.LSR, self.mode_ABX, 7),

            # --- NOP ---
            0xEA: (self.NOP, self.mode_IMP, 2),

            # --- ORA ---
            0x09: (self.ORA, self.mode_IMM, 2),
            0x05: (self.ORA, self.mode_ZP,  3),
            0x15: (self.ORA, self.mode_ZPX, 4),
            0x0D: (self.ORA, self.mode_ABS, 4),
            0x1D: (self.ORA, self.mode_ABX, 4),
            0x19: (self.ORA, self.mode_ABY, 4),
            0x01: (self.ORA, self.mode_INX, 6),
            0x11: (self.ORA, self.mode_INY, 5),

            # --- Stack ---
            0x48: (self.PHA, self.mode_IMP, 3),
            0x08: (self.PHP, self.mode_IMP, 3),
            0x68: (self.PLA, self.mode_IMP, 4),
            0x28: (self.PLP, self.mode_IMP, 4),

            # --- ROL ---
            0x2A: (self.ROL, self.mode_ACC, 2),
            0x26: (self.ROL, self.mode_ZP,  5),
            0x36: (self.ROL, self.mode_ZPX, 6),
            0x2E: (self.ROL, self.mode_ABS, 6),
            0x3E: (self.ROL, self.mode_ABX, 7),

            # --- ROR ---
            0x6A: (self.ROR, self.mode_ACC, 2),
            0x66: (self.ROR, self.mode_ZP,  5),
            0x76: (self.ROR, self.mode_ZPX, 6),
            0x6E: (self.ROR, self.mode_ABS, 6),
            0x7E: (self.ROR, self.mode_ABX, 7),

            # --- SBC ---
            0xE9: (self.SBC, self.mode_IMM, 2),
            0xE5: (self.SBC, self.mode_ZP,  3),
            0xF5: (self.SBC, self.mode_ZPX, 4),
            0xED: (self.SBC, self.mode_ABS, 4),
            0xFD: (self.SBC, self.mode_ABX, 4),
            0xF9: (self.SBC, self.mode_ABY, 4),
            0xE1: (self.SBC, self.mode_INX, 6),
            0xF1: (self.SBC, self.mode_INY, 5),

            # --- STA ---
            0x85: (self.STA, self.mode_ZP,  3),
            0x95: (self.STA, self.mode_ZPX, 4),
            0x8D: (self.STA, self.mode_ABS, 4),
            0x9D: (self.STA, self.mode_ABX, 5),
            0x99: (self.STA, self.mode_ABY, 5),
            0x81: (self.STA, self.mode_INX, 6),
            0x91: (self.STA, self.mode_INY, 6),

            # --- STX ---
            0x86: (self.STX, self.mode_ZP,  3),
            0x96: (self.STX, self.mode_ZPY, 4),
            0x8E: (self.STX, self.mode_ABS, 4),

            # --- STY ---
            0x84: (self.STY, self.mode_ZP,  3),
            0x94: (self.STY, self.mode_ZPX, 4),
            0x8C: (self.STY, self.mode_ABS, 4),

            # --- Transfers ---
            0xAA: (self.TAX, self.mode_IMP, 2),
            0xA8: (self.TAY, self.mode_IMP, 2),
            0xBA: (self.TSX, self.mode_IMP, 2),
            0x8A: (self.TXA, self.mode_IMP, 2),
            0x9A: (self.TXS, self.mode_IMP, 2),
            0x98: (self.TYA, self.mode_IMP, 2),

            # --- Undocumented ---
            0x9E: (self.SHX, self.mode_ABY, 5),
            0x1B: (self.SLO, self.mode_ABY, 7),
            0x3B: (self.RLA, self.mode_ABY, 7),
        }

    def _flush_tia(self, clocks=None):
        if clocks is None:
            clocks = self.pending_tia_clocks
            self.pending_tia_clocks = 0
        else:
            clocks = min(clocks, self.pending_tia_clocks)
            self.pending_tia_clocks -= clocks
        if clocks > 0:
            self.tia.step(clocks)

    def read(self, address):
        address &= 0x1FFF
        if address & 0x1000:
            if len(self.rom_banks) == 2:
                if address == 0x1FF8:
                    self.current_bank = 0
                elif address == 0x1FF9:
                    self.current_bank = 1
            return self.rom_banks[self.current_bank][address & 0x0FFF] & 0xFF
        if (address & 0x0080) != 0 and (address & 0x0200) == 0:
            return self.memory[0x0080 | (address & 0x7F)] & 0xFF
        if (address & 0x0080) != 0 and (address & 0x0200) != 0:
            return self.riot.read(address) & 0xFF
        return self.tia.read(address) & 0xFF

    def write(self, address, value):
        addr = address & 0x1FFF
        v = (value or 0) & 0xFF
        if addr & 0x1000:
            if len(self.rom_banks) == 2:
                if addr == 0x1FF8:
                    self.current_bank = 0
                elif addr == 0x1FF9:
                    self.current_bank = 1
            return
        if (addr & 0x0080) == 0:
            self.tia.write(addr & 0x3F, v)
        elif (addr & 0x0200) == 0:
            self.memory[0x0080 | (addr & 0x7F)] = v
        else:
            self.riot.write(addr, v)

    def fetch_byte(self):
        val = self.read(self.PC)
        self.PC = (self.PC + 1) & 0xFFFF
        return val

    def update_zn(self, value):
        self.flags['Z'] = 1 if value == 0 else 0
        self.flags['N'] = 1 if (value & 0x80) else 0

    # --- Addressing Modes ---
    def mode_ACC(self):
        return None

    def mode_IMP(self):
        return None

    def mode_IMM(self):
        addr = self.PC
        self.PC = (self.PC + 1) & 0xFFFF
        return addr

    def mode_ZP(self):
        return self.fetch_byte()

    def mode_ZPX(self):
        return (self.fetch_byte() + self.X) & 0xFF

    def mode_ZPY(self):
        return (self.fetch_byte() + self.Y) & 0xFF

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
        zp_addr = (self.fetch_byte() + self.X) & 0xFF
        low = self.read(zp_addr)
        high = self.read((zp_addr + 1) & 0xFF)
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

    # --- Instructies ---
    def JMP(self, addr):
        self.PC = addr

    def SEI(self, addr):
        self.flags['I'] = 1

    def CLI(self, addr):
        self.flags['I'] = 0

    def CLD(self, addr):
        self.flags['D'] = 0

    def SED(self, addr=None):
        self.flags['D'] = 1

    def CLV(self, addr):
        self.flags['V'] = 0

    def CLC(self, addr):
        self.flags['C'] = 0

    def SEC(self, addr):
        self.flags['C'] = 1

    def LDA(self, addr):
        self.A = self.read(addr)
        self.update_zn(self.A)

    def LDX(self, addr):
        self.X = self.read(addr)
        self.update_zn(self.X)

    def LDY(self, addr):
        self.Y = self.read(addr)
        self.update_zn(self.Y)

    def STA(self, addr):
        self.write(addr, self.A)

    def STX(self, addr):
        self.write(addr, self.X)

    def STY(self, addr):
        self.write(addr, self.Y)

    def TXS(self, addr):
        self.SP = self.X

    def TSX(self, addr):
        self.X = self.SP
        self.update_zn(self.X)

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

    def _branch(self, condition, addr):
        if condition:
            old_pc = self.PC
            self.PC = addr
            self.extra_cycles += 1
            if (old_pc & 0xFF00) != (self.PC & 0xFF00):
                self.extra_cycles += 1

    def BNE(self, addr):
        self._branch(self.flags['Z'] == 0, addr)

    def BEQ(self, addr):
        self._branch(self.flags['Z'] == 1, addr)

    def BPL(self, addr):
        self._branch(self.flags['N'] == 0, addr)

    def BMI(self, addr):
        self._branch(self.flags['N'] == 1, addr)

    def BCC(self, addr):
        self._branch(self.flags['C'] == 0, addr)

    def BCS(self, addr):
        self._branch(self.flags['C'] == 1, addr)

    def BVC(self, addr):
        self._branch(self.flags['V'] == 0, addr)

    def BVS(self, addr):
        self._branch(self.flags['V'] == 1, addr)

    def DEC(self, addr):
        val = (self.read(addr) - 1) & 0xFF
        self.write(addr, val)
        self.update_zn(val)

    def INC(self, addr):
        val = (self.read(addr) + 1) & 0xFF
        self.write(addr, val)
        self.update_zn(val)

    def push_byte(self, value):
        self.write(0x0100 | self.SP, value)
        self.SP = (self.SP - 1) & 0xFF

    def pull_byte(self):
        self.SP = (self.SP + 1) & 0xFF
        return self.read(0x0100 | self.SP)

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
        status = self.pull_byte()
        self.flags['N'] = (status >> 7) & 1
        self.flags['V'] = (status >> 6) & 1
        self.flags['D'] = (status >> 3) & 1
        self.flags['I'] = (status >> 2) & 1
        self.flags['Z'] = (status >> 1) & 1
        self.flags['C'] = status & 1

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
        self.push_byte((return_addr >> 8) & 0xFF)
        self.push_byte(return_addr & 0xFF)
        self.PC = addr

    def RTS(self, addr):
        low = self.pull_byte()
        high = self.pull_byte()
        self.PC = (((high << 8) | low) + 1) & 0xFFFF

    def ASL(self, addr):
        val = self.A if addr is None else self.read(addr)
        self.flags['C'] = 1 if (val & 0x80) else 0
        result = (val << 1) & 0xFF
        self.update_zn(result)
        if addr is None:
            self.A = result
        else:
            self.write(addr, result)

    def LSR(self, addr):
        val = self.A if addr is None else self.read(addr)
        self.flags['C'] = val & 0x01
        result = val >> 1
        self.update_zn(result)
        if addr is None:
            self.A = result
        else:
            self.write(addr, result)

    def ROL(self, addr):
        val = self.A if addr is None else self.read(addr)
        old_carry = self.flags['C']
        self.flags['C'] = (val >> 7) & 0x01
        result = ((val << 1) | old_carry) & 0xFF
        if addr is None:
            self.A = result
        else:
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
            self.write(addr, result)
        self.update_zn(result)

    def PHA(self, addr):
        self.push_byte(self.A)

    def PLA(self, addr):
        self.A = self.pull_byte()
        self.update_zn(self.A)

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

    def NOP(self, addr):
        pass

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

    def _tick_cycles(self, count=1):
        if count <= 0:
            return
        for _ in range(count):
            if self.pending_tia_clocks >= 3:
                self.pending_tia_clocks -= 3
                self.tia.step(3)
            elif self.pending_tia_clocks > 0:
                rem = self.pending_tia_clocks
                self.pending_tia_clocks = 0
                self.tia.step(rem)
            self.riot.step(1)

    def _flush_tia(self, clocks=None):
        if clocks is None:
            clocks = self.pending_tia_clocks
            self.pending_tia_clocks = 0
        else:
            clocks = min(clocks, self.pending_tia_clocks)
            self.pending_tia_clocks -= clocks
        if clocks > 0:
            self.tia.step(clocks)

    def step_frame(self):
        self._prev_pc = self.PC
        self.tia.frame_ready = False
        while not self.tia.frame_ready:
            if self.tia.wsync:
                remaining = 228 - self.tia.clock
                self.tia.step(remaining)
                self.riot.step(max(1, remaining // 3))
            else:
                self.step()

    def step(self):
        self._hist.append((self.PC, self.read(self.PC), self.A, self.X, self.Y, self.SP))
        if len(self._hist) > 25:
            self._hist.pop(0)

        self.extra_cycles = 0
        self._page_crossed = False

        opcode = self.fetch_byte()

        if opcode not in self.opcodes:
            for h in self._hist:
                print(f"PC={hex(h[0])} OP={hex(h[1])} A={hex(h[2])} X={hex(h[3])} Y={hex(h[4])} SP={hex(h[5])}")
            raise NotImplementedError(f"Onbekende opcode: {hex(opcode)} op {hex(self.PC - 1)}")


        instruction, mode, cycles = self.opcodes[opcode]
        target_addr = mode()
        if self._page_crossed and instruction in (
            self.ADC, self.AND, self.CMP, self.EOR, self.LDA, self.LDX, self.LDY, self.ORA, self.SBC
        ):
            self.extra_cycles += 1

        # 1. Flush de base cycles minus de laatste cycle vóór de instructie:
        base_flush = max(0, (cycles - 1) * 3)
        self.pending_tia_clocks = cycles * 3
        self._flush_tia(base_flush)

        # 2. Voer de instructie uit (hier kan self.extra_cycles oplopen, bijv. bij _branch):
        instruction(target_addr)

        # 3. Tel eventuele extra_cycles erbij op en flush de resterende clocks:
        total_cycles = cycles + self.extra_cycles
        self.pending_tia_clocks += self.extra_cycles * 3
        self._flush_tia()
        self.riot.step(total_cycles)
        return total_cycles
