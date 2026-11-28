#ifndef ANTIC5200_H
#define ANTIC5200_H

#include "cpu6502.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct GTIA5200 GTIA5200;
typedef struct Antic5200 {
    uint16_t scanline;
    uint8_t cycle;
    uint8_t dmactl, chactl, chbase, pmbase, hscrol, vscrol, nmien, nmist;
    uint16_t dlist;
    uint16_t screen_address;
    uint8_t instruction;
    uint8_t lines_left;
    uint8_t mode_row, mode_last_row, vscrol_start, vscrol_dli;
    bool first_mode_line, scroll_active, scroll_ending;
    bool mode_line_end;
    uint8_t dma_stage;
    uint8_t jump_low;
    bool dma_pending;
    bool wait_vblank;
    bool wsync;
    bool unsupported;
    bool dli_this_line;
    bool dlist_dma_latched;
    bool playfield_dma_pending;
    bool playfield_dma_virtual;
    bool character_dma;
    uint8_t playfield_column;
    uint8_t line_buffer[48];
    uint8_t pm_pending;
    uint16_t pm_address[5], pm_scanline[5];
    GTIA5200 *gtia;
    CPU6502 *cpu;
} Antic5200;

void antic5200_init(Antic5200 *antic);
uint8_t antic5200_read(void *context, uint16_t address);
void antic5200_write(void *context, uint16_t address, uint8_t value);
void antic5200_tick(Antic5200 *antic);
void antic5200_dma_byte(Antic5200 *antic, uint8_t value);
uint16_t antic5200_playfield_address(const Antic5200 *antic);
void antic5200_playfield_byte(Antic5200 *antic, uint8_t value);
void antic5200_pm_byte(Antic5200 *antic, unsigned slot, uint8_t value);

#endif
