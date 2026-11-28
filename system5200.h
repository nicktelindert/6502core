#ifndef SYSTEM5200_H
#define SYSTEM5200_H

#include "cpu6502.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Antic5200 Antic5200;
typedef struct GTIA5200 GTIA5200;
typedef struct Pokey5200 Pokey5200;
typedef enum {
    SYSTEM5200_CART_NONE,
    SYSTEM5200_CART_4K,
    SYSTEM5200_CART_8K,
    SYSTEM5200_CART_16K,
    SYSTEM5200_CART_16K_EE,
    SYSTEM5200_CART_32K
} System5200CartType;

typedef struct {
    void *context;
    /* Callbacks receive the full CPU address; devices decode their own registers. */
    uint8_t (*read)(void *context, uint16_t address);
    void (*write)(void *context, uint16_t address, uint8_t value);
} System5200Device;

typedef struct {
    uint8_t ram[0x4000];
    uint8_t bios[0x800];
    uint8_t cartridge[0x8000];
    uint8_t data_bus;
    bool bios_loaded;
    System5200CartType cart_type;
    int cycles_this_step;
    uint64_t elapsed_cycles;
    Antic5200 *antic_chip;
    GTIA5200 *gtia_chip;
    Pokey5200 *pokey_chip;
    System5200Device gtia;
    System5200Device antic;
    System5200Device pokey;
    void *clock_context;
    /* Called before each CPU bus access and for CPU idle cycles. */
    void (*clock)(void *context, int cycles);
} System5200;

void system5200_init(System5200 *system);
bool system5200_load_bios(System5200 *system, const uint8_t *data, size_t size);
bool system5200_load_bios_file(System5200 *system, const char *path, char *error, size_t error_size);
bool system5200_load_cartridge(System5200 *system, System5200CartType type,
                               const uint8_t *data, size_t size);
void system5200_eject_cartridge(System5200 *system);
void system5200_tick(System5200 *system, int cycles);
uint8_t system5200_read(System5200 *system, uint16_t address);
void system5200_write(System5200 *system, uint16_t address, uint8_t value);
/* The system and any device contexts must outlive the CPU using this bus. */
CpuBus system5200_cpu_bus(System5200 *system);
void system5200_attach_antic(System5200 *system, Antic5200 *antic, CPU6502 *cpu);
void system5200_attach_gtia(System5200 *system, GTIA5200 *gtia);
void system5200_attach_pokey(System5200 *system, Pokey5200 *pokey);

#endif
