from addressing import AddressingMixin
from instructions import InstructionsMixin


class CPU6507(AddressingMixin, InstructionsMixin):
    def __init__(self, bus):
        self.bus = bus
        self.opcodes = self._build_opcode_table()

        self.A = 0x00
        self.X = 0x00
        self.Y = 0x00
        self.SP = 0xFD
        self.flags = {'N': 0, 'V': 0, 'B': 0, 'D': 0, 'I': 1, 'Z': 0, 'C': 0}

        self.extra_cycles = 0
        self._page_crossed = False
        self._hist = []

        self.reset()

    def reset(self):
        low = self.read(0xFFFC)
        high = self.read(0xFFFD)
        self.PC = (high << 8) | low
        print("Reset vector:", hex(self.PC))

    def read(self, address):
        return self.bus.read(address)

    def write(self, address, value):
        self.bus.write(address, value)

    def fetch_byte(self):
        val = self.read(self.PC)
        self.PC = (self.PC + 1) & 0xFFFF
        return val

    def push_byte(self, value):
        self.write(0x0100 | self.SP, value)
        self.SP = (self.SP - 1) & 0xFF

    def pull_byte(self):
        self.SP = (self.SP + 1) & 0xFF
        return self.read(0x0100 | self.SP)

    def update_zn(self, value):
        self.flags['Z'] = 1 if value == 0 else 0
        self.flags['N'] = 1 if (value & 0x80) else 0

    def step(self):
        self.bus.cycles_this_step = 0
        self._hist.append((self.PC, self.A, self.X, self.Y, self.SP))
        if len(self._hist) > 25:
            self._hist.pop(0)

        self.extra_cycles = 0
        self._page_crossed = False

        opcode = self.fetch_byte()
        if opcode not in self.opcodes:
            for h in self._hist:
                print(f"PC={hex(h[0])} A={hex(h[1])} X={hex(h[2])} Y={hex(h[3])} SP={hex(h[4])}")
            raise NotImplementedError(f"Onbekende opcode: {hex(opcode)} op {hex(self.PC - 1)}")

        instruction, mode, cycles, page_penalty = self.opcodes[opcode]
        target_addr = mode()
        if self._page_crossed and page_penalty:
            self.extra_cycles += 1

        instruction(target_addr)

        expected_cycles = cycles + self.extra_cycles
        remaining = expected_cycles - self.bus.cycles_this_step
        if remaining > 0:
            self.bus.tick(remaining)

        return max(expected_cycles, self.bus.cycles_this_step)