#include "cartridge.h"
#include "detectchip.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t e7_ram[4][256];
static int e7_lower_bank = 0;      /* 0..7 voor 2K ROM-banken, of 0..3 voor RAM-bank */
static bool e7_lower_is_ram = false;

static void e7_reset_state(void) {
    memset(e7_ram, 0, sizeof(e7_ram));
    e7_lower_bank = 0;
    e7_lower_is_ram = false;
}

static void check_hotspot_ua(Cartridge *cart, uint16_t address) {
    if (!cart || (address & 0x1000u) == 0) {
        return;
    }
    uint8_t lo = (uint8_t)(address & 0x00FFu);
    if (lo == 0x20u) {
        cart->current_bank = 0;
    } else if (lo == 0x40u) {
        cart->current_bank = 1;
    }
}

typedef struct {
    uint8_t tops[8];
    uint8_t bottoms[8];
    uint16_t counters[8];
    uint8_t flags[8];
    uint32_t music_phase[3];
    bool music_enabled[3];
    uint8_t random_number;
    const uint8_t *display_rom; /* 2048 bytes (offset 0x2000) */
} DpcState;

static DpcState dpc;

static void dpc_reset_state(const uint8_t *rom_data, size_t size) {
    memset(&dpc, 0, sizeof(dpc));
    dpc.random_number = 0x01u;

    if (rom_data && size >= 0x2800u) {
        dpc.display_rom = &rom_data[0x2000u];
    }
}


static uint8_t dpc_bit_reverse(uint8_t b) {
    b = (uint8_t)(((b & 0xF0u) >> 4) | ((b & 0x0Fu) << 4));
    b = (uint8_t)(((b & 0xCCu) >> 2) | ((b & 0x33u) << 2));
    b = (uint8_t)(((b & 0xAAu) >> 1) | ((b & 0x55u) << 1));
    return b;
}



static void dpc_clock_random(void) {
    uint8_t bit = (uint8_t)(((dpc.random_number >> 7) ^
                             (dpc.random_number >> 5) ^
                             (dpc.random_number >> 4) ^
                             (dpc.random_number >> 3)) & 1u);
    dpc.random_number = (uint8_t)((dpc.random_number << 1) | bit);
}

static void dpc_update_flag(int f) {
    uint8_t low = (uint8_t)(dpc.counters[f] & 0x00FFu);
    if (low == dpc.tops[f]) {
        dpc.flags[f] = 0xFFu;
    } else if (low == dpc.bottoms[f]) {
        dpc.flags[f] = 0x00u;
    }
}

static void dpc_step_music(void) {
    for (int ch = 0; ch < 3; ch++) {
        if (!dpc.music_enabled[ch]) {
            continue;
        }

        dpc.music_phase[ch]++;
        if (dpc.music_phase[ch] >= 1u) {
            dpc.music_phase[ch] = 0;
            dpc.flags[5 + ch] = dpc.flags[5 + ch] ? 0u : 0xFFu;
        }
    }
}


static uint8_t dpc_music_amplitude(void) {
    static const uint8_t amp_table[8] = { 0, 4, 5, 9, 6, 10, 11, 15 };
    dpc_step_music();
    uint8_t mask = 0;
    for (int ch = 0; ch < 3; ch++) {
        if (dpc.music_enabled[ch] && dpc.flags[5 + ch]) {
            mask |= (uint8_t)(1u << ch);
        }
    }
    return amp_table[mask];
}

static uint8_t dpc_fetch_byte(int f) {
    if (!dpc.display_rom) {
        return 0xFFu;
    }

    uint16_t current = (uint16_t)(dpc.counters[f] & 0x07FFu);
    uint16_t rom_index = (uint16_t)(0x07FFu - current);
    uint8_t val = dpc.display_rom[rom_index];

    /* Fetchers tellen per read omlaag en wikkelen modulo 0x800. */
    dpc.counters[f] = (uint16_t)((current - 1u) & 0x07FFu);

    /* De flags vergelijken de lage 8 bits van de nieuwe teller. */
    dpc_update_flag(f);
    return val;
}

static uint8_t read_dpc(Cartridge *cart, uint16_t address) {
    if (!cart || !cart->rom_data) {
        return 0xFFu;
    }
    // Hangt rond de 0x1006
    uint16_t addr = (uint16_t)(address & 0x1FFFu);

    /* Bankswitch hotspots (0x1FF8 / 0x1FF9) */
    if (addr == 0x1FF8u) {
        cart->current_bank = 0;
        return cart->banks[cart->current_bank][addr & 0x0FFFu];
    }
    if (addr == 0x1FF9u) {
        cart->current_bank = 1;
        return cart->banks[cart->current_bank][addr & 0x0FFFu];
    }

    /* DPC Read Registers: $1000 - $103F */
    if (addr >= 0x1000u && addr <= 0x103Fu) {
        unsigned reg = (unsigned)(addr - 0x1000u);
        int f = (int)(reg & 0x07u);

        if (reg <= 0x03u) {
            /* $1000-$1003: Random number generator */
            dpc_clock_random();
            return dpc.random_number;
        }
        if (reg <= 0x07u) {
            /* $1004-$1007: Music amplitude mixer */
            return dpc_music_amplitude();
        }
        if (reg <= 0x0Fu) {
            /* $1008-$100F: Direct fetchers */
            return dpc_fetch_byte(f);
        }
        if (reg <= 0x17u) {
            /* $1010-$1017: Masked fetchers */
            uint8_t flag = dpc.flags[f];
            uint8_t data = dpc_fetch_byte(f);
            return (uint8_t)(data & flag);
        }
        if (reg <= 0x1Fu) {
            /* $1018-$101F: Masked + bit-reversed fetchers */
            uint8_t flag = dpc.flags[f];
            uint8_t data = dpc_fetch_byte(f);
            return dpc_bit_reverse((uint8_t)(data & flag));
        }
        if (reg <= 0x27u) {
            /* $1020-$1027: Masked + nibble-swapped fetchers */
            uint8_t flag = dpc.flags[f];
            uint8_t data = dpc_fetch_byte(f);
            uint8_t masked = (uint8_t)(data & flag);
            return (uint8_t)(((masked & 0x0Fu) << 4) |
                             ((masked & 0xF0u) >> 4));
        }
        if (reg <= 0x2Fu) {
            /* $1028-$102F: Masked + shift-right fetchers */
            uint8_t flag = dpc.flags[f];
            uint8_t data = dpc_fetch_byte(f);
            return (uint8_t)((data & flag) >> 1);
        }
        if (reg <= 0x37u) {
            /* $1030-$1037: Masked + shift-left fetchers */
            uint8_t flag = dpc.flags[f];
            uint8_t data = dpc_fetch_byte(f);
            return (uint8_t)((data & flag) << 1);
        }

        /* $1038-$103F: Fetcher flags */
        return dpc.flags[f];
    }

    return cart->banks[cart->current_bank][addr & 0x0FFFu];
}

static void write_dpc(Cartridge *cart, uint16_t address, uint8_t value) {
    if (!cart) {
        return;
    }

    uint16_t addr = (uint16_t)(address & 0x1FFFu);

    if (addr == 0x1FF8u) {
        cart->current_bank = 0;
        return;
    }
    if (addr == 0x1FF9u) {
        cart->current_bank = 1;
        return;
    }

    /* DPC Write Registers: $1040 - $107F */
    if (addr >= 0x1040u && addr <= 0x107Fu) {
        unsigned reg = (unsigned)(addr - 0x1040u);
        int f = (int)(reg & 0x07u);

        if (reg <= 0x07u) {
            dpc.tops[f] = value;
            dpc_update_flag(f);
        } else if (reg <= 0x0Fu) {
            dpc.bottoms[f] = value;
            dpc_update_flag(f);
        } else if (reg <= 0x17u) {
            dpc.counters[f] =
                (uint16_t)((dpc.counters[f] & 0x0700u) | value);
            dpc_update_flag(f);
        } else if (reg <= 0x1Fu) {
            dpc.counters[f] =
                (uint16_t)((dpc.counters[f] & 0x00FFu) |
                           ((uint16_t)(value & 0x07u) << 8));

            if (f >= 5) {
                int ch = f - 5;
                dpc.music_enabled[ch] = ((value &0x10u) != 0);

                if (!dpc.music_enabled[ch]) {
                    dpc.music_phase[ch] = 0;
                    dpc.flags[f] = 0;
                }
            }

            dpc_update_flag(f);
        } else if (reg >= 0x30u && reg <= 0x37u) {
            /* $1070-$1077 */
            dpc.random_number = 0x01u;
        }
    }
}

static uint8_t read_ua(Cartridge *cart, uint16_t address) {
    check_hotspot_ua(cart, address);
    if (!cart || cart->current_bank < 0 || cart->current_bank >= cart->num_banks) {
        return 0xFFu;
    }
    return cart->banks[cart->current_bank][address & 0x0FFFu];
}

static void write_ua(Cartridge *cart, uint16_t address, uint8_t value) {
    (void)value;
    check_hotspot_ua(cart, address);
}

static void check_hotspot_e7(uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1FE0u && addr <= 0x1FE7u) {
        e7_lower_is_ram = false;
        e7_lower_bank = (int)(addr - 0x1FE0u);
    } else if (addr >= 0x1FE8u && addr <= 0x1FEBu) {
        e7_lower_is_ram = true;
        e7_lower_bank = (int)(addr - 0x1FE8u);
    }
}

static uint8_t read_e7(Cartridge *cart, uint16_t address) {
    check_hotspot_e7(address);
    if (!cart || !cart->rom_data || cart->rom_size < 0x4000u) {
        return 0xFFu;
    }

    uint16_t addr = (uint16_t)(address & 0x0FFFu);
    if (addr >= 0x0800u) {
        return cart->rom_data[0x3800u + (addr - 0x0800u)];
    }

    if (e7_lower_is_ram) {
        if (addr >= 0x0400u) {
            return e7_ram[e7_lower_bank & 3][addr & 0x00FFu];
        }
        return 0xFFu;
    }

    size_t bank_offset = (size_t)(e7_lower_bank & 7) * 0x0800u;
    return cart->rom_data[bank_offset + addr];
}

static void write_e7(Cartridge *cart, uint16_t address, uint8_t value) {
    (void)cart;
    check_hotspot_e7(address);

    uint16_t addr = (uint16_t)(address & 0x0FFFu);
    if (e7_lower_is_ram && addr < 0x0400u) {
        e7_ram[e7_lower_bank & 3][addr & 0x00FFu] = value;
    }
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

static void check_hotspot_f4(Cartridge *cart, uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1FF4u && addr <= 0x1FFBu) {
        cart->current_bank = (int)(addr - 0x1FF4u);
    }
}

static uint8_t read_f4(Cartridge *cart, uint16_t address) {
    check_hotspot_f4(cart, address);
    if (!cart || cart->current_bank < 0 || cart->current_bank >= cart->num_banks) {
        return 0xFFu;
    }
    return cart->banks[cart->current_bank][address & 0x0FFFu];
}

static void write_f4(Cartridge *cart, uint16_t address, uint8_t value) {
    (void)value;
    check_hotspot_f4(cart, address);
}

static void check_hotspot_ef(Cartridge *cart, uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1FE0u && addr <= 0x1FEFu) {
        cart->current_bank = (int)(addr - 0x1FE0u);
    }
}

static uint8_t read_ef(Cartridge *cart, uint16_t address) {
    check_hotspot_ef(cart, address);
    if (!cart || cart->current_bank < 0 || cart->current_bank >= cart->num_banks) {
        return 0xFFu;
    }
    return cart->banks[cart->current_bank][address & 0x0FFFu];
}

static void write_ef(Cartridge *cart, uint16_t address, uint8_t value) {
    (void)value;
    check_hotspot_ef(cart, address);
}

static void check_hotspot_3f(Cartridge *cart, uint16_t address, uint8_t value) {
    if ((address & 0x003Fu) == 0x003Fu && cart->num_banks > 0) {
        cart->current_bank = (int)(value % (uint8_t)cart->num_banks);
    }
}

static uint8_t read_3f(Cartridge *cart, uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x0FFFu);
    if (addr >= 0x0800u && cart->rom_size >= 0x0800u) {
        return cart->rom_data[(cart->rom_size - 0x0800u) + (addr - 0x0800u)];
    }
    return cart->banks[cart->current_bank][addr & 0x07FFu];
}

static void write_3f(Cartridge *cart, uint16_t address, uint8_t value) {
    check_hotspot_3f(cart, address, value);
}

static void check_hotspot_e0(Cartridge *cart, uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1FE0u && addr <= 0x1FE7u) {
        size_t slice = (size_t)(addr - 0x1FE0u);
        cart->banks[0] = &cart->rom_data[slice * 0x0400u];
    } else if (addr >= 0x1FE8u && addr <= 0x1FEFu) {
        size_t slice = (size_t)(addr - 0x1FE8u);
        cart->banks[1] = &cart->rom_data[slice * 0x0400u];
    } else if (addr >= 0x1FF0u && addr <= 0x1FF7u) {
        size_t slice = (size_t)(addr - 0x1FF0u);
        cart->banks[2] = &cart->rom_data[slice * 0x0400u];
    }
}

static uint8_t read_e0(Cartridge *cart, uint16_t address) {
    check_hotspot_e0(cart, address);
    uint16_t addr = (uint16_t)(address & 0x0FFFu);
    size_t slot = (size_t)(addr >> 10);
    return cart->banks[slot][addr & 0x03FFu];
}

static void write_e0(Cartridge *cart, uint16_t address, uint8_t value) {
    (void)value;
    check_hotspot_e0(cart, address);
}

static void check_hotspot_fa(Cartridge *cart, uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1FF8u && addr <= 0x1FFAu) {
        cart->current_bank = (int)(addr - 0x1FF8u);
    }
}

static uint8_t read_fa(Cartridge *cart, uint16_t address) {
    check_hotspot_fa(cart, address);
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1100u && addr <= 0x11FFu) {
        return cart->superchip_ram[addr & 0x00FFu];
    }
    return cart->banks[cart->current_bank][address & 0x0FFFu];
}

static void write_fa(Cartridge *cart, uint16_t address, uint8_t value) {
    check_hotspot_fa(cart, address);
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1000u && addr <= 0x10FFu) {
        cart->superchip_ram[addr & 0x00FFu] = value;
    }
}

static uint8_t read_f6sc(Cartridge *cart, uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1080u && addr <= 0x10FFu) {
        return cart->superchip_ram[addr & 0x007Fu];
    }
    return read_f6(cart, address);
}

static uint8_t read_f8sc(Cartridge *cart, uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1080u && addr <= 0x10FFu) {
        return cart->superchip_ram[addr & 0x007Fu];
    }
    return read_f8(cart, address);
}

static void write_f8sc(Cartridge *cart, uint16_t address, uint8_t value) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1000u && addr <= 0x107Fu) {
        cart->superchip_ram[addr & 0x007Fu] = value;
        return;
    }
    write_f8(cart, address, value);
}

static void write_f6sc(Cartridge *cart, uint16_t address, uint8_t value) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1000u && addr <= 0x107Fu) {
        cart->superchip_ram[addr & 0x007Fu] = value;
        return;
    }
    write_f6(cart, address, value);
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
    return cart->banks[cart->current_bank][addr & 0x0FFFu];
}

void write_fe(Cartridge *cart, uint16_t addr, uint8_t val) {
    (void)addr;
    (void)val;
    if (!cart || !cart->has_superchip_ram) {
        return;
    }
}

bool cart_init_from_bytes(Cartridge *cart, const uint8_t *rom_data, size_t size, const char *name) {
    if (!cart || !rom_data || size == 0) {
        return false;
    }

    if (size > 0x10000u) {
        fprintf(stderr, "Niet-ondersteunde ROM-grootte: %zu bytes\n", size);
        return false;
    }

    memset(cart, 0, sizeof(*cart));
    e7_reset_state();

    cart->rom_data = (uint8_t *)malloc(size);
    if (!cart->rom_data) {
        return false;
    }
    memcpy(cart->rom_data, rom_data, size);
    cart->rom_size = size;

    cart->type = detect_cart_type(cart->rom_data, size);

    switch (cart->type) {
        case CART_TYPE_2K:
        case CART_TYPE_4K:
        case CART_TYPE_CV:
            cart->mask = (size <= 0x0800u) ? 0x07FFu : 0x0FFFu;
            cart->read = read_2k4k;
            cart->write = write_noop;
            break;

        case CART_TYPE_FE:
            cart->num_banks = 2;
            cart->banks[0] = &cart->rom_data[0x0000u];
            cart->banks[1] = &cart->rom_data[0x1000u];
            cart->read = read_fe;
            cart->write = write_fe;
            {
                uint16_t vec0 = (uint16_t)(cart->banks[0][0xFFCu] | ((uint16_t)cart->banks[0][0xFFDu] << 8));
                cart->current_bank = (vec0 & 0x1000u) ? 0 : 1;
            }
            break;

        case CART_TYPE_UA:
            cart->num_banks = 2;
            cart->banks[0] = &cart->rom_data[0x0000u];
            cart->banks[1] = &cart->rom_data[0x1000u];
            cart->read = read_ua;
            cart->write = write_ua;
            cart->current_bank = 1;
            break;

        case CART_TYPE_E7:
            cart->num_banks = 8;
            cart->read = read_e7;
            cart->write = write_e7;
            e7_lower_is_ram = false;
            e7_lower_bank = 0;
            break;

        case CART_TYPE_3F:
            cart->num_banks = (int)(size / 0x0800u);
            for (int b = 0; b < cart->num_banks && b < 16; b++) {
                cart->banks[b] = &cart->rom_data[(size_t)b * 0x0800u];
            }
            cart->current_bank = 0;
            cart->read = read_3f;
            cart->write = write_3f;
            break;

        case CART_TYPE_E0:
            cart->num_banks = 8;
            cart->banks[0] = &cart->rom_data[0x0000u];
            cart->banks[1] = &cart->rom_data[0x0400u];
            cart->banks[2] = &cart->rom_data[0x0800u];
            cart->banks[3] = &cart->rom_data[0x1C00u];
            cart->read = read_e0;
            cart->write = write_e0;
            break;

        case CART_TYPE_FA:
            cart->num_banks = 3;
            cart->has_superchip_ram = true;
            cart->banks[0] = &cart->rom_data[0x0000u];
            cart->banks[1] = &cart->rom_data[0x1000u];
            cart->banks[2] = &cart->rom_data[0x2000u];
            cart->read = read_fa;
            cart->write = write_fa;
            break;

        case CART_TYPE_F8SC:
            cart->has_superchip_ram = true;
            /* fallthrough */
        case CART_TYPE_F8:
            cart->num_banks = 2;
            cart->banks[0] = &cart->rom_data[0x0000u];
            cart->banks[1] = &cart->rom_data[0x1000u];
            cart->read = cart->has_superchip_ram ? read_f8sc : read_f8;
            cart->write = cart->has_superchip_ram ? write_f8sc : write_f8;
            {
                uint16_t vec0 = (uint16_t)(cart->banks[0][0xFFCu] | ((uint16_t)cart->banks[0][0xFFDu] << 8));
                cart->current_bank = (vec0 & 0x1000u) ? 0 : 1;
            }
            break;

        case CART_TYPE_DPC:
            cart->num_banks = 2;
            cart->banks[0] = &cart->rom_data[0x0000u];
            cart->banks[1] = &cart->rom_data[0x1000u];
            cart->current_bank = 1;
            cart->read = read_dpc;
            cart->write = write_dpc;
            dpc_reset_state(cart->rom_data, size);
            break;

        case CART_TYPE_F6SC:
            cart->has_superchip_ram = true;
            /* fallthrough */
        case CART_TYPE_F6:
            cart->num_banks = 4;
            cart->read = cart->has_superchip_ram ? read_f6sc : read_f6;
            cart->write = cart->has_superchip_ram ? write_f6sc : write_f6;
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
            break;

        case CART_TYPE_F4SC:
            cart->has_superchip_ram = true;
            /* fallthrough */
        case CART_TYPE_F4:
            cart->num_banks = 8;
            cart->read = read_f4;
            cart->write = write_f4;
            for (int b = 0; b < 8; b++) {
                cart->banks[b] = &cart->rom_data[(size_t)b * 0x1000u];
            }
            cart->current_bank = 7;
            break;

        case CART_TYPE_EF:
            cart->num_banks = 16;
            cart->read = read_ef;
            cart->write = write_ef;
            for (int b = 0; b < 16 && ((size_t)b + 1u) * 0x1000u <= size; b++) {
                cart->banks[b] = &cart->rom_data[(size_t)b * 0x1000u];
            }
            cart->current_bank = 0;
            break;

        default:
            cart->type = CART_TYPE_4K;
            cart->mask = (size <= 0x0800u) ? 0x07FFu : 0x0FFFu;
            cart->read = read_2k4k;
            cart->write = write_noop;
            break;
    }

    const char *base_name = (name && name[0] != '\0') ? name : "Unknown";
    snprintf(cart->name, sizeof(cart->name), "%s (type %d)", base_name, (int)cart->type);

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
