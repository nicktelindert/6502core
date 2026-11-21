import array
import pygame

class TIAAudio:
    """Cycle-accurate TIA 2-kanaals audio (31.440 Hz, 2 samples per scanline)."""

    def __init__(self):
        self.audc = [0, 0]   # Waveform control (0x15, 0x16)
        self.audf = [0, 0]   # Frequency divider (0x17, 0x18)
        self.audv = [0, 0]   # Volume (0x19, 0x1A)

        self.div_counter = [0, 0]
        self.poly4 = [0x0F, 0x0F]
        self.poly5 = [0x1F, 0x1F]
        self.poly9 = [0x1FF, 0x1FF]
        self.out = [0, 0]

        self.samples = array.array("h")
        self.channel = None

        if not pygame.mixer.get_init():
            pygame.mixer.init(frequency=44100, size=-16, channels=1, buffer=1024)
        self.channel = pygame.mixer.Channel(0)

    def _clock_channel(self, ch):
        self.div_counter[ch] += 1
        if self.div_counter[ch] <= self.audf[ch]:
            return
        self.div_counter[ch] = 0

        ctrl = self.audc[ch] & 0x0F
        p4, p5, p9 = self.poly4[ch], self.poly5[ch], self.poly9[ch]

        # 5-bit LFSR feedback
        in5 = ((p5 & 1) ^ ((p5 >> 2) & 1))
        if ctrl in (0x00, 0x0B):
            self.out[ch] = 1
            return

        # Wanneer mag het 4-bit / 9-bit register stappen?
        clock_4 = True
        if ctrl in (0x01, 0x02, 0x03, 0x06, 0x07, 0x0A, 0x0E, 0x0F):
            # Alleen stappen op specifieke taps van poly5
            if ctrl in (0x02, 0x03):
                clock_4 = (p5 & 0x1F) == 0x10
            else:
                clock_4 = bool(p5 & 1)

        if clock_4:
            if ctrl == 0x08:  # 9-bit witte ruis (explosies / ruis)
                in9 = ((p9 & 1) ^ ((p9 >> 4) & 1))
                self.poly9[ch] = (p9 >> 1) | (in9 << 8)
                self.out[ch] = self.poly9[ch] & 1
            elif ctrl in (0x04, 0x05):  # Pure blokgolf (deelt door 2)
                self.out[ch] ^= 1
            elif ctrl in (0x0C, 0x0D):  # Lage bas / deelt door 3
                self.poly4[ch] = ((p4 >> 1) | ((1 if (p4 & 3) == 0 else 0) << 3)) & 0x0F
                self.out[ch] = self.poly4[ch] & 1
            else:  # Standaard 4-bit LFSR
                in4 = ((p4 & 1) ^ ((p4 >> 1) & 1))
                self.poly4[ch] = (p4 >> 1) | (in4 << 3)
                self.out[ch] = self.poly4[ch] & 1

        self.poly5[ch] = (p5 >> 1) | (in5 << 4)

    def tick_half_scanline(self):
        """Roep 2x per scanline aan (bij clock ~114 en clock 228)."""
        self._clock_channel(0)
        self._clock_channel(1)
        v0 = self.audv[0] if self.out[0] else 0
        v1 = self.audv[1] if self.out[1] else 0
        # Schaal 0..30 naar 16-bit signed audio
        sample = (v0 + v1) * 700
        self.samples.append(sample)

    def flush_frame(self):
        """Speel de verzamelde samples van dit frame af."""
        if self.samples and self.channel is not None:
            snd = pygame.mixer.Sound(buffer=self.samples.tobytes())
            if not self.channel.get_busy():
                self.channel.play(snd)
            elif self.channel.get_queue() is None:
                self.channel.queue(snd)
        self.samples = array.array("h")
