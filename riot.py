class RIOT:
    def __init__(self):
        # I/O registers
        self.swcha = 0xFF   # $0280: Joystick richtingen (actief laag: 0 = ingedrukt)
        self.swacnt = 0x00  # $0281: Data Direction Register A
        self.swchb = 0xFF   # $0282: Console switches (bit 0=Reset, bit 1=Select, bit 3=BW/Color)
        self.swbcnt = 0x00  # $0283: Data Direction Register B

        # Interval timer
        self.timer = 0x00
        self.interval = 1       # 1, 8, 64 of 1024 CPU-cycles per tick
        self._prescaler = 0
        self._after_underflow = False
        self.timer_flag = 0x00  # 0x80 wanneer timer is afgelopen

    def step(self, cycles=1):
        """Laat de RIOT-timer 'cycles' CPU-cycles vooruitlopen."""
        if cycles <= 0:
            return

        remaining = cycles
        while remaining > 0:
            step_size = 1 if self._after_underflow else self.interval
            needed = step_size - self._prescaler
            if remaining < needed:
                self._prescaler += remaining
                remaining = 0
            else:
                remaining -= needed
                self._prescaler = 0
                if self.timer == 0:
                    self.timer = 0xFF
                    self.timer_flag = 0x80
                    self._after_underflow = True
                else:
                    self.timer = (self.timer - 1) & 0xFF

    def _set_timer(self, value, interval):
        self.timer = value & 0xFF
        self.interval = interval
        self._prescaler = 0
        self._after_underflow = False
        self.timer_flag = 0x00

    def read(self, addr):
        reg = addr & 0x1F
        if reg == 0x00:    # SWCHA
            return self.swcha & 0xFF
        elif reg == 0x01:  # SWACNT
            return self.swacnt & 0xFF
        elif reg == 0x02:  # SWCHB
            return self.swchb & 0xFF
        elif reg == 0x03:  # SWBCNT
            return self.swbcnt & 0xFF
        elif reg in (0x04, 0x06):  # INTIM (en spiegeladressen)
            self.timer_flag = 0x00
            return self.timer & 0xFF
        elif reg in (0x05, 0x07):  # INSTAT
            return self.timer_flag & 0xFF
        return 0xFF

    def write(self, addr, val):
        v = (val or 0) & 0xFF
        reg = addr & 0x1F
        if reg == 0x00:    # SWCHA
            self.swcha = v
        elif reg == 0x01:  # SWACNT
            self.swacnt = v
        elif reg == 0x02:  # SWCHB
            self.swchb = v
        elif reg == 0x03:  # SWBCNT
            self.swbcnt = v
        elif reg in (0x14, 0x1C):  # TIM1T
            self._set_timer(v, 1)
        elif reg in (0x15, 0x1D):  # TIM8T
            self._set_timer(v, 8)
        elif reg in (0x16, 0x1E):  # TIM64T
            self._set_timer(v, 64)
        elif reg in (0x17, 0x1F):  # T1024T
            self._set_timer(v, 1024)
