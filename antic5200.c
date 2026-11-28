#include "antic5200.h"
#include "gtia5200.h"
#include <stdio.h>
#include <string.h>

static const uint8_t mode_heights[16] =
    {1, 1, 8, 10, 8, 16, 8, 16, 8, 4, 4, 2, 1, 2, 1, 1};

static bool visible_scanline(const Antic5200 *antic) {
    return antic->scanline >= GTIA5200_FIRST_SCANLINE &&
           antic->scanline < GTIA5200_FIRST_SCANLINE + GTIA5200_HEIGHT;
}

void antic5200_init(Antic5200 *antic) {
    memset(antic, 0, sizeof(*antic));
    antic->nmist = 0x1f;
}

uint8_t antic5200_read(void *context, uint16_t address) {
    Antic5200 *antic = context;
    switch (address & 0x0f) {
        case 0x0b: return (uint8_t)(antic->scanline >> 1);
        case 0x0f: return antic->nmist;
        default: return 0xff;
    }
}

void antic5200_write(void *context, uint16_t address, uint8_t value) {
    Antic5200 *antic = context;
    switch (address & 0x0f) {
        case 0x00:
            antic->dmactl = value;
            if (!(value & 3)) antic->playfield_dma_pending = false;
            if (!(value & 0x20)) {
                antic->dma_pending = false;
                antic->dma_stage = 0;
            }
            break;
        case 0x01: antic->chactl = value; break;
        case 0x02: antic->dlist = (uint16_t)((antic->dlist & 0xff00u) | value); break;
        case 0x03: antic->dlist = (uint16_t)((antic->dlist & 0x00ffu) | ((uint16_t)value << 8)); break;
        case 0x04: antic->hscrol = value & 0x0f; break;
        case 0x05: antic->vscrol = value & 0x0f; break;
        case 0x07: antic->pmbase = value; break;
        case 0x09: antic->chbase = value; break;
        case 0x0a: antic->wsync = true; break;
        case 0x0e: antic->nmien = value; break;
        case 0x0f: antic->nmist = 0x1f; break;
        default: break;
    }
}

static void request_pm(Antic5200 *antic, unsigned slot) {
    uint8_t mask = (uint8_t)(1u << slot);
    if (antic->pm_pending & mask) {
        fprintf(stderr, "ANTIC player/missile DMA overrun\n");
        antic->unsupported = true;
        return;
    }
    bool single = (antic->dmactl & 0x10) != 0;
    unsigned base = ((unsigned)antic->pmbase & (single ? 0xf8u : 0xfcu)) << 8;
    unsigned stride = single ? 256 : 128;
    unsigned line = single ? antic->scanline : antic->scanline >> 1;
    antic->pm_address[slot] = (uint16_t)(base + (slot + 3) * stride + line);
    antic->pm_scanline[slot] = antic->scanline;
    antic->pm_pending |= mask;
}

static void request_playfield(Antic5200 *antic, unsigned column, bool character) {
    if (antic->playfield_dma_pending) {
        fprintf(stderr, "ANTIC playfield DMA overrun\n");
        antic->unsupported = true;
        antic->playfield_dma_pending = false;
        return;
    }
    antic->playfield_column = (uint8_t)column;
    antic->character_dma = character;
    antic->playfield_dma_pending = true;
    antic->playfield_dma_virtual = antic->cycle >= 106;
}

static unsigned fetch_width(const Antic5200 *antic) {
    unsigned width = antic->dmactl & 3;
    if (width && width < 3 && (antic->instruction & 0x10)) width++;
    return width;
}

static void render_window(const Antic5200 *antic, int *origin, unsigned *left, unsigned *right) {
    unsigned width = antic->dmactl & 3;
    unsigned fetch = fetch_width(antic);
    *origin = fetch == 1 ? 32 : fetch == 2 ? 0 : -32;
    if (antic->instruction & 0x10) *origin += 2 * antic->hscrol;
    *left = width == 1 ? 32 : 0;
    *right = width == 1 ? 288 : 320;
}

void antic5200_tick(Antic5200 *antic) {
    antic->cycle++;
    if (antic->cycle == 1) antic->vscrol_start = antic->vscrol;
    if (antic->cycle == 6) antic->vscrol_dli = antic->vscrol;
    if (antic->cycle == 1 && visible_scanline(antic) && antic->gtia)
        gtia5200_begin_line(antic->gtia, antic->scanline - GTIA5200_FIRST_SCANLINE);
    if (antic->cycle == 105 && visible_scanline(antic) && antic->gtia)
        gtia5200_end_line(antic->gtia, antic->scanline - GTIA5200_FIRST_SCANLINE);
    if (antic->cycle == 106) antic->wsync = false;
    if (antic->cycle == 7 && antic->scanline >= 8 && antic->scanline < 248 &&
        (antic->instruction & 0x80) &&
        (antic->scroll_ending ? antic->mode_row == antic->vscrol_dli :
         antic->lines_left == 1 || antic->wait_vblank) && !antic->unsupported) {
        antic->dli_this_line = true;
        antic->nmist = 0x9f;
    }
    if (antic->scanline == 248 && antic->cycle == 7) antic->nmist = 0x5f;
    if (antic->cycle == 8 && antic->dli_this_line && (antic->nmien & 0x80) && antic->cpu) {
        cpu_set_nmi(antic->cpu, true);
        cpu_set_nmi(antic->cpu, false);
    }
    if (antic->scanline == 248 && antic->cycle == 8 && (antic->nmien & 0x40) && antic->cpu) {
        cpu_set_nmi(antic->cpu, true);
        cpu_set_nmi(antic->cpu, false);
    }
    if (antic->cycle == 1 && antic->scanline >= 8 && antic->scanline < 248 &&
        !antic->lines_left && !antic->wait_vblank && !antic->unsupported &&
        antic->dlist_dma_latched) {
        antic->dma_stage = 0;
        antic->dma_pending = true;
    }
    if ((antic->cycle == 6 && antic->dma_stage == 1) ||
        (antic->cycle == 7 && antic->dma_stage == 2)) antic->dma_pending = true;
    if (antic->cycle == 109 && antic->lines_left) {
        unsigned last = antic->scroll_ending ? antic->vscrol : antic->mode_last_row;
        antic->mode_line_end = antic->mode_row == last;
    }
    if (antic->scanline >= 8 && antic->scanline < 248 &&
        antic->cycle >= 2 && antic->cycle <= 5 && (antic->dmactl & 8))
        request_pm(antic, antic->cycle - 1);
    uint8_t mode = antic->instruction & 0x0f;
    unsigned width = fetch_width(antic);
    unsigned delay = (antic->instruction & 0x10) ? antic->hscrol / 2 : 0;
    unsigned start = (width == 1 ? 28 : width == 2 ? 20 : 12) + delay;
    if (antic->cycle >= start && antic->scanline >= 8 && antic->scanline < 248 &&
        antic->lines_left && !antic->wait_vblank && !antic->unsupported &&
        mode >= 8 && width) {
        unsigned interval = mode <= 9 ? 8 : mode <= 12 ? 4 : 2;
        unsigned offset = antic->cycle - start;
        if (offset < (width == 1 ? 64u : width == 2 ? 80u : 96u) && offset % interval == 0) {
            unsigned column = offset / interval;
            if (antic->first_mode_line) {
                request_playfield(antic, column, false);
            } else if (antic->gtia && visible_scanline(antic)) {
                int origin;
                unsigned left, right;
                render_window(antic, &origin, &left, &right);
                gtia5200_bitmap_window(antic->gtia, antic->scanline - GTIA5200_FIRST_SCANLINE, column,
                                       mode, origin, left, right, antic->line_buffer[column]);
            }
        }
    }
    if (mode >= 2 && mode <= 7 && width && antic->scanline >= 8 && antic->scanline < 248 &&
        antic->lines_left && !antic->wait_vblank && !antic->unsupported) {
        unsigned interval = mode < 6 ? 2 : 4;
        unsigned name_start = (width == 1 ? 26 : width == 2 ? 18 : 10) + delay;
        unsigned span = width == 1 ? 64 : width == 2 ? 80 : 96;
        if (antic->cycle >= name_start && antic->cycle < name_start + span &&
            (antic->cycle - name_start) % interval == 0 &&
            antic->first_mode_line)
            request_playfield(antic, (antic->cycle - name_start) / interval, false);
        if (antic->cycle >= name_start + 3 && antic->cycle < name_start + 3 + span &&
            (antic->cycle - name_start - 3) % interval == 0 && !antic->unsupported)
            request_playfield(antic, (antic->cycle - name_start - 3) / interval, true);
    }
    if (antic->cycle == 113) antic->dlist_dma_latched = (antic->dmactl & 0x20) != 0;
    if (antic->cycle == 114) {
        antic->cycle = 0;
        antic->dli_this_line = false;
        if (antic->lines_left) {
            antic->first_mode_line = false;
            if (antic->mode_line_end) {
                antic->lines_left = 0;
                if (antic->wait_vblank) antic->scroll_ending = false;
            } else {
                antic->mode_row = (antic->mode_row + 1) & 0x0f;
                unsigned last = antic->scroll_ending ? antic->vscrol : antic->mode_last_row;
                antic->lines_left = (uint8_t)(((last - antic->mode_row) & 0x0f) + 1);
                if ((antic->instruction & 0x0f) == 1 && !antic->wait_vblank)
                    antic->dma_stage = 1;
            }
        }
        antic->scanline++;
        if (antic->scanline == 262) {
            antic->scanline = 0;
            antic->wait_vblank = false;
            antic->lines_left = 0;
            antic->unsupported = false;
        }
        if (antic->scanline >= 8 && antic->scanline < 248 && (antic->dmactl & 0x0c))
            request_pm(antic, 0);
    }
}

void antic5200_pm_byte(Antic5200 *antic, unsigned slot, uint8_t value) {
    if (slot >= 5 || !(antic->pm_pending & (1u << slot))) return;
    if (antic->gtia) {
        if (slot == 0) gtia5200_dma_missiles(antic->gtia, value, antic->pm_scanline[slot]);
        else gtia5200_dma_player(antic->gtia, slot - 1, value, antic->pm_scanline[slot]);
    }
    antic->pm_pending &= (uint8_t)~(1u << slot);
}

void antic5200_dma_byte(Antic5200 *antic, uint8_t value) {
    antic->dlist = (uint16_t)((antic->dlist & 0xfc00u) | ((antic->dlist + 1u) & 0x03ffu));
    if (antic->dma_stage == 0) {
        antic->instruction = value;
        unsigned mode = value & 0x0f;
        bool scrolling = mode >= 2 && (value & 0x20);
        bool entering = scrolling && !antic->scroll_active;
        antic->scroll_ending = !scrolling && antic->scroll_active;
        antic->scroll_active = scrolling;
        antic->first_mode_line = true;
        antic->mode_row = entering ? antic->vscrol_start : 0;
        antic->mode_line_end = false;
        if ((value & 0x0f) == 0) {
            antic->mode_last_row = (value >> 4) & 7;
            antic->dma_pending = false;
        } else if ((value & 0x0f) == 1) {
            antic->dma_stage = 1;
            antic->mode_last_row = 0;
            antic->dma_pending = antic->cycle >= 6;
        } else {
            antic->mode_last_row = mode_heights[mode] - 1;
            antic->dma_stage = (value & 0x40) ? 1 : 0;
            antic->dma_pending = antic->dma_stage && antic->cycle >= 6;
        }
        unsigned last = antic->scroll_ending ? antic->vscrol : antic->mode_last_row;
        antic->lines_left = (uint8_t)(((last - antic->mode_row) & 0x0f) + 1);

    } else if (antic->dma_stage == 1) {
        antic->jump_low = value;
        antic->dma_stage = 2;
        antic->dma_pending = antic->cycle >= 7;
    } else {
        uint16_t target = (uint16_t)(antic->jump_low | ((uint16_t)value << 8));
        if ((antic->instruction & 0x0f) == 1) {
            antic->dlist = target;
            antic->wait_vblank = (antic->instruction & 0x40) != 0;
        } else {
            antic->screen_address = target;
        }

        antic->dma_stage = 0;
        antic->dma_pending = false;
    }
}

uint16_t antic5200_playfield_address(const Antic5200 *antic) {
    if (!antic->character_dma) return antic->screen_address;
    unsigned mode = antic->instruction & 0x0f;
    unsigned row = antic->mode_row;
    if (mode == 5 || mode == 7) row /= 2;
    row &= 7;
    if (antic->chactl & 4) row ^= 7;
    uint8_t name = antic->line_buffer[antic->playfield_column];
    unsigned base = ((unsigned)antic->chbase & (mode < 6 ? 0xfcu : 0xfeu)) << 8;
    return (uint16_t)(base | ((name & (mode < 6 ? 0x7fu : 0x3fu)) << 3) | row);
}

void antic5200_playfield_byte(Antic5200 *antic, uint8_t value) {
    uint8_t mode = antic->instruction & 0x0f;
    if (antic->character_dma) {
        uint8_t name = antic->line_buffer[antic->playfield_column];
        unsigned row = antic->mode_row;
        if (mode <= 3 && ((name & 0x60) == 0x60 ? row < 2 && mode == 3 :
                          row >= 8 && row < 10)) value = 0;
        if (mode <= 3 && (name & 0x80)) {
            if (antic->chactl & 1) value = 0;
            if (antic->chactl & 2) value ^= 0xff;
        }
        if (antic->gtia && visible_scanline(antic)) {
            int origin;
            unsigned left, right;
            render_window(antic, &origin, &left, &right);
            gtia5200_character_window(antic->gtia, antic->scanline - GTIA5200_FIRST_SCANLINE, antic->playfield_column,
                                      mode, origin, left, right, name, value);
        }
        antic->playfield_dma_pending = false;
        return;
    }
    antic->line_buffer[antic->playfield_column] = value;
    if (mode >= 8 && antic->gtia && visible_scanline(antic)) {
        int origin;
        unsigned left, right;
        render_window(antic, &origin, &left, &right);
        gtia5200_bitmap_window(antic->gtia, antic->scanline - GTIA5200_FIRST_SCANLINE, antic->playfield_column,
                               mode, origin, left, right, value);
    }
    antic->screen_address = (uint16_t)((antic->screen_address & 0xf000u) |
                                       ((antic->screen_address + 1u) & 0x0fffu));
    antic->playfield_dma_pending = false;
}
