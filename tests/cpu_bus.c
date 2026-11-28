#include "../cpu6502.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", \
                __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

typedef struct {
    uint8_t memory[65536];
    int cycles;
    uint16_t addresses[32];
    int access_count;
} TestBus;

static uint8_t test_read(void *context, uint16_t address) {
    TestBus *bus = context;
    bus->addresses[bus->access_count++] = address;
    bus->cycles++;
    return bus->memory[address];
}

static void test_write(void *context, uint16_t address, uint8_t value) {
    TestBus *bus = context;
    bus->addresses[bus->access_count++] = address;
    bus->cycles++;
    bus->memory[address] = value;
}

static void test_tick(void *context, int cycles) {
    TestBus *bus = context;
    bus->cycles += cycles;
}

static void test_reset_cycles(void *context) {
    TestBus *bus = context;
    bus->cycles = 0;
    bus->access_count = 0;
}

static int test_cycles(void *context) {
    TestBus *bus = context;
    return bus->cycles;
}

int main(void) {
    TestBus bus = {0};
    CPU6502 cpu;
    CpuBus interface = {
        .context = &bus,
        .read = test_read,
        .write = test_write,
        .tick = test_tick,
        .reset_cycles = test_reset_cycles,
        .get_cycles = test_cycles,
    };

    bus.memory[0xFFFC] = 0x00;
    bus.memory[0xFFFD] = 0x80;
    cpu_init_with_bus(&cpu, interface);
    CHECK(cpu.PC == 0x8000);
    CHECK(bus.addresses[0] == 0xFFFC);
    CHECK(bus.addresses[1] == 0xFFFD);

    bus.memory[0x8000] = 0xA9; /* LDA #$5A */
    bus.memory[0x8001] = 0x5A;
    bus.memory[0x8002] = 0x8D; /* STA $E000 */
    bus.memory[0x8003] = 0x00;
    bus.memory[0x8004] = 0xE0;
    bus.memory[0x8005] = 0xEA; /* NOP */
    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.A == 0x5A);
    CHECK(bus.addresses[0] == 0x8000);
    CHECK(bus.addresses[1] == 0x8001);
    CHECK(cpu_step(&cpu) == 4);
    CHECK(bus.memory[0xE000] == 0x5A);
    CHECK(bus.addresses[3] == 0xE000);
    CHECK(cpu_step(&cpu) == 2);
    CHECK(bus.cycles == 2);

    cpu.PC = 0x80FD;
    cpu.status &= (uint8_t)~CPU_FLAG_Z;
    bus.memory[0x80FD] = 0xD0; /* BNE +2, crossing to $8101 */
    bus.memory[0x80FE] = 0x02;
    CHECK(cpu_step(&cpu) == 4);
    CHECK(cpu.PC == 0x8101);
    CHECK(bus.access_count == 4);
    CHECK(bus.addresses[0] == 0x80FD);
    CHECK(bus.addresses[1] == 0x80FE);
    CHECK(bus.addresses[2] == 0x80FF);
    CHECK(bus.addresses[3] == 0x8001);

    cpu.PC = 0x8200;
    bus.memory[0x8200] = 0x20; /* JSR $8300 */
    bus.memory[0x8201] = 0x00;
    bus.memory[0x8202] = 0x83;
    bus.memory[0x8300] = 0x60; /* RTS */
    CHECK(cpu_step(&cpu) == 6);
    CHECK(cpu.PC == 0x8300);
    CHECK(bus.memory[0x01FD] == 0x82);
    CHECK(bus.memory[0x01FC] == 0x02);
    CHECK(cpu_step(&cpu) == 6);
    CHECK(cpu.PC == 0x8203);

    static const uint8_t opcodes[] = {0x0E, 0x4E, 0x2E, 0x6E};
    static const uint8_t expected[] = {0x02, 0x00, 0x02, 0x00};
    for (size_t i = 0; i < sizeof(opcodes); i++) {
        cpu.PC = 0x8400;
        cpu.A = 0x55;
        cpu.status &= (uint8_t)~CPU_FLAG_C;
        bus.memory[0x8400] = opcodes[i];
        bus.memory[0x8401] = 0xFF;
        bus.memory[0x8402] = 0xFF;
        bus.memory[0xFFFF] = 0x01;
        CHECK(cpu_step(&cpu) == 6);
        CHECK(cpu.A == 0x55);
        CHECK(bus.memory[0xFFFF] == expected[i]);
    }

    static const uint8_t accumulator_opcodes[] = {0x0A, 0x4A, 0x2A, 0x6A};
    for (size_t i = 0; i < sizeof(accumulator_opcodes); i++) {
        cpu.PC = 0x8400;
        cpu.A = 0x01;
        cpu.status &= (uint8_t)~CPU_FLAG_C;
        bus.memory[0x8400] = accumulator_opcodes[i];
        bus.memory[0xFFFF] = 0x55;
        CHECK(cpu_step(&cpu) == 2);
        CHECK(cpu.A == expected[i]);
        CHECK(bus.memory[0xFFFF] == 0x55);
    }

    puts("CPU independent bus test passed");
    return EXIT_SUCCESS;
}
