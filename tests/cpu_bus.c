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

static void test_valid_bcd_exhaustively(CPU6502 *cpu, TestBus *bus) {
    for (int op = 0; op < 2; op++) {
        for (int left = 0; left < 100; left++) {
            for (int right = 0; right < 100; right++) {
                for (int carry = 0; carry < 2; carry++) {
                    uint8_t a = (uint8_t)((left / 10) * 16 + left % 10);
                    uint8_t b = (uint8_t)((right / 10) * 16 + right % 10);
                    int binary = op ? a - b - (1 - carry) : a + b + carry;
                    int decimal = op ? left - right - (1 - carry) : left + right + carry;
                    int wrapped = (decimal + 100) % 100;
                    uint8_t result = (uint8_t)((wrapped / 10) * 16 + wrapped % 10);
                    uint8_t flag_result = (uint8_t)binary;
                    uint8_t flags = CPU_FLAG_U | CPU_FLAG_D;
                    if (op ? decimal >= 0 : decimal >= 100) flags |= CPU_FLAG_C;
                    if (!op) {
                        int high = (a >> 4) + (b >> 4) +
                                   ((a & 15) + (b & 15) + carry >= 10);
                        flag_result = (uint8_t)((high << 4) | (binary & 15));
                    }
                    if ((uint8_t)binary == 0) flags |= CPU_FLAG_Z;
                    if (flag_result & 0x80) flags |= CPU_FLAG_N;
                    if ((op ? (a ^ b) : ~(a ^ b)) & (a ^ flag_result) & 0x80)
                        flags |= CPU_FLAG_V;
                    cpu->PC = 0x8400;
                    cpu->A = a;
                    cpu->status = CPU_FLAG_U | CPU_FLAG_D | (uint8_t)carry;
                    bus->memory[0x8400] = op ? 0xE9 : 0x69;
                    bus->memory[0x8401] = b;
                    CHECK(cpu_step(cpu) == 2);
                    if (cpu->A != result || cpu->status != flags) {
                        fprintf(stderr, "BCD %s %02X %02X carry %d: got %02X/%02X, expected %02X/%02X\n",
                                op ? "SBC" : "ADC", a, b, carry,
                                cpu->A, cpu->status, result, flags);
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
    }
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

    static const struct {
        uint8_t op, a, operand, input_flags, result, flags;
    } decimal_cases[] = {
        {0x69, 0x50, 0x50, CPU_FLAG_D, 0x00, CPU_FLAG_D | CPU_FLAG_C | CPU_FLAG_V | CPU_FLAG_N},
        {0x69, 0x79, 0x00, CPU_FLAG_D | CPU_FLAG_C, 0x80, CPU_FLAG_D | CPU_FLAG_V | CPU_FLAG_N},
        {0x69, 0x89, 0x76, CPU_FLAG_D | CPU_FLAG_C, 0x66, CPU_FLAG_D | CPU_FLAG_C | CPU_FLAG_Z},
        {0x69, 0x80, 0xFA, CPU_FLAG_D, 0xE0, CPU_FLAG_D | CPU_FLAG_C | CPU_FLAG_N},
        {0x69, 0x2F, 0x4F, CPU_FLAG_D, 0x74, CPU_FLAG_D},
        {0xE9, 0x00, 0x01, CPU_FLAG_D | CPU_FLAG_C, 0x99, CPU_FLAG_D | CPU_FLAG_N},
        {0xE9, 0x50, 0x50, CPU_FLAG_D | CPU_FLAG_C, 0x00, CPU_FLAG_D | CPU_FLAG_C | CPU_FLAG_Z},
    };
    for (size_t i = 0; i < sizeof(decimal_cases) / sizeof(decimal_cases[0]); i++) {
        cpu.PC = 0x8400;
        cpu.A = decimal_cases[i].a;
        cpu.status = CPU_FLAG_U | decimal_cases[i].input_flags;
        bus.memory[0x8400] = decimal_cases[i].op;
        bus.memory[0x8401] = decimal_cases[i].operand;
        CHECK(cpu_step(&cpu) == 2);
        CHECK(cpu.A == decimal_cases[i].result);
        CHECK(cpu.status == (CPU_FLAG_U | decimal_cases[i].flags));
    }
    test_valid_bcd_exhaustively(&cpu, &bus);

    cpu_init_with_bus(&cpu, interface);
    cpu.PC = 0x8500;
    cpu.SP = 0xFD;
    cpu.status = CPU_FLAG_U | CPU_FLAG_I;
    bus.memory[0x8500] = 0x58; /* CLI */
    bus.memory[0x8501] = 0xEA; /* NOP */
    bus.memory[0x8502] = 0xEA;
    bus.memory[0xFFFE] = 0x00;
    bus.memory[0xFFFF] = 0x90;
    bus.memory[0x9000] = 0x40; /* RTI */
    cpu_set_irq(&cpu, true);
    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.PC == 0x8501);
    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.PC == 0x8502);
    CHECK(cpu_step(&cpu) == 7);
    CHECK(cpu.PC == 0x9000);
    CHECK(bus.access_count == 7);
    CHECK(bus.addresses[0] == 0x8502 && bus.addresses[1] == 0x8502);
    CHECK(bus.addresses[2] == 0x01FD && bus.addresses[3] == 0x01FC);
    CHECK(bus.addresses[4] == 0x01FB);
    CHECK(bus.addresses[5] == 0xFFFE && bus.addresses[6] == 0xFFFF);
    CHECK(bus.memory[0x01FD] == 0x85 && bus.memory[0x01FC] == 0x02);
    CHECK((bus.memory[0x01FB] & (CPU_FLAG_U | CPU_FLAG_B | CPU_FLAG_I)) == CPU_FLAG_U);
    cpu_set_irq(&cpu, false);
    CHECK(cpu_step(&cpu) == 6);
    CHECK(cpu.PC == 0x8502);
    CHECK((cpu.status & CPU_FLAG_I) == 0);

    cpu.PC = 0x8600;
    bus.memory[0xFFFA] = 0x00;
    bus.memory[0xFFFB] = 0x91;
    bus.memory[0x9100] = 0xEA;
    cpu_set_nmi(&cpu, true);
    cpu_set_irq(&cpu, true);
    CHECK(cpu_step(&cpu) == 7);
    CHECK(cpu.PC == 0x9100);
    CHECK(bus.addresses[5] == 0xFFFA && bus.addresses[6] == 0xFFFB);
    CHECK(cpu_step(&cpu) == 2); /* NMI is edge-triggered */
    cpu_set_nmi(&cpu, false);
    cpu_set_nmi(&cpu, true);
    CHECK(cpu_step(&cpu) == 7);
    CHECK(cpu.PC == 0x9100);

    cpu_init_with_bus(&cpu, interface);
    cpu.PC = 0x8700;
    cpu.status = CPU_FLAG_U;
    bus.memory[0x8700] = 0x78; /* SEI: IRQ uses the previous I flag */
    CHECK(cpu_step(&cpu) == 2);
    cpu_set_irq(&cpu, true);
    CHECK(cpu_step(&cpu) == 7);
    CHECK(cpu.PC == 0x9000);
    CHECK((bus.memory[0x01FB] & CPU_FLAG_I) != 0);

    cpu_init_with_bus(&cpu, interface);
    cpu.PC = 0x8800;
    cpu.status = CPU_FLAG_U | CPU_FLAG_I;
    cpu.SP = 0xFC;
    bus.memory[0x8800] = 0x28; /* PLP clears I */
    bus.memory[0x8801] = 0xEA;
    bus.memory[0x01FD] = CPU_FLAG_U;
    cpu_set_irq(&cpu, true);
    CHECK(cpu_step(&cpu) == 4);
    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.PC == 0x8802);
    CHECK(cpu_step(&cpu) == 7);
    CHECK(cpu.PC == 0x9000);

    cpu_init_with_bus(&cpu, interface);
    cpu.PC = 0x8900;
    cpu.SP = 0x00;
    cpu_set_nmi(&cpu, true);
    CHECK(cpu_step(&cpu) == 7);
    CHECK(cpu.SP == 0xFD);
    CHECK(bus.addresses[2] == 0x0100 && bus.addresses[3] == 0x01FF);
    CHECK(bus.addresses[4] == 0x01FE);
    cpu_reset(&cpu);
    CHECK((cpu.status & CPU_FLAG_I) != 0);
    CHECK(!cpu.nmi_pending);

    puts("CPU independent bus test passed");
    return EXIT_SUCCESS;
}
