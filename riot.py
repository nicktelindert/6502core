class RIOT:
    def __init__(self):
        self.swcha = 0xFF
        self.swchb = 0x0B
        self.timer = 0x00
        self.interval = 1
        self.subcycles = 0
        self.expired = False
        self.zero_passed = False

    def write(self, addr, value):
        reg = addr & 0x1F
        val = value & 0xFF
        if reg in (0x14, 0x15, 0x16, 0x17):  # TIM1T / TIM8T / TIM64T / T1024T
            intervals = {0x14: 1, 0x15: 8, 0x16: 64, 0x17: 1024}
            self.timer = val
            self.interval = intervals[reg]
            self.subcycles = 0
            self.expired = False
            self.zero_passed = False

    def step(self, cycles):
        """Laat de RIOT-timer 'cycles' CPU-kloktikken aftellen."""
        if self.expired and self.interval == 1:
            for _ in range(cycles):
                prev = self.timer
                self.timer = (self.timer - 1) & 0xFF
                if self.timer == 0x00 or (prev > 0x00 and self.timer > prev):
                    self.zero_passed = True
            return

        self.subcycles += cycles
        while self.subcycles >= self.interval:
            self.subcycles -= self.interval
            self.timer = (self.timer - 1) & 0xFF
            if self.timer == 0xFF:
                self.interval = 1
                self.expired = True
                self.zero_passed = True

    def read(self, addr):
        reg = addr & 0x07
        if reg == 0x00:
            return self.swcha
        elif reg == 0x02:
            return self.swchb
        elif reg in (0x04, 0x06):  # $0284: INTIM
            if self.expired and self.zero_passed:
                self.zero_passed = False
                return 0x00
            return self.timer
        return 0
