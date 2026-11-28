#ifndef GTIA5200_H
#define GTIA5200_H

#include <stdbool.h>
#include <stdint.h>

#define GTIA5200_WIDTH 320
#define GTIA5200_FIRST_SCANLINE 8
#define GTIA5200_HEIGHT 240

typedef struct GTIA5200 {
    uint8_t colpf0, colpf1, colpf2, colpf3, colbk;
    uint8_t hposp[4], hposm[4], sizep[4], sizem, grafp[4], grafm, colpm[4], prior, vdelay, gractl;
    uint8_t collisions[16];
    uint8_t consol;
    uint8_t trigger[4], trigger_latch[4];
    uint8_t playfield[GTIA5200_WIDTH], playfield_collision[GTIA5200_WIDTH];
    uint8_t framebuffer[GTIA5200_HEIGHT][GTIA5200_WIDTH];
} GTIA5200;

void gtia5200_init(GTIA5200 *gtia);
uint8_t gtia5200_read(void *context, uint16_t address);
void gtia5200_write(void *context, uint16_t address, uint8_t value);
void gtia5200_begin_line(GTIA5200 *gtia, unsigned y);
void gtia5200_end_line(GTIA5200 *gtia, unsigned y);
void gtia5200_dma_player(GTIA5200 *gtia, unsigned player, uint8_t value, unsigned scanline);
void gtia5200_dma_missiles(GTIA5200 *gtia, uint8_t value, unsigned scanline);
void gtia5200_set_trigger(GTIA5200 *gtia, unsigned port, bool pressed);
void gtia5200_mode_f_byte(GTIA5200 *gtia, unsigned y, unsigned column, uint8_t bits);
void gtia5200_bitmap_byte(GTIA5200 *gtia, unsigned y, unsigned column,
                          uint8_t mode, uint8_t width, uint8_t bits);
void gtia5200_character_byte(GTIA5200 *gtia, unsigned y, unsigned column,
                             uint8_t mode, uint8_t width, uint8_t name, uint8_t bits);
void gtia5200_bitmap_window(GTIA5200 *gtia, unsigned y, unsigned column,
                            uint8_t mode, int origin, unsigned left, unsigned right, uint8_t bits);
void gtia5200_character_window(GTIA5200 *gtia, unsigned y, unsigned column,
                               uint8_t mode, int origin, unsigned left, unsigned right,
                               uint8_t name, uint8_t bits);

#endif
