#include "../cpu6502.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint8_t memory[65536];
    int cycles;
} TestBus;

static uint8_t read_byte(void *context, uint16_t address) {
    TestBus *bus = context;
    bus->cycles++;
    return bus->memory[address];
}

static void write_byte(void *context, uint16_t address, uint8_t value) {
    TestBus *bus = context;
    bus->cycles++;
    bus->memory[address] = value;
}

static void tick(void *context, int cycles) {
    TestBus *bus = context;
    bus->cycles += cycles;
}

static void reset_cycles(void *context) {
    TestBus *bus = context;
    bus->cycles = 0;
}

static int get_cycles(void *context) {
    TestBus *bus = context;
    return bus->cycles;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s 6502_functional_test.bin\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }
    TestBus bus = {0};
    size_t size = fread(bus.memory, 1, sizeof bus.memory, file);
    int extra = fgetc(file);
    if (ferror(file) || size != sizeof bus.memory || extra != EOF) {
        fprintf(stderr, "Expected exactly 65536 bytes in %s\n", argv[1]);
        fclose(file);
        return EXIT_FAILURE;
    }
    fclose(file);

    CpuBus interface = {
        .context = &bus, .read = read_byte, .write = write_byte,
        .tick = tick, .reset_cycles = reset_cycles, .get_cycles = get_cycles,
    };
    CPU6502 cpu;
    cpu_init_with_bus(&cpu, interface);
    cpu.PC = 0x0400;

    for (unsigned long steps = 0; steps < 100000000ul; steps++) {
        uint16_t previous = cpu.PC;
        cpu_step(&cpu);
        if (cpu.PC == previous) {
            if (cpu.PC == 0x3469) {
                printf("6502 functional test passed after %lu instructions\n", steps + 1);
                return EXIT_SUCCESS;
            }
            fprintf(stderr, "6502 functional test trapped at $%04X after %lu instructions\n",
                    cpu.PC, steps + 1);
            return EXIT_FAILURE;
        }
    }
    fprintf(stderr, "6502 functional test timed out at $%04X\n", cpu.PC);
    return EXIT_FAILURE;
}
