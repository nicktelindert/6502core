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
    test_playfield_second_half_write();
    test_branch_write_timing();
    test_rmw_bus_writes();
    test_wsync_positions();
    puts("Atari 2600 CPU and scanline timing tests passed");
    return EXIT_SUCCESS;
}
