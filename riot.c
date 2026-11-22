#include "riot.h"

static void riot_set_timer(RIOT *riot, uint8_t value, uint16_t interval) {
    riot->timer = value;
    riot->interval = interval;
    riot->prescaler = 0;
    riot->after_underflow = false;
    riot->timer_flag = 0x00u;
}

void riot_init(RIOT *riot) {
    riot->swcha = 0xFFu;
    riot->swacnt = 0x00u;
    riot->swchb = 0xFFu;
    riot->swbcnt = 0x00u;

    riot->timer = 0x00u;
    riot->interval = 1u;
    riot->prescaler = 0u;
    riot->after_underflow = false;
    riot->timer_flag = 0x00u;
}

void riot_step(RIOT *riot, int cycles) {
    if (cycles <= 0) {
        return;
    }

    int remaining = cycles;
    uint16_t prescaler = riot->prescaler;
    uint8_t timer = riot->timer;
    uint8_t timer_flag = riot->timer_flag;
    bool after_underflow = riot->after_underflow;
    uint16_t interval = riot->interval;

    while (remaining > 0) {
        uint16_t step_size = after_underflow ? 1u : interval;
        int needed = (int)step_size - (int)prescaler;

        if (remaining < needed) {
            prescaler = (uint16_t)(prescaler + (uint16_t)remaining);
            remaining = 0;
        } else {
            remaining -= needed;
            prescaler = 0;
            if (timer == 0u) {
                timer = 0xFFu;
                timer_flag = 0x80u;
                after_underflow = true;
            } else {
                timer = (uint8_t)((timer - 1u) & 0xFFu);
            }
        }
    }

    riot->prescaler = prescaler;
    riot->timer = timer;
    riot->timer_flag = timer_flag;
    riot->after_underflow = after_underflow;
}

uint8_t riot_read(RIOT *riot, uint16_t addr) {
    uint8_t reg = (uint8_t)(addr & 0x1Fu);
    switch (reg) {
        case 0x00u: /* SWCHA */
            return riot->swcha;
        case 0x01u: /* SWACNT */
            return riot->swacnt;
        case 0x02u: /* SWCHB */
            return riot->swchb;
        case 0x03u: /* SWBCNT */
            return riot->swbcnt;
        case 0x04u: /* INTIM */
        case 0x06u:
            riot->timer_flag = 0x00u;
            return riot->timer;
        case 0x05u: /* INSTAT */
        case 0x07u:
            return riot->timer_flag;
        default:
            return 0xFFu;
    }
}

void riot_write(RIOT *riot, uint16_t addr, uint8_t val) {
    uint8_t v = val;
    uint8_t reg = (uint8_t)(addr & 0x1Fu);
    switch (reg) {
        case 0x00u: /* SWCHA */
            riot->swcha = v;
            break;
        case 0x01u: /* SWACNT */
            riot->swacnt = v;
            break;
        case 0x02u: /* SWCHB */
            riot->swchb = v;
            break;
        case 0x03u: /* SWBCNT */
            riot->swbcnt = v;
            break;
        case 0x14u: /* TIM1T */
        case 0x1Cu:
            riot_set_timer(riot, v, 1u);
            break;
        case 0x15u: /* TIM8T */
        case 0x1Du:
            riot_set_timer(riot, v, 8u);
            break;
        case 0x16u: /* TIM64T */
        case 0x1Eu:
            riot_set_timer(riot, v, 64u);
            break;
        case 0x17u: /* T1024T */
        case 0x1Fu:
            riot_set_timer(riot, v, 1024u);
            break;
        default:
            break;
    }
}