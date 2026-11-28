#include "../system2600.h"
#include "../tia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

typedef struct {
    uint16_t address;
    uint8_t value;
    int line, clock, timer;
} WriteEvent;

static WriteEvent events[16];
static unsigned event_count;
static RIOT *event_riot;

static void record_write(TIA *tia, uint16_t address, uint8_t value) {
    CHECK(event_count < sizeof(events) / sizeof(events[0]));
    events[event_count++] = (WriteEvent){address, value, tia->scanline, tia->clock,
                                         event_riot->timer};
    tia_write(tia, address, value);
}

typedef struct {
    TIA tia;
    RIOT riot;
    System2600 bus;
    CPU6502 cpu;
    Cartridge cart;
} Fixture;

static void setup(Fixture *fixture, const uint8_t *program, size_t length) {
    uint8_t rom[4096] = {0};
    CHECK(length <= 0xeff);
    memcpy(rom + 0x100, program, length);
    rom[0xffc] = 0;
    rom[0xffd] = 0xf1;
    memset(fixture, 0, sizeof(*fixture));
    CHECK(cart_init_from_bytes(&fixture->cart, rom, sizeof(rom), "timing test"));
    tia_init(&fixture->tia, NULL);
    riot_init(&fixture->riot);
    system2600_init(&fixture->bus, &fixture->tia, &fixture->riot, &fixture->cart,
                    tia_step, tia_read, record_write);
    event_riot = &fixture->riot;
    event_count = 0;
    cpu_init_with_bus(&fixture->cpu, system2600_cpu_bus(&fixture->bus));
    CHECK(fixture->tia.clock == 6);
}

static void cleanup(Fixture *fixture) {
    tia_destroy(&fixture->tia);
    cart_free(&fixture->cart);
}

static void check_pixel(const TIA *tia, int y, int x, uint8_t color) {
    const uint8_t *pixel = tia->framebuffer + (y * TIA_FB_WIDTH + x) * 3;
    if (memcmp(pixel, tia->color_lut[color], 3) != 0) {
        fprintf(stderr, "pixel (%d,%d) differs from color %02x\n", x, y, color);
        exit(EXIT_FAILURE);
    }
}

static void test_color_write_positions(void) {
    static const int clocks[] = {24, 66, 69, 147, 150, 225, 228};
    const uint8_t program[] = {0xa9, 0x46, 0x85, 0x09};
    for (unsigned i = 0; i < sizeof(clocks) / sizeof(clocks[0]); i++) {
        Fixture fixture;
        setup(&fixture, program, sizeof(program));
        tia_write(&fixture.tia, 0x09, 0x26);
        tia_step(&fixture.tia, clocks[i] - 15 - 6);
        CHECK(cpu_step(&fixture.cpu) == 2);
        CHECK(cpu_step(&fixture.cpu) == 3);
        CHECK(event_count == 1 && events[0].address == 9 && events[0].value == 0x46);
        CHECK(events[0].line == clocks[i] / 228 && events[0].clock == clocks[i] % 228);
        if (fixture.tia.scanline == 0) tia_step(&fixture.tia, 228 - fixture.tia.clock);
        tia_step(&fixture.tia, 228);
        for (int x = 0; x < TIA_FB_WIDTH; x++) {
            check_pixel(&fixture.tia, 0, x, x + 68 < clocks[i] ? 0x26 : 0x46);
            check_pixel(&fixture.tia, 1, x, 0x46);
        }
        cleanup(&fixture);
    }
}

static void test_playfield_second_half_write(void) {
    const uint8_t program[] = {0xa9, 0xf0, 0x85, 0x0d};
    Fixture fixture;
    setup(&fixture, program, sizeof(program));
    tia_write(&fixture.tia, 0x08, 0x0e);
    tia_step(&fixture.tia, 144 - 15 - 6);
    CHECK(cpu_step(&fixture.cpu) == 2);
    CHECK(cpu_step(&fixture.cpu) == 3);
    CHECK(event_count == 1 && events[0].address == 0x0d);
    CHECK(events[0].line == 0 && events[0].clock == 144);
    tia_step(&fixture.tia, 228 - fixture.tia.clock);
    for (int x = 0; x < TIA_FB_WIDTH; x++)
        check_pixel(&fixture.tia, 0, x, x >= 80 && x < 96 ? 0x0e : 0);
    cleanup(&fixture);
}

static void test_color_write_sweep(void) {
    const uint8_t program[] = {0xa9, 0x46, 0x85, 0x09};
    for (int standard = VIDEO_NTSC; standard <= VIDEO_PAL; standard++) {
        Fixture fixture;
        setup(&fixture, program, sizeof(program));
        for (int clock = 21; clock <= 228; clock++) {
            tia_destroy(&fixture.tia);
            tia_init(&fixture.tia, NULL);
            tia_set_video_standard(&fixture.tia, (VideoStandard)standard);
            fixture.cpu.PC = 0xf100;
            event_count = 0;
            tia_write(&fixture.tia, 9, 0x26);
            tia_step(&fixture.tia, clock - 15);
            CHECK(cpu_step(&fixture.cpu) == 2);
            CHECK(cpu_step(&fixture.cpu) == 3);
            CHECK(event_count == 1 && events[0].address == 9);
            CHECK(events[0].line == clock / 228 && events[0].clock == clock % 228);
            if (fixture.tia.scanline == 0) tia_step(&fixture.tia, 228 - fixture.tia.clock);
            tia_step(&fixture.tia, 228);
            for (int x = 0; x < TIA_FB_WIDTH; x++) {
                check_pixel(&fixture.tia, 0, x, x + 68 < clock ? 0x26 : 0x46);
                check_pixel(&fixture.tia, 1, x, 0x46);
            }
        }
        cleanup(&fixture);
    }
}

static void test_playfield_registers_second_half(void) {
    static const struct {
        uint8_t reg, value;
        int clock, first, last;
    } cases[2][3] = {
        {{0x0d, 0xf0, 144, 80, 96}, {0x0e, 0xff, 156, 96, 128},
         {0x0f, 0xff, 186, 128, 160}},
        {{0x0f, 0xff, 144, 80, 112}, {0x0e, 0xff, 174, 112, 144},
         {0x0d, 0xf0, 207, 144, 160}}
    };
    for (unsigned reflected = 0; reflected < 2; reflected++)
    for (unsigned i = 0; i < 3; i++) {
        uint8_t program[] = {0xa9, cases[reflected][i].value, 0x85, cases[reflected][i].reg};
        Fixture fixture;
        setup(&fixture, program, sizeof(program));
        tia_write(&fixture.tia, 0x08, 0x0e);
        tia_write(&fixture.tia, 0x0a, (uint8_t)reflected);
        tia_step(&fixture.tia, cases[reflected][i].clock - 21);
        CHECK(cpu_step(&fixture.cpu) == 2 && cpu_step(&fixture.cpu) == 3);
        CHECK(event_count == 1 && events[0].address == cases[reflected][i].reg);
        CHECK(events[0].clock == cases[reflected][i].clock && events[0].line == 0);
        tia_step(&fixture.tia, 228 - fixture.tia.clock);
        for (int x = 0; x < TIA_FB_WIDTH; x++) {
            bool foreground = x >= cases[reflected][i].first && x < cases[reflected][i].last;
            check_pixel(&fixture.tia, 0, x, foreground ? 0x0e : 0);
        }
        cleanup(&fixture);
    }
}

static bool playfield_register_pixel(unsigned reg, bool reflected, int x) {
    int bit = x / 4;
    if (bit >= 20) bit = reflected ? 39 - bit : bit - 20;
    return reg == 0 ? bit < 4 : reg == 1 ? bit >= 4 && bit < 12 : bit >= 12;
}

static void test_playfield_write_boundaries(void) {
    /* Stella TIA: PF writes are delayed two clocks; the playfield samples every four pixels. */
    for (unsigned reflected = 0; reflected < 2; reflected++)
    for (unsigned reg = 0; reg < 3; reg++) {
        uint8_t program[] = {0xa9, reg == 0 ? 0xf0 : 0xff, 0x85, (uint8_t)(0x0d + reg)};
        Fixture fixture;
        setup(&fixture, program, sizeof(program));
        for (int group = 0; group < 160; group += 4) {
            if (!playfield_register_pixel(reg, reflected != 0, group)) continue;
            for (int offset = -4; offset <= 4; offset++) {
                int write_clock = 68 + group + offset;
                tia_destroy(&fixture.tia);
                tia_init(&fixture.tia, NULL);
                fixture.cpu.PC = 0xf100;
                event_count = 0;
                tia_write(&fixture.tia, 0x08, 0x0e);
                tia_write(&fixture.tia, 0x0a, (uint8_t)reflected);
                tia_step(&fixture.tia, write_clock - 15);
                CHECK(cpu_step(&fixture.cpu) == 2 && cpu_step(&fixture.cpu) == 3);
                CHECK(event_count == 1 && events[0].address == 0x0d + reg);
                CHECK(events[0].line == write_clock / 228 && events[0].clock == write_clock % 228);
                if (fixture.tia.scanline == 0) tia_step(&fixture.tia, 228 - fixture.tia.clock);
                tia_step(&fixture.tia, 228);
                for (int x = 0; x < TIA_FB_WIDTH; x++) {
                    bool bit = playfield_register_pixel(reg, reflected != 0, x);
                    bool active = bit && 68 + (x & ~3) >= write_clock + 2;
                    check_pixel(&fixture.tia, 0, x, active ? 0x0e : 0);
                    check_pixel(&fixture.tia, 1, x, bit ? 0x0e : 0);
                }
            }
        }
        cleanup(&fixture);
    }
}

static void test_playfield_pending_writes(void) {
    TIA tia = {0};
    tia_init(&tia, NULL);
    tia_write(&tia, 0x0d, 0x10);
    tia_step(&tia, 1);
    tia_write(&tia, 0x0d, 0x20);
    tia_write(&tia, 0x0e, 0x80);
    tia_step(&tia, 1);
    CHECK(tia.pf0 == 0 && tia.pf1 == 0);
    tia_step(&tia, 1);
    CHECK(tia.pf0 == 0x10 && tia.pf1 == 0);
    tia_step(&tia, 1);
    CHECK(tia.pf0 == 0x20 && tia.pf1 == 0x80);
    tia_write(&tia, 0x0f, 0xff);
    tia_write(&tia, 0x0f, 0x55);
    tia_step(&tia, 3);
    CHECK(tia.pf2 == 0x55);
    tia_destroy(&tia);
    tia_init(&tia, NULL);
    CHECK(tia.pf0 == 0 && tia.pf1 == 0 && tia.pf2 == 0);
    tia_step(&tia, 3);
    CHECK(tia.pf0 == 0 && tia.pf1 == 0 && tia.pf2 == 0);
    tia_destroy(&tia);
}

static void test_playfield_clear_and_collision(void) {
    const uint8_t program[] = {0xa9, 0x00, 0x85, 0x0d};
    Fixture fixture;
    setup(&fixture, program, sizeof(program));
    tia_write(&fixture.tia, 0x08, 0x0e);
    tia_write(&fixture.tia, 0x0d, 0xf0);
    tia_write(&fixture.tia, 0x06, 0x08);
    tia_write(&fixture.tia, 0x1b, 0xff);
    fixture.tia.p0_x = 8;
    tia_step(&fixture.tia, 59);
    CHECK(cpu_step(&fixture.cpu) == 2 && cpu_step(&fixture.cpu) == 3);
    CHECK(event_count == 1 && events[0].clock == 80);
    tia_step(&fixture.tia, 228 - fixture.tia.clock);
    for (int x = 0; x < TIA_FB_WIDTH; x++) {
        uint8_t color = x >= 8 && x < 16 ? 0x08 : x < 16 ? 0x0e : 0;
        check_pixel(&fixture.tia, 0, x, color);
    }
    CHECK(fixture.tia.cxp0fb & 0x80);
    tia_write(&fixture.tia, 0x2c, 0);
    tia_step(&fixture.tia, 228);
    CHECK(!(fixture.tia.cxp0fb & 0x80));
    cleanup(&fixture);
}

static void test_object_color_write(void) {
    for (unsigned player = 0; player < 2; player++) {
        uint8_t program[] = {0xa9, 0x46, 0x85, player ? 0x06 : 0x08};
        Fixture fixture;
        setup(&fixture, program, sizeof(program));
        if (player) {
            tia_write(&fixture.tia, 0x06, 0x26);
            tia_write(&fixture.tia, 0x04, 7);
            tia_write(&fixture.tia, 0x10, 0);
            tia_write(&fixture.tia, 0x1b, 0xff);
            CHECK(fixture.tia.p0_x == 4);
        } else {
            tia_write(&fixture.tia, 0x08, 0x26);
            tia_write(&fixture.tia, 0x0d, 0xf0);
            tia_write(&fixture.tia, 0x0e, 0xff);
            tia_write(&fixture.tia, 0x0f, 0xff);
        }
        tia_step(&fixture.tia, 84 - 21);
        CHECK(cpu_step(&fixture.cpu) == 2 && cpu_step(&fixture.cpu) == 3);
        CHECK(event_count == 1 && events[0].clock == 84);
        CHECK(events[0].address == (player ? 6 : 8));
        tia_step(&fixture.tia, 228 - fixture.tia.clock);
        for (int x = 0; x < TIA_FB_WIDTH; x++) {
            bool foreground = !player || (x >= 4 && x < 36);
            check_pixel(&fixture.tia, 0, x, foreground ? (x < 16 ? 0x26 : 0x46) : 0);
        }
        cleanup(&fixture);
    }
}

static void test_multiple_color_writes(void) {
    const uint8_t program[] = {
        0xa9, 0x46, 0x85, 0x09,
        0xa9, 0x86, 0x85, 0x09,
        0xa9, 0xc6, 0x85, 0x09
    };
    for (int standard = VIDEO_NTSC; standard <= VIDEO_PAL; standard++) {
        Fixture fixture;
        setup(&fixture, program, sizeof(program));
        tia_set_video_standard(&fixture.tia, (VideoStandard)standard);
        tia_write(&fixture.tia, 9, 0x26);
        tia_step(&fixture.tia, 63);
        for (int i = 0; i < 3; i++) {
            CHECK(cpu_step(&fixture.cpu) == 2);
            CHECK(cpu_step(&fixture.cpu) == 3);
            CHECK(event_count == (unsigned)i + 1);
            CHECK(events[i].address == 9 && events[i].line == 0 &&
                  events[i].clock == 84 + 15 * i);
        }
        tia_step(&fixture.tia, 228 - fixture.tia.clock);
        for (int x = 0; x < TIA_FB_WIDTH; x++) {
            uint8_t color = x < 16 ? 0x26 : x < 31 ? 0x46 : x < 46 ? 0x86 : 0xc6;
            check_pixel(&fixture.tia, 0, x, color);
        }
        cleanup(&fixture);
    }
}

static void test_branch_write_timing(void) {
    const uint8_t program[] = {
        0xd0, 0x02, 0xea, 0xea, /* BNE skips two NOPs */
        0xa9, 0x46, 0x85, 0x09
    };
    for (unsigned taken = 0; taken < 2; taken++) {
        Fixture fixture;
        setup(&fixture, program, sizeof(program));
        tia_write(&fixture.tia, 9, 0x26);
        tia_step(&fixture.tia, 54);
        if (taken) fixture.cpu.status &= (uint8_t)~CPU_FLAG_Z;
        else fixture.cpu.status |= CPU_FLAG_Z;
        CHECK(cpu_step(&fixture.cpu) == (taken ? 3 : 2));
        if (!taken) {
            CHECK(cpu_step(&fixture.cpu) == 2);
            CHECK(cpu_step(&fixture.cpu) == 2);
        }
        CHECK(cpu_step(&fixture.cpu) == 2);
        CHECK(cpu_step(&fixture.cpu) == 3);
        int clock = taken ? 84 : 93;
        CHECK(event_count == 1 && events[0].address == 9 && events[0].clock == clock);
        tia_step(&fixture.tia, 228 - fixture.tia.clock);
        for (int x = 0; x < TIA_FB_WIDTH; x++)
            check_pixel(&fixture.tia, 0, x, x + 68 < clock ? 0x26 : 0x46);
        cleanup(&fixture);
    }

    uint8_t crossing[0x105] = {0};
    crossing[0xfd] = 0xd0;
    crossing[0xfe] = 0x02;
    crossing[0x101] = 0xa9;
    crossing[0x102] = 0x46;
    crossing[0x103] = 0x85;
    crossing[0x104] = 0x09;
    Fixture fixture;
    setup(&fixture, crossing, sizeof(crossing));
    tia_write(&fixture.tia, 9, 0x26);
    tia_step(&fixture.tia, 54);
    fixture.cpu.PC = 0xf1fd;
    fixture.cpu.status &= (uint8_t)~CPU_FLAG_Z;
    CHECK(cpu_step(&fixture.cpu) == 4);
    CHECK(fixture.cpu.PC == 0xf201);
    CHECK(cpu_step(&fixture.cpu) == 2);
    CHECK(cpu_step(&fixture.cpu) == 3);
    CHECK(event_count == 1 && events[0].address == 9 && events[0].clock == 87);
    tia_step(&fixture.tia, 228 - fixture.tia.clock);
    for (int x = 0; x < TIA_FB_WIDTH; x++)
        check_pixel(&fixture.tia, 0, x, x + 68 < 87 ? 0x26 : 0x46);
    cleanup(&fixture);
}

static void test_rmw_bus_writes(void) {
    const uint8_t program[] = {0xe6, 0x09}; /* INC COLUBK */
    Fixture fixture;
    setup(&fixture, program, sizeof(program));
    tia_write(&fixture.tia, 9, 0x46);
    tia_step(&fixture.tia, 54);
    CHECK(cpu_step(&fixture.cpu) == 5);
    CHECK(event_count == 2);
    CHECK(events[0].address == 9 && events[0].value == 0 && events[0].clock == 72);
    CHECK(events[1].address == 9 && events[1].value == 1 && events[1].clock == 75);
    tia_step(&fixture.tia, 228 - fixture.tia.clock);
    for (int x = 0; x < TIA_FB_WIDTH; x++)
        check_pixel(&fixture.tia, 0, x, x < 4 ? 0x46 : 0);
    cleanup(&fixture);
}

static void test_wsync_positions(void) {
    static const int clocks[] = {3, 69, 225, 228};
    const uint8_t program[] = {
        0xa9, 0x00, 0x85, 0x02, /* LDA #0; STA WSYNC */
        0xa9, 0x02, 0x85, 0x00  /* LDA #2; STA VSYNC */
    };
    for (unsigned i = 0; i < sizeof(clocks) / sizeof(clocks[0]); i++) {
        Fixture fixture;
        setup(&fixture, program, sizeof(program));
        int start = clocks[i] == 3 ? 216 : clocks[i] - 15;
        tia_step(&fixture.tia, start - 6);
        riot_write(&fixture.riot, 0x294, 200);
        CHECK(system2600_step_frame(&fixture.bus, &fixture.cpu));
        CHECK(event_count == 2);
        CHECK(events[0].address == 2 && events[0].clock == clocks[i] % 228);
        CHECK(events[0].line == (clocks[i] == 3 || clocks[i] == 228 ? 1 : 0));
        CHECK(events[1].address == 0 && events[1].value == 2);
        CHECK(events[1].line == (clocks[i] == 3 ? 2 : 1) && events[1].clock == 15);
        int wait = clocks[i] == 228 ? 0 : 228 - clocks[i];
        CHECK(events[1].timer == 200 - (15 + wait + 15) / 3);
        CHECK(!fixture.tia.wsync && fixture.tia.frame_ready);
        cleanup(&fixture);
    }
}

int main(void) {
    test_color_write_positions();
    test_color_write_sweep();
    test_playfield_second_half_write();
    test_playfield_registers_second_half();
    test_playfield_write_boundaries();
    test_playfield_pending_writes();
    test_playfield_clear_and_collision();
    test_object_color_write();
    test_multiple_color_writes();
    test_branch_write_timing();
    test_rmw_bus_writes();
    test_wsync_positions();
    puts("Atari 2600 CPU and scanline timing tests passed");
    return EXIT_SUCCESS;
}
