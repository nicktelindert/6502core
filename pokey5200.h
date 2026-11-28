#ifndef POKEY5200_H
#define POKEY5200_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define POKEY5200_AUDIO_RATE 44100
#define POKEY5200_AUDIO_CAPACITY 4096

typedef struct Pokey5200 {
    uint8_t audf[4], audc[4], audctl, skctl;
    uint8_t pot_input[8], pot_value[8], allpot, pot_counter;
    unsigned pot_phase;
    bool pot_scanning;
    uint8_t kbcode, irqen, irqst, skstat;
    uint8_t key_scan, key_state, key_compare;
    unsigned key_phase;
    uint16_t keypad[4];
    bool upper_trigger[4], break_down;
    uint8_t controller_lines;
    uint32_t audio_counter[4], poly4, poly5, poly9, poly17;
    bool audio_output[4], audio_filter[2], audio_capture, audio_overflow;
    uint32_t audio_phase, audio_ticks;
    int64_t audio_sum, audio_dc;
    int16_t audio_samples[POKEY5200_AUDIO_CAPACITY];
    size_t audio_count;
} Pokey5200;

void pokey5200_init(Pokey5200 *pokey);
uint8_t pokey5200_read(void *context, uint16_t address);
void pokey5200_write(void *context, uint16_t address, uint8_t value);
void pokey5200_tick(Pokey5200 *pokey);
bool pokey5200_set_pot(Pokey5200 *pokey, unsigned pot, unsigned value);
void pokey5200_set_controller(Pokey5200 *pokey, unsigned port, uint16_t keys, bool upper_trigger);
void pokey5200_set_controller_lines(Pokey5200 *pokey, uint8_t lines);
bool pokey5200_irq(const Pokey5200 *pokey);
void pokey5200_capture_audio(Pokey5200 *pokey, bool enabled);
size_t pokey5200_take_samples(Pokey5200 *pokey, int16_t *dst, size_t max_samples);

#endif
