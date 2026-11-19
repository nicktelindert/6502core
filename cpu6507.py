class CPU6507:
    def __init__(self, rom_data):
        self.memory = bytearray(0x2000)
        self.memory[0x1000:0x2000] = rom_data

        self.A = 0x00
        self.X = 0x00
        self.Y = 0x00
        self.SP = 0xFD
        self.flags = {'N': 0, 'V': 0, 'B': 0, 'D': 0, 'I': 1, 'Z': 0, 'C': 0}

        low = self.read(0xFFFC)
        high = self.read(0xFFFD)
        self.PC = (high << 8) | low

        # De Opcode Tabel: opcode -> (instructie, addressing_mode, cycles)
        self.opcodes = {
            0x78: (self.SEI, self.mode_IMP, 2),
            0xD8: (self.CLD, self.mode_IMP, 2),
            0xA9: (self.LDA, self.mode_IMM, 2),
            0xA5: (self.LDA, self.mode_ZP,  3),
            0xB5: (self.LDA, self.mode_ZPX, 4),
            0xAD: (self.LDA, self.mode_ABS, 4),
            0x85: (self.STA, self.mode_ZP,  3),
            0x95: (self.STA, self.mode_ZPX, 4),
            0xA2: (self.LDX, self.mode_IMM, 2),
            0x9A: (self.TXS, self.mode_IMP, 2),
            0xCA: (self.DEX, self.mode_IMP, 2),
            0xD0: (self.BNE, self.mode_REL, 2),
            0xE8: (self.INX, self.mode_IMP, 2),
            0x20: (self.JSR, self.mode_ABS, 6),
            0x60: (self.RTS, self.mode_IMP, 6),
            0x86: (self.STX, self.mode_ZP, 3),
            0x96: (self.STX, self.mode_ZPY, 4),
            0x8E: (self.STX, self.mode_ABS, 4),
            0x84: (self.STY, self.mode_ZP, 3),
            0x8C: (self.STY, self.mode_ABS, 4),
            0x10: (self.BPL, self.mode_REL, 2),
            0x30: (self.BMI, self.mode_REL, 2),
            0x0A: (self.ASL, self.mode_ACC, 2),
            0x06: (self.ASL, self.mode_ZP,  5),
            0x16: (self.ASL, self.mode_ZPX, 6),
            0x0E: (self.ASL, self.mode_ABS, 6),
        }

    # --- Geheugen & Helpers ---
    def read(self, address):
        return self.memory[address & 0x1FFF]

    def write(self, address, value):
        addr = address & 0x1FFF
        if addr < 0x1000:
            self.memory[addr] = value & 0xFF

    def fetch_byte(self):
        val = self.read(self.PC)
        self.PC = (self.PC + 1) & 0xFFFF
        return val

    def update_zn(self, value):
        self.flags['Z'] = 1 if value == 0 else 0
        self.flags['N'] = 1 if (value & 0x80) else 0

    def mode_ACC(self):
        return None

    # --- Addressing Modes (geven altijd een geheugenadres terug, of None) ---
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

    def mode_REL(self):
        offset = self.fetch_byte()
        # Zet unsigned byte (0..255) om naar signed (-128..127)
        if offset & 0x80:
            offset -= 0x100
        return (self.PC + offset) & 0xFFFF

    # --- Instructies (ontvangen het berekende doeladres) ---
    def SEI(self, addr):
        self.flags['I'] = 1

    def CLD(self, addr):
        self.flags['D'] = 0

    def LDA(self, addr):
        self.A = self.read(addr)
        self.update_zn(self.A)

    def LDX(self, addr):
        self.X = self.read(addr)
        self.update_zn(self.X)

    def STA(self, addr):
        self.write(addr, self.A)

    def TXS(self, addr):
        self.SP = self.X

    def DEX(self, addr):
        self.X = (self.X - 1) & 0xFF
        self.update_zn(self.X)

    def BNE(self, addr):
        if self.flags['Z'] == 0:
            self.PC = addr

    def INX(self, addr):
        self.X = (self.X+1) & 0XFF
        self.update_zn(self.X)

    def push_byte(self, value):
        # Stack zit vast op pagina 1 (0x0100 + SP)
        self.write(0x0100 | self.SP, value)
        self.SP = (self.SP - 1) & 0xFF

    def pull_byte(self):
        self.SP = (self.SP + 1) & 0xFF
        return self.read(0x0100 | self.SP)
    
    def JSR(self, addr):
        return_addr = (self.PC -1) & 0xFFFF
        self.push_byte((return_addr >> 8) & 0xFF)  # High byte eerst
        self.push_byte(return_addr & 0xFF)         # Low byte daarna
        self.PC = addr

    def RTS(self, addr):
        low = self.pull_byte()
        high = self.pull_byte()
        self.PC = (((high << 8) | low) + 1) & 0xFFFF    
    
    def STX(self, addr):
        self.write(addr, self.X)

    def STY(self, addr):
        self.write(addr, self.Y)

    def BPL(self, addr):
        if self.flags['N'] == 0:
            self.PC = addr

    def BMI(self, addr):
        if self.flags['N'] ==1:
            self.PC = addr
    
    def ASL(self, addr):
        if addr is None:
            val = self.A
        else:
            val = self.read(addr)

        # Bit 7 gaat naar de Carry-flag
        self.flags['C'] = 1 if (val & 0x80) else 0

        # Schuif 1 bit naar links en kap af op 8 bits
        result = (val << 1) & 0xFF
        self.update_zn(result)

        if addr is None:
            self.A = result
        else:
            self.write(addr, result)
    
    # --- De hoofd-stap ---
    def step(self):
        opcode = self.fetch_byte()
        if opcode not in self.opcodes:
            raise NotImplementedError(f"Onbekende opcode: {hex(opcode)} op {hex(self.PC - 1)}")

        instruction, mode, cycles = self.opcodes[opcode]
        target_addr = mode()
        instruction(target_addr)
        return cycles
