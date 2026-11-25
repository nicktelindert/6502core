#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "detectchip.h"



typedef struct Cartridge Cartridge;

typedef uint8_t (*CartReadFn)(Cartridge *cart, uint16_t addr);
typedef void (*CartWriteFn)(Cartridge *cart, uint16_t addr, uint8_t val);

struct Cartridge {
    CartType type;
    char name[128];
    uint32_t crc32;

    uint8_t *rom_data;       /* Eigendom van Cartridge wanneer via cart_create geladen */
    size_t rom_size;
    uint16_t mask;           /* 0x07FF voor <= 2K, 0x0FFF voor 4K */

    const uint8_t *banks[4]; /* Pointers naar 4KB banks (voor F8 / F6 / F6SC) */
    int num_banks;           /* 2 voor F8, 4 voor F6/F6SC */
    int current_bank;

    uint8_t superchip_ram[128];
    bool has_superchip_ram;

    CartReadFn read;
    CartWriteFn write;
};

uint32_t cart_crc32(const uint8_t *data, size_t len);
bool cart_init_from_bytes(Cartridge *cart, const uint8_t *rom_data, size_t size, const char *name);
void cart_free(Cartridge *cart);

uint8_t cart_read(Cartridge *cart, uint16_t addr);
void cart_write(Cartridge *cart, uint16_t addr, uint8_t val);

void cart_fe_on_rts_hi(Cartridge *cart, uint8_t hi);
void cart_fe_on_byte(Cartridge *cart, uint8_t val);
#endif /* CARTRIDGE_H */