#ifndef VIDEO_H
#define VIDEO_H

typedef enum {
    VIDEO_NTSC,
    VIDEO_PAL
} VideoStandard;

static inline double video_cpu_clock(VideoStandard standard) {
    return standard == VIDEO_PAL ? 1182298.0 : 1193191.66666667;
}

static inline double video_frame_rate(VideoStandard standard) {
    return standard == VIDEO_PAL ? 50.0 : 60.0;
}

#endif
