#include "gtia5200.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

void gtia5200_init(GTIA5200 *gtia) {
    memset(gtia, 0, sizeof(*gtia));
    memset(gtia->trigger, 1, sizeof(gtia->trigger));
    memset(gtia->trigger_latch, 1, sizeof(gtia->trigger_latch));
}

void gtia5200_set_trigger(GTIA5200 *gtia, unsigned port, bool pressed) {
    if (port >= 4) {
        fprintf(stderr, "GTIA trigger requires port 0-3\n");
        return;
    }
    gtia->trigger[port] = pressed ? 0 : 1;
    if (pressed && (gtia->gractl & 4)) gtia->trigger_latch[port] = 0;
}

uint8_t gtia5200_read(void *context, uint16_t address) {
    GTIA5200 *gtia = context;
    if ((address & 0x1f) < 16) return gtia->collisions[address & 0x0f];
    if ((address & 0x1f) < 20) {
        unsigned port = (address & 0x1f) - 16;
        return gtia->trigger[port] & gtia->trigger_latch[port];
    }
    if ((address & 0x1f) == 0x1f) return (uint8_t)(~gtia->consol & 15);
    return 0xff;
}

void gtia5200_write(void *context, uint16_t address, uint8_t value) {
    GTIA5200 *gtia = context;
    unsigned reg = address & 0x1f;
    if (reg < 4) { gtia->hposp[reg] = value; return; }
    if (reg < 8) { gtia->hposm[reg - 4] = value; return; }
    if (reg < 12) { gtia->sizep[reg - 8] = value & 3; return; }
    if (reg >= 13 && reg <= 16) { gtia->grafp[reg - 13] = value; return; }
    if (reg >= 18 && reg <= 21) { gtia->colpm[reg - 18] = value & 0xfe; return; }
    switch (reg) {
        case 0x0c: gtia->sizem = value; break;
        case 0x11: gtia->grafm = value; break;
        case 0x16: gtia->colpf0 = value & 0xfe; break;
        case 0x17: gtia->colpf1 = value & 0xfe; break;
        case 0x18: gtia->colpf2 = value & 0xfe; break;
        case 0x19: gtia->colpf3 = value & 0xfe; break;
        case 0x1a: gtia->colbk = value & 0xfe; break;
        case 0x1b: gtia->prior = value; break;
        case 0x1c: gtia->vdelay = value; break;
        case 0x1d:
            gtia->gractl = value;
            if (!(value & 4)) memset(gtia->trigger_latch, 1, sizeof(gtia->trigger_latch));
            else for (unsigned i = 0; i < 4; i++) gtia->trigger_latch[i] &= gtia->trigger[i];
            break;
        case 0x1e: memset(gtia->collisions, 0, sizeof(gtia->collisions)); break;
        case 0x1f: gtia->consol = value & 15; break;
        default: break;
    }
}

void gtia5200_dma_player(GTIA5200 *gtia, unsigned player, uint8_t value, unsigned scanline) {
    if (player < 4 && (gtia->gractl & 2) &&
        ((scanline & 1) || !(gtia->vdelay & (0x10u << player))))
        gtia->grafp[player] = value;
}

void gtia5200_dma_missiles(GTIA5200 *gtia, uint8_t value, unsigned scanline) {
    if (!(gtia->gractl & 1)) return;
    for (unsigned i = 0; i < 4; i++) {
        uint8_t mask = (uint8_t)(3u << (2 * i));
        if ((scanline & 1) || !(gtia->vdelay & (1u << i)))
            gtia->grafm = (uint8_t)((gtia->grafm & ~mask) | (value & mask));
    }
}

static void playfield_pixels(GTIA5200 *gtia, unsigned y, int x, unsigned width,
                             unsigned left, unsigned right,
                             uint8_t color, uint8_t pf, uint8_t collision) {
    int start = x < (int)left ? (int)left : x;
    int end = x + (int)width;
    if (end > (int)right) end = (int)right;
    if (start >= end) return;
    memset(&gtia->framebuffer[y][start], color, (size_t)(end - start));
    memset(&gtia->playfield[start], pf, (size_t)(end - start));
    memset(&gtia->playfield_collision[start], collision, (size_t)(end - start));
}

void gtia5200_character_byte(GTIA5200 *gtia, unsigned y, unsigned column,
                             uint8_t mode, uint8_t width, uint8_t name, uint8_t bits) {
    if (y >= GTIA5200_HEIGHT || mode < 2 || mode > 7 || (width != 1 && width != 2)) return;
    unsigned columns = mode < 6 ? 40 : 20;
    if (column >= (width == 1 ? columns * 4 / 5 : columns)) return;
    gtia5200_character_window(gtia, y, column, mode, width == 1 ? 32 : 0,
                              width == 1 ? 32 : 0, width == 1 ? 288 : 320, name, bits);
}

void gtia5200_character_window(GTIA5200 *gtia, unsigned y, unsigned column,
                               uint8_t mode, int origin, unsigned left, unsigned right,
                               uint8_t name, uint8_t bits) {
    if (y >= GTIA5200_HEIGHT || mode < 2 || mode > 7 || column >= (mode < 6 ? 48u : 24u) ||
        left > right || right > GTIA5200_WIDTH) return;
    if (mode <= 3) {
        gtia5200_bitmap_window(gtia, y, column, 15, origin, left, right, bits);
        return;
    }
    int x = origin + (int)(column * (mode < 6 ? 8 : 16));
    uint8_t colors[] = {gtia->colpf0, gtia->colpf1, gtia->colpf2, gtia->colpf3};
    if (mode <= 5) {
        for (unsigned i = 0; i < 4; i++) {
            unsigned value = (bits >> (6 - 2 * i)) & 3;
            uint8_t color = value ? colors[value == 3 && (name & 0x80) ? 3 : value - 1] : gtia->colbk;
            unsigned pf = value ? (value == 3 && (name & 0x80) ? 3 : value - 1) : 0;
            playfield_pixels(gtia, y, x + (int)(2 * i), 2, left, right, color,
                             value ? (uint8_t)(1u << pf) : 0, value ? (uint8_t)(1u << pf) : 0);
        }
    } else {
        for (unsigned i = 0; i < 8; i++) {
            bool set = (bits & (0x80u >> i)) != 0;
            uint8_t pf = set ? (uint8_t)(1u << (name >> 6)) : 0;
            playfield_pixels(gtia, y, x + (int)(2 * i), 2, left, right,
                             set ? colors[name >> 6] : gtia->colbk, pf, pf);
        }
    }
}

void gtia5200_bitmap_byte(GTIA5200 *gtia, unsigned y, unsigned column,
                          uint8_t mode, uint8_t width, uint8_t bits) {
    if (y >= GTIA5200_HEIGHT || mode < 8 || mode > 15 || (width != 1 && width != 2)) return;
    unsigned normal_bytes = mode <= 9 ? 10 : mode <= 12 ? 20 : 40;
    unsigned bytes = width == 1 ? normal_bytes * 4 / 5 : normal_bytes;
    if (column >= bytes) return;
    gtia5200_bitmap_window(gtia, y, column, mode, width == 1 ? 32 : 0,
                           width == 1 ? 32 : 0, width == 1 ? 288 : 320, bits);
}

void gtia5200_bitmap_window(GTIA5200 *gtia, unsigned y, unsigned column,
                            uint8_t mode, int origin, unsigned left, unsigned right, uint8_t bits) {
    if (y >= GTIA5200_HEIGHT || mode < 8 || mode > 15 ||
        left > right || right > GTIA5200_WIDTH) return;
    unsigned normal_bytes = mode <= 9 ? 10 : mode <= 12 ? 20 : 40;
    if (column >= normal_bytes * 6 / 5) return;
    bool two_bits = mode == 8 || mode == 10 || mode == 13 || mode == 14;
    unsigned count = two_bits ? 4 : 8;
    unsigned pixel_width = GTIA5200_WIDTH / normal_bytes / count;
    const uint8_t colors[] = {gtia->colbk, gtia->colpf0, gtia->colpf1, gtia->colpf2};
    for (unsigned i = 0; i < count; i++) {
        unsigned value = two_bits ? (bits >> (6 - 2 * i)) & 3 : (bits >> (7 - i)) & 1;
        uint8_t color = colors[value];
        if (mode == 15) {
            uint8_t luminance = value ? gtia->colpf1 : gtia->colpf2;
            color = (uint8_t)((gtia->colpf2 & 0xf0) | (luminance & 0x0e));
        }
        uint8_t pf = mode == 15 ? 4 : value ? (uint8_t)(1u << (value - 1)) : 0;
        uint8_t collision = mode == 15 ? (value ? 4 : 0) : pf;
        int x = origin + (int)(column * (GTIA5200_WIDTH / normal_bytes) + i * pixel_width);
        playfield_pixels(gtia, y, x, pixel_width, left, right, color, pf, collision);
    }
}

void gtia5200_begin_line(GTIA5200 *gtia, unsigned y) {
    if (y < GTIA5200_HEIGHT) {
        memset(gtia->framebuffer[y], gtia->colbk, GTIA5200_WIDTH);
        memset(gtia->playfield, 0, sizeof(gtia->playfield));
        memset(gtia->playfield_collision, 0, sizeof(gtia->playfield_collision));
    }
}

static unsigned sprite_width(unsigned size) {
    static const unsigned widths[] = {2, 4, 2, 8};
    return widths[size & 3];
}

static bool sprite_pixel(unsigned x, uint8_t position, unsigned width, uint8_t bits, unsigned count) {
    int relative = (int)x - ((int)position - 0x30) * 2;
    if (relative < 0 || (unsigned)relative >= width * count) return false;
    return (bits & (1u << (count - 1 - (unsigned)relative / width))) != 0;
}

void gtia5200_end_line(GTIA5200 *gtia, unsigned y) {
    if (y >= GTIA5200_HEIGHT) return;
    for (unsigned x = 0; x < GTIA5200_WIDTH; x++) {
        uint8_t players = 0, missiles = 0;
        for (unsigned i = 0; i < 4; i++) {
            if (sprite_pixel(x, gtia->hposp[i], sprite_width(gtia->sizep[i]), gtia->grafp[i], 8))
                players |= (uint8_t)(1u << i);
            if (sprite_pixel(x, gtia->hposm[i], sprite_width(gtia->sizem >> (2 * i)),
                             (uint8_t)((gtia->grafm >> (2 * i)) & 3), 2))
                missiles |= (uint8_t)(1u << i);
        }
        uint8_t pf = gtia->playfield[x];
        uint8_t pf_collision = gtia->playfield_collision[x];
        if (pf == 4) pf_collision |= gtia->playfield_collision[x ^ 1u];
        for (unsigned i = 0; i < 4; i++) {
            if (players & (1u << i)) {
                gtia->collisions[4 + i] |= pf_collision;
                gtia->collisions[12 + i] |= players & (uint8_t)~(1u << i);
            }
            if (missiles & (1u << i)) {
                gtia->collisions[i] |= pf_collision;
                gtia->collisions[8 + i] |= players;
            }
        }
        uint8_t objects = players | missiles;
        bool p01 = (objects & 3) != 0, p23 = (objects & 12) != 0;
        bool pf01 = (pf & 3) != 0, pf23 = (pf & 12) != 0;
        unsigned priority = gtia->prior & 15;
        bool pri0 = (priority & 1) != 0, pri1 = (priority & 2) != 0;
        bool pri2 = (priority & 4) != 0, pri3 = (priority & 8) != 0;
        bool show0 = !(pf01 && (pri2 || pri3)) && !(pri2 && pf23);
        bool show2 = !p01 && !(pf23 && (pri1 || pri2)) && !(pf01 && !pri0);
        uint8_t color = 0;
        bool has_object = false;
        if ((objects & 1) && show0) { color |= gtia->colpm[0]; has_object = true; }
        if ((objects & 2) && show0 && (!(objects & 1) || (gtia->prior & 0x20))) {
            color |= gtia->colpm[1]; has_object = true;
        }
        if ((objects & 4) && show2) { color |= gtia->colpm[2]; has_object = true; }
        if ((objects & 8) && show2 && (!(objects & 4) || (gtia->prior & 0x20))) {
            color |= gtia->colpm[3]; has_object = true;
        }
        bool show_pf = (pf01 && !(p23 && pri0) && !(p01 && (pri0 || pri1))) ||
                       (pf23 && !(p23 && (pri0 || pri3)) && !(p01 && !pri2));
        if (has_object) gtia->framebuffer[y][x] = color | (show_pf ? gtia->framebuffer[y][x] : 0);
        else if (!show_pf && pf && objects) gtia->framebuffer[y][x] = 0;
    }
}

void gtia5200_mode_f_byte(GTIA5200 *gtia, unsigned y, unsigned column, uint8_t bits) {
    gtia5200_bitmap_byte(gtia, y, column, 15, 2, bits);
}
