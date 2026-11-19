class RIOT:
    def __init__(self):
        self.timer = 0
        self.interval = 1024
        self.subcycles = 0

        # I/O poorten (Active Low op de Atari 2600: bit = 0 betekent ingedrukt)
        self.swcha = 0xFF  # $0280: P0 & P1 joysticks (bits 4..7 = P0, bits 0..3 = P1)
        self.swchb = 0x0B  # $0282: Console switches (Color mode aan, Reset/Select los)

    def step(self, cycles):
        """Laat de RIOT-timer 'cycles' CPU-kloktikken aftellen."""
        self.subcycles += cycles
        while self.subcycles >= self.interval:
            self.subcycles -= self.interval
            self.timer = (self.timer - 1) & 0xFF
            if self.timer == 0xFF:
                # Na underflow telt de timer per 1 CPU-cycle verder af
                self.interval = 1

    def read(self, addr):
        reg = addr & 0x07
        if reg == 0x00:              # $0280: SWCHA (Joystick richtingen)
            return self.swcha
        elif reg == 0x02:            # $0282: SWCHB (Reset / Select / Difficulty)
            return self.swchb
        elif reg in (0x04, 0x06):    # $0284: INTIM (Huidige timerwaarde)
            return self.timer
        return 0

    def write(self, addr, val):
        reg = addr & 0x1F
        if reg == 0x14:              # $0294: TIM1T (1 cycle per tick)
            self.timer, self.interval, self.subcycles = val, 1, 0
        elif reg == 0x15:            # $0295: TIM8T (8 cycles per tick)
            self.timer, self.interval, self.subcycles = val, 8, 0
        elif reg == 0x16:            # $0296: TIM64T (64 cycles per tick)
            self.timer, self.interval, self.subcycles = val, 64, 0
        elif reg == 0x17:            # $0297: T1024T (1024 cycles per tick)
            self.timer, self.interval, self.subcycles = val, 1024, 0
