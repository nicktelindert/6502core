class TIA:
    NTSC_HUES = [
        (0, 0, 0),       # 0: Zwart / grijs
        (212, 168, 32),  # 1: Goud / geel
        (212, 128, 24),  # 2: Oranje-geel
        (198, 88, 24),   # 3: Oranje
        (184, 56, 48),   # 4: Rood-oranje
        (168, 40, 96),   # 5: Paars-rood
        (128, 44, 148),  # 6: Paars
        (84, 56, 176),   # 7: Indigo / blauw-paars
        (48, 80, 196),   # 8: Blauw
        (32, 112, 188),  # 9: Helder blauw
        (24, 136, 152),  # A: Cyaan / groenblauw
        (32, 148, 96),   # B: Zeegroen
        (48, 152, 48),   # C: Groen
        (96, 144, 32),   # D: Geelgroen / olijf
        (144, 128, 24),  # E: Licht olijf / zand
        (184, 108, 24),  # F: Warm bruin / oker
    ]
    def __init__(self):
        # TIA Registers
        self.vsync = 0      # $00: Vertical Sync Set-Clear
        self.vblank = 0     # $01: Vertical Blank Set-Clear
        self.wsync = False  # $02: Wait for Horizontal Blank
        self.colupf = 0x00  # $08: Playfield Color
        self.colubk = 0x00  # $09: Background Color
        self.ctrlpf = 0x00  # $0A: Playfield Control (bit 0 = REF)
        self.pf0 = 0x00     # $0D: Playfield register 0 (bits 4..7)
        self.pf1 = 0x00     # $0E: Playfield register 1 (bits 0..7)
        self.pf2 = 0x00     # $0F: Playfield register 2 (bits 0..7)
        self.colup0 = 0x00  # $06: Player 0 Color
        self.colup1 = 0x00  # $07: Player 1 Color
        self.grp0 = 0x00    # $1B: Player 0 Graphics
        self.grp1 = 0x00    # $1C: Player 1 Graphics
        self.p0_x = 0       # Horizontale positie (0..159)
        self.p1_x = 0
        self.nusiz0 = 0x00  # $04: Number-Size Player 0
        self.nusiz1 = 0x00  # $05: Number-Size Player 1 
        self.hmp0 = 0x00  # $20: Horizontal Motion Player 0
        self.hmp1 = 0x00  # $21: Horizontal Motion Player 1 
        
        # Beam positie
        self.clock = 0      # 0..227 color clocks per lijn
        self.scanline = 0   # 0..261 scanlines per frame
        self.frame_ready = False

        # Framebuffer (160 breed x 192 hoog, RGB tuples)
        self.width = 160
        self.height = 192
        self.framebuffer = [[(0, 0, 0)] * self.width for _ in range(self.height)]
    # Helper voor signed 4-bit motion (-8..+7)
    def _hm_offset(self, reg_val):
        raw = (reg_val >> 4) & 0x0F
        signed = raw - 16 if raw >= 8 else raw
        # Op de TIA: positief = naar links, negatief = naar rechts
        return -signed
    
    def write(self, addr, val):
        reg = addr & 0x3F
        v = val & 0xFF
        if reg == 0x00:    # VSYNC
            if (v & 0x02) and not (self.vsync & 0x02):
                self.scanline = 0
                self.frame_ready = True
            self.vsync = v
        elif reg == 0x01:  # VBLANK
            self.vblank = v
        elif reg == 0x02:  # WSYNC
            self.wsync = True
        elif reg == 0x08:  # COLUPF
            self.colupf = v
        elif reg == 0x09:  # COLUBK
            self.colubk = v
        elif reg == 0x0A:  # CTRLPF
            self.ctrlpf = v
        elif reg == 0x0D:  # PF0
            self.pf0 = v
        elif reg == 0x0E:  # PF1
            self.pf1 = v
        elif reg == 0x0F:  # PF2
            self.pf2 = v
        elif reg == 0x06:  # COLUP0
            self.colup0 = v
        elif reg == 0x07:  # COLUP1
            self.colup1 = v
        elif reg == 0x10:  # RESP0
            self.p0_x = max(0, (self.clock - 68) + 3) if self.clock >= 68 else 0
        elif reg == 0x11:  # RESP1
            self.p1_x = max(0, (self.clock - 68) + 3) if self.clock >= 68 else 0
        elif reg == 0x1B:  # GRP0
            self.grp0 = v
        elif reg == 0x1C:  # GRP1
            self.grp1 = v
        elif reg == 0x04:  # NUSIZ0
            self.nusiz0 = v
        elif reg == 0x05:  # NUSIZ1
            self.nusiz1 = v    
        elif reg == 0x20:  # HMP0
            self.hmp0 = v
        elif reg == 0x21:  # HMP1
            self.hmp1 = v
        elif reg == 0x2A:  # HMOVE
            self.p0_x = max(0, min(159, self.p0_x + self._hm_offset(self.hmp0)))
            self.p1_x = max(0, min(159, self.p1_x + self._hm_offset(self.hmp1)))
        elif reg == 0x2B:  # HMCLR: wis horizontale motion-registers
            self.hmp0 = 0x00
            self.hmp1 = 0x00    

    def _playfield_bit(self, pf_x):
        """Geeft 0 of 1 voor playfield-positie pf_x (0..19 binnen een helft van 80 px)."""
        if pf_x < 4:
            # PF0: bits 4, 5, 6, 7 (van links naar rechts)
            bit = 4 + pf_x
            return (self.pf0 >> bit) & 0x01
        elif pf_x < 12:
            # PF1: bits 7, 6, 5, 4, 3, 2, 1, 0 (van links naar rechts)
            bit = 7 - (pf_x - 4)
            return (self.pf1 >> bit) & 0x01
        else:
            # PF2: bits 0, 1, 2, 3, 4, 5, 6, 7 (van links naar rechts)
            bit = pf_x - 12
            return (self.pf2 >> bit) & 0x01

    def _is_pf_on(self, x):
        """Bepaalt of de playfield aan staat op scherm-x (0..159)."""
        if x < 80:
            pf_x = x // 4  # 0..19
        else:
            right_x = x - 80  # 0..79
            if self.ctrlpf & 0x01:  # REF: spiegel de linkerhelft
                pf_x = 19 - (right_x // 4)
            else:                   # Repeat: herhaal de 20 bits
                pf_x = right_x // 4
        return bool(self._playfield_bit(pf_x))
    
    def _player_color(self, x, player_x, grp, colup, nusiz):
        """Geeft de kleur van een player-sprite (inclusief kopieën en 2x/4x breedte) op x, of None."""
        mode = nusiz & 0x07
        scale = 1
        offsets = [0]

        if mode == 1:
            offsets = [0, 16]
        elif mode == 2:
            offsets = [0, 32]
        elif mode == 3:
            offsets = [0, 16, 32]
        elif mode == 4:
            offsets = [0, 64]
        elif mode == 5:
            scale = 2
        elif mode == 6:
            offsets = [0, 32, 64]
        elif mode == 7:
            scale = 4

        sprite_width = 8 * scale
        for off in offsets:
            start = player_x + off
            if start <= x < start + sprite_width:
                bit = 7 - ((x - start) // scale)
                if (grp >> bit) & 0x01:
                    return colup
        return None
   
    def _pixel_color(self, x):
        p0 = self._player_color(x, self.p0_x, self.grp0, self.colup0, self.nusiz0)
        p1 = self._player_color(x, self.p1_x, self.grp1, self.colup1, self.nusiz1)
        pf = self.colupf if self._is_pf_on(x) else None
        bk = self.colubk

        # CTRLPF bit 2 (0x04): PF vóór Players
        if self.ctrlpf & 0x04:
            return pf or p0 or p1 or bk
        return p0 or p1 or pf or bk

    def step(self, color_clocks):
        """Laat de TIA 'color_clocks' vooruitlopen (3x CPU cycles)."""
        for _ in range(color_clocks):
            if not (self.vblank & 0x02):
                x = self.clock - 68
                y = self.scanline - 40
                if 0 <= x < self.width and 0 <= y < self.height:
                    color = self._pixel_color(x)
                    self.framebuffer[y][x] = self.ntsc_to_rgb(color)

            self.clock += 1
            if self.clock >= 228:
                self.clock = 0
                self.scanline += 1
                self.wsync = False
                if self.scanline >= 262:
                    self.scanline = 0
                    self.frame_ready = True
    
    def ntsc_to_rgb(self, color_byte):
        hue = (color_byte >> 4) & 0x0F
        lum = (color_byte >> 1) & 0x07
        br, bg, bb = self.NTSC_HUES[hue]
        if hue == 0:
            g = lum * 32
            return (g, g, g)
        # Schaal helderheid (lum 0..7) rond het midden (lum 3-4)
        scale = (lum + 1) / 5.0
        r = min(255, int(br * scale))
        g = min(255, int(bg * scale))
        b = min(255, int(bb * scale))
        return (r, g, b)
