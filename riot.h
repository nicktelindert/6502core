#ifndef RIOT_H
#define RIOT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    /* I/O registers */
    uint8_t swcha;       /* $0280: Joystick richtingen (actief laag: 0 = ingedrukt) */
    uint8_t swacnt;      /* $0281: Data Direction Register A */
    uint8_t swchb;       /* $0282: Console switches */
    uint8_t swbcnt;      /* $0283: Data Direction Register B */

    /* Interval timer */
    uint8_t timer;
    uint16_t interval;   /* 1, 8, 64 of 1024 CPU-cycles per tick */
    uint16_t prescaler;
    uint8_t timer_flag;  /* 0x80 wanneer timer is afgelopen */
    bool wrapped_this_cycle;
} RIOT;

void riot_init(RIOT *riot);
void riot_step(RIOT *riot, int cycles);
uint8_t riot_read(RIOT *riot, uint16_t addr);
void riot_write(RIOT *riot, uint16_t addr, uint8_t val);

#endif /* RIOT_H */