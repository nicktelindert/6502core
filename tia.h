#ifndef TIA_H
#define TIA_H

#include <stdint.h>
#include <stdbool.h>

#define TIA_FB_WIDTH  160
#define TIA_FB_HEIGHT 192
#define TIA_FB_SIZE   (TIA_FB_WIDTH * TIA_FB_HEIGHT * 6)

typedef struct TIAAudio TIAAudio;

typedef struct {
    int scale;
    int offsets[3];
    int num_offsets;
} NusizLayout;

typedef struct TIA {
    TIAAudio *audio;
    int audio_clock;
    bool audio_mid_done;
    bool vsync_on;
    bool prev_vsync_on;
    bool start_drawing;

    int frame_p0_top_y;
    int debug_p0_x;
    int debug_p1_x;
    int debug_bl_x;
    int y;

    /* Sync & Control */
    uint8_t vsync;      /* $00 */
    uint8_t vblank;     /* $01 */
    bool wsync;         /* $02 */

    /* Kleuren */
    uint8_t colup0;     /* $06 */
    uint8_t colup1;     /* $07 */
    uint8_t colupf;     /* $08 */
    uint8_t colubk;     /* $09 */

    /* Playfield */
    uint8_t ctrlpf;     /* $0A */
    uint8_t pf0;        /* $0D */
    uint8_t pf1;        /* $0E */
    uint8_t pf2;        /* $0F */

    /* Players */
    uint8_t grp0;       /* $1B */
    uint8_t grp1;       /* $1C */
    uint8_t grp0_old;
    uint8_t grp1_old;
    int p0_x;
    int p1_x;
    uint8_t nusiz0;     /* $04 */
    uint8_t nusiz1;     /* $05 */
    uint8_t refp0;      /* $0B */
    uint8_t refp1;      /* $0C */
    uint8_t vdelp0;     /* $25 */
    uint8_t vdelp1;     /* $26 */

    /* Missiles & Ball */
    uint8_t enam0;      /* $1D */
    uint8_t enam1;      /* $1E */
    uint8_t enabl;      /* $1F */
    uint8_t enabl_old;
    uint8_t vdelbl;     /* $27 */
    uint8_t resmp0;     /* $28 */
    uint8_t resmp1;     /* $29 */
    int m0_x;
    int m1_x;
    int bl_x;

    /* Horizontal Motion */
    uint8_t hmp0;       /* $20 */
    uint8_t hmp1;       /* $21 */
    uint8_t hmm0;       /* $22 */
    uint8_t hmm1;       /* $23 */
    uint8_t hmbl;       /* $24 */

    /* Collision latches */
    uint8_t cxm0p;
    uint8_t cxm1p;
    uint8_t cxp0fb;
    uint8_t cxp1fb;
    uint8_t cxm0fb;
    uint8_t cxm1fb;
    uint8_t cxblpf;
    uint8_t cxppmm;

    /* Input latches */
    uint8_t inpt4;
    uint8_t inpt5;

    /* Beam positie */
    int rendered_clock;
    int clock;          /* 0..227 color clocks per lijn */
    int scanline;       /* 0..261 scanlines per frame */
    bool frame_ready;

    int width;
    int height;
    uint8_t framebuffer[TIA_FB_SIZE];
    uint8_t ntsc_lut[256][3];
} TIA;

void tia_init(TIA *tia, TIAAudio *audio);
uint8_t tia_read(TIA *tia, uint16_t addr);
void tia_write(TIA *tia, uint16_t addr, uint8_t val);
void tia_step(TIA *tia, int color_clocks);

#endif /* TIA_H */