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
        # Sync & Control
        self.vsync = 0      # $00: Vertical Sync Set-Clear
        self.vblank = 0     # $01: Vertical Blank Set-Clear
        self.wsync = False  # $02: Wait for Horizontal Blank

        # Kleuren
        self.colup0 = 0x00  # $06: Player 0 / Missile 0 Color
        self.colup1 = 0x00  # $07: Player 1 / Missile 1 Color
        self.colupf = 0x00  # $08: Playfield / Ball Color
        self.colubk = 0x00  # $09: Background Color

        # Playfield
        self.ctrlpf = 0x00  # $0A: Playfield Control (bit 0=REF, 1=SCORE, 2=PFP, 4..5=BALL size)
        self.pf0 = 0x00     # $0D: Playfield register 0 (bits 4..7)
        self.pf1 = 0x00     # $0E: Playfield register 1 (bits 0..7)
        self.pf2 = 0x00     # $0F: Playfield register 2 (bits 0..7)

        # Players
        self.grp0 = 0x00    # $1B: Player 0 Graphics
        self.grp1 = 0x00    # $1C: Player 1 Graphics
        self.grp0_old = 0x00
        self.grp1_old = 0x00
        self.p0_x = 0       # Horizontale positie (0..159)
        self.p1_x = 0
        self.nusiz0 = 0x00  # $04: Number-Size Player/Missile 0
        self.nusiz1 = 0x00  # $05: Number-Size Player/Missile 1
        self.refp0 = 0x00   # $0B: Reflect Player 0 (bit 3)
        self.refp1 = 0x00   # $0C: Reflect Player 1 (bit 3)
        self.vdelp0 = 0x00  # $25: Vertical Delay Player 0
        self.vdelp1 = 0x00  # $26: Vertical Delay Player 1

        # Missiles & Ball
        self.enam0 = 0x00   # $1D: Enable Missile 0 (bit 1)
        self.enam1 = 0x00   # $1E: Enable Missile 1 (bit 1)
        self.enabl = 0x00   # $1F: Enable Ball (bit 1)
        self.enabl_old = 0x00
        self.vdelbl = 0x00  # $27: Vertical Delay Ball
        self.resmp0 = 0x00  # $28: Reset Missile 0 to Player 0 (bit 1)
        self.resmp1 = 0x00  # $29: Reset Missile 1 to Player 1 (bit 1)
        self.m0_x = 0
        self.m1_x = 0
        self.bl_x = 0

        # Horizontal Motion
        self.hmp0 = 0x00    # $20: Horizontal Motion Player 0
        self.hmp1 = 0x00    # $21: Horizontal Motion Player 1
        self.hmm0 = 0x00    # $22: Horizontal Motion Missile 0
        self.hmm1 = 0x00    # $23: Horizontal Motion Missile 1
        self.hmbl = 0x00    # $24: Horizontal Motion Ball

        # Collision latches ($00..$07 bij read)
        self.cxm0p = 0x00
        self.cxm1p = 0x00
        self.cxp0fb = 0x00
        self.cxp1fb = 0x00
        self.cxm0fb = 0x00
        self.cxm1fb = 0x00
        self.cxblpf = 0x00
        self.cxppmm = 0x00

        # Input latches (bit 7 = 1 betekent niet ingedrukt)
        self.inpt4 = 0x80   # $0C: Player 0 fire button
        self.inpt5 = 0x80   # $0D: Player 1 fire button

        # Beam positie
        self.rendered_clock = 0
        self.clock = 0      # 0..227 color clocks per lijn
        self.scanline = 0   # 0..261 scanlines per frame
        self.frame_ready = False

        # Snelle flat RGB framebuffer (160 * 192 * 3 bytes) + NTSC lookup table
        self.width = 160
        self.height = 192
        self.framebuffer = bytearray(self.width * self.height * 3)
        self.ntsc_lut = [self.ntsc_to_rgb(c) for c in range(256)]

    def _hm_offset(self, reg_val):
        raw = (reg_val >> 4) & 0x0F
        signed = raw - 16 if raw >= 8 else raw
        return -signed

    def read(self, addr):
        reg = addr & 0x0F
        if reg <= 0x07:
            self._render_up_to_clock(self.clock)
            if reg == 0x00:
                return self.cxm0p
            elif reg == 0x01:
                return self.cxm1p
            elif reg == 0x02:
                return self.cxp0fb
            elif reg == 0x03:
                return self.cxp1fb
            elif reg == 0x04:
                return self.cxm0fb
            elif reg == 0x05:
                return self.cxm1fb
            elif reg == 0x06:
                return self.cxblpf
            elif reg == 0x07:
                return self.cxppmm
        elif reg == 0x0C:
            return self.inpt4
        elif reg == 0x0D:
            return self.inpt5
        return 0

    def write(self, addr, val):
        self._render_up_to_clock(self.clock)
        reg = addr & 0x3F
        v = (val or 0) & 0xFF

        if reg == 0x00:    # VSYNC (einde van VSYNC start het nieuwe frame)
            if not (v & 0x02) and (self.vsync & 0x02):
                self.scanline = 0
                self.frame_ready = True
            self.vsync = v
        elif reg == 0x01:  # VBLANK
            self.vblank = v
        elif reg == 0x02:  # WSYNC
            self.wsync = True
        elif reg == 0x04:  # NUSIZ0
            self.nusiz0 = v
        elif reg == 0x05:  # NUSIZ1
            self.nusiz1 = v
        elif reg == 0x06:  # COLUP0
            self.colup0 = v
        elif reg == 0x07:  # COLUP1
            self.colup1 = v
        elif reg == 0x08:  # COLUPF
            self.colupf = v
        elif reg == 0x09:  # COLUBK
            self.colubk = v
        elif reg == 0x0A:  # CTRLPF
            self.ctrlpf = v
        elif reg == 0x0B:  # REFP0
            self.refp0 = v
        elif reg == 0x0C:  # REFP1
            self.refp1 = v
        elif reg == 0x0D:  # PF0
            self.pf0 = v
        elif reg == 0x0E:  # PF1
            self.pf1 = v
        elif reg == 0x0F:  # PF2
            self.pf2 = v
        elif reg == 0x10:  # RESP0
            self.p0_x = ((self.clock - 68) + 5) % 160 if self.clock >= 68 else 3
            if self.resmp0:
                self.m0_x = (self.p0_x + 4) % 160
        elif reg == 0x11:  # RESP1
            self.p1_x = ((self.clock - 68) + 5) % 160 if self.clock >= 68 else 3
            if self.resmp1:
                self.m1_x = (self.p1_x + 4) % 160
        elif reg == 0x12:  # RESM0
            self.m0_x = ((self.clock - 68) + 4) % 160 if self.clock >= 68 else 2
        elif reg == 0x13:  # RESM1
            self.m1_x = ((self.clock - 68) + 4) % 160 if self.clock >= 68 else 2
        elif reg == 0x14:  # RESBL
            self.bl_x = ((self.clock - 68) + 4) % 160 if self.clock >= 68 else 2
        elif reg == 0x1B:  # GRP0
            self.grp0 = v
            self.grp1_old = self.grp1
        elif reg == 0x1C:  # GRP1
            self.grp1 = v
            self.grp0_old = self.grp0
            self.enabl_old = self.enabl
        elif reg == 0x1D:  # ENAM0
            self.enam0 = v
        elif reg == 0x1E:  # ENAM1
            self.enam1 = v
        elif reg == 0x1F:  # ENABL
            self.enabl = v
        elif reg == 0x20:  # HMP0
            self.hmp0 = v
        elif reg == 0x21:  # HMP1
            self.hmp1 = v
        elif reg == 0x22:  # HMM0
            self.hmm0 = v
        elif reg == 0x23:  # HMM1
            self.hmm1 = v
        elif reg == 0x24:  # HMBL
            self.hmbl = v
        elif reg == 0x25:  # VDELP0
            self.vdelp0 = v & 0x01
        elif reg == 0x26:  # VDELP1
            self.vdelp1 = v & 0x01
        elif reg == 0x27:  # VDELBL
            self.vdelbl = v & 0x01
        elif reg == 0x28:  # RESMP0
            if self.resmp0 or (v & 0x02):
                self.m0_x = (self.p0_x + 4) % 160
            self.resmp0 = v & 0x02
        elif reg == 0x29:  # RESMP1
            if self.resmp1 or (v & 0x02):
                self.m1_x = (self.p1_x + 4) % 160
            self.resmp1 = v & 0x02
        elif reg == 0x2A:  # HMOVE
            self.p0_x = (self.p0_x + self._hm_offset(self.hmp0)) % 160
            self.p1_x = (self.p1_x + self._hm_offset(self.hmp1)) % 160
            if self.resmp0:
                self.m0_x = (self.p0_x + 4) % 160
            else:
                self.m0_x = (self.m0_x + self._hm_offset(self.hmm0)) % 160
            if self.resmp1:
                self.m1_x = (self.p1_x + 4) % 160
            else:
                self.m1_x = (self.m1_x + self._hm_offset(self.hmm1)) % 160
            self.bl_x = (self.bl_x + self._hm_offset(self.hmbl)) % 160
        elif reg == 0x2B:  # HMCLR
            self.hmp0 = 0x00
            self.hmp1 = 0x00
            self.hmm0 = 0x00
            self.hmm1 = 0x00
            self.hmbl = 0x00
        elif reg == 0x2C:  # CXCLR: wis alle collision-latches
            self.cxm0p = 0x00
            self.cxm1p = 0x00
            self.cxp0fb = 0x00
            self.cxp1fb = 0x00
            self.cxm0fb = 0x00
            self.cxm1fb = 0x00
            self.cxblpf = 0x00
            self.cxppmm = 0x00

    def _nusiz_layout(self, nusiz):
        mode = nusiz & 0x07
        if mode == 1:
            return 1, (0, 16)
        elif mode == 2:
            return 1, (0, 32)
        elif mode == 3:
            return 1, (0, 16, 32)
        elif mode == 4:
            return 1, (0, 64)
        elif mode == 5:
            return 2, (0,)
        elif mode == 6:
            return 1, (0, 32, 64)
        elif mode == 7:
            return 4, (0,)
        return 1, (0,)

    def _render_up_to_clock(self, target_clock):
        y = self.scanline - 37
        if not (0 <= y < self.height):
            self.rendered_clock = target_clock
            return

        start = self.rendered_clock if self.rendered_clock > 68 else 68
        end = target_clock if target_clock < 228 else 228
        if start >= end:
            self.rendered_clock = target_clock
            return

        fb = self.framebuffer
        row_offset = y * 480  # 160 * 3

        if self.vblank & 0x02:
            for clk in range(start, end):
                idx = row_offset + (clk - 68) * 3
                fb[idx] = 0
                fb[idx + 1] = 0
                fb[idx + 2] = 0
            self.rendered_clock = target_clock
            return

        lut = self.ntsc_lut
        active_grp0 = self.grp0_old if self.vdelp0 else self.grp0
        active_grp1 = self.grp1_old if self.vdelp1 else self.grp1
        active_enabl = self.enabl_old if self.vdelbl else self.enabl

        p0_x = self.p0_x
        p1_x = self.p1_x
        m0_x = self.m0_x
        m1_x = self.m1_x
        bl_x = self.bl_x

        p0_scale, p0_offsets = self._nusiz_layout(self.nusiz0)
        p1_scale, p1_offsets = self._nusiz_layout(self.nusiz1)
        p0_w = 8 * p0_scale
        p1_w = 8 * p1_scale
        refp0 = bool(self.refp0 & 0x08)
        refp1 = bool(self.refp1 & 0x08)

        m0_en = bool(self.enam0 & 0x02) and not self.resmp0
        m1_en = bool(self.enam1 & 0x02) and not self.resmp1
        m0_w = 1 << ((self.nusiz0 >> 4) & 0x03)
        m1_w = 1 << ((self.nusiz1 >> 4) & 0x03)

        bl_en = bool(active_enabl & 0x02)
        bl_w = 1 << ((self.ctrlpf >> 4) & 0x03)

        pf0 = self.pf0
        pf1 = self.pf1
        pf2 = self.pf2
        pf_ref = bool(self.ctrlpf & 0x01)
        pf_score = bool(self.ctrlpf & 0x02) and not (self.ctrlpf & 0x04)
        pf_prio = bool(self.ctrlpf & 0x04)

        colup0 = self.colup0
        colup1 = self.colup1
        colupf = self.colupf
        colubk = self.colubk

        cxm0p = self.cxm0p
        cxm1p = self.cxm1p
        cxp0fb = self.cxp0fb
        cxp1fb = self.cxp1fb
        cxm0fb = self.cxm0fb
        cxm1fb = self.cxm1fb
        cxblpf = self.cxblpf
        cxppmm = self.cxppmm

        for clk in range(start, end):
            x = clk - 68

            # Player 0
            p0_on = False
            if active_grp0:
                for off in p0_offsets:
                    dx = (x - (p0_x + off)) % 160
                    if dx < p0_w:
                        px = dx // p0_scale
                        bit = px if refp0 else (7 - px)
                        if (active_grp0 >> bit) & 0x01:
                            p0_on = True
                            break

            # Player 1
            p1_on = False
            if active_grp1:
                for off in p1_offsets:
                    dx = (x - (p1_x + off)) % 160
                    if dx < p1_w:
                        px = dx // p1_scale
                        bit = px if refp1 else (7 - px)
                        if (active_grp1 >> bit) & 0x01:
                            p1_on = True
                            break

            # Missiles & Ball
            m0_on = False
            if m0_en:
                for off in p0_offsets:
                    if ((x - (m0_x + off)) % 160) < m0_w:
                        m0_on = True
                        break

            m1_on = False
            if m1_en:
                for off in p1_offsets:
                    if ((x - (m1_x + off)) % 160) < m1_w:
                        m1_on = True
                        break

            bl_on = bl_en and (((x - bl_x) % 160) < bl_w)

            # Playfield
            if x < 80:
                pf_x = x >> 2
            else:
                rx = (x - 80) >> 2
                pf_x = (19 - rx) if pf_ref else rx

            if pf_x < 4:
                pf_on = bool((pf0 >> (4 + pf_x)) & 0x01)
            elif pf_x < 12:
                pf_on = bool((pf1 >> (11 - pf_x)) & 0x01)
            else:
                pf_on = bool((pf2 >> (pf_x - 12)) & 0x01)

            # Collisions
            if m0_on:
                if p1_on: cxm0p |= 0x80
                if p0_on: cxm0p |= 0x40
                if pf_on: cxm0fb |= 0x80
                if bl_on: cxm0fb |= 0x40
                if m1_on: cxppmm |= 0x40
            if m1_on:
                if p0_on: cxm1p |= 0x80
                if p1_on: cxm1p |= 0x40
                if pf_on: cxm1fb |= 0x80
                if bl_on: cxm1fb |= 0x40
            if p0_on:
                if pf_on: cxp0fb |= 0x80
                if bl_on: cxp0fb |= 0x40
                if p1_on: cxppmm |= 0x80
            if p1_on:
                if pf_on: cxp1fb |= 0x80
                if bl_on: cxp1fb |= 0x40
            if bl_on and pf_on:
                cxblpf |= 0x80

            # Kleurprioriteit
            pfc = (colup0 if x < 80 else colup1) if pf_score else colupf
            if pf_prio:
                if pf_on or bl_on:
                    c = pfc
                elif p0_on or m0_on:
                    c = colup0
                elif p1_on or m1_on:
                    c = colup1
                else:
                    c = colubk
            else:
                if p0_on or m0_on:
                    c = colup0
                elif p1_on or m1_on:
                    c = colup1
                elif pf_on or bl_on:
                    c = pfc
                else:
                    c = colubk

            r, g, b = lut[c]
            idx = row_offset + x * 3
            fb[idx] = r
            fb[idx + 1] = g
            fb[idx + 2] = b

        self.cxm0p = cxm0p
        self.cxm1p = cxm1p
        self.cxp0fb = cxp0fb
        self.cxp1fb = cxp1fb
        self.cxm0fb = cxm0fb
        self.cxm1fb = cxm1fb
        self.cxblpf = cxblpf
        self.cxppmm = cxppmm
        self.rendered_clock = target_clock

    def step(self, color_clocks):
        """Laat de TIA 'color_clocks' vooruitlopen (3x CPU cycles)."""
        self.clock += color_clocks
        while self.clock >= 228:
            self._render_up_to_clock(228)
            if self.wsync:
                self.clock = 0
                self.wsync = False
            else:
                self.clock -= 228
            self.rendered_clock = 0
            self.scanline += 1
            if self.scanline >= 262:
                self.scanline = 0

    def ntsc_to_rgb(self, color_byte):
        hue = (color_byte >> 4) & 0x0F
        lum = (color_byte >> 1) & 0x07
        br, bg, bb = self.NTSC_HUES[hue]
        if hue == 0:
            g = lum * 32
            return (g, g, g)
        scale = (lum + 1) / 5.0
        r = min(255, int(br * scale))
        g = min(255, int(bg * scale))
        b = min(255, int(bb * scale))
        return (r, g, b)
