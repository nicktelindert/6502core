#include "demo2600_pattern.h"
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_color_clock_boundaries(void) {
    TIA tia = {0};
    tia_init(&tia, NULL);
    tia_write(&tia, 0x09, 0x26);
    tia_step(&tia, 68);
    CHECK(tia.scanline == 0 && tia.clock == 68 && tia.framebuffer == NULL);
    tia_step(&tia, 1);
    CHECK(tia.clock == 69 && tia.frame_height == 1);
    CHECK(memcmp(tia.framebuffer, tia.color_lut[0x26], 3) == 0);
    tia_step(&tia, 158);
    CHECK(tia.clock == 227 && tia.scanline == 0);
    tia_step(&tia, 1);
    CHECK(tia.clock == 0 && tia.scanline == 1);
    CHECK(memcmp(tia.framebuffer + 159 * 3, tia.color_lut[0x26], 3) == 0);
    tia_destroy(&tia);
}

static void test_variable_frame_heights(void) {
    static const int heights[] = {201, 312, 263};
    TIA tia = {0};
    tia_init(&tia, NULL);
    for (unsigned i = 0; i < sizeof(heights) / sizeof(heights[0]); i++) {
        tia.frame_ready = false;
        tia_write(&tia, 0x00, 0);
        tia_write(&tia, 0x01, 2);
        tia_write(&tia, 0x09, 0x26);
        tia_step(&tia, 228);
        tia_write(&tia, 0x01, 0);
        tia_step(&tia, 228 * (heights[i] - 2));
        tia_write(&tia, 0x09, 0x46);
        tia_write(&tia, 0x00, 2);
        tia_step(&tia, 227);
        CHECK(!tia.frame_ready && tia.scanline == heights[i] - 1 && tia.clock == 227);
        tia_step(&tia, 1);
        CHECK(tia.frame_ready && tia.scanline == 0 && tia.clock == 0);
        CHECK(tia.completed_frame_height == heights[i]);
        static const uint8_t black[] = {0, 0, 0};
        CHECK(memcmp(tia.framebuffer, black, 3) == 0);
        CHECK(memcmp(tia.framebuffer + TIA_FB_WIDTH * 3, tia.color_lut[0x26], 3) == 0);
        CHECK(memcmp(tia.framebuffer + (heights[i] * TIA_FB_WIDTH - 1) * 3,
                     tia.color_lut[0x46], 3) == 0);
    }
    tia_destroy(&tia);
}

static void test_synthetic_frames(VideoStandard standard) {
    Demo2600 demo;
    CHECK(demo2600_init(&demo, standard));
    CHECK(system2600_step_frame(&demo.system, &demo.cpu));
    int total = standard == VIDEO_PAL ? 312 : 262;
    int first = standard == VIDEO_PAL ? 47 : 39;
    int band = standard == VIDEO_PAL ? 76 : 64;
    for (int frame = 0; frame < 3; frame++) {
        CHECK(system2600_step_frame(&demo.system, &demo.cpu));
        CHECK(demo.tia.completed_frame_height == total);
        CHECK(demo.tia.framebuffer_capacity >= (size_t)total * TIA_FB_WIDTH * 3);
        CHECK(demo.tia.p0_x == 33);
        for (int y = 0; y < total; y++) {
            for (int x = 0; x < TIA_FB_WIDTH; x++) {
                const uint8_t *pixel = demo.tia.framebuffer + (y * TIA_FB_WIDTH + x) * 3;
                if (y < first || y >= first + 3 * band) {
                    static const uint8_t black[3] = {0, 0, 0};
                    CHECK(memcmp(pixel, black, 3) == 0);
                } else {
                    static const uint8_t colors[] = {0x26, 0x46, 0x86};
                    uint8_t color = x >= demo.tia.p0_x && x < demo.tia.p0_x + 8 ? 0x0e :
                                    (x < 4 || (x >= 80 && x < 84)) ? 0x0a :
                                    colors[(y - first) / band];
                    if (memcmp(pixel, demo.tia.color_lut[color], 3) != 0) {
                        fprintf(stderr, "standard=%d frame=%d x=%d y=%d expected=%02x\n",
                                standard, frame, x, y, color);
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
    }
    demo2600_destroy(&demo);
}

int main(void) {
    test_color_clock_boundaries();
    test_variable_frame_heights();
    test_synthetic_frames(VIDEO_NTSC);
    test_synthetic_frames(VIDEO_PAL);
    puts("Atari 2600 frame and scanline tests passed");
    return EXIT_SUCCESS;
}
