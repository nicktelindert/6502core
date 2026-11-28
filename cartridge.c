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
    uint64_t total_cycles;
    uint64_t audio_cycles;
    double fractional_clocks;
    double music_pitch;
    double clock_rate;    
    uint8_t tops[8];
    uint8_t bottoms[8];
    uint16_t counters[8];
    uint8_t flags[8];
    bool music_enabled[3];
    uint8_t random_number;
    const uint8_t *display_rom; /* 2048 bytes (offset 0x2000) */
} DpcState;

static DpcState dpc;

static void dpc_reset_state(const uint8_t *rom_data, size_t size) {
    memset(&dpc, 0, sizeof(dpc));
    dpc.random_number = 0x01u;
    dpc.music_pitch = 20000.0; /* Stella default DPC oscillator frequency */
    dpc.clock_rate = 1193191.66666667; /* Stella NTSC */

    if (rom_data && size >= 0x2800u) {
        dpc.display_rom = &rom_data[0x2000u];
    }
}

static void dpc_clock_random(void) {
    uint8_t bit = (uint8_t)((((dpc.random_number >> 7) ^
                              (dpc.random_number >> 5) ^
                              (dpc.random_number >> 4) ^
                              (dpc.random_number >> 3)) & 1u) ^ 1u);
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

static void dpc_update_music_fetchers(void) {
    uint64_t cycles = dpc.total_cycles - dpc.audio_cycles;
    dpc.audio_cycles = dpc.total_cycles;

    double clocks = (dpc.music_pitch * (double)cycles) /
                    dpc.clock_rate + dpc.fractional_clocks;
    uint32_t whole_clocks = (uint32_t)clocks;
    dpc.fractional_clocks = clocks - (double)whole_clocks;

    if (whole_clocks == 0) {
        return;
    }

    for (int f = 5; f < 8; ++f) {
        int ch = f - 5;
        if (!dpc.music_enabled[ch]) {
            continue;
        }

        int top = (int)dpc.tops[f] + 1;
        int new_low = (int)(dpc.counters[f] & 0x00FFu);

        if (dpc.tops[f] != 0) {
            new_low -= (int)(whole_clocks % (uint32_t)top);
            if (new_low < 0) {
                new_low += top;
            }
        } else {
            new_low = 0;
        }

        if (new_low <= dpc.bottoms[f]) {
            dpc.flags[f] = 0x00u;
        } else if (new_low <= dpc.tops[f]) {
            dpc.flags[f] = 0xFFu;
        }

        dpc.counters[f] =
            (uint16_t)((dpc.counters[f] & 0x0700u) | (uint16_t)new_low);
    }
}

static uint8_t dpc_music_amplitude(void) {
    dpc_update_music_fetchers();
    static const uint8_t amp_table[8] = { 0, 4, 5, 9, 6, 10, 11, 15 };
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
    return dpc.display_rom[0x07FFu - current];
}

static uint8_t read_dpc(Cartridge *cart, uint16_t address) {
    if (!cart || !cart->rom_data) {
        return 0xFFu;
    }
    dpc_clock_random();
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
        unsigned function = (reg >> 3u) & 0x07u;

        dpc_update_flag(f);

        uint8_t result = 0u;
        switch (function) {
            case 0u:
                result = (f < 4) ? dpc.random_number : dpc_music_amplitude();
                break;
            case 1u:
                result = dpc_fetch_byte(f);
                break;
            case 2u:
                result = (uint8_t)(dpc_fetch_byte(f) & dpc.flags[f]);
                break;
            case 7u:
                result = dpc.flags[f];
                break;
            default:
                result = 0u;
                break;
        }

        if (f < 5 || !dpc.music_enabled[f - 5]) {
            dpc.counters[f] =
                (uint16_t)((dpc.counters[f] - 1u) & 0x07FFu);
        }
        return result;
    }

    return cart->banks[cart->current_bank][addr & 0x0FFFu];
}

static void write_dpc(Cartridge *cart, uint16_t address, uint8_t value) {
    if (!cart) {
        return;
    }
    dpc_clock_random();
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
            dpc.flags[f] = 0x00u;
        } else if (reg <= 0x0Fu) {
            dpc.bottoms[f] = value;
        } else if (reg <= 0x17u) {
            uint8_t low = value;
            if (f >= 5 && dpc.music_enabled[f - 5]) {
                low = dpc.tops[f];
            }
            dpc.counters[f] =
                (uint16_t)((dpc.counters[f] & 0x0700u) | low);
        } else if (reg <= 0x1Fu) {
            dpc.counters[f] =
                (uint16_t)((dpc.counters[f] & 0x00FFu) |
                           ((uint16_t)(value & 0x07u) << 8));

            if (f >= 5) {
                int ch = f - 5;
                dpc.music_enabled[ch] = ((value &0x10u) != 0);
            }

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
    if ((address & 0x1FFFu) <= 0x003Fu && cart->num_banks > 0) {
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

static uint8_t read_f4sc(Cartridge *cart, uint16_t address) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1080u && addr <= 0x10FFu) {
        return cart->superchip_ram[addr & 0x007Fu];
    }
    return read_f4(cart, address);
}

static void write_f4sc(Cartridge *cart, uint16_t address, uint8_t value) {
    uint16_t addr = (uint16_t)(address & 0x1FFFu);
    if (addr >= 0x1000u && addr <= 0x107Fu) {
        cart->superchip_ram[addr & 0x007Fu] = value;
        return;
    }
    write_f4(cart, address, value);
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
            for (int b = 0; b < cart->num_banks; b++) {
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
            cart->read = cart->has_superchip_ram ? read_f4sc : read_f4;
            cart->write = cart->has_superchip_ram ? write_f4sc : write_f4;
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

uint8_t cart_read(Cartridge *cart, uint16_t addr, uint8_t data_bus) {
    uint16_t physical_addr = (uint16_t)(addr & 0x1FFFu);
    if ((cart->type == CART_TYPE_F4SC || cart->type == CART_TYPE_F6SC ||
         cart->type == CART_TYPE_F8SC) &&
        physical_addr >= 0x1000u && physical_addr <= 0x107Fu) {
        cart->superchip_ram[physical_addr & 0x007Fu] = data_bus;
        return data_bus;
    }
    return cart->read ? cart->read(cart, addr) : 0xFFu;
}

void cart_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (cart->write) {
        cart->write(cart, addr, val);
    }
}

void cart_step_cycles(Cartridge *cart, int cycles) {
    if (!cart || cart->type != CART_TYPE_DPC || cycles <= 0) {
        return;
    }

    dpc.total_cycles += (uint64_t)cycles;
}

void cart_set_video_standard(Cartridge *cart, VideoStandard standard) {
    if (cart && cart->type == CART_TYPE_DPC) {
        dpc_update_music_fetchers();
        dpc.clock_rate = video_cpu_clock(standard);
    }
}
