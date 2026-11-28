#define _POSIX_C_SOURCE 200809L
#include "../system5200.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

typedef struct {
    uint16_t address;
    uint8_t value;
    int reads, writes, cycles;
} Chip;

static uint8_t chip_read(void *context, uint16_t address) {
    Chip *chip = context;
    chip->address = address;
    chip->reads++;
    return chip->value;
}

static void chip_write(void *context, uint16_t address, uint8_t value) {
    Chip *chip = context;
    chip->address = address;
    chip->value = value;
    chip->writes++;
}

static void chip_tick(void *context, int cycles) {
    ((Chip *)context)->cycles += cycles;
}

static void test_memory_and_chips(void) {
    System5200 system;
    system5200_init(&system);
    Chip gtia = {.value = 0x11}, antic = {.value = 0x22}, pokey = {.value = 0x33};
    system.gtia = (System5200Device){&gtia, chip_read, chip_write};
    system.antic = (System5200Device){&antic, chip_read, chip_write};
    system.pokey = (System5200Device){&pokey, chip_read, chip_write};
    system.clock_context = &pokey;
    system.clock = chip_tick;

    for (unsigned addr = 0; addr < 0x10000; addr++) {
        system5200_write(&system, (uint16_t)addr, (uint8_t)(addr ^ (addr >> 8)));
        uint8_t expected;
        if (addr < 0x4000) expected = (uint8_t)(addr ^ (addr >> 8));
        else if (addr >= 0xc000 && addr <= 0xcfff) expected = gtia.value;
        else if (addr >= 0xd400 && addr <= 0xd4ff) expected = antic.value;
        else if (addr >= 0xe800 && addr <= 0xefff) expected = pokey.value;
        else expected = 0xff;
        CHECK(system5200_read(&system, (uint16_t)addr) == expected);
    }
    CHECK(gtia.reads == 0x1000 && gtia.writes == 0x1000 && gtia.address == 0xcfff);
    CHECK(antic.reads == 0x100 && antic.writes == 0x100 && antic.address == 0xd4ff);
    CHECK(pokey.reads == 0x800 && pokey.writes == 0x800 && pokey.address == 0xefff);
    CHECK(pokey.cycles == 2 * 65536);
    CHECK(system.cycles_this_step == 2 * 65536);
    system5200_tick(&system, 3);
    CHECK(system.cycles_this_step == 2 * 65536 + 3 && pokey.cycles == 2 * 65536 + 3);
}

static void test_images(void) {
    System5200 system;
    system5200_init(&system);
    uint8_t bios[2048], cart[32768];
    for (size_t i = 0; i < sizeof(bios); i++) bios[i] = (uint8_t)(i ^ (i >> 8));
    for (size_t i = 0; i < sizeof(cart); i++) cart[i] = (uint8_t)((i >> 8) ^ i);
    CHECK(!system5200_load_bios(&system, bios, sizeof(bios) - 1));
    CHECK(!system5200_load_bios(&system, NULL, sizeof(bios)));
    CHECK(system5200_load_bios(&system, bios, sizeof(bios)));
    CHECK(!system5200_load_bios(&system, bios, sizeof(bios) - 1));
    for (unsigned a = 0xf800; a <= 0xffff; a++) {
        CHECK(system5200_read(&system, (uint16_t)a) == bios[a - 0xf800]);
        system5200_write(&system, (uint16_t)a, 0x7f);
        CHECK(system5200_read(&system, (uint16_t)a) == bios[a - 0xf800]);
    }
    static const struct { System5200CartType type; size_t size; } types[] = {
        {SYSTEM5200_CART_4K, 4096}, {SYSTEM5200_CART_8K, 8192},
        {SYSTEM5200_CART_16K, 16384}, {SYSTEM5200_CART_16K_EE, 16384},
        {SYSTEM5200_CART_32K, 32768},
    };
    for (size_t t = 0; t < sizeof(types) / sizeof(types[0]); t++) {
        CHECK(system5200_load_cartridge(&system, types[t].type, cart, types[t].size));
        for (unsigned a = 0x4000; a <= 0xbfff; a++) {
            uint8_t expected = 0xff;
            if (types[t].type == SYSTEM5200_CART_32K) expected = cart[a - 0x4000];
            else if (types[t].type == SYSTEM5200_CART_16K_EE)
                expected = cart[a < 0x6000 ? a - 0x4000 :
                                a < 0xa000 ? a - 0x6000 : a - 0x8000];
            else if (a >= 0x8000) expected = cart[(a - 0x8000) % types[t].size];
            CHECK(system5200_read(&system, (uint16_t)a) == expected);
            system5200_write(&system, (uint16_t)a, (uint8_t)~expected);
            CHECK(system5200_read(&system, (uint16_t)a) == expected);
        }
    }
    CHECK(!system5200_load_cartridge(&system, SYSTEM5200_CART_16K, cart, 8192));
    CHECK(!system5200_load_cartridge(&system, SYSTEM5200_CART_32K, NULL, sizeof(cart)));
    CHECK(!system5200_load_cartridge(&system, (System5200CartType)99, cart, sizeof(cart)));
    CHECK(system5200_read(&system, 0x4000) == cart[0]);
    system5200_eject_cartridge(&system);
    CHECK(system5200_read(&system, 0x4000) == 0xff);
}

static void test_cpu_integration(void) {
    System5200 system;
    CPU6502 cpu;
    system5200_init(&system);
    uint8_t bios[2048] = {0}, cart[32768] = {0};
    bios[0] = 0x4c; bios[1] = 0x00; bios[2] = 0x40; /* JMP $4000 */
    bios[0x20] = 0x40; /* RTI */
    bios[0x7fa] = 0x20; bios[0x7fb] = 0xf8;
    bios[0x7fc] = 0x00; bios[0x7fd] = 0xf8;
    const uint8_t program[] = {0xa9, 0x5a, 0x8d, 0xff, 0x3f, 0xad, 0xff, 0x3f};
    memcpy(cart, program, sizeof(program));
    CHECK(system5200_load_bios(&system, bios, sizeof(bios)));
    CHECK(system5200_load_cartridge(&system, SYSTEM5200_CART_32K, cart, sizeof(cart)));
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    CHECK(cpu.PC == 0xf800 && system.cycles_this_step == 2);
    CHECK(cpu_step(&cpu) == 3 && cpu.PC == 0x4000);
    cpu_set_nmi(&cpu, true);
    CHECK(cpu_step(&cpu) == 7 && cpu.PC == 0xf820);
    cpu_set_nmi(&cpu, false);
    CHECK(cpu_step(&cpu) == 6 && cpu.PC == 0x4000);
    CHECK(cpu_step(&cpu) == 2 && cpu.A == 0x5a);
    CHECK(cpu_step(&cpu) == 4 && system.ram[0x3fff] == 0x5a);
    cpu.A = 0;
    CHECK(cpu_step(&cpu) == 4 && cpu.A == 0x5a);
    CHECK(system.cycles_this_step == 4);
}

static void test_bios_file(void) {
    char name[] = "/tmp/6502core-bios-XXXXXX", error[256];
    int fd = mkstemp(name);
    CHECK(fd >= 0);
    FILE *f = fdopen(fd, "wb");
    CHECK(f);
    uint8_t bytes[2049];
    memset(bytes, 0x5a, sizeof(bytes));
    CHECK(fwrite(bytes, 1, 2048, f) == 2048 && fclose(f) == 0);
    System5200 system;
    system5200_init(&system);
    CHECK(system5200_load_bios_file(&system, name, error, sizeof(error)));
    CHECK(system.bios_loaded && system.bios[0] == 0x5a);
    f = fopen(name, "ab");
    CHECK(f && fputc(0, f) != EOF && fclose(f) == 0);
    CHECK(!system5200_load_bios_file(&system, name, error, sizeof(error)));
    CHECK(system.bios[0] == 0x5a);
    f = fopen(name, "wb");
    CHECK(f && fputc(0, f) != EOF && fclose(f) == 0);
    CHECK(!system5200_load_bios_file(&system, name, error, sizeof(error)));
    CHECK(!system5200_load_bios_file(&system, "/missing-5200-bios", error, sizeof(error)));
    CHECK(!system5200_load_bios_file(&system, "/tmp", error, sizeof(error)));
    CHECK(unlink(name) == 0);
}

int main(void) {
    test_memory_and_chips();
    test_images();
    test_cpu_integration();
    test_bios_file();
    puts("Atari 5200 system tests passed");
    return 0;
}
