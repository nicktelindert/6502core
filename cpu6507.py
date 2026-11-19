class CPU6507:
    def __init__(self, rom_data):
        self.memory = bytearray(0x2000)
        self.memory[0x1000:0x2000] = rom_data
        self.cycles = 0
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
            0xC8: (self.INY, self.mode_IMP, 2),
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
            0xBD: (self.LDA, self.mode_ABX, 4),
            0x4A: (self.LSR, self.mode_ACC, 2),
            0x46: (self.LSR, self.mode_ZP,  5),
            0x56: (self.LSR, self.mode_ZPX, 6),
            0x4E: (self.LSR, self.mode_ABS, 6),
            0x48: (self.PHA, self.mode_IMP, 3),
            0x68: (self.PLA, self.mode_IMP, 4),
            0x29: (self.AND, self.mode_IMM, 2),
            0x25: (self.AND, self.mode_ZP,  3),
            0x35: (self.AND, self.mode_ZPX, 4),
            0x2D: (self.AND, self.mode_ABS, 4),
            0x3D: (self.AND, self.mode_ABX, 4),
            0xA0: (self.LDY, self.mode_IMM, 2),
            0Xc9: (self.CMP, self.mode_IMM, 2),
            0xE0: (self.CPX, self.mode_IMM, 2),
            0xE4: (self.CPX, self.mode_ZP,  3),
            0xEC: (self.CPX, self.mode_ABS, 4),
            0xC0: (self.CPY, self.mode_IMM, 2),
            0xC4: (self.CPY, self.mode_ZP,  3),
            0xCC: (self.CPY, self.mode_ABS, 4),
            0xB0: (self.BCS, self.mode_REL, 2),
            0x90: (self.BCC, self.mode_REL, 2),
            0xA6: (self.LDX, self.mode_ZP, 3),
            0xF0: (self.BEQ, self.mode_REL, 2),
            0x45: (self.EOR, self.mode_ZP, 3),
            0x49: (self.EOR, self.mode_IMM, 2),
            0x18: (self.CLC, self.mode_IMP, 2),
            0x38: (self.SEC, self.mode_IMP, 2),
            0x69: (self.ADC, self.mode_IMM, 2),
            0x65: (self.ADC, self.mode_ZP,  3),
            0x75: (self.ADC, self.mode_ZPX, 4),
            0x6D: (self.ADC, self.mode_ABS, 4),
            0xA8: (self.TAY, self.mode_IMP, 2),
            0xAA: (self.TAX, self.mode_IMP, 2),
            0x98: (self.TYA, self.mode_IMP, 2),
            0x8A: (self.TXA, self.mode_IMP, 2),
            0xB9: (self.LDA, self.mode_ABY, 4),
            0x99: (self.STA, self.mode_ABY, 5),
            0x09: (self.ORA, self.mode_IMM, 2),
            0x05: (self.ORA, self.mode_ZP,  3),
            0x15: (self.ORA, self.mode_ZPX, 4),
            0x0D: (self.ORA, self.mode_ABS, 4),
            0x88: (self.DEY, self.mode_IMP, 2),
            0xA4: (self.LDY, self.mode_ZP,  3),
            0xB4: (self.LDY, self.mode_ZPX, 4),
            0xAC: (self.LDY, self.mode_ABS, 4),
            0xBC: (self.LDY, self.mode_ABX, 4),
            0xE9: (self.SBC, self.mode_IMM, 2),
            0xE5: (self.SBC, self.mode_ZP,  3),
            0xF5: (self.SBC, self.mode_ZPX, 4),
            0xED: (self.SBC, self.mode_ABS, 4),
            0x94: (self.STY, self.mode_ZPX, 4),
            0xB1: (self.LDA, self.mode_INY, 5),
            0x91: (self.STA, self.mode_INY, 6),
            0xD1: (self.CMP, self.mode_INY, 5),
            0xC6: (self.DEC, self.mode_ZP,  5),
            0xD6: (self.DEC, self.mode_ZPX, 6),
            0xCE: (self.DEC, self.mode_ABS, 6),
            0xDE: (self.DEC, self.mode_ABX, 7),
            0xE6: (self.INC, self.mode_ZP,  5),
            0xF6: (self.INC, self.mode_ZPX, 6),
            0xEE: (self.INC, self.mode_ABS, 6),
            0xFE: (self.INC, self.mode_ABX, 7),
            0xEA: (self.NOP, self.mode_IMP, 2),
            0x8D: (self.STA, self.mode_ABS, 4),
            0x9D: (self.STA, self.mode_ABX, 5),
            0x6A: (self.ROR, self.mode_ACC, 2),
            0x66: (self.ROR, self.mode_ZP,  5),
            0x76: (self.ROR, self.mode_ZPX, 6),
            0x6E: (self.ROR, self.mode_ABS, 6),
            0x7E: (self.ROR, self.mode_ABX, 7),
            0x2A: (self.ROL, self.mode_ACC, 2),
            0x26: (self.ROL, self.mode_ZP,  5),
            0x36: (self.ROL, self.mode_ZPX, 6),
            0x2E: (self.ROL, self.mode_ABS, 6),
            0x3E: (self.ROL, self.mode_ABX, 7),
            0x4C: (self.JMP, self.mode_ABS, 3),
            0xC5: (self.CMP, self.mode_ZP,  3),
            0xD5: (self.CMP, self.mode_ZPX, 4),
            0xCD: (self.CMP, self.mode_ABS, 4),
            0xDD: (self.CMP, self.mode_ABX, 4),
            0xD9: (self.CMP, self.mode_ABY, 4),
            0x24: (self.BIT, self.mode_ZP,  3),
            0x2C: (self.BIT, self.mode_ABS, 4),
            0x50: (self.BVC, self.mode_REL, 2),
            0x70: (self.BVS, self.mode_REL, 2),
            0x39: (self.AND, self.mode_ABY, 4),
            0x08: (self.PHP, self.mode_IMP, 3),
            0x28: (self.PLP, self.mode_IMP, 4),
        }

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
        return (base_addr + self.X) & 0xFFFF

    def mode_ABY(self):
        low = self.fetch_byte()
        high = self.fetch_byte()
        base_addr = (high << 8) | low
        return (base_addr + self.Y) & 0xFFFF

    def mode_REL(self):
        offset = self.fetch_byte()
        # Zet unsigned byte (0..255) om naar signed (-128..127)
        if offset & 0x80:
            offset -= 0x100
        return (self.PC + offset) & 0xFFFF

    def mode_INY(self):
        zp_addr = self.read(self.PC)
        self.PC = (self.PC + 1) & 0xFFFF

        # Lees de 16-bit pointer uit Zero Page (wrap-around binnen Zero Page!)
        low = self.read(zp_addr)
        high = self.read((zp_addr + 1) & 0xFF)
        base_addr = (high << 8) | low

        return (base_addr + self.Y) & 0xFFFF

    def JMP(self, addr):
        self.PC = addr
    
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

    def LDY(self, addr):
        self.Y = self.read(addr)
        self.update_zn(self.Y)

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
    def INY(self, addr):
        self.Y = (self.Y + 1) & 0xFF
        self.update_zn(self.Y)
   
    def DEC(self, addr):
        val = (self.read(addr) - 1) & 0xFF
        self.write(addr, val)
        self.update_zn(val)

    def INC(self, addr):
        val = (self.read(addr) + 1) & 0xFF
        self.write(addr, val)
        self.update_zn(val)

    def push_byte(self, value):
        # Stack zit vast op pagina 1 (0x0100 + SP)
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
        self.write(0x0100 + self.SP, status)
        self.SP = (self.SP - 1) & 0xFF

    def PLP(self, addr):
        self.SP = (self.SP + 1) & 0xFF
        status = self.read(0x0100 + self.SP)
        self.flags['N'] = (status >> 7) & 1
        self.flags['V'] = (status >> 6) & 1
        self.flags['D'] = (status >> 3) & 1
        self.flags['I'] = (status >> 2) & 1
        self.flags['Z'] = (status >> 1) & 1
        self.flags['C'] = status & 1

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

    def LSR (self, addr):
        if addr is None:
            val = self.A
        else:
            val = self.read(addr)

        # Bit 0 (het meest rechtse bit) gaat naar de Carry-flag
        self.flags['C'] = val & 0x01

        # Schuif 1 bit naar rechts (bit 7 wordt automatisch 0)
        result = val >> 1
        self.update_zn(result)

        if addr is None:
            self.A = result
        else:
            self.write(addr, result)
    
    def PHA(self, addr):
        self.push_byte(self.A)

    def PLA(self, addr):
        self.A = self.pull_byte()
        self.update_zn(self.A)

    def AND(self, addr):
        self.A =  self.A & self.read(addr)
        self.update_zn(self.A)

    def BIT(self, addr):
        val = self.read(addr)
        self.flags['Z'] = 1 if (self.A & val) == 0 else 0
        self.flags['N'] = (val >> 7) & 0x01
        self.flags['V'] = (val >> 6) & 0x01

    def CMP(self, addr):
        val = self.read(addr)
        result = (self.A - val) & 0xFF

        # Let op: Carry (C) is 1 als A >= val (geen 'borrow' nodig!)
        self.flags['C'] = 1 if self.A >= val else 0
        self.update_zn(result)

    def CPX(self, addr):
        val = self.read(addr)
        result = (self.X - val) & 0xFF
        
        # Carry (C) is 1 als X >= val
        self.flags['C'] = 1 if self.X >= val else 0
        self.update_zn(result)

    def CPY(self, addr):
        val = self.read(addr)
        result = (self.Y - val) & 0xFF
        
        # Carry (C) is 1 als Y >= val
        self.flags['C'] = 1 if self.Y >= val else 0
        self.update_zn(result)     

    def BCS(self, addr):
        if self.flags['C'] == 1:
            self.PC = addr;

    def BCC(self, addr):
        if self.flags['C'] == 0:
            self.PC = addr;

    def BEQ(self, addr):
        if self.flags['Z'] == 1:
            # +1 cycle omdat de sprong wordt genomen
            self.cycles += 1
            
            # +1 extra cycle als de high byte (de 'page') verandert
            if (self.PC & 0xFF00) != (addr & 0xFF00):
                self.cycles += 1
                
            self.PC = addr
    
    def EOR(self, addr):
        self.A = self.A ^ self.read(addr)
        self.update_zn(self.A)
   
    def ORA(self, addr):
        self.A = self.A | self.read(addr)
        self.update_zn(self.A)

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

    def CLC(self, addr):
        self.flags['C'] = 0

    def SEC(self, addr):
        self.flags['C'] = 1
    
    def ADC(self, addr):
        val = self.read(addr)
        carry_in = self.flags['C']

        # Tel A, de gelezen waarde en de huidige Carry bij elkaar op
        total = self.A + val + carry_in
        result = total & 0xFF

        # Carry (C): 1 als de som boven 255 (0xFF) uitkomt
        self.flags['C'] = 1 if total > 0xFF else 0

        # Overflow (V): 1 als twee getallen met hetzelfde teken een resultaat met het omgekeerde teken geven
        # (bijv. 127 + 1 = -128 in signed 8-bit)
        self.flags['V'] = 1 if (~(self.A ^ val) & (self.A ^ result) & 0x80) else 0

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

    def DEY(self, addr):
        self.Y = (self.Y - 1) & 0xFF
        self.update_zn(self.Y)

    def SBC(self, addr):
        # Op de 6502 is A - val - (1 - C) gelijk aan A + (~val) + C
        val = self.read(addr) ^ 0xFF
        carry_in = self.flags['C']
    
        total = self.A + val + carry_in
        result = total & 0xFF
    
        self.flags['C'] = 1 if total > 0xFF else 0
        self.flags['V'] = 1 if (~(self.A ^ val) & (self.A ^ result) & 0x80) else 0
    
        self.A = result
        self.update_zn(self.A)

    def BVC(self, addr):
        if self.flags['V'] == 0:
            self.PC = addr

    def BVS(self, addr):
        if self.flags['V'] == 1:
            self.PC = addr

    def NOP(self, addr):
        # Just burning up some cycles:)
        pass
    
    # --- De hoofd-stap ---
    def step(self):
        opcode = self.fetch_byte()
        if opcode not in self.opcodes:
            raise NotImplementedError(f"Onbekende opcode: {hex(opcode)} op {hex(self.PC - 1)}")

        instruction, mode, cycles = self.opcodes[opcode]
        target_addr = mode()
        instruction(target_addr)
        return cycles
