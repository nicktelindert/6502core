#include "cartridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint32_t F6SC_CRC32_LIST[] = {
    0xEE7B80D1u, /* Another Dig Dug (USA) */
    0xB629D904u, /* Dig Dug (USA) */
    0xD352052Eu, /* Dig Dug (Europe) */
    0x5A27B58Cu  /* Dig Dug (Rev 1) */
};


static const uint32_t F8FE_CRC32_LIST[] = {
    0xe127c012, // Robot Tank
};

static const uint32_t F8SC_CRC32_LIST[] = {
    0xcde3530e
};

uint32_t cart_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static bool is_f6sc(const uint8_t *rom_data, size_t size, uint32_t crc) {
    if (size != 0x4000u) {
        return false;
    }

    size_t num_known = sizeof(F6SC_CRC32_LIST) / sizeof(F6SC_CRC32_LIST[0]);
    for (size_t i = 0; i < num_known; i++) {
        if (crc == F6SC_CRC32_LIST[i]) {
            return true;
        }
    }

    for (int bank = 0; bank < 4; bank++) {
        const uint8_t *page = &rom_data[(size_t)bank * 0x1000u];
        uint8_t first = page[0];
        if (first == 0x00u || first == 0xFFu) {
            bool all_same = true;
            for (int i = 1; i < 0x100; i++) {
                if (page[i] != first) {
                    all_same = false;
                    break;
                }
            }
            if (all_same) {
                return true;
            }
        }
    }

    return false;
}

static bool is_f8sc(size_t size, uint32_t crc) {
    if (size != 0x2000u) {
        return false;
    }

    size_t num_known = sizeof(F8SC_CRC32_LIST) / sizeof(F8SC_CRC32_LIST[0]);
    for (size_t i = 0; i < num_known; i++) {
        if (crc == F8SC_CRC32_LIST[i]) {
            return true;
        }
    }

    return false;
}


static uint8_t read_2k4k(Cartridge *cart, uint16_t address) {
    return cart->rom_data[address & cart->mask];
}

static void write_noop(Cartridge *cart, uint16_t address, uint8_t value) {
    (void)cart;
    (void)address;
    (void)value;
}

static void check_hotspot_f8(Cartridge *cart, uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr == 0x1FF8u) {
        cart->current_bank = 0;
    } else if (addr == 0x1FF9u) {
        cart->current_bank = 1;
    }
}


static uint8_t read_f8(Cartridge *cart, uint16_t address) {
    check_hotspot_f8(cart, address);
    
    return cart->banks[cart->current_bank][address & 0x0FFFu];
}

static void write_f8(Cartridge *cart, uint16_t address, uint8_t value) {
    (void)value;
    check_hotspot_f8(cart, address);
    
}

static void check_hotspot_f6(Cartridge *cart, uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1FF6u && addr <= 0x1FF9u) {
        cart->current_bank = (int)(addr - 0x1FF6u);
    }
}

static uint8_t read_f6(Cartridge *cart, uint16_t address) {
    check_hotspot_f6(cart, address);
    if (!cart || cart->current_bank < 0 || cart->current_bank >= cart->num_banks) {
        return 0xFFu;
    }
    const uint8_t *bank = cart->banks[cart->current_bank];
    if (!bank) {
        return 0xFFu;
    }
    return bank[address & 0x0FFFu];
}

static void write_f6(Cartridge *cart, uint16_t address, uint8_t value) {
    (void)value;
    check_hotspot_f6(cart, address);
}

static uint8_t read_f6sc(Cartridge *cart, uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1080u && addr <= 0x10FFu) {
        return cart->superchip_ram[addr & 0x007Fu];
    }
    return read_f6(cart, addr);
}

static uint8_t read_f8sc(Cartridge *cart, uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1080u && addr <= 0x10FFu) {
        return cart->superchip_ram[addr & 0x007Fu];
    }
    return read_f8(cart, addr);
}

static void write_f8sc(Cartridge *cart, uint16_t address, uint8_t value)  {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1000u && addr <= 0x107Fu) {
        cart->superchip_ram[addr & 0x007Fu] = value;
    }
    return write_f8(cart, addr, value);
}


static void write_f6sc(Cartridge *cart, uint16_t address, uint8_t value) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1000u && addr <= 0x107Fu) {
        cart->superchip_ram[addr & 0x007Fu] = value;
        return;
    }
    write_f6(cart, addr, value);
}

void cart_fe_on_byte(Cartridge *cart, uint8_t val) {
    if (!cart || cart->type != CART_TYPE_FE || cart->num_banks < 2) {
        return;
    }
    cart->current_bank = (val & 0x20u) ? 0 : 1;
}

void cart_fe_on_rts_hi(Cartridge *cart, uint8_t hi) {
    cart_fe_on_byte(cart, hi);
}

uint8_t read_fe(Cartridge *cart, uint16_t addr) {
    uint8_t val = cart->banks[cart->current_bank][addr & 0x0FFFu];
    return val;
}

void write_fe(Cartridge *cart, uint16_t addr, uint8_t val) {
    (void)addr;
    (void)val;
    if (!cart || !cart->has_superchip_ram) {
        return;
    }
}

bool is_f8fe (size_t size, uint32_t crc) {    
    if (size != 0x2000u) {
        return false;
    }

    size_t num_known = sizeof(F8FE_CRC32_LIST) / sizeof(F8FE_CRC32_LIST[0]);
    for (size_t i = 0; i < num_known; i++) {
        if (crc == F8FE_CRC32_LIST[i]) {
            return true;
        }
    }

    return false;

}

bool cart_init_from_bytes(Cartridge *cart, const uint8_t *rom_data, size_t size, const char *name) {
    if (!cart || !rom_data || size == 0) {
        return false;
    }

    memset(cart, 0, sizeof(*cart));

    if (size <= 0x1000u) {
        cart->type = CART_TYPE_2K4K;
        cart->mask = (size <= 0x0800u) ? 0x07FFu : 0x0FFFu;
        cart->read = read_2k4k;
        cart->write = write_noop;
    } else if (size == 0x2000u) {
        cart->crc32 = cart_crc32(rom_data, size);
        if (is_f8fe(size, cart->crc32)) {
            cart->type = CART_TYPE_FE;
            cart->num_banks = 2;
            cart->read = read_fe;
            cart->write = write_fe;
        } else if(is_f8sc(size, cart->crc32)) {
            cart->type = CART_TYPE_F8SC;
            cart->num_banks = 2;
            cart->has_superchip_ram = true;
            cart->read = read_f8sc;
            cart->write = write_f8sc;
        }
        
        else {
            cart->type = CART_TYPE_F8;
            cart->num_banks = 2;
            cart->read = read_f8;
            cart->write = write_f8;
        }

    } else if (size == 0x4000u) {
        cart->crc32 = cart_crc32(rom_data, size);
        if (is_f6sc(rom_data, size, cart->crc32)) {
            cart->type = CART_TYPE_F6SC;
            cart->has_superchip_ram = true;
            cart->read = read_f6sc;
            cart->write = write_f6sc;
        } else {
            cart->type = CART_TYPE_F6;
            cart->read = read_f6;
            cart->write = write_f6;
        }
        cart->num_banks = 4;
    } else {
        fprintf(stderr, "Niet-ondersteunde ROM-grootte: %zu bytes\n", size);
        return false;
    }

    cart->rom_data = (uint8_t *)malloc(size);
    if (!cart->rom_data) {
        return false;
    }
    memcpy(cart->rom_data, rom_data, size);
    cart->rom_size = size;

    if (cart->crc32 == 0) {
        cart->crc32 = cart_crc32(cart->rom_data, size);
    }

    const char *base_name = (name && name[0] != '\0') ? name : "Unknown";
    if (cart->type == CART_TYPE_F8) {
        snprintf(cart->name, sizeof(cart->name), "%s 8K 0x%08X", base_name, cart->crc32);
    } else if (cart->type == CART_TYPE_F6SC) {
        snprintf(cart->name, sizeof(cart->name), "%s 16K SC 0x%08X", base_name, cart->crc32);
    } else if (cart->type == CART_TYPE_F6) {
        snprintf(cart->name, sizeof(cart->name), "%s 16K 0x%08X", base_name, cart->crc32);
    } else {
        snprintf(cart->name, sizeof(cart->name), "%s 0x%08X", base_name, cart->crc32);
    }

    if (cart->type == CART_TYPE_F8 || cart->type == CART_TYPE_F8SC) {
        cart->banks[0] = &cart->rom_data[0x0000u];
        cart->banks[1] = &cart->rom_data[0x1000u];
        uint16_t vec0 = (uint16_t)(cart->banks[0][0xFFCu] | ((uint16_t)cart->banks[0][0xFFDu] << 8));
        cart->current_bank = (vec0 & 0x1000u) ? 0 : 1;
    } else if (cart->type == CART_TYPE_F6 || cart->type == CART_TYPE_F6SC) {
        for (int b = 0; b < 4; b++) {
            cart->banks[b] = &cart->rom_data[(size_t)b * 0x1000u];
        }
        cart->current_bank = 0;
        for (int b = 0; b < 4; b++) {
            uint16_t vec = (uint16_t)(cart->banks[b][0xFFCu] | ((uint16_t)cart->banks[b][0xFFDu] << 8));
            if (vec & 0x1000u) {
                cart->current_bank = b;
                break;
            }
        }
    }

    if (cart->type == CART_TYPE_FE) {
        cart->num_banks = 2;
        cart->banks[0] = &cart->rom_data[0x0000];
        cart->banks[1] = &cart->rom_data[0x1000];
        // Startbank op basis van reset-vector ($1FFC/$1FFD)
        uint16_t vec0 = (uint16_t)(cart->banks[0][0xFFCu] | ((uint16_t)cart->banks[0][0xFFDu] << 8));
        cart->current_bank = (vec0 & 0x1000u) ? 0 : 1;
    }

    return true;
}

void cart_free(Cartridge *cart) {
    if (!cart) {
        return;
    }
    free(cart->rom_data);
    cart->rom_data = NULL;
    cart->rom_size = 0;
}

uint8_t cart_read(Cartridge *cart, uint16_t addr) {
    return cart->read ? cart->read(cart, addr) : 0xFFu;
}

void cart_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (cart->write) {
        cart->write(cart, addr, val);
    }
}