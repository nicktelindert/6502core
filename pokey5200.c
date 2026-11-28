#include "pokey5200.h"
#include <stdio.h>
#include <string.h>

#define POKEY_CLOCK 1789773u

static unsigned audio_period(const Pokey5200 *pokey, unsigned channel) {
    bool high = channel >= 2;
    unsigned low = high ? 2 : 0;
    bool fast = (pokey->audctl & (high ? 0x20 : 0x40)) != 0;
    bool linked = (pokey->audctl & (high ? 0x08 : 0x10)) != 0;
    if ((channel & 1) && linked) {
        unsigned value = pokey->audf[low] | ((unsigned)pokey->audf[channel] << 8);
        return fast ? value + 7 : (value + 1) * ((pokey->audctl & 1) ? 114 : 28);
    }
    if (!(channel & 1) && fast) return (unsigned)pokey->audf[channel] + 4;
    return ((unsigned)pokey->audf[channel] + 1) * ((pokey->audctl & 1) ? 114 : 28);
}

static void reset_audio_timers(Pokey5200 *pokey) {
    for (unsigned i = 0; i < 4; i++) {
        pokey->audio_counter[i] = audio_period(pokey, i);
        pokey->audio_output[i] = i >= 2;
    }
    pokey->audio_filter[0] = pokey->audio_filter[1] = false;
}

void pokey5200_init(Pokey5200 *pokey) {
    memset(pokey, 0, sizeof(*pokey));
    memset(pokey->pot_input, 228, sizeof(pokey->pot_input));
    memset(pokey->pot_value, 228, sizeof(pokey->pot_value));
    pokey->kbcode = pokey->irqst = pokey->skstat = 0xff;
    reset_audio_timers(pokey);
}

void pokey5200_capture_audio(Pokey5200 *pokey, bool enabled) {
    pokey->audio_capture = enabled;
    pokey->audio_phase = pokey->audio_ticks = 0;
    pokey->audio_sum = pokey->audio_dc = 0;
    pokey->audio_count = 0;
    pokey->audio_overflow = false;
}

size_t pokey5200_take_samples(Pokey5200 *pokey, int16_t *dst, size_t max_samples) {
    size_t n = pokey->audio_count < max_samples ? pokey->audio_count : max_samples;
    if (!n) return 0;
    memcpy(dst, pokey->audio_samples, n * sizeof(*dst));
    pokey->audio_count -= n;
    memmove(pokey->audio_samples, pokey->audio_samples + n, pokey->audio_count * sizeof(*dst));
    return n;
}

void pokey5200_set_controller(Pokey5200 *pokey, unsigned port, uint16_t keys, bool upper_trigger) {
    if (port >= 4) {
        fprintf(stderr, "POKEY controller requires port 0-3\n");
        return;
    }
    pokey->keypad[port] = keys & 0xfffe;
    pokey->upper_trigger[port] = upper_trigger;
}

void pokey5200_set_controller_lines(Pokey5200 *pokey, uint8_t lines) {
    pokey->controller_lines = lines & 7;
}

bool pokey5200_irq(const Pokey5200 *pokey) {
    return (pokey->irqen & (uint8_t)~pokey->irqst) != 0;
}

bool pokey5200_set_pot(Pokey5200 *pokey, unsigned pot, unsigned value) {
    if (pot >= 8 || value > 228) {
        fprintf(stderr, "POKEY potentiometer requires index 0-7 and value 0-228\n");
        return false;
    }
    pokey->pot_input[pot] = (uint8_t)value;
    return true;
}

uint8_t pokey5200_read(void *context, uint16_t address) {
    Pokey5200 *pokey = context;
    unsigned reg = address & 15;
    if (reg < 8) return pokey->pot_value[reg];
    if (reg == 8) return pokey->allpot;
    if (reg == 9) return pokey->kbcode;
    if (reg == 14) return pokey->irqst;
    if (reg == 15) return pokey->skstat;
    return 0xff;
}

void pokey5200_write(void *context, uint16_t address, uint8_t value) {
    Pokey5200 *pokey = context;
    unsigned reg = address & 15;
    if (reg < 8) {
        if (reg & 1) pokey->audc[reg / 2] = value;
        else pokey->audf[reg / 2] = value;
    } else if (reg == 8) {
        pokey->audctl = value;
    } else if (reg == 9) {
        reset_audio_timers(pokey);
    } else if (reg == 10) {
        pokey->skstat |= 0xe0;
    } else if (reg == 11) {
        pokey->pot_counter = 0;
        pokey->pot_phase = 0;
        pokey->allpot = 0xff;
        memset(pokey->pot_value, 0, sizeof(pokey->pot_value));
        pokey->pot_scanning = true;
    } else if (reg == 14) {
        pokey->irqen = value;
        pokey->irqst |= (uint8_t)~value;
    } else if (reg == 15) {
        pokey->skctl = value;
        if (!(value & 2)) {
            pokey->key_scan = pokey->key_state = 0;
            pokey->key_phase = 0;
            pokey->skstat |= 4;
            pokey->break_down = false;
        }
    }
}

static uint32_t poly_tick(uint32_t state, unsigned bits, unsigned tap) {
    return (state >> 1) | (((~(state ^ (state >> tap))) & 1u) << (bits - 1));
}

static void audio_tick(Pokey5200 *pokey) {
    if (pokey->skctl & 3) {
        pokey->poly4 = poly_tick(pokey->poly4, 4, 1);
        pokey->poly5 = poly_tick(pokey->poly5, 5, 2);
        pokey->poly9 = poly_tick(pokey->poly9, 9, 4);
        pokey->poly17 = poly_tick(pokey->poly17, 17, 5);
    } else {
        pokey->poly4 = pokey->poly5 = pokey->poly9 = pokey->poly17 = 0;
    }
    for (unsigned i = 0; i < 4; i++) {
        unsigned low = i & ~1u;
        bool fast = (pokey->audctl & (low ? 0x20 : 0x40)) &&
                    (!(i & 1) || (pokey->audctl & (low ? 0x08 : 0x10)));
        if (!(pokey->skctl & 3) && !fast) continue;
        if (--pokey->audio_counter[i]) continue;
        pokey->audio_counter[i] = audio_period(pokey, i);
        uint8_t control = pokey->audc[i];
        if ((control & 0x80) || (pokey->poly5 & 1)) {
            if (control & 0x20) pokey->audio_output[i] = !pokey->audio_output[i];
            else {
                uint32_t noise = control & 0x40 ? pokey->poly4 :
                                 pokey->audctl & 0x80 ? pokey->poly9 : pokey->poly17;
                pokey->audio_output[i] = ((noise & 1) != 0) ^ (i < 2);
            }
        }
        if (i >= 2) pokey->audio_filter[i - 2] = pokey->audio_output[i - 2];
    }
    if (!pokey->audio_capture) return;
    int mix = 0;
    for (unsigned i = 0; i < 4; i++) {
        bool output = pokey->audio_output[i];
        if (i < 2 && (pokey->audctl & (i ? 2 : 4))) output ^= pokey->audio_filter[i];
        if ((pokey->audc[i] & 0x10) || output) mix += pokey->audc[i] & 15;
    }
    pokey->audio_sum += mix * 500;
    pokey->audio_ticks++;
    pokey->audio_phase += POKEY5200_AUDIO_RATE;
    if (pokey->audio_phase < POKEY_CLOCK) return;
    pokey->audio_phase -= POKEY_CLOCK;
    int64_t value = pokey->audio_sum / pokey->audio_ticks;
    pokey->audio_sum = 0;
    pokey->audio_ticks = 0;
    pokey->audio_dc += ((value << 16) - pokey->audio_dc) / 115;
    value -= pokey->audio_dc >> 16;
    if (pokey->audio_count < POKEY5200_AUDIO_CAPACITY) {
        pokey->audio_samples[pokey->audio_count++] = (int16_t)value;
    } else if (!pokey->audio_overflow) {
        fprintf(stderr, "POKEY audio buffer full; samples dropped\n");
        pokey->audio_overflow = true;
    }
}

static void keyboard_tick(Pokey5200 *pokey) {
    if (!(pokey->skctl & 2) || ++pokey->key_phase < 114) return;
    pokey->key_phase = 0;
    unsigned scan = pokey->key_scan;
    unsigned port = pokey->controller_lines & 3;
    bool upper = pokey->upper_trigger[port];
    if (scan == 0x10) {
        if (upper) pokey->skstat &= (uint8_t)~8;
        else pokey->skstat |= 8;
    }
    if (scan == 0x30) {
        if (upper && !pokey->break_down && (pokey->irqen & 0x80))
            pokey->irqst &= (uint8_t)~0x80;
        pokey->break_down = upper;
    }
    bool down = (pokey->keypad[port] & (1u << ((scan >> 1) & 15))) != 0;
    bool match = !(pokey->skctl & 1) || scan == pokey->key_compare;
    switch (pokey->key_state) {
        case 0:
            if (down) { pokey->key_compare = (uint8_t)scan; pokey->key_state = 1; }
            break;
        case 1:
            if (match) {
                if (down) {
                    if (!(pokey->irqst & 0x40)) pokey->skstat &= (uint8_t)~0x40;
                    pokey->kbcode = (uint8_t)(scan | (upper ? 0xc0 : 0));
                    pokey->skstat &= (uint8_t)~4;
                    if (pokey->irqen & 0x40) pokey->irqst &= (uint8_t)~0x40;
                    pokey->key_state = 3;
                } else pokey->key_state = 0;
            } else if (down) pokey->key_state = 0;
            break;
        case 3:
            if (match && !down) pokey->key_state = 2;
            break;
        case 2:
            if (match && !down) { pokey->skstat |= 4; pokey->key_state = 0; }
            break;
    }
    pokey->key_scan = (uint8_t)((scan + 1) & 63);
}

void pokey5200_tick(Pokey5200 *pokey) {
    audio_tick(pokey);
    keyboard_tick(pokey);
    if (!pokey->pot_scanning) return;
    bool fast = (pokey->skctl & 4) != 0;
    if (!fast && !(pokey->skctl & 3)) return;
    for (unsigned i = 0; i < 8; i++) {
        uint8_t mask = (uint8_t)(1u << i);
        unsigned input = (pokey->controller_lines & 4) ? pokey->pot_input[i] : 228;
        if ((pokey->allpot & mask) && input <= pokey->pot_counter) {
            pokey->pot_value[i] = pokey->pot_counter;
            pokey->allpot &= (uint8_t)~mask;
        }
    }
    if (!pokey->allpot) {
        pokey->pot_scanning = false;
        return;
    }
    if (++pokey->pot_phase < (fast ? 1u : 114u)) return;
    pokey->pot_phase = 0;
    if (pokey->pot_counter < (fast ? 229 : 228)) pokey->pot_counter++;
    for (unsigned i = 0; i < 8; i++) {
        uint8_t mask = (uint8_t)(1u << i);
        if (!(pokey->allpot & mask)) continue;
        pokey->pot_value[i] = pokey->pot_counter;
        unsigned input = (pokey->controller_lines & 4) ? pokey->pot_input[i] : 228;
        if (pokey->pot_counter >= input ||
            pokey->pot_counter == (fast ? 229 : 228))
            pokey->allpot &= (uint8_t)~mask;
    }
    if (!pokey->allpot) pokey->pot_scanning = false;
}
