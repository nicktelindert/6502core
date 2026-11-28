#ifndef SYSTEM2600_H
#define SYSTEM2600_H

#include <stdint.h>
#include <stdbool.h>
#include "cpu6502.h"
#include "riot.h"
#include "cartridge.h"

typedef struct TIA TIA;
typedef struct System2600 System2600;

typedef uint8_t (*TiaReadFn)(TIA *tia, uint16_t addr);
typedef void (*TiaWriteFn)(TIA *tia, uint16_t addr, uint8_t val);
typedef void (*TiaStepFn)(TIA *tia, int clocks);

struct System2600 {
    TIA *tia;
    RIOT *riot;
    Cartridge *cart;

    TiaStepFn tia_step;
    TiaReadFn tia_read;
    TiaWriteFn tia_write;

    uint8_t ram[128];
    uint8_t data_bus;
    int cycles_this_step;
};

void system2600_init(
    System2600 *bus,
    TIA *tia,
    RIOT *riot,
    Cartridge *cart,
    TiaStepFn tia_step,
    TiaReadFn tia_read,
    TiaWriteFn tia_write
);

void system2600_tick(System2600 *bus, int cycles);
uint8_t system2600_read(System2600 *bus, uint16_t address);
void system2600_write(System2600 *bus, uint16_t address, uint8_t value);

int system2600_get_cycles_this_step(const System2600 *bus);
void system2600_reset_cycles_this_step(System2600 *bus);
/* The system and its devices must outlive the CPU using this bus. */
CpuBus system2600_cpu_bus(System2600 *bus);

#define SYSTEM2600_MAX_FRAME_SCANLINES 32768

/* Returns false when no frame sync occurs within the scanline limit. */
bool system2600_step_frame(System2600 *bus, CPU6502 *cpu);

#endif /* SYSTEM2600_H */