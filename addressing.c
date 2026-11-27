#include "addressing.h"
#include "cpu6507.h"

typedef struct Bus Bus;

extern uint8_t bus_read(Bus *bus, uint16_t address);
extern void bus_tick(Bus *bus, int cycles);

/* Gedeelde interne CPU-helpers uit cpu6507.c (of maak ze non-static in cpu6507.c) */
extern uint8_t cpu_read_mem(CPU6507 *cpu, uint16_t address);
extern uint8_t cpu_fetch_u8(CPU6507 *cpu);
extern void cpu_set_page_crossed(CPU6507 *cpu, bool crossed);

uint16_t mode_ACC(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    bus_tick(cpu->bus, 1);
    return ADDR_NONE;
}

uint16_t mode_IMP(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    bus_tick(cpu->bus, 1);
    return ADDR_NONE;
}

uint16_t mode_IMM(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    uint16_t addr = cpu->PC;
    cpu->PC = (uint16_t)(cpu->PC + 1u);
    return addr;
}

uint16_t mode_ZP(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    return (uint16_t)cpu_fetch_u8(cpu);
}

uint16_t mode_ZPX(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    uint8_t zp = cpu_fetch_u8(cpu);
    bus_tick(cpu->bus, 1);
    return (uint16_t)((zp + cpu->X) & 0xFFu);
}

uint16_t mode_ZPY(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    uint8_t zp = cpu_fetch_u8(cpu);
    bus_tick(cpu->bus, 1);
    return (uint16_t)((zp + cpu->Y) & 0xFFu);
}

uint16_t mode_ABS(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    uint8_t low = cpu_fetch_u8(cpu);
    uint8_t high = cpu_fetch_u8(cpu);
    return (uint16_t)(((uint16_t)high << 8) | low);
}

uint16_t mode_ABX(CPU6507 *cpu, bool page_penalty) {
    uint8_t low = cpu_fetch_u8(cpu);
    uint8_t high = cpu_fetch_u8(cpu);
    uint16_t base_addr = (uint16_t)(((uint16_t)high << 8) | low);
    uint16_t addr = (uint16_t)(base_addr + cpu->X);
    bool crossed = (base_addr & 0xFF00u) != (addr & 0xFF00u);

    cpu_set_page_crossed(cpu, crossed);
    if (crossed && page_penalty) {
        bus_tick(cpu->bus, 1);
        cpu->extra_cycles += 1;
    }
    return addr;
}

uint16_t mode_ABY(CPU6507 *cpu, bool page_penalty) {
    uint8_t low = cpu_fetch_u8(cpu);
    uint8_t high = cpu_fetch_u8(cpu);
    uint16_t base_addr = (uint16_t)(((uint16_t)high << 8) | low);
    uint16_t addr = (uint16_t)(base_addr + cpu->Y);
    bool crossed = (base_addr & 0xFF00u) != (addr & 0xFF00u);

    cpu_set_page_crossed(cpu, crossed);
    if (crossed && page_penalty) {
        bus_tick(cpu->bus, 1);
        cpu->extra_cycles += 1;
    }
    return addr;
}

uint16_t mode_ABX_W(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    uint8_t low = cpu_fetch_u8(cpu);
    uint8_t high = cpu_fetch_u8(cpu);
    uint16_t base_addr = (uint16_t)(((uint16_t)high << 8) | low);
    bus_tick(cpu->bus, 1);
    return (uint16_t)((base_addr + cpu->X) & 0xFFFFu);
}

uint16_t mode_ABY_W(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    uint8_t low = cpu_fetch_u8(cpu);
    uint8_t high = cpu_fetch_u8(cpu);
    uint16_t base_addr = (uint16_t)(((uint16_t)high << 8) | low);
    bus_tick(cpu->bus, 1);
    return (uint16_t)((base_addr + cpu->Y) & 0xFFFFu);
}

uint16_t mode_REL(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    int8_t offset = (int8_t)cpu_fetch_u8(cpu);
    return (uint16_t)((int32_t)cpu->PC + (int32_t)offset);
}

uint16_t mode_INX(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    uint8_t zp = (uint8_t)((cpu_fetch_u8(cpu) + cpu->X) & 0xFFu);
    bus_tick(cpu->bus, 1);
    uint8_t low = cpu_read_mem(cpu, zp);
    uint8_t high = cpu_read_mem(cpu, (uint16_t)((zp + 1u) & 0xFFu));
    return (uint16_t)(((uint16_t)high << 8) | low);
}

uint16_t mode_INY(CPU6507 *cpu, bool page_penalty) {
    uint8_t zp_addr = cpu_fetch_u8(cpu);
    uint8_t low = cpu_read_mem(cpu, zp_addr);
    uint8_t high = cpu_read_mem(cpu, (uint16_t)((zp_addr + 1u) & 0xFFu));
    uint16_t base_addr = (uint16_t)(((uint16_t)high << 8) | low);
    uint16_t addr = (uint16_t)(base_addr + cpu->Y);
    bool crossed = (base_addr & 0xFF00u) != (addr & 0xFF00u);

    cpu_set_page_crossed(cpu, crossed);
    if (crossed && page_penalty) {
        bus_tick(cpu->bus, 1);
        cpu->extra_cycles += 1;
    }
    return addr;
}

uint16_t mode_INY_W(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    uint8_t zp_addr = cpu_fetch_u8(cpu);
    uint8_t low = cpu_read_mem(cpu, zp_addr);
    uint8_t high = cpu_read_mem(cpu, (uint16_t)((zp_addr + 1u) & 0xFFu));
    uint16_t base_addr = (uint16_t)(((uint16_t)high << 8) | low);
    bus_tick(cpu->bus, 1);
    return (uint16_t)((base_addr + cpu->Y) & 0xFFFFu);
}

uint16_t mode_IND(CPU6507 *cpu, bool page_penalty) {
    (void)page_penalty;
    uint8_t low_ptr = cpu_read_mem(cpu, cpu->PC);
    uint8_t high_ptr = cpu_read_mem(cpu, (uint16_t)((cpu->PC + 1u) & 0xFFFFu));
    cpu->PC = (uint16_t)(cpu->PC + 2u);
    uint16_t ptr = (uint16_t)(((uint16_t)high_ptr << 8) | low_ptr);

    uint8_t low = cpu_read_mem(cpu, ptr);
    uint16_t high_addr = (uint16_t)((ptr & 0xFF00u) | ((ptr + 1u) & 0x00FFu));
    uint8_t high = cpu_read_mem(cpu, high_addr);
    return (uint16_t)(((uint16_t)high << 8) | low);
}
