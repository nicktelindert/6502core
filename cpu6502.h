#ifndef CPU6502_H
#define CPU6502_H
#ifndef ADDR_NONE
#define ADDR_NONE 0xFFFFu
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct CPU6502 CPU6502;

typedef struct {
    void *context;
    uint8_t (*read)(void *context, uint16_t address);
    void (*write)(void *context, uint16_t address, uint8_t value);
    void (*tick)(void *context, int cycles);
    void (*reset_cycles)(void *context);
    int (*get_cycles)(void *context);
    void (*on_jsr)(void *context, uint8_t target_high);
    void (*on_rts)(void *context, uint8_t return_high);
} CpuBus;

enum {
    CPU_FLAG_C = 1u << 0, /* 0x01 Carry */
    CPU_FLAG_Z = 1u << 1, /* 0x02 Zero */
    CPU_FLAG_I = 1u << 2, /* 0x04 Interrupt Disable */
    CPU_FLAG_D = 1u << 3, /* 0x08 Decimal */
    CPU_FLAG_B = 1u << 4, /* 0x10 Break */
    CPU_FLAG_U = 1u << 5, /* 0x20 Unused (meestal 1 bij pushes) */
    CPU_FLAG_V = 1u << 6, /* 0x40 Overflow */
    CPU_FLAG_N = 1u << 7  /* 0x80 Negative */
};

typedef uint16_t (*AddrModeFn)(CPU6502 *cpu, bool page_penalty);
typedef void (*InstrFn)(CPU6502 *cpu, uint16_t addr);

typedef struct {
    InstrFn exec;
    AddrModeFn mode;
    uint8_t base_cycles;
    bool page_penalty;
} OpcodeEntry;

struct CPU6502 {
    CpuBus bus;

    uint8_t A;
    uint8_t X;
    uint8_t Y;
    uint8_t SP;
    uint16_t PC;
    uint8_t status;

    int extra_cycles;
    bool page_crossed;
    bool accumulator_mode;
    bool irq_asserted;
    bool irq_mask_sample;
    bool nmi_asserted;
    bool nmi_pending;

    uint16_t hist[256];
    uint8_t hist_idx;

    OpcodeEntry opcodes[256];
};

void cpu_init_with_bus(CPU6502 *cpu, CpuBus bus);
void cpu_reset(CPU6502 *cpu);
/* asserted=true represents an active (low) input pin. */
void cpu_set_irq(CPU6502 *cpu, bool asserted);
void cpu_set_nmi(CPU6502 *cpu, bool asserted);
int cpu_step(CPU6502 *cpu);

#endif /* CPU6502_H */