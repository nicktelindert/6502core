#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include <stdbool.h>
#include "riot.h"
#include "cartridge.h"

typedef struct TIA TIA;
typedef struct CPU6507 CPU6507;
typedef struct Bus Bus;

typedef uint8_t (*TiaReadFn)(TIA *tia, uint16_t addr);
typedef void (*TiaWriteFn)(TIA *tia, uint16_t addr, uint8_t val);
typedef void (*TiaStepFn)(TIA *tia, int clocks);

struct Bus {
    TIA *tia;
    RIOT *riot;
    Cartridge *cart;

    TiaStepFn tia_step;
    TiaReadFn tia_read;
    TiaWriteFn tia_write;

    uint8_t ram[128];
    int cycles_this_step;
};

void bus_init(
    Bus *bus,
    TIA *tia,
    RIOT *riot,
    Cartridge *cart,
    TiaStepFn tia_step,
    TiaReadFn tia_read,
    TiaWriteFn tia_write
);

void bus_tick(Bus *bus, int cycles);
uint8_t bus_read(Bus *bus, uint16_t address);
void bus_write(Bus *bus, uint16_t address, uint8_t value);

int bus_get_cycles_this_step(const Bus *bus);
void bus_reset_cycles_this_step(Bus *bus);

void bus_step_frame(Bus *bus, CPU6507 *cpu);

#endif /* BUS_H */