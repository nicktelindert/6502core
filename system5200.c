#define _POSIX_C_SOURCE 200809L
#include "system5200.h"
#include "antic5200.h"
#include "gtia5200.h"
#include "pokey5200.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void system5200_init(System5200 *system) {
    memset(system, 0, sizeof(*system));
}

bool system5200_load_bios(System5200 *system, const uint8_t *data, size_t size) {
    if (!system || !data || size != sizeof(system->bios)) {
        fprintf(stderr, "Atari 5200 BIOS must be exactly 2048 bytes\n");
        return false;
    }
    memmove(system->bios, data, size);
    system->bios_loaded = true;
    return true;
}

bool system5200_load_bios_file(System5200 *system, const char *path, char *error, size_t error_size) {
    if (!system || !path) {
        snprintf(error, error_size, "Invalid BIOS path");
        return false;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(error, error_size, "Open BIOS: %s", strerror(errno));
        return false;
    }
    struct stat st;
    if (fstat(fileno(f), &st) != 0 || !S_ISREG(st.st_mode)) {
        snprintf(error, error_size, "BIOS must be a regular file");
        fclose(f);
        return false;
    }
    uint8_t data[0x801];
    size_t read_size = fread(data, 1, sizeof(data), f);
    bool read_error = ferror(f) != 0;
    int close_result = fclose(f);
    if (read_error || close_result != 0) {
        snprintf(error, error_size, "Unable to read BIOS file");
        return false;
    }
    if (read_size != sizeof(data) - 1) {
        snprintf(error, error_size, "BIOS must be exactly 2048 bytes");
        return false;
    }
    return system5200_load_bios(system, data, read_size);
}

bool system5200_load_cartridge(System5200 *system, System5200CartType type,
                               const uint8_t *data, size_t size) {
    size_t expected;
    switch (type) {
        case SYSTEM5200_CART_4K: expected = 0x1000; break;
        case SYSTEM5200_CART_8K: expected = 0x2000; break;
        case SYSTEM5200_CART_16K:
        case SYSTEM5200_CART_16K_EE: expected = 0x4000; break;
        case SYSTEM5200_CART_32K: expected = 0x8000; break;
        default:
            fprintf(stderr, "Unsupported Atari 5200 cartridge type\n");
            return false;
    }
    if (!system || !data || size != expected) {
        fprintf(stderr, "Atari 5200 cartridge type requires %zu bytes\n", expected);
        return false;
    }
    memmove(system->cartridge, data, size);
    system->cart_type = type;
    return true;
}

void system5200_eject_cartridge(System5200 *system) {
    system->cart_type = SYSTEM5200_CART_NONE;
}

static void complete_virtual_dma(System5200 *system) {
    if (system->antic_chip && system->antic_chip->playfield_dma_pending &&
        system->antic_chip->playfield_dma_virtual)
        /* Idle CPU cycles currently retain the previous bus value. */
        antic5200_playfield_byte(system->antic_chip, system->data_bus);
}

static void system5200_advance(System5200 *system, int cycles, bool defer_virtual) {
    if (cycles <= 0) return;
    system->cycles_this_step += cycles;
    for (int i = 0; i < cycles; i++) {
        system->elapsed_cycles++;
        if (system->antic_chip) antic5200_tick(system->antic_chip);
        if (!defer_virtual) complete_virtual_dma(system);
        if (system->pokey_chip) {
            if (system->gtia_chip)
                pokey5200_set_controller_lines(system->pokey_chip, system->gtia_chip->consol);
            pokey5200_tick(system->pokey_chip);
            if (system->antic_chip && system->antic_chip->cpu)
                cpu_set_irq(system->antic_chip->cpu, pokey5200_irq(system->pokey_chip));
        }
        if (system->clock) system->clock(system->clock_context, 1);
    }
}

void system5200_tick(System5200 *system, int cycles) {
    system5200_advance(system, cycles, false);
}

static System5200Device *device_for_address(System5200 *system, uint16_t address) {
    if (address >= 0xc000 && address <= 0xcfff) return &system->gtia;
    if (address >= 0xd400 && address <= 0xd4ff) return &system->antic;
    if (address >= 0xe800 && address <= 0xefff) return &system->pokey;
    return NULL;
}

static uint8_t read_cartridge(const System5200 *system, uint16_t address) {
    size_t offset;
    switch (system->cart_type) {
        case SYSTEM5200_CART_4K:
            if (address < 0x8000) return 0xff;
            offset = (address - 0x8000) & 0x0fff;
            break;
        case SYSTEM5200_CART_8K:
            if (address < 0x8000) return 0xff;
            offset = (address - 0x8000) & 0x1fff;
            break;
        case SYSTEM5200_CART_16K:
            if (address < 0x8000) return 0xff;
            offset = address - 0x8000;
            break;
        case SYSTEM5200_CART_16K_EE:
            offset = address < 0x6000 ? address - 0x4000 :
                     address < 0xa000 ? address - 0x6000 : address - 0x8000;
            break;
        case SYSTEM5200_CART_32K:
            offset = address - 0x4000;
            break;
        case SYSTEM5200_CART_NONE:
        default:
            return 0xff;
    }
    return system->cartridge[offset];
}

static uint8_t read_mapped_value(System5200 *system, uint16_t address) {
    if (address < 0x4000) return system->ram[address];
    if (address < 0xc000) return read_cartridge(system, address);
    if (address >= 0xf800) return system->bios_loaded ? system->bios[address - 0xf800] : 0xff;
    System5200Device *device = device_for_address(system, address);
    if (device && device->read) return device->read(device->context, address);
    return 0xff;
}

static uint8_t read_mapped(System5200 *system, uint16_t address) {
    system->data_bus = read_mapped_value(system, address);
    return system->data_bus;
}

uint8_t system5200_read(System5200 *system, uint16_t address) {
    system5200_advance(system, 1, true);
    uint8_t value = read_mapped(system, address);
    complete_virtual_dma(system);
    return value;
}

static void write_mapped(System5200 *system, uint16_t address, uint8_t value) {
    system->data_bus = value;
    if (address < 0x4000) {
        system->ram[address] = value;
        return;
    }
    System5200Device *device = device_for_address(system, address);
    if (device && device->write) device->write(device->context, address, value);
    if (system->pokey_chip && system->antic_chip && system->antic_chip->cpu)
        cpu_set_irq(system->antic_chip->cpu, pokey5200_irq(system->pokey_chip));
}

void system5200_write(System5200 *system, uint16_t address, uint8_t value) {
    system5200_advance(system, 1, true);
    write_mapped(system, address, value);
    complete_virtual_dma(system);
}

static void cpu_wait_for_antic(System5200 *system, bool wait_wsync) {
    Antic5200 *antic = system->antic_chip;
    while (antic && ((wait_wsync && antic->wsync) || antic->dma_pending ||
                     antic->playfield_dma_pending || antic->pm_pending)) {
        if (antic->pm_pending & 1) {
            uint8_t value = read_mapped(system, antic->pm_address[0]);
            antic5200_pm_byte(antic, 0, value);
        } else if (antic->dma_pending) {
            uint8_t value = read_mapped(system, antic->dlist);
            antic5200_dma_byte(antic, value);
        } else if (antic->pm_pending) {
            unsigned slot = 1;
            while (!(antic->pm_pending & (1u << slot))) slot++;
            uint8_t value = read_mapped(system, antic->pm_address[slot]);
            antic5200_pm_byte(antic, slot, value);
        } else if (antic->playfield_dma_pending) {
            uint8_t value = read_mapped(system, antic5200_playfield_address(antic));
            antic5200_playfield_byte(antic, value);
        }
        system5200_tick(system, 1);
        system->cycles_this_step--;
    }
}

static uint8_t cpu_read(void *context, uint16_t address) {
    System5200 *system = context;
    cpu_wait_for_antic(system, true);
    system5200_advance(system, 1, true);
    Antic5200 *antic = system->antic_chip;
    if (antic && antic->playfield_dma_pending && antic->playfield_dma_virtual &&
        !antic->wsync && !antic->pm_pending && !antic->dma_pending) {
        uint8_t value = read_mapped(system, address);
        complete_virtual_dma(system);
        return value;
    }
    complete_virtual_dma(system);
    cpu_wait_for_antic(system, true);
    return read_mapped(system, address);
}

static void cpu_write(void *context, uint16_t address, uint8_t value) {
    System5200 *system = context;
    /* DMA halts the CPU even on writes; WSYNC's RDY wait does not. */
    cpu_wait_for_antic(system, false);
    system5200_advance(system, 1, true);
    Antic5200 *antic = system->antic_chip;
    if (antic && antic->playfield_dma_pending && antic->playfield_dma_virtual &&
        !antic->pm_pending && !antic->dma_pending) {
        write_mapped(system, address, value);
        complete_virtual_dma(system);
        return;
    }
    complete_virtual_dma(system);
    cpu_wait_for_antic(system, false);
    write_mapped(system, address, value);
}

static void cpu_tick(void *context, int cycles) {
    System5200 *system = context;
    for (int i = 0; i < cycles; i++) {
        cpu_wait_for_antic(system, true);
        system5200_tick(system, 1);
        cpu_wait_for_antic(system, true);
    }
}

static void cpu_reset_cycles(void *context) {
    ((System5200 *)context)->cycles_this_step = 0;
}

static int cpu_cycles(void *context) {
    return ((System5200 *)context)->cycles_this_step;
}

CpuBus system5200_cpu_bus(System5200 *system) {
    return (CpuBus){
        .context = system,
        .read = cpu_read,
        .write = cpu_write,
        .tick = cpu_tick,
        .reset_cycles = cpu_reset_cycles,
        .get_cycles = cpu_cycles
    };
}

void system5200_attach_antic(System5200 *system, Antic5200 *antic, CPU6502 *cpu) {
    system->antic_chip = antic;
    antic->cpu = cpu;
    antic->gtia = system->gtia_chip;
    system->antic = (System5200Device){antic, antic5200_read, antic5200_write};
}

void system5200_attach_gtia(System5200 *system, GTIA5200 *gtia) {
    system->gtia_chip = gtia;
    if (system->antic_chip) system->antic_chip->gtia = gtia;
    system->gtia = (System5200Device){gtia, gtia5200_read, gtia5200_write};
}

void system5200_attach_pokey(System5200 *system, Pokey5200 *pokey) {
    system->pokey_chip = pokey;
    system->pokey = (System5200Device){pokey, pokey5200_read, pokey5200_write};
}
