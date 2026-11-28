#include "audio.h"
#include <string.h>

static const uint8_t POLY4[15] = {
    1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0
};

static const uint8_t POLY5[31] = {
    0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0,
    1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1
};

static const uint8_t DIV31[31] = {
    0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const uint8_t DIV6[6] = {
    0, 1, 0, 0, 1, 0
};

static void clock_channel(TIAAudio *audio, int ch) {
    audio->div_counter[ch] += 1;
    if (audio->div_counter[ch] <= (int)audio->audf[ch]) {
        return;
    }
    audio->div_counter[ch] = 0;

    uint8_t ctrl = (uint8_t)(audio->audc[ch] & 0x0Fu);
    int p5 = audio->p5_idx[ch];
    audio->p5_idx[ch] = (p5 + 1) % 31;

    switch (ctrl) {
        case 0x00u:
        case 0x0Bu:
            audio->out[ch] = 1u;
            break;
        case 0x01u:
            audio->out[ch] = POLY5[p5];
            break;
        case 0x02u:
        case 0x03u:
            if (DIV31[p5]) {
                int idx4 = audio->p4_idx[ch];
                audio->p4_idx[ch] = (idx4 + 1) % 15;
                audio->out[ch] = POLY4[idx4];
            }
            break;
        case 0x04u:
        case 0x05u:
            audio->out[ch] ^= 1u;
            break;
        case 0x06u:
        case 0x07u:
        case 0x0Au:
            if (DIV31[p5]) {
                audio->out[ch] ^= 1u;
            }
            break;
        case 0x08u: {
            uint16_t p9 = audio->poly9[ch];
            uint16_t in9 = (uint16_t)((p9 & 1u) ^ ((p9 >> 4) & 1u));
            audio->poly9[ch] = (uint16_t)((p9 >> 1) | (in9 << 8));
            audio->out[ch] = (uint8_t)(audio->poly9[ch] & 1u);
            break;
        }
        case 0x09u:
            audio->out[ch] = POLY5[p5];
            break;
        case 0x0Cu:
        case 0x0Du: {
            int idx6 = audio->d6_idx[ch];
            audio->d6_idx[ch] = (idx6 + 1) % 6;
            if (DIV6[idx6]) {
                audio->out[ch] ^= 1u;
            }
            break;
        }
        case 0x0Eu:
        case 0x0Fu:
            if (DIV31[p5]) {
                int idx6 = audio->d6_idx[ch];
                audio->d6_idx[ch] = (idx6 + 1) % 6;
                if (DIV6[idx6]) {
                    audio->out[ch] ^= 1u;
                }
            }
            break;
        default:
            break;
    }
}

void tia_audio_init(TIAAudio *audio) {
    memset(audio, 0, sizeof(*audio));
    audio->poly9[0] = 0x1FFu;
    audio->poly9[1] = 0x1FFu;
    audio->phase = 0.0;
    audio->step = TIA_AUDIO_RATE / TIA_OUT_RATE;
    audio->sample_count = 0;
}

void tia_audio_set_video_standard(TIAAudio *audio, VideoStandard standard) {
    audio->step = (TIA_AUDIO_RATE / TIA_OUT_RATE) *
                  (video_cpu_clock(standard) / video_cpu_clock(VIDEO_NTSC));
    audio->phase = 0.0;
    audio->sample_count = 0;
}

void tia_audio_tick_half_scanline(TIAAudio *audio) {
    if (!audio) {
        return;
    }

    clock_channel(audio, 0);
    clock_channel(audio, 1);

    int v0 = audio->out[0] ? (int)audio->audv[0] : -(int)audio->audv[0];
    int v1 = audio->out[1] ? (int)audio->audv[1] : -(int)audio->audv[1];
    int16_t cur = (int16_t)((v0 + v1) * 500);

    audio->phase += 1.0;
    while (audio->phase >= audio->step) {
        if (audio->sample_count < TIA_AUDIO_MAX_SAMPLES) {
            audio->samples[audio->sample_count++] = cur;
        }
        audio->phase -= audio->step;
    }
}

void tia_audio_flush_frame(TIAAudio *audio) {
    if (!audio) {
        return;
    }
    /* Voorkomt oneindige bufferopbouw als het SDL-audiokanaal nog niet leest */
    if (audio->sample_count > 4410u) {
        size_t keep = 1470u;
        size_t start = audio->sample_count - keep;
        memmove(audio->samples, &audio->samples[start], keep * sizeof(int16_t));
        audio->sample_count = keep;
    }
}

size_t tia_audio_take_samples(TIAAudio *audio, int16_t *dst, size_t max_samples) {
    if (!audio || !dst || max_samples == 0 || audio->sample_count == 0) {
        return 0;
    }
    size_t n = (audio->sample_count < max_samples) ? audio->sample_count : max_samples;
    memcpy(dst, audio->samples, n * sizeof(int16_t));
    if (n < audio->sample_count) {
        size_t rem = audio->sample_count - n;
        memmove(audio->samples, &audio->samples[n], rem * sizeof(int16_t));
        audio->sample_count = rem;
    } else {
        audio->sample_count = 0;
    }
    return n;
}