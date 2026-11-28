#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "video.h"

#define TIA_AUDIO_RATE       31400.0
#define TIA_OUT_RATE       44100.0
#define TIA_AUDIO_MAX_SAMPLES 16384

typedef struct TIAAudio {
    uint8_t audc[2];      /* Waveform control (0x15, 0x16) */
    uint8_t audf[2];      /* Frequency divider (0x17, 0x18) */
    uint8_t audv[2];      /* Volume (0x19, 0x1A) */

    int div_counter[2];
    int p4_idx[2];
    int p5_idx[2];
    int d6_idx[2];
    uint16_t poly9[2];
    uint8_t out[2];

    double phase;
    double step;

    int16_t samples[TIA_AUDIO_MAX_SAMPLES];
    size_t sample_count;
} TIAAudio;

void tia_audio_init(TIAAudio *audio);
void tia_audio_set_video_standard(TIAAudio *audio, VideoStandard standard);
void tia_audio_tick_half_scanline(TIAAudio *audio);
void tia_audio_flush_frame(TIAAudio *audio);

/* Hulpfunctie voor je SDL2/SDL3 audio-laag om verzamelde samples op te halen en te legen */
size_t tia_audio_take_samples(TIAAudio *audio, int16_t *dst, size_t max_samples);

#endif /* AUDIO_H */