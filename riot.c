#include "riot.h"

static void riot_set_timer(RIOT *riot, uint8_t value, uint16_t interval) {
    riot->timer = value;
    riot->interval = interval;
    riot->prescaler = (uint16_t)(interval - 1u);
    if (!riot->wrapped_this_cycle) {
        riot->timer_flag = 0x00u;
    }
}

void riot_init(RIOT *riot) {
    riot->swcha = 0xFFu;
    riot->swacnt = 0x00u;
    riot->swchb = 0xFFu;
    riot->swbcnt = 0x00u;

    /* Power-on timer contents are unspecified; use a reproducible value. */
    riot->timer = 0x80u;
    riot->interval = 1024u;
    riot->prescaler = 0u;
    riot->timer_flag = 0x00u;
    riot->wrapped_this_cycle = false;
}

void riot_step(RIOT *riot, int cycles) {
    if (cycles <= 0) {
        return;
    }

    uint32_t elapsed = (uint32_t)cycles;
    uint32_t total = elapsed + riot->prescaler;
    uint32_t ticks = total / riot->interval;
    uint16_t previous_prescaler = riot->prescaler;
    riot->prescaler = (uint16_t)(total % riot->interval);
    riot->wrapped_this_cycle = false;

    if (riot->timer_flag == 0) {
        if (ticks <= riot->timer) {
            riot->timer = (uint8_t)(riot->timer - ticks);
            return;
        }
        uint32_t until_wrap = ((uint32_t)riot->timer + 1u) * riot->interval -
                              previous_prescaler;
        elapsed -= until_wrap;
        riot->timer = 0xFFu;
        riot->timer_flag = 0x80u;
    }

    riot->timer = (uint8_t)(riot->timer - elapsed);
    riot->wrapped_this_cycle = riot->timer == 0xFFu;
}

uint8_t riot_read(RIOT *riot, uint16_t addr) {
    uint8_t reg = (uint8_t)(addr & 0x07u);
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
            if (!riot->wrapped_this_cycle) {
                riot->timer_flag = 0x00u;
            }
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
    /* A2 selects I/O vs timer/edge control; A4 matters only for the latter. */
    uint8_t reg = (uint8_t)(addr & ((addr & 0x04u) ? 0x17u : 0x03u));
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
            riot_set_timer(riot, v, 1u);
            break;
        case 0x15u: /* TIM8T */
            riot_set_timer(riot, v, 8u);
            break;
        case 0x16u: /* TIM64T */
            riot_set_timer(riot, v, 64u);
            break;
        case 0x17u: /* T1024T */
            riot_set_timer(riot, v, 1024u);
            break;
        default:
            break;
    }
}