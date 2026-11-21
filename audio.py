import array
import pygame


class TIAAudio:
    """Cycle-accurate TIA 2-kanaals audio met exacte Stella/TIASound bit-patronen."""

    TIA_RATE = 31400.0
    OUT_RATE = 44100.0

    # Exacte hardware bit-sequenties van de Atari 2600 TIA (Ron Fries / Stella TIASound)
    _POLY4 = (1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0)  # 15 stappen
    _POLY5 = (
        0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0,
        1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1,
    )  # 31 stappen
    _DIV31 = (
        0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    )  # 31 stappen (2 pulsen per 31: 18 + 13)
    _DIV6 = (0, 1, 0, 0, 1, 0)  # 6 stappen (2 pulsen per 6: 3 + 3)

    def __init__(self):
        self.audc = [0, 0]   # Waveform control (0x15, 0x16)
        self.audf = [0, 0]   # Frequency divider (0x17, 0x18)
        self.audv = [0, 0]   # Volume (0x19, 0x1A)

        self.div_counter = [0, 0]
        self.p4_idx = [0, 0]
        self.p5_idx = [0, 0]
        self.d6_idx = [0, 0]
        self.poly9 = [0x1FF, 0x1FF]
        self.out = [0, 0]

        self._phase = 0.0
        self._step = self.TIA_RATE / self.OUT_RATE

        self.samples = array.array("h")
        if not pygame.mixer.get_init():
            pygame.mixer.init(frequency=44100, size=-16, channels=1, buffer=2048)
        self.channel = pygame.mixer.Channel(0)

    def _clock_channel(self, ch):
        self.div_counter[ch] += 1
        if self.div_counter[ch] <= self.audf[ch]:
            return
        self.div_counter[ch] = 0

        ctrl = self.audc[ch] & 0x0F
        p5 = self.p5_idx[ch]
        self.p5_idx[ch] = (p5 + 1) % 31

        if ctrl in (0x00, 0x0B):
            self.out[ch] = 1
        elif ctrl == 0x01:
            # 5-bit poly
            self.out[ch] = self._POLY5[p5]
        elif ctrl in (0x02, 0x03):
            # div31 -> 4-bit poly
            if self._DIV31[p5]:
                idx4 = self.p4_idx[ch]
                self.p4_idx[ch] = (idx4 + 1) % 15
                self.out[ch] = self._POLY4[idx4]
        elif ctrl in (0x04, 0x05):
            # Pure toon: deelt door 2 (1 hoog, 1 laag)
            self.out[ch] ^= 1
        elif ctrl in (0x06, 0x07, 0x0A):
            # Pure toon: deelt door 31 (18 hoog, 13 laag)
            if self._DIV31[p5]:
                self.out[ch] ^= 1
        elif ctrl == 0x08:
            # 9-bit poly (witte ruis)
            p9 = self.poly9[ch]
            in9 = (p9 & 1) ^ ((p9 >> 4) & 1)
            self.poly9[ch] = (p9 >> 1) | (in9 << 8)
            self.out[ch] = self.poly9[ch] & 1
        elif ctrl == 0x09:
            # 5-bit poly
            self.out[ch] = self._POLY5[p5]
        elif ctrl in (0x0C, 0x0D):
            # Pure toon: deelt door 6 (3 hoog, 3 laag)
            idx6 = self.d6_idx[ch]
            self.d6_idx[ch] = (idx6 + 1) % 6
            if self._DIV6[idx6]:
                self.out[ch] ^= 1
        elif ctrl in (0x0E, 0x0F):
            # div31 -> deelt door 6
            if self._DIV31[p5]:
                idx6 = self.d6_idx[ch]
                self.d6_idx[ch] = (idx6 + 1) % 6
                if self._DIV6[idx6]:
                    self.out[ch] ^= 1

    def tick_half_scanline(self):
        """Roep 2x per scanline aan (bij clock ~114 en clock 228)."""
        self._clock_channel(0)
        self._clock_channel(1)
        v0 = self.audv[0] if self.out[0] else -self.audv[0]
        v1 = self.audv[1] if self.out[1] else -self.audv[1]
        cur = (v0 + v1) * 500

        self._phase += 1.0
        while self._phase >= self._step:
            self.samples.append(cur)
            self._phase -= self._step

    def flush_frame(self):
        """Stuur verzamelde 44.100 Hz samples naar pygame.mixer."""
        if not self.samples or self.channel is None:
            return

        if not self.channel.get_busy():
            if len(self.samples) < 1470:
                return
            snd = pygame.mixer.Sound(buffer=self.samples.tobytes())
            self.channel.play(snd)
            self.samples = array.array("h")
        elif self.channel.get_queue() is None:
            snd = pygame.mixer.Sound(buffer=self.samples.tobytes())
            self.channel.queue(snd)
            self.samples = array.array("h")
        elif len(self.samples) > 4410:
            self.samples = self.samples[-1470:]