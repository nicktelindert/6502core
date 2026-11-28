#include "../antic5200.h"
#include "../gtia5200.h"
#include "../system5200.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); \
} } while (0)

#define LINE32 (32 - GTIA5200_FIRST_SCANLINE)

static void test_visible_scanline_bounds(void) {
    static const unsigned lines[] = {7, 8, 28, 31, 32, 223, 224, 247, 248};
    for (unsigned i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        Antic5200 antic;
        GTIA5200 gtia;
        antic5200_init(&antic);
        gtia5200_init(&gtia);
        memset(gtia.framebuffer, 0x55, sizeof(gtia.framebuffer));
        antic.gtia = &gtia;
        antic.scanline = lines[i];
        gtia.colbk = 0x10;
        gtia.colpf0 = 0x20;
        gtia.colpm[0] = 0x40;
        gtia.hposp[0] = 0x30;
        gtia.grafp[0] = 0x80;
        gtia.prior = 1;
        antic5200_tick(&antic);
        antic.dmactl = 2;
        antic.instruction = 7;
        antic.character_dma = true;
        antic5200_playfield_byte(&antic, 0x80);
        antic.dmactl = 0;
        while (antic.cycle < 105) antic5200_tick(&antic);
        if (lines[i] < GTIA5200_FIRST_SCANLINE ||
            lines[i] >= GTIA5200_FIRST_SCANLINE + GTIA5200_HEIGHT) {
            CHECK(gtia.framebuffer[0][0] == 0x55);
            CHECK(gtia.framebuffer[GTIA5200_HEIGHT - 1][0] == 0x55);
            CHECK(gtia.collisions[4] == 0);
            continue;
        }
        unsigned y = lines[i] - GTIA5200_FIRST_SCANLINE;
        CHECK(gtia.framebuffer[y][0] == 0x40);
        CHECK(gtia.framebuffer[y][2] == 0x10);
        CHECK(gtia.collisions[4] & 1);

        antic.dmactl = 2;
        antic.instruction = 8;
        antic.character_dma = false;
        gtia5200_begin_line(&gtia, y);
        antic5200_playfield_byte(&antic, 0x40);
        CHECK(gtia.framebuffer[y][0] == 0x20);
        gtia5200_begin_line(&gtia, y);
        antic.line_buffer[0] = 0x40;
        antic.lines_left = 1;
        antic.first_mode_line = false;
        antic.cycle = 19;
        antic5200_tick(&antic);
        CHECK(gtia.framebuffer[y][0] == 0x20);
    }
}

static void test_mode_f(void) {
    System5200 system;
    Antic5200 antic;
    GTIA5200 gtia;
    CPU6502 cpu;
    system5200_init(&system);
    antic5200_init(&antic);
    gtia5200_init(&gtia);
    system5200_attach_antic(&system, &antic, &cpu);
    system5200_attach_gtia(&system, &gtia);
    system.bios_loaded = true;
    system.bios[0x7fc] = 0x00;
    system.bios[0x7fd] = 0x02;
    system.ram[0x200] = 0x4c;
    system.ram[0x201] = 0x00;
    system.ram[0x202] = 0x02;
    /* 24 blank lines, then a mode F LMS on visible line 32. */
    system.ram[0x300] = 0x70;
    system.ram[0x301] = 0x70;
    system.ram[0x302] = 0x70;
    system.ram[0x303] = 0x4f;
    system.ram[0x304] = 0xf0;
    system.ram[0x305] = 0x1f;
    system.ram[0x306] = 0x41;
    system.ram[0x307] = 0x00;
    system.ram[0x308] = 0x03;
    system.ram[0x1ff0] = 0x80;
    system.ram[0x1000] = 0x01;
    system5200_write(&system, 0xc018, 0x42);
    system5200_write(&system, 0xc017, 0x0e);
    system5200_write(&system, 0xc01a, 0x20);
    system5200_write(&system, 0xd402, 0x00);
    system5200_write(&system, 0xd403, 0x03);
    system5200_write(&system, 0xd400, 0x22);
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    while (antic.scanline < 34) cpu_step(&cpu);
    CHECK(!antic.unsupported);
    CHECK(gtia.framebuffer[LINE32][0] == 0x4e);
    for (unsigned x = 1; x < GTIA5200_WIDTH; x++)
        CHECK(gtia.framebuffer[LINE32][x] == (x == 16 * 8 + 7 ? 0x4e : 0x42));
    CHECK(antic.screen_address == 0x1018);
    for (unsigned x = 0; x < GTIA5200_WIDTH; x++) CHECK(gtia.framebuffer[LINE32 + 1][x] == 0x20);
    gtia5200_write(&gtia, 0xc037, 0xff);
    gtia5200_write(&gtia, 0xc038, 0x55);
    CHECK(gtia.colpf1 == 0xfe && gtia.colpf2 == 0x54);
    gtia5200_mode_f_byte(&gtia, 1, 0, 0x80);
    CHECK(gtia.framebuffer[1][0] == 0x5e && gtia.framebuffer[1][1] == 0x54);
    gtia5200_begin_line(&gtia, 1);
    gtia5200_bitmap_byte(&gtia, 1, 0, 15, 1, 0x80);
    CHECK(gtia.framebuffer[1][31] == 0x20 && gtia.framebuffer[1][32] == 0x5e &&
          gtia.framebuffer[1][33] == 0x54 && gtia.framebuffer[1][288] == 0x20);
}

static void test_bitmap_modes(void) {
    static const struct { uint8_t mode, height, bytes; } modes[] = {
        {8, 8, 10}, {9, 4, 10}, {10, 4, 20}, {11, 2, 20},
        {12, 1, 20}, {13, 2, 40}, {14, 1, 40}, {15, 1, 40}
    };
    for (size_t t = 0; t < sizeof(modes) / sizeof(modes[0]); t++)
    for (unsigned width = 1; width <= 2; width++) {
        System5200 system;
        Antic5200 antic;
        GTIA5200 gtia;
        CPU6502 cpu;
        system5200_init(&system);
        antic5200_init(&antic);
        gtia5200_init(&gtia);
        system5200_attach_antic(&system, &antic, &cpu);
        system5200_attach_gtia(&system, &gtia);
        system.bios_loaded = true;
        system.bios[0x7fc] = 0;
        system.bios[0x7fd] = 2;
        system.ram[0x200] = 0x4c;
        system.ram[0x201] = 0;
        system.ram[0x202] = 2;
        system.ram[0x300] = 0x70;
        system.ram[0x301] = 0x70;
        system.ram[0x302] = 0x70;
        system.ram[0x303] = (uint8_t)(0x40 | modes[t].mode);
        system.ram[0x304] = 0;
        system.ram[0x305] = 0x10;
        system.ram[0x306] = 0x41;
        system.ram[0x307] = 0;
        system.ram[0x308] = 3;
        system.ram[0x1000] = 0x6c; /* 01,10,11,00 or 01101100 */
        system5200_write(&system, 0xc016, 0x22);
        system5200_write(&system, 0xc017, 0x44);
        system5200_write(&system, 0xc018, 0x66);
        system5200_write(&system, 0xc01a, 0x88);
        system5200_write(&system, 0xd402, 0x00);
        system5200_write(&system, 0xd403, 0x03);
        system5200_write(&system, 0xd400, (uint8_t)(0x20 | width));
        cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
        while (antic.scanline < 33) cpu_step(&cpu);
        system.ram[0x1000] = 0;
        while (antic.scanline < (unsigned)(33 + modes[t].height)) cpu_step(&cpu);
        unsigned bytes = width == 1 ? modes[t].bytes * 4 / 5 : modes[t].bytes;
        CHECK(!antic.unsupported && antic.screen_address == 0x1000 + bytes);
        bool two_bits = modes[t].mode == 8 || modes[t].mode == 10 ||
                        modes[t].mode == 13 || modes[t].mode == 14;
        unsigned pixel_width = GTIA5200_WIDTH / modes[t].bytes / (two_bits ? 4 : 8);
        unsigned start = width == 1 ? 32 : 0;
        for (unsigned y = 0; y < modes[t].height; y++) {
            CHECK(gtia.framebuffer[LINE32 + y][start] == (modes[t].mode == 15 ? 0x66 : two_bits ? 0x22 : 0x88));
            CHECK(gtia.framebuffer[LINE32 + y][start + pixel_width] == (modes[t].mode == 15 ? 0x64 : two_bits ? 0x44 : 0x22));
            CHECK(gtia.framebuffer[LINE32 + y][start + 2 * pixel_width] == (modes[t].mode == 15 ? 0x64 : two_bits ? 0x66 : 0x22));
            if (two_bits) CHECK(gtia.framebuffer[LINE32 + y][start + 3 * pixel_width] == 0x88);
            if (width == 1) CHECK(gtia.framebuffer[LINE32 + y][0] == 0x88 && gtia.framebuffer[LINE32 + y][319] == 0x88);
        }
        CHECK(gtia.framebuffer[LINE32 + modes[t].height][0] == 0x88);
    }
}

static void test_character_modes(void) {
    for (unsigned mode = 2; mode <= 7; mode++)
    for (unsigned width = 1; width <= 2; width++) {
        System5200 system;
        Antic5200 antic;
        GTIA5200 gtia;
        CPU6502 cpu;
        system5200_init(&system);
        antic5200_init(&antic);
        gtia5200_init(&gtia);
        system5200_attach_antic(&system, &antic, &cpu);
        system5200_attach_gtia(&system, &gtia);
        system.bios_loaded = true;
        system.bios[0x7fc] = 0;
        system.bios[0x7fd] = 2;
        system.ram[0x200] = 0x4c;
        system.ram[0x201] = 0;
        system.ram[0x202] = 2;
        system.ram[0x300] = 0x70;
        system.ram[0x301] = 0x70;
        system.ram[0x302] = 0x70;
        system.ram[0x303] = (uint8_t)(0x40 | mode);
        system.ram[0x304] = 0xf0;
        system.ram[0x305] = 0x1f;
        system.ram[0x306] = 0x41;
        system.ram[0x307] = 0;
        system.ram[0x308] = 3;
        system.ram[0x1ff0] = 0x81;
        if (mode == 2) {
            system.bios[8] = 0xc0;
            system.bios[9] = 0x80;
        } else {
            system.ram[0x2808] = 0xc0;
            system.ram[0x2809] = 0x80;
        }
        system5200_write(&system, 0xc016, 0x22);
        system5200_write(&system, 0xc017, 0x44);
        system5200_write(&system, 0xc018, 0x66);
        system5200_write(&system, 0xc019, 0xaa);
        system5200_write(&system, 0xc01a, 0x88);
        system5200_write(&system, 0xd409, mode == 2 ? 0xf9 : 0x29);
        system5200_write(&system, 0xd402, 0);
        system5200_write(&system, 0xd403, 3);
        system5200_write(&system, 0xd400, (uint8_t)(0x20 | width));
        cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
        while (antic.scanline < 33) cpu_step(&cpu);
        system.ram[0x1ff0] = 0;
        while (antic.scanline < 35) cpu_step(&cpu);
        unsigned start = width == 1 ? 32 : 0;
        CHECK(!antic.unsupported);
        if (mode <= 3) {
            CHECK(gtia.framebuffer[LINE32][start] == 0x64);
            CHECK(gtia.framebuffer[LINE32][start + 2] == 0x66);
            CHECK(gtia.framebuffer[LINE32 + 1][start] == 0x64);
            CHECK(gtia.framebuffer[LINE32 + 1][start + 1] == 0x66);
        } else if (mode <= 5) {
            CHECK(gtia.framebuffer[LINE32][start] == 0xaa);
            CHECK(gtia.framebuffer[LINE32][start + 2] == 0x88);
        } else {
            CHECK(gtia.framebuffer[LINE32][start] == 0x66);
            CHECK(gtia.framebuffer[LINE32][start + 4] == 0x88);
        }
        if (width == 1) CHECK(gtia.framebuffer[LINE32][0] == 0x88);
        CHECK(antic.screen_address == 0x1000 + (mode < 6 ? (width == 1 ? 16 : 24) : (width == 1 ? 0 : 4)));
    }
}

static void test_character_attributes(void) {
    Antic5200 antic;
    GTIA5200 gtia;
    antic5200_init(&antic);
    gtia5200_init(&gtia);
    antic.gtia = &gtia;
    antic.scanline = GTIA5200_FIRST_SCANLINE;
    antic.dmactl = 2;
    antic.instruction = 2;
    antic.lines_left = 8;
    antic.character_dma = true;
    antic.line_buffer[0] = 0x81;
    antic.chbase = 0x2b;
    gtia.colpf1 = 0x0e;
    gtia.colpf2 = 0x42;
    for (unsigned control = 0; control < 4; control++) {
        antic.chactl = (uint8_t)control;
        CHECK(antic5200_playfield_address(&antic) == 0x2808);
        antic5200_playfield_byte(&antic, 0x80);
        bool first = !(control & 1);
        if (control & 2) first = !first;
        CHECK(gtia.framebuffer[0][0] == (first ? 0x4e : 0x42));
        CHECK(gtia.framebuffer[0][1] == ((control & 2) ? 0x4e : 0x42));
    }
    antic.chactl = 4;
    CHECK(antic5200_playfield_address(&antic) == 0x280f);
    for (unsigned mode = 5; mode <= 7; mode += 2) {
        antic.instruction = (uint8_t)mode;
        antic.chbase = 0x2b;
        antic.line_buffer[0] = 1;
        for (unsigned row = 0; row < 16; row++) {
            antic.lines_left = (uint8_t)(16 - row);
            antic.mode_row = (uint8_t)row;
            unsigned base = mode == 5 ? 0x2800 : 0x2a00;
            CHECK(antic5200_playfield_address(&antic) == base + 8 + (7 - row / 2));
        }
    }

    antic.instruction = 3;
    antic.chactl = 0;
    for (unsigned row = 0; row < 10; row++) {
        antic.lines_left = (uint8_t)(10 - row);
        antic.mode_row = (uint8_t)row;
        antic.line_buffer[0] = 1;
        CHECK(antic5200_playfield_address(&antic) == 0x2808 + (row & 7));
        antic5200_playfield_byte(&antic, 0x80);
        CHECK(gtia.framebuffer[0][0] == (row < 8 ? 0x4e : 0x42));
        antic.line_buffer[0] = 0x61;
        CHECK(antic5200_playfield_address(&antic) == 0x2b08 + (row & 7));
        antic5200_playfield_byte(&antic, 0x80);
        CHECK(gtia.framebuffer[0][0] == (row >= 2 ? 0x4e : 0x42));
    }
}

static void test_character_colors(void) {
    GTIA5200 gtia;
    gtia5200_init(&gtia);
    gtia.colbk = 0x10;
    gtia.colpf0 = 0x20;
    gtia.colpf1 = 0x30;
    gtia.colpf2 = 0x40;
    gtia.colpf3 = 0x50;
    for (unsigned mode = 4; mode <= 5; mode++)
    for (unsigned attribute = 0; attribute < 2; attribute++) {
        gtia5200_character_byte(&gtia, 0, 0, (uint8_t)mode, 2,
                                attribute ? 0x80 : 0, 0x1b);
        CHECK(gtia.framebuffer[0][0] == 0x10);
        CHECK(gtia.framebuffer[0][2] == 0x20);
        CHECK(gtia.framebuffer[0][4] == 0x30);
        CHECK(gtia.framebuffer[0][6] == (attribute ? 0x50 : 0x40));
    }
    for (unsigned mode = 6; mode <= 7; mode++)
    for (unsigned color = 0; color < 4; color++) {
        gtia5200_character_byte(&gtia, 0, 0, (uint8_t)mode, 2,
                                (uint8_t)(color << 6), 0x80);
        CHECK(gtia.framebuffer[0][0] == 0x20 + color * 0x10);
        CHECK(gtia.framebuffer[0][1] == 0x20 + color * 0x10);
        CHECK(gtia.framebuffer[0][2] == 0x10);
    }
}

static void test_vertical_scroll_image(void) {
    System5200 system;
    Antic5200 antic;
    GTIA5200 gtia;
    CPU6502 cpu;
    system5200_init(&system);
    antic5200_init(&antic);
    gtia5200_init(&gtia);
    system5200_attach_antic(&system, &antic, &cpu);
    system5200_attach_gtia(&system, &gtia);
    system.bios_loaded = true;
    system.bios[0x7fd] = 2;
    system.ram[0x200] = 0x4c;
    system.ram[0x201] = 0;
    system.ram[0x202] = 2;
    system.ram[0x300] = 0x70;
    system.ram[0x301] = 0x70;
    system.ram[0x302] = 0x70;
    system.ram[0x303] = 0x62;
    system.ram[0x304] = 0x00;
    system.ram[0x305] = 0x10;
    system.ram[0x306] = 0x22;
    system.ram[0x307] = 0x02;
    system.ram[0x308] = 0x41;
    system.ram[0x309] = 0;
    system.ram[0x30a] = 3;
    for (unsigned column = 0; column < 40; column++) {
        system.ram[0x1000 + column] = 1;
        system.ram[0x1028 + column] = 2;
        system.ram[0x1050 + column] = 3;
    }
    for (unsigned row = 0; row < 8; row++) {
        system.ram[0x2808 + row] = row & 1 ? 0x80 : 0;
        system.ram[0x2810 + row] = row & 1 ? 0 : 0x80;
        system.ram[0x2818 + row] = row & 1 ? 0x80 : 0;
    }
    system5200_write(&system, 0xc017, 0x44);
    system5200_write(&system, 0xc018, 0x66);
    system5200_write(&system, 0xd405, 3);
    system5200_write(&system, 0xd409, 0x28);
    system5200_write(&system, 0xd402, 0);
    system5200_write(&system, 0xd403, 3);
    system5200_write(&system, 0xd400, 0x22);
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    while (antic.scanline < 50 && !antic.unsupported) cpu_step(&cpu);
    CHECK(!antic.unsupported && antic.screen_address == 0x1078);
    for (unsigned y = 0; y < 17; y++) {
        bool foreground = y < 5 ? ((y + 3) & 1) :
                          y < 13 ? !((y - 5) & 1) : ((y - 13) & 1);
        CHECK(gtia.framebuffer[LINE32 + y][0] == (foreground ? 0x64 : 0x66));
    }
}

static void test_horizontal_scroll_image(void) {
    for (unsigned mode_index = 0; mode_index < 2; mode_index++)
    for (unsigned width = 1; width <= 3; width++)
    for (unsigned scrolling = 0; scrolling <= 1; scrolling++)
    for (unsigned h = 0; h < 16; h++) {
        System5200 system;
        Antic5200 antic;
        GTIA5200 gtia;
        CPU6502 cpu;
        system5200_init(&system);
        antic5200_init(&antic);
        gtia5200_init(&gtia);
        system5200_attach_antic(&system, &antic, &cpu);
        system5200_attach_gtia(&system, &gtia);
        system.bios_loaded = true;
        system.bios[0x7fd] = 2;
        system.ram[0x200] = 0x4c;
        system.ram[0x201] = 0;
        system.ram[0x202] = 2;
        system.ram[0x300] = 0x70;
        system.ram[0x301] = 0x70;
        system.ram[0x302] = 0x70;
        system.ram[0x303] = (uint8_t)(0x40 | (scrolling ? 0x10 : 0) | (mode_index ? 6 : 15));
        system.ram[0x304] = 0;
        system.ram[0x305] = 0x10;
        system.ram[0x306] = 0x41;
        system.ram[0x307] = 0;
        system.ram[0x308] = 3;
        for (unsigned i = 0; i < 48; i++)
            system.ram[0x1000 + i] = mode_index ? (uint8_t)(1 + (i & 1)) :
                                                  (i & 1 ? 0x55 : 0xaa);
        system.ram[0x2808] = 0xaa;
        system.ram[0x2810] = 0x55;
        system5200_write(&system, 0xc016, 0x44);
        system5200_write(&system, 0xc017, 0x44);
        system5200_write(&system, 0xc018, 0x66);
        system5200_write(&system, 0xc01a, 0x20);
        system5200_write(&system, 0xd404, (uint8_t)h);
        system5200_write(&system, 0xd409, 0x28);
        system5200_write(&system, 0xd402, 0);
        system5200_write(&system, 0xd403, 3);
        system5200_write(&system, 0xd400, (uint8_t)(0x20 | width));
        cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
        while (antic.scanline < 33 && !antic.unsupported) cpu_step(&cpu);
        CHECK(!antic.unsupported);
        unsigned fetch = width + (scrolling && width < 3);
        unsigned count = (fetch == 1 ? 32 : fetch == 2 ? 40 : 48) / (mode_index ? 2 : 1);
        CHECK(antic.screen_address == 0x1000 + count);
        int origin = (fetch == 1 ? 32 : fetch == 2 ? 0 : -32) + (scrolling ? 2 * (int)h : 0);
        for (int x = 0; x < GTIA5200_WIDTH; x++) {
            bool inside = (width != 1 || (x >= 32 && x < 288)) && x >= origin &&
                          x < origin + (int)(count * (mode_index ? 16 : 8));
            uint8_t expected = 0x20;
            if (inside) {
                unsigned pixel = (unsigned)(x - origin);
                bool set = mode_index ? (((pixel / 16) + (pixel % 16) / 2) % 2 == 0) :
                                        (((pixel / 8) + pixel % 8) % 2 == 0);
                expected = mode_index ? (set ? 0x44 : 0x20) : (set ? 0x64 : 0x66);
            }
            CHECK(gtia.framebuffer[LINE32][x] == expected);
        }
    }
}

int main(void) {
    test_visible_scanline_bounds();
    test_mode_f();
    test_bitmap_modes();
    test_character_modes();
    test_vertical_scroll_image();
    test_horizontal_scroll_image();
    test_character_attributes();
    test_character_colors();
    puts("Atari 5200 video test passed");
    return 0;
}
