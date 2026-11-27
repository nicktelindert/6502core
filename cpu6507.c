#include "cpu6507.h"
#include "addressing.h"
#include <stdio.h>
#include "bus.h"
#include "cartridge.h"


/* --- Gedeelde helpers (ook gebruikt door addressing.c) --- */

uint8_t cpu_read_mem(CPU6507 *cpu, uint16_t address) {
    return bus_read(cpu->bus, address);
}

uint8_t cpu_fetch_u8(CPU6507 *cpu) {
    uint8_t val = cpu_read_mem(cpu, cpu->PC);
    cpu->PC = (uint16_t)(cpu->PC + 1u);
    return val;
}

void cpu_set_page_crossed(CPU6507 *cpu, bool crossed) {
    cpu->page_crossed = crossed;
}

/* --- Interne CPU-helpers --- */

static void cpu_write_mem(CPU6507 *cpu, uint16_t address, uint8_t value) {
    if (address < 0x0080u &&
        ((address & 0x3Fu) == 0x10u || (address & 0x3Fu) == 0x11u)) {
        fprintf(stderr, "TIA RESP write: PC=%04X addr=%04X value=%02X\n",
                cpu->PC, address, value);
    }
    bus_write(cpu->bus, address, value);
}

static void cpu_push_byte(CPU6507 *cpu, uint8_t value) {
    cpu_write_mem(cpu, (uint16_t)(0x0100u | cpu->SP), value);
    cpu->SP = (uint8_t)(cpu->SP - 1u);
}

static uint8_t cpu_pull_byte(CPU6507 *cpu) {
    cpu->SP = (uint8_t)(cpu->SP + 1u);
    return cpu_read_mem(cpu, (uint16_t)(0x0100u | cpu->SP));
}

static bool cpu_get_flag(const CPU6507 *cpu, uint8_t flag) {
    return (cpu->status & flag) != 0;
}

static void cpu_set_flag(CPU6507 *cpu, uint8_t flag, bool enabled) {
    if (enabled) {
        cpu->status |= flag;
    } else {
        cpu->status &= (uint8_t)~flag;
    }
}

static void cpu_update_zn(CPU6507 *cpu, uint8_t value) {
    cpu_set_flag(cpu, CPU_FLAG_Z, value == 0);
    cpu_set_flag(cpu, CPU_FLAG_N, (value & 0x80u) != 0);
}

/* --- Instructions --- */

static void instr_unimplemented(CPU6507 *cpu, uint16_t addr) {
    (void)addr;
    uint16_t op_pc = (uint16_t)(cpu->PC - 1u);
    uint8_t op = cpu_read_mem(cpu, op_pc);
    fprintf(stderr, "Onbekende opcode: 0x%02X op 0x%04X\n", op, op_pc);
}

static void instr_NOP(CPU6507 *cpu, uint16_t addr) { (void)cpu; (void)addr; }
static void instr_JMP(CPU6507 *cpu, uint16_t addr) { cpu->PC = addr; }
static void instr_SEI(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu_set_flag(cpu, CPU_FLAG_I, true); }
static void instr_CLI(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu_set_flag(cpu, CPU_FLAG_I, false); }
static void instr_CLD(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu_set_flag(cpu, CPU_FLAG_D, false); }
static void instr_SED(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu_set_flag(cpu, CPU_FLAG_D, true); }
static void instr_CLV(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu_set_flag(cpu, CPU_FLAG_V, false); }
static void instr_CLC(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu_set_flag(cpu, CPU_FLAG_C, false); }
static void instr_SEC(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu_set_flag(cpu, CPU_FLAG_C, true); }

static void instr_LDA(CPU6507 *cpu, uint16_t addr) { cpu->A = cpu_read_mem(cpu, addr); cpu_update_zn(cpu, cpu->A); }
static void instr_LDX(CPU6507 *cpu, uint16_t addr) { cpu->X = cpu_read_mem(cpu, addr); cpu_update_zn(cpu, cpu->X); }
static void instr_LDY(CPU6507 *cpu, uint16_t addr) { cpu->Y = cpu_read_mem(cpu, addr); cpu_update_zn(cpu, cpu->Y); }

static void instr_STA(CPU6507 *cpu, uint16_t addr) { cpu_write_mem(cpu, addr, cpu->A); }
static void instr_STX(CPU6507 *cpu, uint16_t addr) { cpu_write_mem(cpu, addr, cpu->X); }
static void instr_STY(CPU6507 *cpu, uint16_t addr) { cpu_write_mem(cpu, addr, cpu->Y); }

static void instr_TXS(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu->SP = cpu->X; }
static void instr_TSX(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu->X = cpu->SP; cpu_update_zn(cpu, cpu->X); }
static void instr_TAY(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu->Y = cpu->A; cpu_update_zn(cpu, cpu->Y); }
static void instr_TAX(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu->X = cpu->A; cpu_update_zn(cpu, cpu->X); }
static void instr_TYA(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu->A = cpu->Y; cpu_update_zn(cpu, cpu->A); }
static void instr_TXA(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu->A = cpu->X; cpu_update_zn(cpu, cpu->A); }

static void instr_DEX(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu->X = (uint8_t)(cpu->X - 1u); cpu_update_zn(cpu, cpu->X); }
static void instr_DEY(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu->Y = (uint8_t)(cpu->Y - 1u); cpu_update_zn(cpu, cpu->Y); }
static void instr_INX(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu->X = (uint8_t)(cpu->X + 1u); cpu_update_zn(cpu, cpu->X); }
static void instr_INY(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu->Y = (uint8_t)(cpu->Y + 1u); cpu_update_zn(cpu, cpu->Y); }

static void branch_if(CPU6507 *cpu, bool condition, uint16_t addr) {
    if (!condition) {
        return;
    }

    /* Dummy-cycle van een genomen branch. */
    bus_tick(cpu->bus, 1);

    uint16_t old_pc = cpu->PC;
    bool crossed = (old_pc & 0xFF00u) != (addr & 0xFF00u);

    cpu->PC = addr;

    if (crossed) {
        /* Extra cycle bij branch over een paginagrens. */
        bus_tick(cpu->bus, 1);
    }
}

static void instr_BNE(CPU6507 *cpu, uint16_t addr) { branch_if(cpu, !cpu_get_flag(cpu, CPU_FLAG_Z), addr); }
static void instr_BEQ(CPU6507 *cpu, uint16_t addr) { branch_if(cpu,  cpu_get_flag(cpu, CPU_FLAG_Z), addr); }
static void instr_BPL(CPU6507 *cpu, uint16_t addr) { branch_if(cpu, !cpu_get_flag(cpu, CPU_FLAG_N), addr); }
static void instr_BMI(CPU6507 *cpu, uint16_t addr) { branch_if(cpu,  cpu_get_flag(cpu, CPU_FLAG_N), addr); }
static void instr_BCC(CPU6507 *cpu, uint16_t addr) { branch_if(cpu, !cpu_get_flag(cpu, CPU_FLAG_C), addr); }
static void instr_BCS(CPU6507 *cpu, uint16_t addr) { branch_if(cpu,  cpu_get_flag(cpu, CPU_FLAG_C), addr); }
static void instr_BVC(CPU6507 *cpu, uint16_t addr) { branch_if(cpu, !cpu_get_flag(cpu, CPU_FLAG_V), addr); }
static void instr_BVS(CPU6507 *cpu, uint16_t addr) { branch_if(cpu,  cpu_get_flag(cpu, CPU_FLAG_V), addr); }

static void instr_DEC(CPU6507 *cpu, uint16_t addr) {
    uint8_t val = (uint8_t)(cpu_read_mem(cpu, addr) - 1u);
    cpu_write_mem(cpu, addr, val);
    cpu_update_zn(cpu, val);
}

static void instr_INC(CPU6507 *cpu, uint16_t addr) {
    uint8_t val = (uint8_t)(cpu_read_mem(cpu, addr) + 1u);
    cpu_write_mem(cpu, addr, val);
    cpu_update_zn(cpu, val);
}

static void instr_PHP(CPU6507 *cpu, uint16_t addr) {
    (void)addr;
    uint8_t st = (uint8_t)(cpu->status | CPU_FLAG_U | CPU_FLAG_B);
    cpu_push_byte(cpu, st);
}

static void instr_PLP(CPU6507 *cpu, uint16_t addr) {
    (void)addr;
    uint8_t st = cpu_pull_byte(cpu);
    cpu->status = (uint8_t)((st & ~(CPU_FLAG_B | CPU_FLAG_U)) | CPU_FLAG_U);
}

static void instr_PHA(CPU6507 *cpu, uint16_t addr) { (void)addr; cpu_push_byte(cpu, cpu->A); }

static void instr_PLA(CPU6507 *cpu, uint16_t addr) {
    (void)addr;
    cpu->A = cpu_pull_byte(cpu);
    cpu_update_zn(cpu, cpu->A);
}

static void instr_BRK(CPU6507 *cpu, uint16_t addr) {
    (void)addr;
    cpu->PC = (uint16_t)(cpu->PC + 1u);
    cpu_push_byte(cpu, (uint8_t)((cpu->PC >> 8) & 0xFFu));
    cpu_push_byte(cpu, (uint8_t)(cpu->PC & 0xFFu));
    instr_PHP(cpu, 0);
    cpu_set_flag(cpu, CPU_FLAG_I, true);
    uint8_t lo = cpu_read_mem(cpu, 0xFFFEu);
    uint8_t hi = cpu_read_mem(cpu, 0xFFFFu);
    cpu->PC = (uint16_t)(((uint16_t)hi << 8) | lo);
}

static void instr_RTI(CPU6507 *cpu, uint16_t addr) {
    (void)addr;
    instr_PLP(cpu, 0);
    uint8_t lo = cpu_pull_byte(cpu);
    uint8_t hi = cpu_pull_byte(cpu);
    cpu->PC = (uint16_t)(((uint16_t)hi << 8) | lo);
}

static void instr_JSR(CPU6507 *cpu, uint16_t addr) {
    uint16_t ret = (uint16_t)(cpu->PC - 1u);
    cpu_push_byte(cpu, (uint8_t)((ret >> 8) & 0xFFu));
    cpu_push_byte(cpu, (uint8_t)(ret & 0xFFu));
    if (cpu->bus && cpu->bus->cart) {
        cart_fe_on_byte(cpu->bus->cart, (uint8_t)((addr >> 8) & 0xFFu));
    }
    cpu->PC = addr;
}

static void instr_RTS(CPU6507 *cpu, uint16_t addr) {
    (void)addr;
    uint8_t lo = cpu_pull_byte(cpu);
    uint8_t hi = cpu_pull_byte(cpu);
    if (cpu->bus && cpu->bus->cart) {
        cart_fe_on_rts_hi(cpu->bus->cart, hi);
    }
    cpu->PC = (uint16_t)((((uint16_t)hi << 8) | lo) + 1u);
}

static void instr_ASL(CPU6507 *cpu, uint16_t addr) {
    bool is_acc = (addr == ADDR_NONE);
    uint8_t val = is_acc ? cpu->A : cpu_read_mem(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x80u) != 0);
    uint8_t res = (uint8_t)(val << 1);
    cpu_update_zn(cpu, res);
    if (is_acc) {
        cpu->A = res;
    } else {
        cpu_write_mem(cpu, addr, res);
    }
}

static void instr_LSR(CPU6507 *cpu, uint16_t addr) {
    bool is_acc = (addr == ADDR_NONE);
    uint8_t val = is_acc ? cpu->A : cpu_read_mem(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x01u) != 0);
    uint8_t res = (uint8_t)(val >> 1);
    cpu_update_zn(cpu, res);
    if (is_acc) {
        cpu->A = res;
    } else {
        cpu_write_mem(cpu, addr, res);
    }
}

static void instr_ROL(CPU6507 *cpu, uint16_t addr) {
    bool is_acc = (addr == ADDR_NONE);
    uint8_t val = is_acc ? cpu->A : cpu_read_mem(cpu, addr);
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 1u : 0u;
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x80u) != 0);
    uint8_t res = (uint8_t)((val << 1) | old_c);
    cpu_update_zn(cpu, res);
    if (is_acc) {
        cpu->A = res;
    } else {
        cpu_write_mem(cpu, addr, res);
    }
}

static void instr_ROR(CPU6507 *cpu, uint16_t addr) {
    bool is_acc = (addr == ADDR_NONE);
    uint8_t val = is_acc ? cpu->A : cpu_read_mem(cpu, addr);
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 1u : 0u;
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x01u) != 0);
    uint8_t res = (uint8_t)((val >> 1) | (old_c << 7));
    cpu_update_zn(cpu, res);
    if (is_acc) {
        cpu->A = res;
    } else {
        cpu_write_mem(cpu, addr, res);
    }
}

static void instr_AND(CPU6507 *cpu, uint16_t addr) { cpu->A &= cpu_read_mem(cpu, addr); cpu_update_zn(cpu, cpu->A); }
static void instr_EOR(CPU6507 *cpu, uint16_t addr) { cpu->A ^= cpu_read_mem(cpu, addr); cpu_update_zn(cpu, cpu->A); }
static void instr_ORA(CPU6507 *cpu, uint16_t addr) { cpu->A |= cpu_read_mem(cpu, addr); cpu_update_zn(cpu, cpu->A); }

static void instr_BIT(CPU6507 *cpu, uint16_t addr) {
    uint8_t val = cpu_read_mem(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_Z, (cpu->A & val) == 0);
    cpu_set_flag(cpu, CPU_FLAG_N, (val & 0x80u) != 0);
    cpu_set_flag(cpu, CPU_FLAG_V, (val & 0x40u) != 0);
}

static void do_cmp(CPU6507 *cpu, uint8_t reg, uint16_t addr) {
    uint8_t val = cpu_read_mem(cpu, addr);
    uint8_t res = (uint8_t)(reg - val);
    cpu_set_flag(cpu, CPU_FLAG_C, reg >= val);
    cpu_update_zn(cpu, res);
}

static void instr_CMP(CPU6507 *cpu, uint16_t addr) { do_cmp(cpu, cpu->A, addr); }
static void instr_CPX(CPU6507 *cpu, uint16_t addr) { do_cmp(cpu, cpu->X, addr); }
static void instr_CPY(CPU6507 *cpu, uint16_t addr) { do_cmp(cpu, cpu->Y, addr); }

static void instr_ADC(CPU6507 *cpu, uint16_t addr) {
    uint8_t val = cpu_read_mem(cpu, addr);
    uint8_t carry_in = cpu_get_flag(cpu, CPU_FLAG_C) ? 1u : 0u;

    if (cpu_get_flag(cpu, CPU_FLAG_D)) {
        int low = (cpu->A & 0x0F) + (val & 0x0F) + carry_in;
        if (low > 9) low += 6;
        int high = (cpu->A >> 4) + (val >> 4) + (low > 0x0F ? 1 : 0);
        uint8_t bin_res = (uint8_t)((cpu->A + val + carry_in) & 0xFFu);
        cpu_set_flag(cpu, CPU_FLAG_V, ((~(cpu->A ^ val) & (cpu->A ^ bin_res)) & 0x80u) != 0);
        if (high > 9) high += 6;
        cpu_set_flag(cpu, CPU_FLAG_C, high > 0x0F);
        cpu->A = (uint8_t)(((high & 0x0F) << 4) | (low & 0x0F));
        cpu_update_zn(cpu, cpu->A);
        return;
    }

    int total = (int)cpu->A + (int)val + (int)carry_in;
    uint8_t res = (uint8_t)(total & 0xFF);
    cpu_set_flag(cpu, CPU_FLAG_C, total > 0xFF);
    cpu_set_flag(cpu, CPU_FLAG_V, ((~(cpu->A ^ val) & (cpu->A ^ res)) & 0x80u) != 0);
    cpu->A = res;
    cpu_update_zn(cpu, cpu->A);
}

static void instr_SBC(CPU6507 *cpu, uint16_t addr) {
    uint8_t val = cpu_read_mem(cpu, addr);
    uint8_t raw_val = (uint8_t)(val ^ 0xFFu);
    uint8_t carry_in = cpu_get_flag(cpu, CPU_FLAG_C) ? 1u : 0u;

    if (cpu_get_flag(cpu, CPU_FLAG_D)) {
        int low = (cpu->A & 0x0F) - (val & 0x0F) - (1 - carry_in);
        int borrow_low = 0;
        if (low < 0) {
            low -= 6;
            borrow_low = 1;
        }
        int high = (cpu->A >> 4) - (val >> 4) - borrow_low;
        if (high < 0) high -= 6;
        int bin_total = (int)cpu->A + (int)raw_val + (int)carry_in;
        uint8_t bin_res = (uint8_t)(bin_total & 0xFF);
        cpu_set_flag(cpu, CPU_FLAG_C, bin_total > 0xFF);
        cpu_set_flag(cpu, CPU_FLAG_V, ((~(cpu->A ^ raw_val) & (cpu->A ^ bin_res)) & 0x80u) != 0);
        cpu->A = (uint8_t)(((high & 0x0F) << 4) | (low & 0x0F));
        cpu_update_zn(cpu, cpu->A);
        return;
    }

    int total = (int)cpu->A + (int)raw_val + (int)carry_in;
    uint8_t res = (uint8_t)(total & 0xFF);
    cpu_set_flag(cpu, CPU_FLAG_C, total > 0xFF);
    cpu_set_flag(cpu, CPU_FLAG_V, ((~(cpu->A ^ raw_val) & (cpu->A ^ res)) & 0x80u) != 0);
    cpu->A = res;
    cpu_update_zn(cpu, cpu->A);
}

/* --- Undocumented --- */

static void instr_SHX(CPU6507 *cpu, uint16_t addr) {
    uint8_t high = (uint8_t)(((addr - cpu->Y) >> 8) & 0xFFu);
    cpu_write_mem(cpu, addr, (uint8_t)(cpu->X & (uint8_t)(high + 1u)));
}

static void instr_SLO(CPU6507 *cpu, uint16_t addr) {
    uint8_t val = cpu_read_mem(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x80u) != 0);
    val = (uint8_t)(val << 1);
    cpu_write_mem(cpu, addr, val);
    cpu->A |= val;
    cpu_update_zn(cpu, cpu->A);
}

static void instr_RLA(CPU6507 *cpu, uint16_t addr) {
    uint8_t val = cpu_read_mem(cpu, addr);
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 1u : 0u;
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x80u) != 0);
    val = (uint8_t)((val << 1) | old_c);
    cpu_write_mem(cpu, addr, val);
    cpu->A &= val;
    cpu_update_zn(cpu, cpu->A);
}

/* --- Opcode Table & Lifecycle --- */

static void set_op(CPU6507 *cpu, uint8_t op, InstrFn exec, AddrModeFn mode, uint8_t base_cycles, bool page_penalty) {
    cpu->opcodes[op].exec = exec;
    cpu->opcodes[op].mode = mode;
    cpu->opcodes[op].base_cycles = base_cycles;
    cpu->opcodes[op].page_penalty = page_penalty;
}

static void build_opcode_table(CPU6507 *cpu) {
    for (int i = 0; i < 256; i++) {
        set_op(cpu, (uint8_t)i, instr_unimplemented, mode_IMP, 2, false);
    }

    /* ADC */
    set_op(cpu, 0x69, instr_ADC, mode_IMM, 2, false);
    set_op(cpu, 0x65, instr_ADC, mode_ZP,  3, false);
    set_op(cpu, 0x75, instr_ADC, mode_ZPX, 4, false);
    set_op(cpu, 0x6D, instr_ADC, mode_ABS, 4, false);
    set_op(cpu, 0x7D, instr_ADC, mode_ABX, 4, true);
    set_op(cpu, 0x79, instr_ADC, mode_ABY, 4, true);
    set_op(cpu, 0x61, instr_ADC, mode_INX, 6, false);
    set_op(cpu, 0x71, instr_ADC, mode_INY, 5, true);

    /* AND */
    set_op(cpu, 0x29, instr_AND, mode_IMM, 2, false);
    set_op(cpu, 0x25, instr_AND, mode_ZP,  3, false);
    set_op(cpu, 0x35, instr_AND, mode_ZPX, 4, false);
    set_op(cpu, 0x2D, instr_AND, mode_ABS, 4, false);
    set_op(cpu, 0x3D, instr_AND, mode_ABX, 4, true);
    set_op(cpu, 0x39, instr_AND, mode_ABY, 4, true);
    set_op(cpu, 0x21, instr_AND, mode_INX, 6, false);
    set_op(cpu, 0x31, instr_AND, mode_INY, 5, true);

    /* ASL */
    set_op(cpu, 0x0A, instr_ASL, mode_ACC,   2, false);
    set_op(cpu, 0x06, instr_ASL, mode_ZP,    5, false);
    set_op(cpu, 0x16, instr_ASL, mode_ZPX,   6, false);
    set_op(cpu, 0x0E, instr_ASL, mode_ABS,   6, false);
    set_op(cpu, 0x1E, instr_ASL, mode_ABX_W, 7, false);

    /* BIT */
    set_op(cpu, 0x24, instr_BIT, mode_ZP,  3, false);
    set_op(cpu, 0x2C, instr_BIT, mode_ABS, 4, false);

    /* Branches */
    set_op(cpu, 0x10, instr_BPL, mode_REL, 2, true);
    set_op(cpu, 0x30, instr_BMI, mode_REL, 2, true);
    set_op(cpu, 0x50, instr_BVC, mode_REL, 2, true);
    set_op(cpu, 0x70, instr_BVS, mode_REL, 2, true);
    set_op(cpu, 0x90, instr_BCC, mode_REL, 2, true);
    set_op(cpu, 0xB0, instr_BCS, mode_REL, 2, true);
    set_op(cpu, 0xD0, instr_BNE, mode_REL, 2, true);
    set_op(cpu, 0xF0, instr_BEQ, mode_REL, 2, true);

    /* BRK / RTI / RTS / JSR / JMP */
    set_op(cpu, 0x00, instr_BRK, mode_IMP, 7, false);
    set_op(cpu, 0x40, instr_RTI, mode_IMP, 6, false);
    set_op(cpu, 0x60, instr_RTS, mode_IMP, 6, false);
    set_op(cpu, 0x20, instr_JSR, mode_ABS, 6, false);
    set_op(cpu, 0x4C, instr_JMP, mode_ABS, 3, false);
    set_op(cpu, 0x6C, instr_JMP, mode_IND, 5, false);

    /* CMP */
    set_op(cpu, 0xC9, instr_CMP, mode_IMM, 2, false);
    set_op(cpu, 0xC5, instr_CMP, mode_ZP,  3, false);
    set_op(cpu, 0xD5, instr_CMP, mode_ZPX, 4, false);
    set_op(cpu, 0xCD, instr_CMP, mode_ABS, 4, false);
    set_op(cpu, 0xDD, instr_CMP, mode_ABX, 4, true);
    set_op(cpu, 0xD9, instr_CMP, mode_ABY, 4, true);
    set_op(cpu, 0xC1, instr_CMP, mode_INX, 6, false);
    set_op(cpu, 0xD1, instr_CMP, mode_INY, 5, true);

    /* CPX / CPY */
    set_op(cpu, 0xE0, instr_CPX, mode_IMM, 2, false);
    set_op(cpu, 0xE4, instr_CPX, mode_ZP,  3, false);
    set_op(cpu, 0xEC, instr_CPX, mode_ABS, 4, false);
    set_op(cpu, 0xC0, instr_CPY, mode_IMM, 2, false);
    set_op(cpu, 0xC4, instr_CPY, mode_ZP,  3, false);
    set_op(cpu, 0xCC, instr_CPY, mode_ABS, 4, false);

    /* DEC / DEX / DEY */
    set_op(cpu, 0xC6, instr_DEC, mode_ZP,    5, false);
    set_op(cpu, 0xD6, instr_DEC, mode_ZPX,   6, false);
    set_op(cpu, 0xCE, instr_DEC, mode_ABS,   6, false);
    set_op(cpu, 0xDE, instr_DEC, mode_ABX_W, 7, false);
    set_op(cpu, 0xCA, instr_DEX, mode_IMP,   2, false);
    set_op(cpu, 0x88, instr_DEY, mode_IMP,   2, false);

    /* EOR */
    set_op(cpu, 0x49, instr_EOR, mode_IMM, 2, false);
    set_op(cpu, 0x45, instr_EOR, mode_ZP,  3, false);
    set_op(cpu, 0x55, instr_EOR, mode_ZPX, 4, false);
    set_op(cpu, 0x4D, instr_EOR, mode_ABS, 4, false);
    set_op(cpu, 0x5D, instr_EOR, mode_ABX, 4, true);
    set_op(cpu, 0x59, instr_EOR, mode_ABY, 4, true);
    set_op(cpu, 0x41, instr_EOR, mode_INX, 6, false);
    set_op(cpu, 0x51, instr_EOR, mode_INY, 5, true);

    /* Flags */
    set_op(cpu, 0x18, instr_CLC, mode_IMP, 2, false);
    set_op(cpu, 0x38, instr_SEC, mode_IMP, 2, false);
    set_op(cpu, 0x58, instr_CLI, mode_IMP, 2, false);
    set_op(cpu, 0x78, instr_SEI, mode_IMP, 2, false);
    set_op(cpu, 0xB8, instr_CLV, mode_IMP, 2, false);
    set_op(cpu, 0xD8, instr_CLD, mode_IMP, 2, false);
    set_op(cpu, 0xF8, instr_SED, mode_IMP, 2, false);

    /* INC / INX / INY */
    set_op(cpu, 0xE6, instr_INC, mode_ZP,    5, false);
    set_op(cpu, 0xF6, instr_INC, mode_ZPX,   6, false);
    set_op(cpu, 0xEE, instr_INC, mode_ABS,   6, false);
    set_op(cpu, 0xFE, instr_INC, mode_ABX_W, 7, false);
    set_op(cpu, 0xE8, instr_INX, mode_IMP,   2, false);
    set_op(cpu, 0xC8, instr_INY, mode_IMP,   2, false);

    /* LDA */
    set_op(cpu, 0xA9, instr_LDA, mode_IMM, 2, false);
    set_op(cpu, 0xA5, instr_LDA, mode_ZP,  3, false);
    set_op(cpu, 0xB5, instr_LDA, mode_ZPX, 4, false);
    set_op(cpu, 0xAD, instr_LDA, mode_ABS, 4, false);
    set_op(cpu, 0xBD, instr_LDA, mode_ABX, 4, true);
    set_op(cpu, 0xB9, instr_LDA, mode_ABY, 4, true);
    set_op(cpu, 0xA1, instr_LDA, mode_INX, 6, false);
    set_op(cpu, 0xB1, instr_LDA, mode_INY, 5, true);

    /* LDX */
    set_op(cpu, 0xA2, instr_LDX, mode_IMM, 2, false);
    set_op(cpu, 0xA6, instr_LDX, mode_ZP,  3, false);
    set_op(cpu, 0xB6, instr_LDX, mode_ZPY, 4, false);
    set_op(cpu, 0xAE, instr_LDX, mode_ABS, 4, false);
    set_op(cpu, 0xBE, instr_LDX, mode_ABY, 4, true);

    /* LDY */
    set_op(cpu, 0xA0, instr_LDY, mode_IMM, 2, false);
    set_op(cpu, 0xA4, instr_LDY, mode_ZP,  3, false);
    set_op(cpu, 0xB4, instr_LDY, mode_ZPX, 4, false);
    set_op(cpu, 0xAC, instr_LDY, mode_ABS, 4, false);
    set_op(cpu, 0xBC, instr_LDY, mode_ABX, 4, true);

    /* LSR */
    set_op(cpu, 0x4A, instr_LSR, mode_ACC,   2, false);
    set_op(cpu, 0x46, instr_LSR, mode_ZP,    5, false);
    set_op(cpu, 0x56, instr_LSR, mode_ZPX,   6, false);
    set_op(cpu, 0x4E, instr_LSR, mode_ABS,   6, false);
    set_op(cpu, 0x5E, instr_LSR, mode_ABX_W, 7, false);

    /* NOP */
    set_op(cpu, 0xEA, instr_NOP, mode_IMP, 2, false);

    /* ORA */
    set_op(cpu, 0x09, instr_ORA, mode_IMM, 2, false);
    set_op(cpu, 0x05, instr_ORA, mode_ZP,  3, false);
    set_op(cpu, 0x15, instr_ORA, mode_ZPX, 4, false);
    set_op(cpu, 0x0D, instr_ORA, mode_ABS, 4, false);
    set_op(cpu, 0x1D, instr_ORA, mode_ABX, 4, true);
    set_op(cpu, 0x19, instr_ORA, mode_ABY, 4, true);
    set_op(cpu, 0x01, instr_ORA, mode_INX, 6, false);
    set_op(cpu, 0x11, instr_ORA, mode_INY, 5, true);

    /* Stack */
    set_op(cpu, 0x48, instr_PHA, mode_IMP, 3, false);
    set_op(cpu, 0x08, instr_PHP, mode_IMP, 3, false);
    set_op(cpu, 0x68, instr_PLA, mode_IMP, 4, false);
    set_op(cpu, 0x28, instr_PLP, mode_IMP, 4, false);

    /* ROL */
    set_op(cpu, 0x2A, instr_ROL, mode_ACC,   2, false);
    set_op(cpu, 0x26, instr_ROL, mode_ZP,    5, false);
    set_op(cpu, 0x36, instr_ROL, mode_ZPX,   6, false);
    set_op(cpu, 0x2E, instr_ROL, mode_ABS,   6, false);
    set_op(cpu, 0x3E, instr_ROL, mode_ABX_W, 7, false);

    /* ROR */
    set_op(cpu, 0x6A, instr_ROR, mode_ACC,   2, false);
    set_op(cpu, 0x66, instr_ROR, mode_ZP,    5, false);
    set_op(cpu, 0x76, instr_ROR, mode_ZPX,   6, false);
    set_op(cpu, 0x6E, instr_ROR, mode_ABS,   6, false);
    set_op(cpu, 0x7E, instr_ROR, mode_ABX_W, 7, false);

    /* SBC */
    set_op(cpu, 0xE9, instr_SBC, mode_IMM, 2, false);
    set_op(cpu, 0xE5, instr_SBC, mode_ZP,  3, false);
    set_op(cpu, 0xF5, instr_SBC, mode_ZPX, 4, false);
    set_op(cpu, 0xED, instr_SBC, mode_ABS, 4, false);
    set_op(cpu, 0xFD, instr_SBC, mode_ABX, 4, true);
    set_op(cpu, 0xF9, instr_SBC, mode_ABY, 4, true);
    set_op(cpu, 0xE1, instr_SBC, mode_INX, 6, false);
    set_op(cpu, 0xF1, instr_SBC, mode_INY, 5, true);

    /* STA */
    set_op(cpu, 0x85, instr_STA, mode_ZP,    3, false);
    set_op(cpu, 0x95, instr_STA, mode_ZPX,   4, false);
    set_op(cpu, 0x8D, instr_STA, mode_ABS,   4, false);
    set_op(cpu, 0x9D, instr_STA, mode_ABX_W, 5, false);
    set_op(cpu, 0x99, instr_STA, mode_ABY_W, 5, false);
    set_op(cpu, 0x81, instr_STA, mode_INX,   6, false);
    set_op(cpu, 0x91, instr_STA, mode_INY_W, 6, false);

    /* STX / STY */
    set_op(cpu, 0x86, instr_STX, mode_ZP,  3, false);
    set_op(cpu, 0x96, instr_STX, mode_ZPY, 4, false);
    set_op(cpu, 0x8E, instr_STX, mode_ABS, 4, false);
    set_op(cpu, 0x84, instr_STY, mode_ZP,  3, false);
    set_op(cpu, 0x94, instr_STY, mode_ZPX, 4, false);
    set_op(cpu, 0x8C, instr_STY, mode_ABS, 4, false);

    /* Transfers */
    set_op(cpu, 0xAA, instr_TAX, mode_IMP, 2, false);
    set_op(cpu, 0xA8, instr_TAY, mode_IMP, 2, false);
    set_op(cpu, 0xBA, instr_TSX, mode_IMP, 2, false);
    set_op(cpu, 0x8A, instr_TXA, mode_IMP, 2, false);
    set_op(cpu, 0x9A, instr_TXS, mode_IMP, 2, false);
    set_op(cpu, 0x98, instr_TYA, mode_IMP, 2, false);

    /* Undocumented */
    set_op(cpu, 0x9E, instr_SHX, mode_ABY_W, 5, false);
    set_op(cpu, 0x1B, instr_SLO, mode_ABY_W, 7, false);
    set_op(cpu, 0x3B, instr_RLA, mode_ABY_W, 7, false);
}

void cpu_init(CPU6507 *cpu, Bus *bus) {
    cpu->bus = bus;
    cpu->A = 0x00;
    cpu->X = 0x00;
    cpu->Y = 0x00;
    cpu->SP = 0xFD;
    cpu->PC = 0x0000;
    cpu->status = CPU_FLAG_I | CPU_FLAG_U;
    cpu->extra_cycles = 0;
    cpu->page_crossed = false;
    cpu->hist_idx = 0;
    for (int i = 0; i < 256; i++) {
        cpu->hist[i] = 0;
    }
    build_opcode_table(cpu);
    cpu_reset(cpu);
}

void cpu_reset(CPU6507 *cpu) {
    uint8_t low = cpu_read_mem(cpu, 0xFFFCu);
    uint8_t high = cpu_read_mem(cpu, 0xFFFDu);
    cpu->PC = (uint16_t)(((uint16_t)high << 8) | low);
    printf("Reset vector: 0x%04X\n", cpu->PC);
}

int cpu_step(CPU6507 *cpu) {
    cpu->extra_cycles = 0;
    cpu->page_crossed = false;
    bus_reset_cycles_this_step(cpu->bus);

    uint8_t opcode = cpu_fetch_u8(cpu);
    OpcodeEntry entry = cpu->opcodes[opcode];
    uint16_t target_addr = entry.mode(cpu, entry.page_penalty);
    entry.exec(cpu, target_addr);

    return bus_get_cycles_this_step(cpu->bus);
}