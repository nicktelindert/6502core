class TIA:
    def __init__(self):
        # TIA Registers
        self.vsync = 0      # $00: Vertical Sync Set-Clear
        self.vblank = 0     # $01: Vertical Blank Set-Clear
        self.wsync = False  # $02: Wait for Horizontal Blank
        self.colubk = 0x00  # $09: Background Color
        self.colupf = 0x00  # $08: Playfield Color

        # Beam positie
        self.clock = 0      # 0..227 color clocks per lijn
        self.scanline = 0   # 0..261 scanlines per frame
        self.frame_ready = False

        # Framebuffer (160 breed x 192 hoog, RGB tuples)
        self.width = 160
        self.height = 192
        self.framebuffer = [[(0, 0, 0)] * self.width for _ in range(self.height)]

    def write(self, addr, val):
        reg = addr & 0x3F  # TIA gebruikt de onderste 6 bits
        if reg == 0x00:    # VSYNC
            if (val & 0x02) and not (self.vsync & 0x02):
                self.scanline = 0
                self.frame_ready = True
            self.vsync = val
        elif reg == 0x01:  # VBLANK
            self.vblank = val
        elif reg == 0x02:  # WSYNC (pauzeer CPU tot einde scanline)
            self.wsync = True
        elif reg == 0x08:  # COLUPF
            self.colupf = val
        elif reg == 0x09:  # COLUBK
            self.colubk = val

    def step(self, color_clocks):
        """Laat de TIA 'color_clocks' vooruitlopen (3x CPU cycles)."""
        for _ in range(color_clocks):
            # Teken alleen in het zichtbare venster (x: 68..227, y: 40..231)
            if not (self.vblank & 0x02):
                x = self.clock - 68
                y = self.scanline - 40
                if 0 <= x < self.width and 0 <= y < self.height:
                    self.framebuffer[y][x] = self.ntsc_to_rgb(self.colubk)

            self.clock += 1
            if self.clock >= 228:
                self.clock = 0
                self.scanline += 1
                self.wsync = False  # Einde scanline: CPU mag weer verder!
                if self.scanline >= 262:
                    self.scanline = 0
                    self.frame_ready = True

    def ntsc_to_rgb(self, color_byte):
        # Snelle placeholder-mapping zodat elke Atari-kleur direct zichtbaar is
        hue = (color_byte >> 4) & 0x0F
        lum = (color_byte >> 1) & 0x07
        if hue == 0:
            g = lum * 36
            return (g, g, g)
        r = ((hue * 37) & 0xFF) * (lum + 1) // 8
        g = ((hue * 91) & 0xFF) * (lum + 1) // 8
        b = ((hue * 151) & 0xFF) * (lum + 1) // 8
        return (r, g, b)
