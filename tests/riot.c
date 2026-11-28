#include "../system2600.h"
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

/* Timer phase follows Stella's M6532 implementation. */
static void test_power_on_timer(void) {
    RIOT riot;
    riot_init(&riot);
    CHECK(riot.timer == 0x80);
    CHECK(riot.interval == 1024);
    CHECK(riot.prescaler == 0);
    riot_step(&riot, 1023);
    CHECK(riot_read(&riot, 0x284) == 0x80);
    riot_step(&riot, 1);
    CHECK(riot_read(&riot, 0x284) == 0x7F);
}

static void test_timer_intervals(void) {
    static const int intervals[] = {1, 8, 64, 1024};
    for (int i = 0; i < 4; i++) {
        RIOT riot;
        riot_init(&riot);
        riot_write(&riot, (uint16_t)(0x294 + i), 2);
        CHECK(riot_read(&riot, 0x284) == 2);
        riot_step(&riot, 1);
        CHECK(riot_read(&riot, 0x284) == 1);
        riot_step(&riot, intervals[i]);
        CHECK(riot_read(&riot, 0x284) == 0);
        riot_step(&riot, intervals[i] - 1);
        CHECK(riot_read(&riot, 0x284) == 0);
        riot_step(&riot, 1);
        CHECK(riot_read(&riot, 0x285) == 0x80);
        CHECK(riot_read(&riot, 0x284) == 0xFF);
        CHECK(riot_read(&riot, 0x285) == 0x80);
        riot_step(&riot, 1);
        CHECK(riot_read(&riot, 0x284) == 0xFE);
        CHECK(riot_read(&riot, 0x285) == 0);

        riot_write(&riot, (uint16_t)(0x29C + i), 3);
        CHECK(riot_read(&riot, 0x284) == 3);
        riot_step(&riot, 1);
        CHECK(riot_read(&riot, 0x284) == 2);
    }
}

static void test_timer_chunking(void) {
    for (int mode = 0; mode < 4; mode++) {
        RIOT single, chunked;
        riot_init(&single);
        riot_init(&chunked);
        riot_write(&single, (uint16_t)(0x294 + mode), 2);
        riot_write(&chunked, (uint16_t)(0x294 + mode), 2);
        for (int i = 0; i < 3500; i++) riot_step(&single, 1);
        riot_step(&chunked, 3500);
        CHECK(single.timer == chunked.timer);
        CHECK(single.prescaler == chunked.prescaler);
        CHECK(single.timer_flag == chunked.timer_flag);
        CHECK(single.wrapped_this_cycle == chunked.wrapped_this_cycle);
    }
}

static void test_timer_underflow_reads_and_writes(void) {
    RIOT riot;
    riot_init(&riot);
    riot_write(&riot, 0x295, 0);
    riot_step(&riot, 1);
    CHECK(riot.timer == 0xFF);
    CHECK(riot.timer_flag == 0x80);
    CHECK(riot.wrapped_this_cycle);
    CHECK(riot_read(&riot, 0x284) == 0xFF);
    CHECK(riot.timer_flag == 0x80);

    riot_step(&riot, 3);
    CHECK(riot_read(&riot, 0x284) == 0xFC);
    CHECK(riot.timer_flag == 0);
    riot_step(&riot, 4);
    CHECK(riot.timer == 0xFC);
    riot_step(&riot, 1);
    CHECK(riot.timer == 0xFB);

    riot_write(&riot, 0x295, 0);
    riot_step(&riot, 1);
    riot_write(&riot, 0x295, 5);
    CHECK(riot.timer_flag == 0x80);
    riot_step(&riot, 1);
    CHECK(riot.timer == 4);
    CHECK(riot_read(&riot, 0x284) == 4);
    CHECK(riot.timer_flag == 0);
    riot_step(&riot, 7);
    CHECK(riot.timer == 4);
    riot_step(&riot, 1);
    CHECK(riot.timer == 3);
}

static void test_timer_bus_phase(void) {
    static const int intervals[] = {1, 8, 64, 1024};
    for (int mode = 0; mode < 4; mode++) {
        RIOT riot;
        System2600 bus;
        riot_init(&riot);
        system2600_init(&bus, NULL, &riot, NULL, NULL, NULL, NULL);
        system2600_write(&bus, (uint16_t)(0x294 + mode), 2);
        CHECK(riot.timer == 2);
        CHECK(system2600_read(&bus, 0x284) == 1);
        if (intervals[mode] > 1) {
            system2600_tick(&bus, intervals[mode] - 1);
        }
        CHECK(system2600_read(&bus, 0x286) == 0);
    }
}

/* Training manual PDF pp. 3-5: 13-bit bus, RAM and 3 color clocks/cycle. */
static void test_ram_mirrors(void) {
    System2600 bus;
    RIOT riot;
    riot_init(&riot);
    system2600_init(&bus, NULL, &riot, NULL, NULL, NULL, NULL);
    for (int offset = 0; offset < 128; offset++) {
        system2600_write(&bus, (uint16_t)(0x80 + offset), (uint8_t)(offset ^ 0xA5));
    }
    for (int mirror = 0; mirror < 8; mirror++) {
        for (int offset = 0; offset < 128; offset++) {
            uint16_t base = (uint16_t)(mirror * 0x2000);
            CHECK(system2600_read(&bus, (uint16_t)(base + 0x80 + offset)) == (uint8_t)(offset ^ 0xA5));
            CHECK(system2600_read(&bus, (uint16_t)(base + 0x180 + offset)) == (uint8_t)(offset ^ 0xA5));
        }
    }
}

static void test_cpu_tia_riot_timing(void) {
    TIA tia;
    RIOT riot;
    System2600 bus;
    CPU6502 cpu;
    memset(&tia, 0, sizeof(tia));
    tia_init(&tia, NULL);
    riot_init(&riot);
    system2600_init(&bus, &tia, &riot, NULL, tia_step, tia_read, tia_write);
    cpu_init_with_bus(&cpu, system2600_cpu_bus(&bus));
    tia.clock = 0;
    cpu.PC = 0x0080;
    bus.ram[0] = 0xA9; /* LDA #$02 */
    bus.ram[1] = 0x02;
    bus.ram[2] = 0x85; /* STA $09 (COLUBK) */
    bus.ram[3] = 0x09;
    bus.ram[4] = 0x8D; /* STA $0295 (TIM8T) */
    bus.ram[5] = 0x95;
    bus.ram[6] = 0x02;
    bus.ram[7] = 0xEA; /* NOP */
    CHECK(cpu_step(&cpu) == 2);
    CHECK(tia.clock == 6);
    CHECK(cpu_step(&cpu) == 3);
    CHECK(tia.clock == 15);
    CHECK(tia.colubk == 2);
    CHECK(cpu_step(&cpu) == 4);
    CHECK(tia.clock == 27);
    CHECK(riot.timer == 2);
    CHECK(riot.interval == 8);
    CHECK(cpu_step(&cpu) == 2);
    CHECK(tia.clock == 33);
    CHECK(riot.timer == 1);
    CHECK(riot.prescaler == 1);
    tia_destroy(&tia);
}

static void test_intim_mirror_timing(void) {
    for (int region = 0; region < 8; region++) {
        for (int mirror = 0; mirror < 4; mirror++) {
            for (int alternate = 0; alternate < 2; alternate++) {
                uint16_t reg = (uint16_t)(0x284 + region * 0x2000 +
                                          mirror * 8 + alternate * 2);
                RIOT riot;
                System2600 bus;
                riot_init(&riot);
                system2600_init(&bus, NULL, &riot, NULL, NULL, NULL, NULL);
                system2600_write(&bus, 0x294, 2);
                CHECK(system2600_read(&bus, reg) == 1);
                CHECK(riot.timer == 1);
                CHECK(system2600_read(&bus, reg) == 0);
                CHECK(riot.timer == 0);
            }
        }
    }
}

static void test_cpu_timer_polling(void) {
    TIA tia;
    RIOT riot;
    System2600 bus;
    CPU6502 cpu;
    memset(&tia, 0, sizeof(tia));
    tia_init(&tia, NULL);
    riot_init(&riot);
    system2600_init(&bus, &tia, &riot, NULL, tia_step, tia_read, tia_write);
    cpu_init_with_bus(&cpu, system2600_cpu_bus(&bus));
    bus.ram[0] = 0xAD; /* LDA $0284 */
    bus.ram[1] = 0x84;
    bus.ram[2] = 0x02;
    bus.ram[3] = 0xD0; /* BNE $0080 */
    bus.ram[4] = 0xFB;
    cpu.PC = 0x0080;
    system2600_write(&bus, 0x295, 2);
    tia.clock = 0;
    for (int expected = 1; expected >= 0; expected--) {
        CHECK(cpu_step(&cpu) == 4);
        CHECK(cpu.A == expected);
        CHECK(cpu_step(&cpu) == (expected ? 3 : 2));
        CHECK(cpu.PC == (expected ? 0x0080 : 0x0085));
    }
    CHECK(tia.clock == 39);
    tia_destroy(&tia);
}

static void test_riot_read_mirrors(void) {
    RIOT riot;
    riot_init(&riot);
    riot.swcha = 0x3C;
    riot.swchb = 0xA5;
    riot.swacnt = 0x12;
    riot.swbcnt = 0x34;
    riot.timer_flag = 0x80;
    static const uint8_t expected[] = {0x3C, 0x12, 0xA5, 0x34};
    for (int mirror = 0; mirror < 4; mirror++) {
        for (int reg = 0; reg < 4; reg++) {
            CHECK(riot_read(&riot, (uint16_t)(0x280 + mirror * 8 + reg)) == expected[reg]);
        }
        CHECK(riot_read(&riot, (uint16_t)(0x285 + mirror * 8)) == 0x80);
        CHECK(riot_read(&riot, (uint16_t)(0x287 + mirror * 8)) == 0x80);
    }
}

static void test_riot_write_mirrors(void) {
    static const uint16_t intervals[] = {1, 8, 64, 1024};
    for (unsigned region = 0; region < 8; region++) {
        for (unsigned mirror = 0; mirror < 16; mirror++) {
            for (unsigned reg = 0; reg < 4; reg++) {
                uint16_t address = (uint16_t)(region * 0x2000u + 0x280u +
                                              mirror * 8u + reg);
                RIOT riot;
                System2600 bus;
                riot_init(&riot);
                system2600_init(&bus, NULL, &riot, NULL, NULL, NULL, NULL);
                system2600_write(&bus, address, 0x5A);
                uint8_t actual[] = {riot.swcha, riot.swacnt, riot.swchb, riot.swbcnt};
                CHECK(actual[reg] == 0x5A);
            }
        }
        for (unsigned mirror = 0; mirror < 4; mirror++) {
            for (unsigned mode = 0; mode < 4; mode++) {
                uint16_t address = (uint16_t)(region * 0x2000u + 0x294u +
                                              mirror * 0x20u + mode);
                RIOT riot;
                System2600 bus;
                riot_init(&riot);
                system2600_init(&bus, NULL, &riot, NULL, NULL, NULL, NULL);
                system2600_write(&bus, address, 0x42);
                CHECK(riot.timer == 0x42 && riot.interval == intervals[mode]);
                system2600_write(&bus, (uint16_t)(address | 0x08u), 0x37);
                CHECK(riot.timer == 0x37 && riot.interval == intervals[mode]);
            }
        }
    }
}

int main(void) {
    test_power_on_timer();
    test_timer_intervals();
    test_timer_chunking();
    test_timer_underflow_reads_and_writes();
    test_timer_bus_phase();
    test_ram_mirrors();
    test_cpu_tia_riot_timing();
    test_intim_mirror_timing();
    test_cpu_timer_polling();
    test_riot_read_mirrors();
    test_riot_write_mirrors();
    puts("RIOT and system timing tests passed");
    return EXIT_SUCCESS;
}
