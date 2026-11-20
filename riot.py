class RIOT:
    def __init__(self):
        self.swcha = 0xFF      # Joysticks (1 = losgelaten)
        self.swchb = 0x0B      # Console switches (Color, P0/P1 diff B, Select/Reset los)
        self.swacnt = 0x00     # Data Direction Register Port A (0 = input)
        self.swbcnt = 0x00     # Data Direction Register Port B (0 = input)

        self.timer = 0x00
        self.interval = 1
        self.subcycles = 0
        self.expired = False
        self.timint_flag = False  # Bit 7 van INSTAT ($0285)

    def write(self, addr, value):
        reg = addr & 0x1F
        val = value & 0xFF

        if reg == 0x00:        # $0280: SWCHA (output bits als DDR=1)
            self.swcha = (self.swcha & ~self.swacnt) | (val & self.swacnt)
        elif reg == 0x01:      # $0281: SWACNT
            self.swacnt = val
        elif reg == 0x02:      # $0282: SWCHB
            self.swchb = (self.swchb & ~self.swbcnt) | (val & self.swbcnt)
        elif reg == 0x03:      # $0283: SWBCNT
            self.swbcnt = val
        elif (reg & 0x17) in (0x14, 0x15, 0x16, 0x17):  # TIM1T / TIM8T / TIM64T / T1024T
            intervals = {0x14: 1, 0x15: 8, 0x16: 64, 0x17: 1024}
            self.interval = intervals[reg & 0x17]
            self.subcycles = 0
            # Eerste aftelslag vindt op de 6532 direct plaats bij de write
            if val == 0:
                self.timer = 0xFF
                self.interval = 1
                self.expired = True
                self.timint_flag = True
            else:
                self.timer = (val - 1) & 0xFF
                self.expired = False
                self.timint_flag = False

    def step(self, cycles):
        """Laat de RIOT-timer 'cycles' CPU-kloktikken aftellen."""
        if self.expired:
            self.timer = (self.timer - cycles) & 0xFF
            return

        self.subcycles += cycles
        while self.subcycles >= self.interval:
            self.subcycles -= self.interval
            self.timer = (self.timer - 1) & 0xFF
            if self.timer == 0xFF:
                self.interval = 1
                self.expired = True
                self.timint_flag = True
                # Eventuele resterende subcycles tellen meteen 1-op-1 door
                if self.subcycles > 0:
                    self.timer = (self.timer - self.subcycles) & 0xFF
                    self.subcycles = 0
                break

    def read(self, addr):
        reg = addr & 0x07
        if reg == 0x00:        # $0280: SWCHA
            return self.swcha
        elif reg == 0x01:      # $0281: SWACNT
            return self.swacnt
        elif reg == 0x02:      # $0282: SWCHB
            return self.swchb
        elif reg == 0x03:      # $0283: SWBCNT
            return self.swbcnt
        elif reg in (0x04, 0x06):  # $0284: INTIM
            self.timint_flag = False
            return self.timer
        elif reg in (0x05, 0x07):  # $0285: INSTAT
            status = 0x80 if self.timint_flag else 0x00
            self.timint_flag = False
            return status
        return 0
