// Tests for the 6507 CPU implementation

#include "../cpu6507.h"
#include "../bus.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", \
                __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void put_ram(Bus *bus, uint16_t address, uint8_t value) {
    /* RIOT-RAM mirrors: A7=1, A9=0. */
    CHECK((address & 0x0280u) == 0x0080u);
    bus->ram[address & 0x007Fu] = value;
}

static void setup(Bus *bus, CPU6507 *cpu, uint16_t pc) {
    bus_init(bus, NULL, NULL, NULL, NULL, NULL, NULL);
    cpu_init(cpu, bus);

    cpu->PC = pc;
    cpu->A = 0;
    cpu->X = 0;
    cpu->Y = 0;
    cpu->SP = 0xFD;
    cpu->status = CPU_FLAG_U;
    bus_reset_cycles_this_step(bus);
}

static void test_immediate_load(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0xA9); /* LDA #$42 */
    put_ram(&bus, 0x0081, 0x42);

    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.A == 0x42);
    CHECK(cpu.PC == 0x0082);
}

static void test_branch_not_taken(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0xD0); /* BNE +2 */
    put_ram(&bus, 0x0081, 0x02);
    cpu.status |= CPU_FLAG_Z;

    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.PC == 0x0082);
}

static void test_branch_taken(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0xD0); /* BNE +2 */
    put_ram(&bus, 0x0081, 0x02);

    CHECK(cpu_step(&cpu) == 3);
    CHECK(cpu.PC == 0x0084);
}

static void test_branch_page_cross(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x01FD);

    put_ram(&bus, 0x01FD, 0xD0); /* BNE +2: $01FF -> $0201 */
    put_ram(&bus, 0x01FE, 0x02);

    CHECK(cpu_step(&cpu) == 4);
    CHECK(cpu.PC == 0x0201);
}

static void test_pla_cycles(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0x68); /* PLA */
    put_ram(&bus, 0x01FD, 0x80);
    cpu.SP = 0xFC;

    CHECK(cpu_step(&cpu) == 4);
    CHECK(cpu.A == 0x80);
    CHECK(cpu.SP == 0xFD);
    CHECK((cpu.status & CPU_FLAG_N) != 0);
}

static void test_plp_cycles(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0x28); /* PLP */
    put_ram(&bus, 0x01FD, CPU_FLAG_C | CPU_FLAG_Z | CPU_FLAG_B);
    cpu.SP = 0xFC;

    CHECK(cpu_step(&cpu) == 4);
    CHECK(cpu.SP == 0xFD);
    CHECK((cpu.status & (CPU_FLAG_C | CPU_FLAG_Z)) ==
          (CPU_FLAG_C | CPU_FLAG_Z));
    CHECK((cpu.status & CPU_FLAG_U) != 0);
    CHECK((cpu.status & CPU_FLAG_B) == 0);
}

static void test_absx_page_cross_cycles(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0xBD); /* LDA $01FF,X */
    put_ram(&bus, 0x0081, 0xFF);
    put_ram(&bus, 0x0082, 0x01);
    cpu.X = 1;

    CHECK(cpu_step(&cpu) == 5);
    CHECK(cpu.PC == 0x0083);
}

int main(void) {
    test_immediate_load();
    test_branch_not_taken();
    test_branch_taken();
    test_branch_page_cross();
    test_pla_cycles();
    test_plp_cycles();
    test_absx_page_cross_cycles();

    puts("CPU cycle tests passed");
    return EXIT_SUCCESS;
}

