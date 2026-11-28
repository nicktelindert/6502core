#include "../antic5200.h"
#include "../gtia5200.h"
#include "../system5200.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); \
} } while (0)

static void test_players_and_collisions(void) {
    GTIA5200 gtia;
    gtia5200_init(&gtia);
    gtia5200_write(&gtia, 0xc020, 0x30);
    gtia5200_write(&gtia, 0xc001, 0x30);
    gtia5200_write(&gtia, 0xc00d, 0x80);
    gtia5200_write(&gtia, 0xc00e, 0x80);
    gtia5200_write(&gtia, 0xc012, 0x22);
    gtia5200_write(&gtia, 0xc013, 0x44);
    gtia5200_write(&gtia, 0xc01b, 1);
    gtia5200_begin_line(&gtia, 0);
    gtia5200_end_line(&gtia, 0);
    CHECK(gtia.framebuffer[0][0] == 0x22 && gtia.framebuffer[0][1] == 0x22);
    CHECK(gtia.framebuffer[0][2] == 0);
    CHECK(gtia5200_read(&gtia, 0xc02c) == 2);
    CHECK(gtia5200_read(&gtia, 0xc00d) == 1);
    gtia5200_write(&gtia, 0xc01e, 0);
    CHECK(gtia5200_read(&gtia, 0xc00c) == 0);
    gtia5200_write(&gtia, 0xc001, 0);
    gtia5200_write(&gtia, 0xc008, 3);
    gtia5200_begin_line(&gtia, 1);
    gtia5200_end_line(&gtia, 1);
    CHECK(gtia.framebuffer[1][7] == 0x22 && gtia.framebuffer[1][8] == 0);
    gtia5200_write(&gtia, 0xc000, 0x2f);
    gtia5200_begin_line(&gtia, 2);
    gtia5200_end_line(&gtia, 2);
    CHECK(gtia.framebuffer[2][0] == 0x22 && gtia.framebuffer[2][5] == 0x22);
    CHECK(gtia.framebuffer[2][6] == 0);
}

static void test_playfield_and_missile(void) {
    GTIA5200 gtia;
    gtia5200_init(&gtia);
    gtia5200_write(&gtia, 0xc000, 0x30);
    gtia5200_write(&gtia, 0xc004, 0x30);
    gtia5200_write(&gtia, 0xc00d, 0x80);
    gtia5200_write(&gtia, 0xc011, 2);
    gtia5200_write(&gtia, 0xc012, 0x22);
    gtia5200_write(&gtia, 0xc016, 0x44);
    gtia5200_write(&gtia, 0xc01b, 1);
    gtia5200_begin_line(&gtia, 0);
    gtia5200_bitmap_byte(&gtia, 0, 0, 14, 2, 0x40);
    gtia5200_end_line(&gtia, 0);
    CHECK(gtia.framebuffer[0][0] == 0x22);
    CHECK(gtia5200_read(&gtia, 0xc000) == 1);
    CHECK(gtia5200_read(&gtia, 0xc004) == 1);
    CHECK(gtia5200_read(&gtia, 0xc008) == 1);
    gtia5200_write(&gtia, 0xc01b, 4);
    gtia5200_begin_line(&gtia, 1);
    gtia5200_bitmap_byte(&gtia, 1, 0, 14, 2, 0x40);
    gtia5200_end_line(&gtia, 1);
    CHECK(gtia.framebuffer[1][0] == 0x44);
    gtia5200_write(&gtia, 0xc01e, 0);
    gtia5200_begin_line(&gtia, 2);
    gtia5200_bitmap_byte(&gtia, 2, 0, 15, 2, 0x40);
    gtia5200_end_line(&gtia, 2);
    CHECK(gtia5200_read(&gtia, 0xc004) == 4);
}

static void test_scrolled_playfield_collision(void) {
    GTIA5200 gtia;
    gtia5200_init(&gtia);
    gtia5200_write(&gtia, 0xc000, 0x40);
    gtia5200_write(&gtia, 0xc00d, 0x80);
    gtia5200_begin_line(&gtia, 0);
    gtia5200_bitmap_window(&gtia, 0, 0, 14, 30, 32, 288, 0x55);
    CHECK(gtia.playfield_collision[30] == 0 && gtia.playfield_collision[31] == 0);
    CHECK(gtia.playfield_collision[32] == 1 && gtia.playfield_collision[33] == 1);
    gtia5200_end_line(&gtia, 0);
    CHECK(gtia5200_read(&gtia, 0xc004) == 1);
}

static void test_priorities_and_sizes(void) {
    GTIA5200 gtia;
    gtia5200_init(&gtia);
    gtia5200_write(&gtia, 0xc000, 0x30);
    gtia5200_write(&gtia, 0xc002, 0x30);
    gtia5200_write(&gtia, 0xc00d, 0x80);
    gtia5200_write(&gtia, 0xc00f, 0x80);
    gtia5200_write(&gtia, 0xc012, 0x20);
    gtia5200_write(&gtia, 0xc014, 0x60);
    gtia5200_write(&gtia, 0xc016, 0x40);
    gtia5200_write(&gtia, 0xc018, 0x80);
    static const uint8_t expected[4][3] = {
        {0x20, 0x20, 0x20}, {0x20, 0x20, 0x20},
        {0x20, 0x80, 0x40}, {0x20, 0x20, 0x40}
    };
    for (unsigned pri = 0; pri < 4; pri++)
    for (unsigned pf = 0; pf < 3; pf++) {
        gtia5200_write(&gtia, 0xc01b, (uint8_t)(1u << pri));
        gtia5200_begin_line(&gtia, 0);
        if (pf) gtia5200_bitmap_byte(&gtia, 0, 0, 14, 2, pf == 1 ? 0xc0 : 0x40);
        gtia5200_end_line(&gtia, 0);
        CHECK(gtia.framebuffer[0][0] == expected[pri][pf]);
    }
    gtia5200_write(&gtia, 0xc00d, 0);
    static const uint8_t expected_p2[4][3] = {
        {0x60, 0x60, 0x60}, {0x60, 0x80, 0x40},
        {0x60, 0x80, 0x40}, {0x60, 0x60, 0x40}
    };
    for (unsigned pri = 0; pri < 4; pri++)
    for (unsigned pf = 0; pf < 3; pf++) {
        gtia5200_write(&gtia, 0xc01b, (uint8_t)(1u << pri));
        gtia5200_begin_line(&gtia, 0);
        if (pf) gtia5200_bitmap_byte(&gtia, 0, 0, 14, 2, pf == 1 ? 0xc0 : 0x40);
        gtia5200_end_line(&gtia, 0);
        CHECK(gtia.framebuffer[0][0] == expected_p2[pri][pf]);
    }
    gtia5200_write(&gtia, 0xc00d, 0x80);
    gtia5200_write(&gtia, 0xc00f, 0);
    for (unsigned size = 0; size < 4; size++) {
        static const unsigned pixels[] = {2, 4, 2, 8};
        gtia5200_write(&gtia, 0xc008, (uint8_t)size);
        gtia5200_begin_line(&gtia, 0);
        gtia5200_end_line(&gtia, 0);
        CHECK(gtia.framebuffer[0][pixels[size] - 1] == 0x20);
        CHECK(gtia.framebuffer[0][pixels[size]] == 0);
    }
}

static void test_antic_line_integration(void) {
    System5200 system;
    Antic5200 antic;
    GTIA5200 gtia;
    CPU6502 cpu = {0};
    system5200_init(&system);
    antic5200_init(&antic);
    gtia5200_init(&gtia);
    system5200_attach_antic(&system, &antic, &cpu);
    system5200_attach_gtia(&system, &gtia);
    system5200_write(&system, 0xc000, 0x30);
    system5200_write(&system, 0xc00d, 0x80);
    system5200_write(&system, 0xc012, 0x26);
    while (antic.scanline != 32 || antic.cycle != 106) system5200_tick(&system, 1);
    CHECK(gtia.framebuffer[32 - GTIA5200_FIRST_SCANLINE][0] == 0x26);
}

static void test_player_dma(void) {
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
    system.ram[0x1b20] = 0x02;
    system.ram[0x1c20] = 0x80;
    system.ram[0x1d20] = 0x40;
    system.ram[0x1e20] = 0x20;
    system.ram[0x1f20] = 0x10;
    system.ram[0x1c21] = 0x40;
    system.ram[0x1b21] = 0x01;
    system5200_write(&system, 0xc000, 0x30);
    system5200_write(&system, 0xc004, 0x38);
    system5200_write(&system, 0xc012, 0x26);
    system5200_write(&system, 0xc01d, 3);
    system5200_write(&system, 0xd407, 0x1f);
    system5200_write(&system, 0xd400, 0x18);
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    while (antic.scanline < 33 || antic.cycle < 110) cpu_step(&cpu);
    CHECK(gtia.framebuffer[32 - GTIA5200_FIRST_SCANLINE][0] == 0x26);
    CHECK(gtia.framebuffer[32 - GTIA5200_FIRST_SCANLINE][16] == 0x26);
    CHECK(gtia.grafp[0] == 0x40 && gtia.grafm == 0x01);
    CHECK(gtia.framebuffer[33 - GTIA5200_FIRST_SCANLINE][0] == 0 &&
          gtia.framebuffer[33 - GTIA5200_FIRST_SCANLINE][2] == 0x26);
    CHECK(!antic.unsupported);
}

static void test_dma_gating_and_vdelay(void) {
    GTIA5200 gtia;
    gtia5200_init(&gtia);
    gtia5200_write(&gtia, 0xc00d, 0x55);
    gtia5200_write(&gtia, 0xc011, 0x55);
    gtia5200_dma_player(&gtia, 0, 0xaa, 32);
    gtia5200_dma_missiles(&gtia, 0xaa, 32);
    CHECK(gtia.grafp[0] == 0x55 && gtia.grafm == 0x55);
    gtia5200_write(&gtia, 0xc01d, 3);
    gtia5200_write(&gtia, 0xc01c, 0x11);
    gtia5200_dma_player(&gtia, 0, 0xaa, 32);
    gtia5200_dma_missiles(&gtia, 0xaa, 32);
    CHECK(gtia.grafp[0] == 0x55 && gtia.grafm == 0xa9);
    gtia5200_dma_player(&gtia, 0, 0xaa, 33);
    gtia5200_dma_missiles(&gtia, 0xaa, 33);
    CHECK(gtia.grafp[0] == 0xaa && gtia.grafm == 0xaa);
}

static void test_two_line_dma(void) {
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
    system.ram[0x1e10] = 0x80;
    system5200_write(&system, 0xc000, 0x30);
    system5200_write(&system, 0xc012, 0x26);
    system5200_write(&system, 0xc01d, 2);
    system5200_write(&system, 0xd407, 0x1f);
    system5200_write(&system, 0xd400, 8);
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    while (antic.scanline < 32 || antic.cycle < 110) cpu_step(&cpu);
    system.ram[0x1e10] = 0x40;
    while (antic.scanline < 33 || antic.cycle < 110) cpu_step(&cpu);
    CHECK(gtia.framebuffer[32 - GTIA5200_FIRST_SCANLINE][0] == 0x26);
    CHECK(gtia.framebuffer[33 - GTIA5200_FIRST_SCANLINE][0] == 0 &&
          gtia.framebuffer[33 - GTIA5200_FIRST_SCANLINE][2] == 0x26);
}

int main(void) {
    test_players_and_collisions();
    test_playfield_and_missile();
    test_scrolled_playfield_collision();
    test_priorities_and_sizes();
    test_antic_line_integration();
    test_player_dma();
    test_dma_gating_and_vdelay();
    test_two_line_dma();
    puts("GTIA 5200 player/missile tests passed");
    return 0;
}
