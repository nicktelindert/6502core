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

static void test_iny_without_page_cross(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0xB1); /* LDA ($90),Y */
    put_ram(&bus, 0x0081, 0x90);
    put_ram(&bus, 0x0090, 0xFD); /* pointer = $0FFD */
    put_ram(&bus, 0x0091, 0x0F);
    cpu.Y = 2;                  /* effective address = $0FFF */

    CHECK(cpu_step(&cpu) == 5);
    CHECK(cpu.extra_cycles == 0);
}

static void test_iny_with_page_cross(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0xB1); /* LDA ($90),Y */
    put_ram(&bus, 0x0081, 0x90);
    put_ram(&bus, 0x0090, 0xFE); /* pointer = $0FFE */
    put_ram(&bus, 0x0091, 0x0F);
    cpu.Y = 2;                   /* effective address = $1000 */

    CHECK(cpu_step(&cpu) == 6);
    CHECK(cpu.extra_cycles == 1);
}

static void test_load_store_zero_page(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0xA5); /* LDA $90 */
    put_ram(&bus, 0x0081, 0x90);
    put_ram(&bus, 0x0090, 0x80);

    CHECK(cpu_step(&cpu) == 3);
    CHECK(cpu.A == 0x80);
    CHECK((cpu.status & CPU_FLAG_N) != 0);

    setup(&bus, &cpu, 0x0080);
    put_ram(&bus, 0x0080, 0x85); /* STA $90 */
    put_ram(&bus, 0x0081, 0x90);
    cpu.A = 0x5A;

    CHECK(cpu_step(&cpu) == 3);
    CHECK(bus.ram[0x10] == 0x5A);
}

static void test_load_registers_and_transfers(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0xA2); /* LDX #$80 */
    put_ram(&bus, 0x0081, 0x80);
    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.X == 0x80);
    CHECK((cpu.status & CPU_FLAG_N) != 0);

    put_ram(&bus, 0x0082, 0x8A); /* TXA */
    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.A == 0x80);

    put_ram(&bus, 0x0083, 0xA0); /* LDY #$00 */
    put_ram(&bus, 0x0084, 0x00);
    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.Y == 0);
    CHECK((cpu.status & CPU_FLAG_Z) != 0);
}

static void test_adc_flags(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0x69); /* ADC #$50 */
    put_ram(&bus, 0x0081, 0x50);
    cpu.A = 0x50;

    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.A == 0xA0);
    CHECK((cpu.status & CPU_FLAG_V) != 0);
    CHECK((cpu.status & CPU_FLAG_N) != 0);
    CHECK((cpu.status & (CPU_FLAG_C | CPU_FLAG_Z)) == 0);
}

static void test_adc_carry_and_zero(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0x69); /* ADC #$00 */
    put_ram(&bus, 0x0081, 0x00);
    cpu.A = 0xFF;
    cpu.status |= CPU_FLAG_C;

    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.A == 0x00);
    CHECK((cpu.status & (CPU_FLAG_C | CPU_FLAG_Z)) ==
          (CPU_FLAG_C | CPU_FLAG_Z));
}

static void test_logic_and_compare(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0x29); /* AND #$0F */
    put_ram(&bus, 0x0081, 0x0F);
    cpu.A = 0xF3;

    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.A == 0x03);

    put_ram(&bus, 0x0082, 0xC9); /* CMP #$03 */
    put_ram(&bus, 0x0083, 0x03);
    CHECK(cpu_step(&cpu) == 2);
    CHECK((cpu.status & (CPU_FLAG_C | CPU_FLAG_Z)) ==
          (CPU_FLAG_C | CPU_FLAG_Z));
}

static void test_inc_zero_page(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0xE6); /* INC $90 */
    put_ram(&bus, 0x0081, 0x90);
    put_ram(&bus, 0x0090, 0xFF);

    CHECK(cpu_step(&cpu) == 5);
    CHECK(bus.ram[0x10] == 0x00);
    CHECK((cpu.status & CPU_FLAG_Z) != 0);
}

static void test_pha_pla_round_trip(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0x48); /* PHA */
    put_ram(&bus, 0x0081, 0xA9); /* LDA #$00 */
    put_ram(&bus, 0x0082, 0x00);
    put_ram(&bus, 0x0083, 0x68); /* PLA */
    cpu.A = 0x5A;

    CHECK(cpu_step(&cpu) == 3);
    CHECK(cpu.SP == 0xFC);

    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.A == 0x00);

    CHECK(cpu_step(&cpu) == 4);
    CHECK(cpu.A == 0x5A);
    CHECK(cpu.SP == 0xFD);
}

static void test_jmp_absolute(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0x4C); /* JMP $0090 */
    put_ram(&bus, 0x0081, 0x90);
    put_ram(&bus, 0x0082, 0x00);

    CHECK(cpu_step(&cpu) == 3);
    CHECK(cpu.PC == 0x0090);
}

static void test_jsr_rts_round_trip(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0x20); /* JSR $0090 */
    put_ram(&bus, 0x0081, 0x90);
    put_ram(&bus, 0x0082, 0x00);
    put_ram(&bus, 0x0090, 0x60); /* RTS */

    CHECK(cpu_step(&cpu) == 6);
    CHECK(cpu.PC == 0x0090);
    CHECK(cpu.SP == 0xFB);

    CHECK(cpu_step(&cpu) == 6);
    CHECK(cpu.PC == 0x0083);
    CHECK(cpu.SP == 0xFD);
}

static void test_branch_backward_taken(void) {
    Bus bus;
    CPU6507 cpu;
    setup(&bus, &cpu, 0x0080);

    put_ram(&bus, 0x0080, 0xD0); /* BNE -2 */
    put_ram(&bus, 0x0081, 0xFE);

    CHECK(cpu_step(&cpu) == 3);
    CHECK(cpu.PC == 0x0080);
}

int main(void) {
    test_immediate_load();
    test_branch_not_taken();
    test_branch_taken();
    test_branch_page_cross();
    test_pla_cycles();
    test_plp_cycles();
    test_absx_page_cross_cycles();
    test_iny_without_page_cross();
    test_iny_with_page_cross();
    test_load_store_zero_page();
    test_load_registers_and_transfers();
    test_adc_flags();
    test_adc_carry_and_zero();
    test_logic_and_compare();
    test_inc_zero_page();
    test_pha_pla_round_trip();
    test_jmp_absolute();
    test_jsr_rts_round_trip();
    test_branch_backward_taken();

    puts("CPU cycle tests passed");
    return EXIT_SUCCESS;
}

