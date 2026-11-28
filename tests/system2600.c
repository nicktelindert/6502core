#include "../bus.h"
#include "../cpu6502.h"
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

static int tia_reads, tia_writes, cart_reads, cart_writes;
static uint16_t last_address;
static uint8_t last_value;

static uint8_t spy_tia_read(TIA *tia, uint16_t address) {
    (void)tia;
    tia_reads++;
    last_address = address;
    return 0xA6;
}

static void spy_tia_write(TIA *tia, uint16_t address, uint8_t value) {
    (void)tia;
    tia_writes++;
    last_address = address;
    last_value = value;
}

static uint8_t spy_cart_read(Cartridge *cart, uint16_t address) {
    (void)cart;
    cart_reads++;
    last_address = address;
    return 0xC5;
}

static void spy_cart_write(Cartridge *cart, uint16_t address, uint8_t value) {
    (void)cart;
    cart_writes++;
    last_address = address;
    last_value = value;
}

static void test_address_decode(void) {
    TIA tia = {0};
    RIOT riot;
    Bus bus;
    Cartridge cart = {0};
    cart.type = CART_TYPE_4K;
    cart.read = spy_cart_read;
    cart.write = spy_cart_write;
    bus_init(&bus, &tia, &riot, &cart, NULL, spy_tia_read, spy_tia_write);

    for (unsigned address = 0; address <= 0xFFFF; address++) {
        unsigned physical = address & 0x1FFF;
        int is_cart = (physical & 0x1000) != 0;
        int is_tia = !is_cart && (physical & 0x80) == 0;
        int is_ram = !is_cart && !is_tia && (physical & 0x200) == 0;
        riot_init(&riot);
        riot.timer = 0x5C;
        riot.interval = 1024;
        riot.swcha = 0x12;
        riot.swacnt = 0x34;
        riot.swchb = 0x56;
        riot.swbcnt = 0x78;
        static const uint8_t riot_values[] = {0x12, 0x34, 0x56, 0x78, 0x5C, 0, 0x5C, 0};
        bus.ram[physical & 0x7F] = 0x9B;
        tia_reads = cart_reads = tia_writes = cart_writes = 0;
        bus_reset_cycles_this_step(&bus);
        uint8_t value = bus_read(&bus, (uint16_t)address);
        CHECK(bus_get_cycles_this_step(&bus) == 1);
        CHECK(tia_reads == is_tia && cart_reads == is_cart);
        CHECK(value == (is_cart ? 0xC5 : is_tia ? 0xA6 :
                        is_ram ? 0x9B : riot_values[physical & 7]));
        CHECK(bus.data_bus == value);
        if (is_tia || is_cart) CHECK(last_address == physical);

        riot_init(&riot);
        riot.timer = 0x5C;
        riot.interval = 1024;
        bus.ram[physical & 0x7F] = 0x9B;
        bus_reset_cycles_this_step(&bus);
        bus_write(&bus, (uint16_t)address, 0x6D);
        CHECK(bus_get_cycles_this_step(&bus) == 1);
        CHECK(tia_writes == is_tia && cart_writes == is_cart);
        CHECK(bus.data_bus == 0x6D);
        CHECK(bus.ram[physical & 0x7F] == (is_ram ? 0x6D : 0x9B));
        if (is_tia || is_cart) {
            CHECK(last_address == (is_tia ? (physical & 0x3F) : physical));
            CHECK(last_value == 0x6D);
        }
    }
}

static void test_synthetic_cartridge_program(void) {
    uint8_t rom[4096] = {0};
    const uint8_t program[] = {
        0xA9, 0x2A,       /* LDA #$2A */
        0x85, 0x80,       /* STA $80 */
        0xE6, 0x80,       /* INC $80 */
        0xA5, 0x80,       /* LDA $80 */
        0x85, 0x09,       /* STA COLUBK */
        0x8D, 0x95, 0x02, /* STA TIM8T */
        0xAD, 0x84, 0x02, /* LDA INTIM */
    };
    memcpy(rom + 0x100, program, sizeof(program));
    rom[0xFFC] = 0x00;
    rom[0xFFD] = 0xF1;
    Cartridge cart;
    TIA tia = {0};
    RIOT riot;
    Bus bus;
    CPU6502 cpu;
    CHECK(cart_init_from_bytes(&cart, rom, sizeof(rom), "system test"));
    CHECK(cart.type == CART_TYPE_4K);
    tia_init(&tia, NULL);
    riot_init(&riot);
    bus_init(&bus, &tia, &riot, &cart, tia_step, tia_read, tia_write);
    cpu_init(&cpu, &bus);
    CHECK(cpu.PC == 0xF100 && tia.clock == 6);
    static const int cycles[] = {2, 3, 5, 3, 3, 4, 4};
    for (size_t i = 0; i < sizeof(cycles) / sizeof(cycles[0]); i++) {
        CHECK(cpu_step(&cpu) == cycles[i]);
    }
    CHECK(bus.ram[0] == 0x2B);
    CHECK(tia.colubk == 0x2B);
    CHECK(cpu.A == 0x2A);
    CHECK(riot.interval == 8);
    CHECK(tia.clock == 78);
    tia_destroy(&tia);
    cart_free(&cart);
}

static int vsync_clock, vsync_scanline, vsync_timer;
static RIOT *observed_riot;

static void observe_tia_write(TIA *tia, uint16_t address, uint8_t value) {
    if (address == 0 && (value & 2)) {
        vsync_clock = tia->clock;
        vsync_scanline = tia->scanline;
        vsync_timer = observed_riot->timer;
    }
    tia_write(tia, address, value);
}

static void test_wsync_system_stall(void) {
    uint8_t rom[4096] = {0};
    const uint8_t program[] = {0xA9, 0x02, 0x85, 0x00, 0x4C, 0x04, 0xF1};
    memcpy(rom + 0x100, program, sizeof(program));
    rom[0xFFC] = 0;
    rom[0xFFD] = 0xF1;
    Cartridge cart;
    TIA tia = {0};
    RIOT riot;
    Bus bus;
    CPU6502 cpu;
    CHECK(cart_init_from_bytes(&cart, rom, sizeof(rom), "wsync test"));
    tia_init(&tia, NULL);
    riot_init(&riot);
    bus_init(&bus, &tia, &riot, &cart, tia_step, tia_read, observe_tia_write);
    cpu_init(&cpu, &bus);
    tia_step(&tia, 24);
    CHECK(tia.clock == 30);
    tia_write(&tia, 0x02, 0);
    riot_write(&riot, 0x294, 200);
    observed_riot = &riot;
    vsync_clock = vsync_scanline = vsync_timer = -1;
    CHECK(bus_step_frame(&bus, &cpu));
    CHECK(vsync_scanline == 1);
    CHECK(vsync_clock == 15);
    CHECK(vsync_timer == 129);
    CHECK(!tia.wsync && tia.frame_ready);
    tia_destroy(&tia);
    cart_free(&cart);
}

static void test_missing_vsync_is_bounded(void) {
    TIA tia;
    RIOT riot;
    Bus bus;
    CPU6502 cpu;
    tia_init(&tia, NULL);
    riot_init(&riot);
    bus_init(&bus, &tia, &riot, NULL, tia_step, tia_read, tia_write);
    cpu_init(&cpu, &bus);
    bus.ram[0] = 0x4C; /* JMP $0080 */
    bus.ram[1] = 0x80;
    bus.ram[2] = 0x00;
    cpu.PC = 0x0080;
    tia.scanline = BUS_MAX_FRAME_SCANLINES - 1;
    tia.clock = 225;
    CHECK(!bus_step_frame(&bus, &cpu));
    CHECK(!tia.frame_ready);
    CHECK(tia.scanline == BUS_MAX_FRAME_SCANLINES);
    tia_destroy(&tia);
}

int main(void) {
    test_address_decode();
    test_synthetic_cartridge_program();
    test_wsync_system_stall();
    test_missing_vsync_is_bounded();
    puts("Atari 2600 system integration tests passed");
    return EXIT_SUCCESS;
}
