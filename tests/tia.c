#include "../tia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", \
                __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void setup(TIA *tia) {
    memset(tia, 0, sizeof(*tia));
    tia_init(tia, NULL);
}

static void test_reinitialization(void) {
    TIA tia;
    setup(&tia);
    tia_write(&tia, 0x00, 0x02);
    tia_write(&tia, 0x01, 0x02);
    tia_write(&tia, 0x02, 0);
    tia_write(&tia, 0x04, 0x37);
    tia_write(&tia, 0x1B, 0xFF);
    tia_write(&tia, 0x1D, 0x02);
    tia_write(&tia, 0x25, 0x01);
    tia.cxp0fb = 0xC0;
    tia.audio_clock = 42;
    tia.p0_x = 25;
    tia_step(&tia, 228);
    tia_destroy(&tia);
    tia_init(&tia, NULL);
    CHECK(tia.vsync == 0 && !tia.vsync_on);
    CHECK(tia.vblank == 0 && !tia.wsync);
    CHECK(tia.nusiz0 == 0 && tia.grp0 == 0 && tia.enam0 == 0);
    CHECK(tia.vdelp0 == 0 && tia.p0_x == 0);
    CHECK(tia.cxp0fb == 0 && tia.audio_clock == 0);
    CHECK(tia.framebuffer == NULL && tia.framebuffer_capacity == 0);
    CHECK(tia.inpt4 == 0x80 && tia.inpt5 == 0x80);
    tia_destroy(&tia);
}

static int pixel_is_color(const TIA *tia, int x, uint8_t color) {
    return memcmp(&tia->framebuffer[x * 3], tia->ntsc_lut[color], 3) == 0;
}

/* Manual PDF pp. 5-7 and figure on p. 20: NUSIZ copies and widths. */
static void test_player_and_missile_copies(void) {
    static const struct {
        int count;
        int offsets[3];
        int player_scale;
    } layouts[] = {
        {1, {0, 0, 0}, 1}, {2, {0, 16, 0}, 1},
        {2, {0, 32, 0}, 1}, {3, {0, 16, 32}, 1},
        {2, {0, 64, 0}, 1}, {1, {0, 0, 0}, 2},
        {3, {0, 32, 64}, 1}, {1, {0, 0, 0}, 4},
    };

    for (int object = 0; object < 2; object++) {
        for (int missile = 0; missile < 2; missile++) {
            for (int mode = 0; mode < 8; mode++) {
                for (int size = 0; size < (missile ? 4 : 1); size++) {
                    TIA tia;
                    setup(&tia);
                    tia_write(&tia, (uint16_t)(0x06 + object), 0x0E);
                    tia_write(&tia, (uint16_t)(0x04 + object),
                              (uint8_t)(mode | (size << 4)));
                    tia_write(&tia, (uint16_t)((missile ? 0x12 : 0x10) + object), 0);
                    tia_write(&tia, (uint16_t)((missile ? 0x1D : 0x1B) + object),
                              missile ? 0x02 : 0xFF);
                    tia_step(&tia, 228);
                    CHECK(tia.framebuffer != NULL);

                    for (int x = 0; x < 160; x++) {
                        int expected = 0;
                        int width = missile ? (1 << size) : 8 * layouts[mode].player_scale;
                        int position = missile ? 2 : 3 + (layouts[mode].player_scale > 1);
                        for (int n = 0; n < layouts[mode].count; n++) {
                            int start = position + layouts[mode].offsets[n];
                            if (x >= start && x < start + width) expected = 1;
                        }
                        if (!pixel_is_color(&tia, x, expected ? 0x0E : 0)) {
                            fprintf(stderr, "object=%d missile=%d mode=%d size=%d x=%d expected=%d\n",
                                    object, missile, mode, size, x, expected);
                            exit(EXIT_FAILURE);
                        }
                    }
                    tia_destroy(&tia);
                }
            }
        }
    }
}

/* Manual PDF pp. 3-4 and 7: playfield and player reflection. */
static void test_reflection(void) {
    TIA tia;
    for (int reflected = 0; reflected < 2; reflected++) {
        setup(&tia);
        tia_write(&tia, 0x08, 0x0E);
        tia_write(&tia, 0x0D, 0x10);
        tia_write(&tia, 0x0A, (uint8_t)reflected);
        tia_step(&tia, 228);
        for (int x = 0; x < 160; x++) {
            int on = x < 4 || (reflected ? x >= 156 : x >= 80 && x < 84);
            CHECK(pixel_is_color(&tia, x, on ? 0x0E : 0));
        }
        tia_destroy(&tia);

        setup(&tia);
        tia_write(&tia, 0x06, 0x0E);
        tia_write(&tia, 0x10, 0);
        tia_write(&tia, 0x1B, 0x80);
        tia_write(&tia, 0x0B, reflected ? 0x08 : 0);
        tia_step(&tia, 228);
        for (int x = 0; x < 160; x++) {
            CHECK(pixel_is_color(&tia, x, x == (reflected ? 10 : 3) ? 0x0E : 0));
        }
        tia_destroy(&tia);
    }
}

/* Manual PDF pp. 8-9: fifteen collision latches and CXCLR. */
static void test_collisions(void) {
    static const struct {
        int left;
        int right;
        int reg;
        uint8_t mask;
    } pairs[] = {
        {0, 1, 7, 0x80}, {0, 2, 0, 0x40}, {0, 3, 1, 0x80},
        {0, 4, 2, 0x40}, {0, 5, 2, 0x80},
        {1, 2, 0, 0x80}, {1, 3, 1, 0x40}, {1, 4, 3, 0x40},
        {1, 5, 3, 0x80}, {2, 3, 7, 0x40}, {2, 4, 4, 0x40},
        {2, 5, 4, 0x80}, {3, 4, 5, 0x40}, {3, 5, 5, 0x80},
        {4, 5, 6, 0x80},
    };
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
        TIA tia;
        setup(&tia);
        tia.p0_x = tia.p1_x = tia.m0_x = tia.m1_x = tia.bl_x = 24;
        int enabled = (1 << pairs[i].left) | (1 << pairs[i].right);
        if (enabled & (1 << 0)) tia_write(&tia, 0x1B, 0x80);
        if (enabled & (1 << 1)) tia_write(&tia, 0x1C, 0x80);
        if (enabled & (1 << 2)) tia_write(&tia, 0x1D, 0x02);
        if (enabled & (1 << 3)) tia_write(&tia, 0x1E, 0x02);
        if (enabled & (1 << 4)) tia_write(&tia, 0x1F, 0x02);
        if (enabled & (1 << 5)) tia_write(&tia, 0x0E, 0x20);
        tia_step(&tia, 68 + 25);
        for (int reg = 0; reg < 8; reg++) {
            CHECK(tia_read(&tia, (uint16_t)reg) == (reg == pairs[i].reg ? pairs[i].mask : 0));
        }
        tia_write(&tia, 0x1B, 0);
        tia_write(&tia, 0x1C, 0);
        tia_write(&tia, 0x1D, 0);
        tia_write(&tia, 0x1E, 0);
        tia_write(&tia, 0x1F, 0);
        tia_write(&tia, 0x0E, 0);
        tia_step(&tia, 5);
        CHECK(tia_read(&tia, (uint16_t)pairs[i].reg) == pairs[i].mask);
        tia_write(&tia, 0x2C, 0);
        for (int reg = 0; reg < 8; reg++) CHECK(tia_read(&tia, (uint16_t)reg) == 0);
        tia_destroy(&tia);
    }
}

static void test_missile_copy_collisions(void) {
    for (int missile = 0; missile < 2; missile++) {
        TIA tia;
        setup(&tia);
        tia_write(&tia, (uint16_t)(0x04 + missile), 0x01);
        tia_write(&tia, (uint16_t)(0x12 + missile), 0);
        tia_write(&tia, (uint16_t)(0x1D + missile), 0x02);
        if (missile == 0) {
            tia.p1_x = 18;
            tia_write(&tia, 0x1C, 0x80);
        } else {
            tia.p0_x = 18;
            tia_write(&tia, 0x1B, 0x80);
        }
        tia_step(&tia, 68 + 19);
        CHECK(tia_read(&tia, (uint16_t)missile) == 0x80);
        tia_destroy(&tia);

        setup(&tia);
        tia_write(&tia, (uint16_t)(0x04 + missile), 0x01);
        tia_write(&tia, (uint16_t)(0x06 + missile), 0x0E);
        tia_write(&tia, (uint16_t)(0x1D + missile), 0x02);
        tia_write(&tia, (uint16_t)(0x28 + missile), 0x02);
        tia_step(&tia, 228);
        for (int x = 0; x < 160; x++) CHECK(pixel_is_color(&tia, x, 0));
        tia_destroy(&tia);
    }
}

/* Manual PDF pp. 10-11: playfield priority and score colors. */
static void test_playfield_priority(void) {
    for (int priority = 0; priority < 2; priority++) {
        TIA tia;
        setup(&tia);
        tia_write(&tia, 0x06, 0x0E);
        tia_write(&tia, 0x08, 0x08);
        tia_write(&tia, 0x0D, 0x10);
        tia_write(&tia, 0x0A, priority ? 0x04 : 0);
        tia.p0_x = 0;
        tia_write(&tia, 0x1B, 0x80);
        tia_step(&tia, 69);
        CHECK(pixel_is_color(&tia, 0, priority ? 0x08 : 0x0E));
        CHECK(tia_read(&tia, 2) == 0x80);
        tia_destroy(&tia);
    }

    TIA tia;
    setup(&tia);
    tia_write(&tia, 0x06, 0x0E);
    tia_write(&tia, 0x07, 0x08);
    tia_write(&tia, 0x0D, 0x10);
    tia_write(&tia, 0x0A, 0x02);
    tia_step(&tia, 228);
    CHECK(pixel_is_color(&tia, 0, 0x0E));
    CHECK(pixel_is_color(&tia, 80, 0x08));
    tia_destroy(&tia);
}

/* Manual PDF p. 3: WSYNC remains asserted until horizontal blank. */
static void test_wsync(void) {
    TIA tia;
    setup(&tia);
    tia_step(&tia, 17);
    tia_write(&tia, 0x02, 0);
    CHECK(tia.wsync);
    tia_step(&tia, 210);
    CHECK(tia.wsync);
    CHECK(tia.clock == 227);
    tia_step(&tia, 1);
    CHECK(!tia.wsync);
    CHECK(tia.clock == 0);
    CHECK(tia.scanline == 1);
    tia_destroy(&tia);
}

int main(void) {
    test_reinitialization();
    test_player_and_missile_copies();
    test_reflection();
    test_collisions();
    test_missile_copy_collisions();
    test_playfield_priority();
    test_wsync();
    puts("TIA tests passed");
    return EXIT_SUCCESS;
}
