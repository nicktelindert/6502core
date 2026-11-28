#include "tia.h"
#include "audio.h"
#include <string.h>
#include <stdlib.h>


/* Optionele audio-hooks (koppel aan jouw TIAAudio in C) */
extern void tia_audio_tick_half_scanline(TIAAudio *audio);
extern void tia_audio_flush_frame(TIAAudio *audio);

static const uint8_t NTSC_HUES[16][3] = {
    {0, 0, 0},       /* 0: Zwart / grijs */
    {212, 168, 32},  /* 1: Goud / geel */
    {212, 128, 24},  /* 2: Oranje-geel */
    {198, 88, 24},   /* 3: Oranje */
    {184, 56, 48},   /* 4: Rood-oranje */
    {168, 40, 96},   /* 5: Paars-rood */
    {128, 44, 148},  /* 6: Paars */
    {84, 56, 176},   /* 7: Indigo / blauw-paars */
    {48, 80, 196},   /* 8: Blauw */
    {32, 112, 188},  /* 9: Helder blauw */
    {24, 136, 152},  /* A: Cyaan / groenblauw */
    {32, 148, 96},   /* B: Zeegroen */
    {48, 152, 48},   /* C: Groen */
    {96, 144, 32},   /* D: Geelgroen / olijf */
    {144, 128, 24},  /* E: Licht olijf / zand */
    {184, 108, 24}   /* F: Warm bruin / oker */
};


static int mod_160(int val) {
    int r = val % 160;
    return r < 0 ? r + 160 : r;
}

static int hm_offset(uint8_t reg_val) {
    int raw = (reg_val >> 4) & 0x0F;
    int signed_val = (raw >= 8) ? (raw - 16) : raw;
    return -signed_val;
}

static NusizLayout get_nusiz_layout(uint8_t nusiz) {
    switch (nusiz & 0x07u) {
        case 0u: return (NusizLayout){1, {0, 0, 0}, 1};
        case 1u: return (NusizLayout){1, {0, 16, 0}, 2};
        case 2u: return (NusizLayout){1, {0, 32, 0}, 2};
        case 3u: return (NusizLayout){1, {0, 16, 32}, 3};
        case 4u: return (NusizLayout){1, {0, 64, 0}, 2};
        case 5u: return (NusizLayout){2, {0, 0, 0}, 1};
        case 6u: return (NusizLayout){1, {0, 32, 64}, 3};
        case 7u: return (NusizLayout){4, {0, 0, 0}, 1};
        default: return (NusizLayout){1, {0, 0, 0}, 1};
    }
}

static bool missile_pixel(int x, int position, int width, NusizLayout layout) {
    for (int i = 0; i < layout.num_offsets; i++) {
        int dx = x - (position + layout.offsets[i]);
        if (dx >= 0 && dx < width) {
            return true;
        }
    }
    return false;
}

static void compute_ntsc_rgb(uint8_t color_byte, uint8_t out_rgb[3]) {
    int hue = (color_byte >> 4) & 0x0F;
    int lum = (color_byte >> 1) & 0x07;
    int br = NTSC_HUES[hue][0];
    int bg = NTSC_HUES[hue][1];
    int bb = NTSC_HUES[hue][2];

    if (hue == 0) {
        uint8_t g = (uint8_t)(lum * 32);
        out_rgb[0] = g;
        out_rgb[1] = g;
        out_rgb[2] = g;
        return;
    }

    double scale = (double)(lum + 1) / 5.0;
    int r = (int)(br * scale);
    int g = (int)(bg * scale);
    int b = (int)(bb * scale);
    out_rgb[0] = (uint8_t)(r > 255 ? 255 : (r < 0 ? 0 : r));
    out_rgb[1] = (uint8_t)(g > 255 ? 255 : (g < 0 ? 0 : g));
    out_rgb[2] = (uint8_t)(b > 255 ? 255 : (b < 0 ? 0 : b));
}

static void compute_pal_rgb(uint8_t color_byte, uint8_t out_rgb[3]) {
    static const uint8_t pal_hue_to_ntsc[16] = {
        0, 0, 1, 13, 3, 12, 4, 11,
        5, 10, 6, 9, 7, 8, 0, 0
    };
    uint8_t hue = pal_hue_to_ntsc[color_byte >> 4];
    compute_ntsc_rgb((uint8_t)((hue << 4) | (color_byte & 0x0Fu)), out_rgb);
}

void tia_set_video_standard(TIA *tia, VideoStandard standard) {
    for (int c = 0; c < 256; c++) {
        if (standard == VIDEO_PAL) {
            compute_pal_rgb((uint8_t)c, tia->color_lut[c]);
        } else {
            compute_ntsc_rgb((uint8_t)c, tia->color_lut[c]);
        }
    }
}

void tia_init(TIA *tia, TIAAudio *audio) {
    memset(tia, 0, sizeof(*tia));
    tia->framebuffer_capacity = 0;
    tia->framebuffer = NULL;
    tia->frame_height = 0;
    tia->completed_frame_height = 0;
    tia->scanline = 0;
    tia->frame_ready = false;    
    tia->frame_sync_pending = false;
    tia->audio = audio;
    tia->frame_p0_top_y = -1;
    tia->inpt4 = 0x80u;
    tia->inpt5 = 0x80u;
    tia->width = TIA_FB_WIDTH;
    tia->height = TIA_FB_HEIGHT;
    tia->clock = 0;
    tia->y = 0;
    tia->visible_top_line = -1;

    tia_set_video_standard(tia, VIDEO_NTSC);
}

uint8_t tia_read(TIA *tia, uint16_t addr) {
    uint8_t reg = (uint8_t)(addr & 0x0Fu);
    switch (reg) {
        case 0x00u: return tia->cxm0p;
        case 0x01u: return tia->cxm1p;
        case 0x02u: return tia->cxp0fb;
        case 0x03u: return tia->cxp1fb;
        case 0x04u: return tia->cxm0fb;
        case 0x05u: return tia->cxm1fb;
        case 0x06u: return tia->cxblpf;
        case 0x07u: return tia->cxppmm;
        case 0x0Cu: return tia->inpt4;
        case 0x0Du: return tia->inpt5;
        default:    return 0;
    }
}

void tia_write(TIA *tia, uint16_t addr, uint8_t val) {
    uint8_t reg = (uint8_t)(addr & 0x3Fu);
    uint8_t v = val;


    switch (reg) {
        case 0x00u: { /* VSYNC */
            bool new_vsync_on = (v & 0x02u) != 0;
            if (new_vsync_on && !tia->vsync_on) {
                tia->frame_sync_pending = true;
                tia->visible_top_line = -1;
            }
            tia->vsync = v;
            tia->vsync_on = new_vsync_on;
            break;
        }
        case 0x01u: { /* VBLANK */
            bool was_vblank = (tia->vblank & 0x02u) != 0;
            bool now_vblank = (v & 0x02u) != 0;

            tia->vblank = v;
            if (was_vblank && !now_vblank && tia->visible_top_line < 0) {
                tia->visible_top_line = tia->scanline;
            }
            break;
        }
        case 0x02u: /* WSYNC */
            if (tia->clock != 0) {
                tia->wsync = true;
            }
            break;
        case 0x04u: /* NUSIZ0 */
            tia->nusiz0 = v;
            break;
        case 0x05u: /* NUSIZ1 */
            tia->nusiz1 = v;
            break;
        case 0x06u: /* COLUP0 */
            tia->colup0 = v;
            break;
        case 0x07u: /* COLUP1 */
            tia->colup1 = v;
            break;
        case 0x08u: /* COLUPF */
            tia->colupf = v;
            break;
        case 0x09u: /* COLUBK */
            tia->colubk = v;
            break;
        case 0x0Au: /* CTRLPF */
            tia->ctrlpf = v;
            break;
        case 0x0Bu: /* REFP0 */
            tia->refp0 = v;
            break;
        case 0x0Cu: /* REFP1 */
            tia->refp1 = v;
            break;
        case 0x0Du: /* PF0 */
            tia->pf0 = v;
            break;
        case 0x0Eu: /* PF1 */
            tia->pf1 = v;
            break;
        case 0x0Fu: /* PF2 */
            tia->pf2 = v;
            break;
        case 0x10u: { /* RESP0 */
            NusizLayout lay = get_nusiz_layout(tia->nusiz0);
            int wide_delay = (lay.scale > 1) ? 1 : 0;
            tia->p0_x = (tia->clock >= 68) ? mod_160((tia->clock - 68) + 5 + wide_delay) : (3 + wide_delay);
            if (tia->resmp0) {
                tia->m0_x = mod_160(tia->p0_x + 4 * lay.scale);
            }
            break;
        }
        case 0x11u: { /* RESP1 */
            NusizLayout lay = get_nusiz_layout(tia->nusiz1);
            int wide_delay = (lay.scale > 1) ? 1 : 0;
            tia->p1_x = (tia->clock >= 68) ? mod_160((tia->clock - 68) + 5 + wide_delay) : (3 + wide_delay);
            if (tia->resmp1) {
                tia->m1_x = mod_160(tia->p1_x + 4 * lay.scale);
            }
            break;
        }
        case 0x12u: /* RESM0 */
            tia->m0_x = (tia->clock >= 68) ? mod_160((tia->clock - 68) + 4) : 2;
            break;
        case 0x13u: /* RESM1 */
            tia->m1_x = (tia->clock >= 68) ? mod_160((tia->clock - 68) + 4) : 2;
            break;
        case 0x14u: /* RESBL */
            tia->bl_x = (tia->clock >= 68) ? mod_160((tia->clock - 68) + 4) : 2;
            break;
        case 0x1Bu: /* GRP0 */
            tia->grp0 = v;
            tia->grp1_old = tia->grp1;
            break;
        case 0x1Cu: /* GRP1 */
            tia->grp1 = v;
            tia->grp0_old = tia->grp0;
            tia->enabl_old = tia->enabl;
            break;
        case 0x1Du: /* ENAM0 */
            tia->enam0 = v;
            break;
        case 0x1Eu: /* ENAM1 */
            tia->enam1 = v;
            break;
        case 0x1Fu: /* ENABL */
            tia->enabl = v;
            break;
        case 0x20u: /* HMP0 */
            tia->hmp0 = v;
            break;
        case 0x21u: /* HMP1 */
            tia->hmp1 = v;
            break;
        case 0x22u: /* HMM0 */
            tia->hmm0 = v;
            break;
        case 0x23u: /* HMM1 */
            tia->hmm1 = v;
            break;
        case 0x24u: /* HMBL */
            tia->hmbl = v;
            break;
        case 0x25u: /* VDELP0 */
            tia->vdelp0 = (uint8_t)(v & 0x01u);
            break;
        case 0x26u: /* VDELP1 */
            tia->vdelp1 = (uint8_t)(v & 0x01u);
            break;
        case 0x27u: /* VDELBL */
            tia->vdelbl = (uint8_t)(v & 0x01u);
            break;
        case 0x28u: { /* RESMP0 */
            NusizLayout lay = get_nusiz_layout(tia->nusiz0);
            if (tia->resmp0 || (v & 0x02u)) {
                tia->m0_x = mod_160(tia->p0_x + 4 * lay.scale);
            }
            tia->resmp0 = (uint8_t)(v & 0x02u);
            break;
        }
        case 0x29u: { /* RESMP1 */
            NusizLayout lay = get_nusiz_layout(tia->nusiz1);
            if (tia->resmp1 || (v & 0x02u)) {
                tia->m1_x = mod_160(tia->p1_x + 4 * lay.scale);
            }
            tia->resmp1 = (uint8_t)(v & 0x02u);
            break;
        }
        case 0x2Au: { /* HMOVE */
            NusizLayout lay0 = get_nusiz_layout(tia->nusiz0);
            NusizLayout lay1 = get_nusiz_layout(tia->nusiz1);
            tia->p0_x = mod_160(tia->p0_x + hm_offset(tia->hmp0));
            tia->p1_x = mod_160(tia->p1_x + hm_offset(tia->hmp1));
            if (tia->resmp0) {
                tia->m0_x = mod_160(tia->p0_x + 4 * lay0.scale);
            } else {
                tia->m0_x = mod_160(tia->m0_x + hm_offset(tia->hmm0));
            }
            if (tia->resmp1) {
                tia->m1_x = mod_160(tia->p1_x + 4 * lay1.scale);
            } else {
                tia->m1_x = mod_160(tia->m1_x + hm_offset(tia->hmm1));
            }
            tia->bl_x = mod_160(tia->bl_x + hm_offset(tia->hmbl));
            break;
        }
        case 0x2Bu: /* HMCLR */
            tia->hmp0 = 0;
            tia->hmp1 = 0;
            tia->hmm0 = 0;
            tia->hmm1 = 0;
            tia->hmbl = 0;
            break;
        case 0x2Cu: /* CXCLR */
            tia->cxm0p = 0;
            tia->cxm1p = 0;
            tia->cxp0fb = 0;
            tia->cxp1fb = 0;
            tia->cxm0fb = 0;
            tia->cxm1fb = 0;
            tia->cxblpf = 0;
            tia->cxppmm = 0;
            break;
        case 0x15u: if (tia->audio) tia->audio->audc[0] = (uint8_t)(v & 0x0Fu); break;
        case 0x16u: if (tia->audio) tia->audio->audc[1] = (uint8_t)(v & 0x0Fu); break;
        case 0x17u: if (tia->audio) tia->audio->audf[0] = (uint8_t)(v & 0x1Fu); break;
        case 0x18u: if (tia->audio) tia->audio->audf[1] = (uint8_t)(v & 0x1Fu); break;
        case 0x19u: if (tia->audio) tia->audio->audv[0] = (uint8_t)(v & 0x0Fu); break;
        case 0x1Au: if (tia->audio) tia->audio->audv[1] = (uint8_t)(v & 0x0Fu); break;    
        default:
            break;
    }
}

static bool tia_reserve_scanlines(TIA *tia, int rows) {
    if (rows <= 0 || (size_t)rows > SIZE_MAX / (TIA_FB_WIDTH * 3u)) {
        return false;
    }

    size_t needed = (size_t)rows * TIA_FB_WIDTH * 3u;
    if (needed <= tia->framebuffer_capacity) {
        return true;
    }

    size_t capacity = tia->framebuffer_capacity
        ? tia->framebuffer_capacity
        : TIA_FB_WIDTH * 3u;

    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2u) {
            capacity = needed;
            break;
        }
        capacity *= 2u;
    }

    uint8_t *buffer = realloc(tia->framebuffer, capacity);
    if (!buffer) {
        return false;
    }

    tia->framebuffer = buffer;
    tia->framebuffer_capacity = capacity;
    return true;
}


static void tia_render_pixel(TIA *tia, int clk) {
    int x = clk - 68;
    if (x < 0 || x >= 160) {
        return;
    }
   
    int y = tia->scanline;

    if (y < 0 || !tia_reserve_scanlines(tia, y + 1)) {
        return;
    }


    if (y + 1 > tia->frame_height) {
        tia->frame_height = y + 1;
    }


    uint8_t *fb = tia->framebuffer;
    
    int idx = (y * TIA_FB_WIDTH + x) * 3;

    if (tia->vblank & 0x02u) {
        fb[idx]     = 0;
        fb[idx + 1] = 0;
        fb[idx + 2] = 0;
        return;
    }

    uint8_t active_grp0 = tia->vdelp0 ? tia->grp0_old : tia->grp0;
    uint8_t active_grp1 = tia->vdelp1 ? tia->grp1_old : tia->grp1;
    uint8_t active_enabl = tia->vdelbl ? tia->enabl_old : tia->enabl;

    bool m0_en = ((tia->enam0 & 0x02u) != 0) && !tia->resmp0;
    bool m1_en = ((tia->enam1 & 0x02u) != 0) && !tia->resmp1;
    bool bl_en = (active_enabl & 0x02u) != 0;

    uint8_t pf0 = tia->pf0;
    uint8_t pf1 = tia->pf1;
    uint8_t pf2 = tia->pf2;
    bool has_pf = (pf0 | pf1 | pf2) != 0;

    uint8_t colup0 = tia->colup0;
    uint8_t colup1 = tia->colup1;
    uint8_t colupf = tia->colupf;
    uint8_t colubk = tia->colubk;

    if (!active_grp0 && !active_grp1 && !m0_en && !m1_en && !bl_en && !has_pf) {
        const uint8_t *bg = tia->color_lut[colubk];
        fb[idx]     = bg[0];
        fb[idx + 1] = bg[1];
        fb[idx + 2] = bg[2];
        return;
    }

    NusizLayout lay0 = get_nusiz_layout(tia->nusiz0);
    NusizLayout lay1 = get_nusiz_layout(tia->nusiz1);

    int p0_scale = lay0.scale;
    int p1_scale = lay1.scale;
    int p0_w = 8 * p0_scale;
    int p1_w = 8 * p1_scale;
    
    bool refp0 = (tia->refp0 & 0x08u) != 0;
    bool refp1 = (tia->refp1 & 0x08u) != 0;

    int m0_w = 1 << ((tia->nusiz0 >> 4) & 0x03);
    int m1_w = 1 << ((tia->nusiz1 >> 4) & 0x03);
    int bl_w = 1 << ((tia->ctrlpf >> 4) & 0x03);

    bool pf_ref = (tia->ctrlpf & 0x01u) != 0;
    bool pf_score = ((tia->ctrlpf & 0x02u) != 0) && ((tia->ctrlpf & 0x04u) == 0);
    bool pf_prio = (tia->ctrlpf & 0x04u) != 0;

    int p0_x = tia->p0_x;
    int p1_x = tia->p1_x;
    int m0_x = tia->m0_x;
    int m1_x = tia->m1_x;
    int bl_x = tia->bl_x;

    uint8_t pf_bits[40];
    memset(pf_bits, 0, sizeof(pf_bits));
    if (has_pf) {
        for (int i = 0; i < 4; i++) {
            if ((pf0 >> (4 + i)) & 1) pf_bits[i] = 1;
        }
        for (int i = 0; i < 8; i++) {
            if ((pf1 >> (7 - i)) & 1) pf_bits[4 + i] = 1;
        }
        for (int i = 0; i < 8; i++) {
            if ((pf2 >> i) & 1) pf_bits[12 + i] = 1;
        }
        if (pf_ref) {
            for (int i = 0; i < 20; i++) pf_bits[39 - i] = pf_bits[i];
        } else {
            for (int i = 0; i < 20; i++) pf_bits[20 + i] = pf_bits[i];
        }
    }

    bool p0_on = false;
    if (active_grp0) {
        for (int k = 0; k < lay0.num_offsets; k++) {
            int dx = x - (p0_x + lay0.offsets[k]);
            if (0 <= dx && dx < p0_w) {
                int px = dx / p0_scale;
                int bit = refp0 ? px : (7 - px);
                if ((active_grp0 >> bit) & 1) {
                    p0_on = true;
                    break;
                }
            }
        }
    }

    bool p1_on = false;
    if (active_grp1) {
        for (int k = 0; k < lay1.num_offsets; k++) {
            int dx = x - (p1_x + lay1.offsets[k]);
            if (0 <= dx && dx < p1_w) {
                int px = dx / p1_scale;
                int bit = refp1 ? px : (7 - px);
                if ((active_grp1 >> bit) & 1) {
                    p1_on = true;
                    break;
                }
            }
        }
    }

    bool m0_on = m0_en && missile_pixel(x, m0_x, m0_w, lay0);
    bool m1_on = m1_en && missile_pixel(x, m1_x, m1_w, lay1);

    bool bl_on = bl_en && (0 <= (x - bl_x) && (x - bl_x) < bl_w);
    bool pf_on = has_pf ? (pf_bits[x >> 2] != 0) : false;

    if (p0_on && tia->frame_p0_top_y < 0) {
        tia->frame_p0_top_y = y;
    }
    

    if (p0_on || p1_on || m0_on || m1_on || bl_on) {
        if (m0_on) {
            if (p1_on) tia->cxm0p |= 0x80u;
            if (p0_on) tia->cxm0p |= 0x40u;
            if (pf_on) tia->cxm0fb |= 0x80u;
            if (bl_on) tia->cxm0fb |= 0x40u;
            if (m1_on) tia->cxppmm |= 0x40u;
        }
        if (m1_on) {
            if (p0_on) tia->cxm1p |= 0x80u;
            if (p1_on) tia->cxm1p |= 0x40u;
            if (pf_on) tia->cxm1fb |= 0x80u;
            if (bl_on) tia->cxm1fb |= 0x40u;
        }
        if (p0_on) {
            if (pf_on) tia->cxp0fb |= 0x80u;
            if (bl_on) tia->cxp0fb |= 0x40u;
            if (p1_on) tia->cxppmm |= 0x80u;
        }
        if (p1_on) {
            if (pf_on) tia->cxp1fb |= 0x80u;
            if (bl_on) tia->cxp1fb |= 0x40u;
        }
        if (bl_on && pf_on) {
            tia->cxblpf |= 0x80u;
        }
    }

    uint8_t pfc = pf_score
    ? ((x < 80) ? colup0 : colup1)
    : colupf;
    uint8_t c;
    if (pf_prio) {
        if (pf_on || bl_on) c = pfc;
        else if (p0_on || m0_on) c = colup0;
        else if (p1_on || m1_on) c = colup1;
        else c = colubk;
    } else {
        if (p0_on || m0_on) c = colup0;
        else if (p1_on || m1_on) c = colup1;
        else if (pf_on || bl_on) c = pfc;
        else c = colubk;
    }

    const uint8_t *rgb = tia->color_lut[c];
    fb[idx]     = rgb[0];
    fb[idx + 1] = rgb[1];
    fb[idx + 2] = rgb[2];
}

void tia_tick_clock(TIA *tia) {
    int clk = tia->clock;

    /* 1. Audio-klok (114 color clocks per halve scanline) */
    tia->audio_clock += 1;
    if (tia->audio_clock >= 114) {
        tia->audio_clock -= 114;
        if (tia->audio) {
            tia_audio_tick_half_scanline(tia->audio);
        }
    }

    /* 2. Zichtbare pixel op color clocks 68..227 */
    if (clk >= 68 && clk < 228) {
        tia_render_pixel(tia, clk);
    }

    /* 3. Color clock vooruitzetten en scanline-/frame-grenzen afhandelen */
    clk += 1;
    if (clk >= 228) {
        clk = 0;

        tia->scanline += 1;
        if (tia->wsync) {
            tia->wsync = false;
        }
        
        if (tia->frame_sync_pending) {
            tia->completed_frame_height = tia->frame_height;
            tia->frame_ready = true;
            if (tia->audio) {
                tia_audio_flush_frame(tia->audio);
            }
            tia->scanline = 0;
            tia->frame_height = 0;
            tia->frame_sync_pending = false;
        }
    }

    tia->clock = clk;
}

void tia_step(TIA *tia, int color_clocks) {
    if (!tia || color_clocks <= 0) {
        return;
    }
    for (int i = 0; i < color_clocks; i++) {
        tia_tick_clock(tia);
    }
}


void tia_destroy(TIA *tia) {
    if (!tia) return;
    free(tia->framebuffer);
    tia->framebuffer = NULL;
    tia->framebuffer_capacity = 0;
}