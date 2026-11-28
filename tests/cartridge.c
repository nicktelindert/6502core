#include "../bus.h"
#include "../cpu6502.h"
#include "../tia.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", \
                __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void init_f6sc(Cartridge *cart, Bus *bus) {
    uint8_t rom[4 * 0x1000] = {0};

    for (int bank = 0; bank < 4; bank++) {
        rom[bank * 0x1000 + 0x0100] = (uint8_t)(0x20 + bank);
        rom[bank * 0x1000 + 0x0200] = (uint8_t)(0x40 + bank);
        rom[bank * 0x1000 + 0x0FFF] = (uint8_t)(0x60 + bank);
        for (int hotspot = 0; hotspot < 4; hotspot++) {
            rom[bank * 0x1000 + 0x0FF6 + hotspot] =
                (uint8_t)(0x80 + bank * 4 + hotspot);
            int program = bank * 0x1000 + 0x0300 + hotspot * 8;
            rom[program] = 0x2C; /* BIT $FFF6+hotspot */
            rom[program + 1] = (uint8_t)(0xF6 + hotspot);
            rom[program + 2] = 0xFF;
            rom[program + 3] = 0xA9; /* LDA #bank */
            rom[program + 4] = (uint8_t)(0x40 + bank);
        }
    }

    CHECK(cart_init_from_bytes(cart, rom, sizeof(rom), "F6SC test"));
    CHECK(cart->type == CART_TYPE_F6SC);
    CHECK(cart->num_banks == 4);
    CHECK(cart->has_superchip_ram);
    bus_init(bus, NULL, NULL, cart, NULL, NULL, NULL);
}

static void test_f6sc_hotspots(void) {
    Cartridge cart;
    Bus bus;
    const uint16_t mirrors[] = {0x1FF6, 0xFFF6};
    init_f6sc(&cart, &bus);

    bus_write(&bus, 0xF000, 0x5A);
    bus_write(&bus, 0xF07F, 0xA5);

    for (size_t mirror = 0; mirror < sizeof(mirrors) / sizeof(mirrors[0]); mirror++) {
        for (int source = 0; source < 4; source++) {
            for (int target = 0; target < 4; target++) {
                uint16_t hotspot = (uint16_t)(mirrors[mirror] + target);

                bus_write(&bus, (uint16_t)(0xFFF6 + source), 0);
                CHECK(cart.current_bank == source);
                CHECK(bus_read(&bus, hotspot) == (uint8_t)(0x80 + target * 4 + target));
                CHECK(cart.current_bank == target);
                CHECK(bus_read(&bus, 0xF200) == (uint8_t)(0x40 + target));
                CHECK(bus_read(&bus, 0xF080) == 0x5A);
                CHECK(bus_read(&bus, 0xF0FF) == 0xA5);

                bus_write(&bus, (uint16_t)(0xFFF6 + source), 0);
                CHECK(cart.current_bank == source);
                bus_write(&bus, hotspot, 0xFF);
                CHECK(cart.current_bank == target);
                CHECK(bus_read(&bus, 0x1200) == (uint8_t)(0x40 + target));
                CHECK(bus_read(&bus, 0x1080) == 0x5A);
                CHECK(bus_read(&bus, 0x10FF) == 0xA5);

                (void)bus_read(&bus, 0xFFF5);
                bus_write(&bus, 0xFFFA, 0);
                CHECK(cart.current_bank == target);
            }
        }
    }

    cart_free(&cart);
}

static void test_f6sc_cpu_hotspots(void) {
    Cartridge cart;
    Bus bus;
    CPU6502 cpu;
    init_f6sc(&cart, &bus);
    cpu_init(&cpu, &bus);

    for (int target = 0; target < 4; target++) {
        bus_write(&bus, (uint16_t)(0xFFF6 + ((target + 1) % 4)), 0);
        bus.ram[0] = 0xAD; /* LDA $FFF6+target */
        bus.ram[1] = (uint8_t)(0xF6 + target);
        bus.ram[2] = 0xFF;
        cpu.PC = 0x0080;
        CHECK(cpu_step(&cpu) == 4);
        CHECK(cart.current_bank == target);
        CHECK(cpu.A == (uint8_t)(0x80 + target * 4 + target));

        bus_write(&bus, (uint16_t)(0xFFF6 + ((target + 1) % 4)), 0);
        bus.ram[0] = 0x8D; /* STA $FFF6+target */
        cpu.PC = 0x0080;
        CHECK(cpu_step(&cpu) == 4);
        CHECK(cart.current_bank == target);
        CHECK(bus_read(&bus, 0xF200) == (uint8_t)(0x40 + target));
    }

    cart_free(&cart);
}

static void test_branch_dummy_hotspot(void) {
    Cartridge cart;
    Bus bus;
    CPU6502 cpu;
    init_f6sc(&cart, &bus);
    cpu_init(&cpu, &bus);
    cart.rom_data[3 * 0x1000 + 0xFF4] = 0xD0; /* BNE +2 */
    cart.rom_data[3 * 0x1000 + 0xFF5] = 0x02;

    bus_write(&bus, 0xFFF9, 0);
    cpu.PC = 0xFFF4;
    cpu.status |= CPU_FLAG_Z;
    CHECK(cpu_step(&cpu) == 2);
    CHECK(cart.current_bank == 3);

    cpu.PC = 0xFFF4;
    cpu.status &= (uint8_t)~CPU_FLAG_Z;
    CHECK(cpu_step(&cpu) == 3);
    CHECK(cpu.PC == 0xFFF8);
    CHECK(cart.current_bank == 0);
    CHECK(bus.data_bus == 0x80);
    cart_free(&cart);
}

static void test_fe_subroutine_bank_switch(void) {
    Cartridge cart = {0};
    Bus bus;
    CPU6502 cpu;
    cart.type = CART_TYPE_FE;
    cart.num_banks = 2;
    bus_init(&bus, NULL, NULL, &cart, NULL, NULL, NULL);
    cpu_init(&cpu, &bus);
    bus.ram[0] = 0x20; /* JSR $20A0 */
    bus.ram[1] = 0xA0;
    bus.ram[2] = 0x20;
    bus.ram[0x20] = 0x60; /* RTS */
    cpu.PC = 0x0080;
    cart.current_bank = 1;
    CHECK(cpu_step(&cpu) == 6);
    CHECK(cpu.PC == 0x20A0);
    CHECK(cart.current_bank == 0);
    CHECK(cpu_step(&cpu) == 6);
    CHECK(cpu.PC == 0x0083);
    CHECK(cart.current_bank == 1);
}

static void test_fa_ram_and_banks(void) {
    uint8_t rom[3 * 4096];
    for (size_t i = 0; i < sizeof(rom); i++) {
        rom[i] = (uint8_t)((i * 13 + (i >> 12) * 31) & 0xFF);
    }
    Cartridge cart;
    Bus bus;
    CHECK(cart_init_from_bytes(&cart, rom, sizeof(rom), "FA test"));
    CHECK(cart.type == CART_TYPE_FA);
    bus_init(&bus, NULL, NULL, &cart, NULL, NULL, NULL);
    for (unsigned offset = 0; offset < 256; offset++) {
        bus_write(&bus, (uint16_t)(0xF000 + offset), (uint8_t)(offset ^ 0xA5));
    }
    for (unsigned bank = 0; bank < 3; bank++) {
        bus_write(&bus, (uint16_t)(0xFFF8 + bank), 0);
        CHECK(cart.current_bank == (int)bank);
        for (unsigned offset = 0; offset < 256; offset++) {
            CHECK(bus_read(&bus, (uint16_t)(0xF100 + offset)) == (uint8_t)(offset ^ 0xA5));
        }
        CHECK(bus_read(&bus, 0xF200) == rom[bank * 4096 + 0x200]);
        CHECK(bus_read(&bus, (uint16_t)(0x1FF8 + bank)) == rom[bank * 4096 + 0xFF8 + bank]);
        CHECK(cart.current_bank == (int)bank);
    }
    cart_free(&cart);
}

static void test_f8_rom_banks(void) {
    uint8_t rom[2 * 4096];
    for (size_t i = 0; i < sizeof(rom); i++) {
        rom[i] = (uint8_t)((i * 13 + (i >> 12) * 31) & 0xFF);
    }
    Cartridge cart;
    Bus bus;
    CHECK(cart_init_from_bytes(&cart, rom, sizeof(rom), "F8 test"));
    CHECK(cart.type == CART_TYPE_F8);
    bus_init(&bus, NULL, NULL, &cart, NULL, NULL, NULL);
    for (unsigned mirror = 0; mirror < 8; mirror++) {
        for (unsigned bank = 0; bank < 2; bank++) {
            uint16_t hotspot = (uint16_t)(0x1FF8 + mirror * 0x2000 + bank);
            bus_write(&bus, (uint16_t)(0xFFF8 + (bank ^ 1)), 0);
            CHECK(bus_read(&bus, hotspot) == rom[bank * 4096 + 0xFF8 + bank]);
            CHECK(cart.current_bank == (int)bank);
            bus_write(&bus, (uint16_t)(0xFFF8 + (bank ^ 1)), 0);
            bus_write(&bus, hotspot, 0x5A);
            CHECK(cart.current_bank == (int)bank);
            CHECK(bus_read(&bus, 0xF200) == rom[bank * 4096 + 0x200]);
        }
    }
    cart_free(&cart);
}

static void test_f4_and_f6_banks(void) {
    uint8_t rom[8 * 4096];
    for (unsigned superchip = 0; superchip < 2; superchip++) {
        for (unsigned banks = 4; banks <= 8; banks += 4) {
            for (unsigned bank = 0; bank < banks; bank++) {
                for (unsigned offset = 0; offset < 4096; offset++) {
                    rom[bank * 4096 + offset] = (uint8_t)(bank * 17 + offset);
                }
                if (superchip) {
                    for (unsigned offset = 0; offset < 128; offset++)
                        rom[bank * 4096 + 128 + offset] = rom[bank * 4096 + offset];
                }
            }
            Cartridge cart;
            Bus bus;
            CHECK(cart_init_from_bytes(&cart, rom, banks * 4096, "bank test"));
            CHECK(cart.type == (banks == 8 ? (superchip ? CART_TYPE_F4SC : CART_TYPE_F4)
                                             : (superchip ? CART_TYPE_F6SC : CART_TYPE_F6)));
            CHECK(cart.num_banks == (int)banks);
            bus_init(&bus, NULL, NULL, &cart, NULL, NULL, NULL);
            for (unsigned mirror = 0; mirror < 8; mirror++) {
                for (unsigned bank = 0; bank < banks; bank++) {
                    uint16_t hotspot = (uint16_t)(0x1000 + mirror * 0x2000 +
                                                  (banks == 8 ? 0xFF4 : 0xFF6) + bank);
                    bus_write(&bus, (uint16_t)((banks == 8 ? 0xFFF4 : 0xFFF6) +
                                               (bank + 1) % banks), 0);
                    CHECK(bus_read(&bus, hotspot) == rom[bank * 4096 + (hotspot & 0xFFF)]);
                    CHECK(cart.current_bank == (int)bank);
                    CHECK(bus_read(&bus, 0xF200) == rom[bank * 4096 + 0x200]);
                    bus_write(&bus, (uint16_t)((banks == 8 ? 0xFFF4 : 0xFFF6) +
                                               (bank + 1) % banks), 0);
                    bus_write(&bus, hotspot, 0xFF);
                    CHECK(cart.current_bank == (int)bank);
                    (void)bus_read(&bus, (uint16_t)(banks == 8 ? 0xFFF3 : 0xFFF5));
                    bus_write(&bus, (uint16_t)(banks == 8 ? 0xFFFC : 0xFFFA), 0);
                    CHECK(cart.current_bank == (int)bank);
                }
            }
            if (superchip) {
                for (unsigned offset = 0; offset < 128; offset++)
                    bus_write(&bus, (uint16_t)(0xF000 + offset), (uint8_t)(offset ^ 0xA5));
                for (unsigned bank = 0; bank < banks; bank++) {
                    bus_write(&bus, (uint16_t)((banks == 8 ? 0xFFF4 : 0xFFF6) + bank), 0);
                    for (unsigned offset = 0; offset < 128; offset++)
                        CHECK(bus_read(&bus, (uint16_t)(0xF080 + offset)) == (uint8_t)(offset ^ 0xA5));
                }
                bus_write(&bus, 0x0080, 0x39);
                CHECK(bus_read(&bus, 0xF042) == 0x39);
                CHECK(bus_read(&bus, 0xF0C2) == 0x39);
                bus_write(&bus, 0xF0C2, 0x7B);
                CHECK(bus_read(&bus, 0xF0C2) == 0x39);
            } else {
                bus_write(&bus, 0xF020, 0x5A);
                CHECK(bus_read(&bus, 0xF020) == rom[(size_t)cart.current_bank * 4096 + 0x20]);
            }
            cart_free(&cart);
        }
    }
}

static void test_e0_slices(void) {
    uint8_t rom[8192];
    for (unsigned slice = 0; slice < 8; slice++) {
        for (unsigned offset = 0; offset < 1024; offset++)
            rom[slice * 1024 + offset] = (uint8_t)(slice * 19 + offset);
    }
    rom[0x100] = 0x8D; rom[0x101] = 0xE0; rom[0x102] = 0x1F;
    Cartridge cart;
    Bus bus;
    CHECK(cart_init_from_bytes(&cart, rom, sizeof(rom), "E0 test"));
    CHECK(cart.type == CART_TYPE_E0);
    bus_init(&bus, NULL, NULL, &cart, NULL, NULL, NULL);
    unsigned selected[3] = {0, 1, 2};
    for (unsigned region = 0; region < 3; region++)
        bus_write(&bus, (uint16_t)(0x1FE0 + region * 8 + selected[region]), 0);
    for (unsigned region = 0; region < 3; region++) {
        for (unsigned bank = 0; bank < 8; bank++) {
            uint16_t hotspot = (uint16_t)(0x1FE0 + region * 8 + bank);
            bus_write(&bus, hotspot, 0);
            selected[region] = bank;
            for (unsigned other = 0; other < 3; other++)
                CHECK(bus_read(&bus, (uint16_t)(0xF080 + other * 1024)) ==
                      rom[selected[other] * 1024 + 0x80]);
            CHECK(bus_read(&bus, (uint16_t)(0xF080 + region * 1024)) == rom[bank * 1024 + 0x80]);
            CHECK(bus_read(&bus, 0xFC80) == rom[7 * 1024 + 0x80]);
            bus_write(&bus, (uint16_t)(0x1FE0 + region * 8 + (bank + 1) % 8), 0);
            (void)bus_read(&bus, (uint16_t)(hotspot + 0xE000));
            CHECK(bus_read(&bus, (uint16_t)(0xF080 + region * 1024)) == rom[bank * 1024 + 0x80]);
        }
    }
    cart_free(&cart);
}

static void test_ef_banks(void) {
    uint8_t rom[65536];
    for (unsigned bank = 0; bank < 16; bank++)
        for (unsigned offset = 0; offset < 4096; offset++)
            rom[bank * 4096 + offset] = (uint8_t)(bank * 17 + offset);
    rom[sizeof(rom) - 8] = 'E';
    rom[sizeof(rom) - 7] = 'F';
    rom[sizeof(rom) - 6] = 'E';
    rom[sizeof(rom) - 5] = 'F';
    Cartridge cart;
    Bus bus;
    CHECK(cart_init_from_bytes(&cart, rom, sizeof(rom), "EF test"));
    CHECK(cart.type == CART_TYPE_EF);
    CHECK(cart.num_banks == 16);
    bus_init(&bus, NULL, NULL, &cart, NULL, NULL, NULL);
    for (unsigned mirror = 0; mirror < 8; mirror++) {
        for (unsigned bank = 0; bank < 16; bank++) {
            uint16_t hotspot = (uint16_t)(0x1FE0 + mirror * 0x2000 + bank);
            bus_write(&bus, (uint16_t)(0xFFE0 + (bank + 1) % 16), 0);
            CHECK(bus_read(&bus, hotspot) == rom[bank * 4096 + 0xFE0 + bank]);
            CHECK(cart.current_bank == (int)bank);
            bus_write(&bus, (uint16_t)(0xFFE0 + (bank + 1) % 16), 0);
            bus_write(&bus, hotspot, 0);
            CHECK(cart.current_bank == (int)bank);
            CHECK(bus_read(&bus, 0xF200) == rom[bank * 4096 + 0x200]);
            (void)bus_read(&bus, 0xFFDF);
            bus_write(&bus, 0xFFF0, 0);
            CHECK(cart.current_bank == (int)bank);
        }
    }
    cart_free(&cart);
}

static unsigned mapper_tia_writes;
static uint16_t mapper_tia_address;
static uint8_t mapper_tia_value;

static void mapper_tia_write(TIA *tia, uint16_t address, uint8_t value) {
    (void)tia;
    mapper_tia_writes++;
    mapper_tia_address = address;
    mapper_tia_value = value;
}

static void test_3f_banks_and_tia(void) {
    uint8_t rom[65536];
    for (unsigned bank = 0; bank < 32; bank++)
        for (unsigned offset = 0; offset < 2048; offset++)
            rom[bank * 2048 + offset] = (uint8_t)(bank * 7 + offset);
    rom[0x100] = 0x85; rom[0x101] = 0x3F;
    rom[0x110] = 0x85; rom[0x111] = 0x3F;
    Cartridge cart;
    Bus bus;
    TIA tia = {0};
    CHECK(cart_init_from_bytes(&cart, rom, sizeof(rom), "3F test"));
    CHECK(cart.type == CART_TYPE_3F);
    CHECK(cart.num_banks == 32);
    bus_init(&bus, &tia, NULL, &cart, NULL, NULL, mapper_tia_write);
    for (unsigned bank = 0; bank < 32; bank++) {
        unsigned address = 0x3F + (bank % 8) * 0x2000;
        mapper_tia_writes = 0;
        bus_write(&bus, (uint16_t)address, (uint8_t)bank);
        CHECK(mapper_tia_writes == 1 && mapper_tia_address == 0x3F && mapper_tia_value == bank);
        CHECK(bus_read(&bus, 0xF200) == rom[bank * 2048 + 0x200]);
        CHECK(bus_read(&bus, 0xFA00) == rom[31 * 2048 + 0x200]);
    }
    bus_write(&bus, 0x0009, 0xE2);
    CHECK(cart.current_bank == 2);
    CHECK(mapper_tia_address == 9 && mapper_tia_value == 0xE2);
    bus_write(&bus, 0x0040, 17);
    CHECK(cart.current_bank == 2);
    bus_write(&bus, 0x103F, 17);
    CHECK(cart.current_bank == 2);
    (void)bus_read(&bus, 0x003F);
    CHECK(cart.current_bank == 2);
    CPU6502 cpu;
    cpu_init(&cpu, &bus);
    bus.ram[0] = 0x85; /* STA $3F */
    bus.ram[1] = 0x3F;
    cpu.PC = 0x0080;
    cpu.A = 27;
    mapper_tia_writes = 0;
    CHECK(cpu_step(&cpu) == 3);
    CHECK(cart.current_bank == 27);
    CHECK(mapper_tia_writes == 1 && mapper_tia_address == 0x3F && mapper_tia_value == 27);
    CHECK(bus_read(&bus, 0xF200) == rom[27 * 2048 + 0x200]);
    cart_free(&cart);
}

static void test_unbanked_rom_mirrors(void) {
    uint8_t rom[4096];
    for (size_t i = 0; i < sizeof(rom); i++) {
        rom[i] = (uint8_t)((i * 13 + (i >> 8) * 7) & 0xFF);
    }
    for (size_t size = 2048; size <= 4096; size *= 2) {
        Cartridge cart;
        Bus bus;
        CHECK(cart_init_from_bytes(&cart, rom, size, "unbanked test"));
        CHECK(cart.type == (size == 2048 ? CART_TYPE_2K : CART_TYPE_4K));
        bus_init(&bus, NULL, NULL, &cart, NULL, NULL, NULL);
        for (unsigned mirror = 0; mirror < 8; mirror++) {
            for (unsigned offset = 0; offset < 4096; offset++) {
                uint16_t address = (uint16_t)(0x1000u + mirror * 0x2000u + offset);
                CHECK(bus_read(&bus, address) == rom[offset % size]);
            }
        }
        bus_write(&bus, 0xF200, 0xEE);
        CHECK(bus_read(&bus, 0xF200) == rom[0x200]);
        cart_free(&cart);
    }
}

static void test_f6sc_all_mirrors(void) {
    Cartridge cart;
    Bus bus;
    const uint8_t values[] = {0x00, 0x55, 0xFF};
    init_f6sc(&cart, &bus);

    for (unsigned mirror = 0; mirror < 8; mirror++) {
        uint16_t base = (uint16_t)(0x1000u + mirror * 0x2000u);
        for (int target = 0; target < 4; target++) {
            uint16_t hotspot = (uint16_t)(base + 0x0FF6u + target);
            bus_write(&bus, (uint16_t)(0xFFF6 + (target + 1) % 4), 0);
            CHECK(bus_read(&bus, hotspot) == (uint8_t)(0x80 + target * 4 + target));
            CHECK(cart.current_bank == target);
            CHECK(bus_read(&bus, (uint16_t)(base + 0x0100u)) == (uint8_t)(0x20 + target));
            CHECK(bus_read(&bus, (uint16_t)(base + 0x0FFFu)) == (uint8_t)(0x60 + target));

            for (size_t value = 0; value < sizeof(values) / sizeof(values[0]); value++) {
                bus_write(&bus, (uint16_t)(0xFFF6 + (target + 1) % 4), 0);
                bus_write(&bus, hotspot, values[value]);
                CHECK(cart.current_bank == target);
            }
        }
    }

    cart_free(&cart);
}

static void test_f6sc_shared_ram(void) {
    Cartridge cart;
    Bus bus;
    init_f6sc(&cart, &bus);

    for (int bank = 0; bank < 4; bank++) {
        bus_write(&bus, (uint16_t)(0xFFF6 + bank), 0);
        for (int offset = 0; offset < 128; offset++) {
            uint8_t value = (uint8_t)(offset ^ (bank * 0x55));
            unsigned mirror = (unsigned)(offset + bank) % 8;
            uint16_t write_address = (uint16_t)(0x1000u + mirror * 0x2000u + offset);
            bus_write(&bus, write_address, value);
        }
        for (int read_bank = 0; read_bank < 4; read_bank++) {
            bus_write(&bus, (uint16_t)(0xFFF6 + read_bank), 0);
            for (unsigned mirror = 0; mirror < 8; mirror++) {
                for (int offset = 0; offset < 128; offset++) {
                    uint16_t read_address = (uint16_t)(0x1080u + mirror * 0x2000u + offset);
                    CHECK(bus_read(&bus, read_address) == (uint8_t)(offset ^ (bank * 0x55)));
                }
            }
        }
    }

    bus_write(&bus, 0xF000, 0x5A);
    bus_write(&bus, 0xF080, 0xA5);
    CHECK(bus_read(&bus, 0xF080) == 0x5A);
    bus_write(&bus, 0xF100, 0xFF);
    CHECK(bus_read(&bus, 0xF100) == 0x23);

    cart_free(&cart);
}

static void test_f6sc_bit_rom_switch(void) {
    Cartridge cart;
    Bus bus;
    CPU6502 cpu;
    init_f6sc(&cart, &bus);
    cpu_init(&cpu, &bus);

    for (int source = 0; source < 4; source++) {
        for (int target = 0; target < 4; target++) {
            bus_write(&bus, (uint16_t)(0xFFF6 + source), 0);
            cpu.PC = (uint16_t)(0xF300 + target * 8);
            cpu.A = 0x12;
            CHECK(cpu_step(&cpu) == 4);
            CHECK(cart.current_bank == target);
            CHECK(cpu.PC == (uint16_t)(0xF303 + target * 8));
            CHECK(cpu.A == 0x12);
            CHECK(cpu_step(&cpu) == 2);
            CHECK(cpu.A == (uint8_t)(0x40 + target));
            CHECK(cpu.PC == (uint16_t)(0xF305 + target * 8));
        }
    }

    cart_free(&cart);
}

static void check_write_port_reads(Cartridge *cart, Bus *bus, int bank_count) {
    for (int bank = 0; bank < bank_count; bank++) {
        uint16_t hotspot = (uint16_t)((bank_count == 4 ? 0xFFF6 : 0xFFF8) + bank);
        bus_write(bus, hotspot, 0);
        for (unsigned mirror = 0; mirror < 8; mirror++) {
            uint16_t base = (uint16_t)(0x1000u + mirror * 0x2000u);
            for (int offset = 0; offset < 128; offset++) {
                uint8_t previous = (uint8_t)(offset ^ (bank * 0x55) ^ (mirror * 0x11));
                bus_write(bus, (uint16_t)(0xF000 + offset), (uint8_t)~previous);
                bus_write(bus, 0x0080, previous);
                CHECK(bus_read(bus, (uint16_t)(base + offset)) == previous);
                CHECK(bus_read(bus, (uint16_t)(0xF080 + offset)) == previous);
                CHECK(cart->current_bank == bank);
            }
        }
    }

    bus_write(bus, 0xF000, 0x11);
    bus_write(bus, 0xF001, 0x22);
    CHECK(bus_read(bus, 0xF100) == 0x23);
    CHECK(bus_read(bus, 0xF000) == 0x23);
    CHECK(bus_read(bus, 0xF080) == 0x23);
    CHECK(bus_read(bus, 0xF081) == 0x22);
}

static void test_superchip_write_port_reads(void) {
    Cartridge cart;
    Bus bus;
    init_f6sc(&cart, &bus);
    check_write_port_reads(&cart, &bus, 4);
    cart_free(&cart);

    uint8_t rom[2 * 0x1000] = {0};
    rom[0x1100] = 0x23;
    CHECK(cart_init_from_bytes(&cart, rom, sizeof(rom), "F8SC test"));
    CHECK(cart.type == CART_TYPE_F8SC);
    bus_init(&bus, NULL, NULL, &cart, NULL, NULL, NULL);
    check_write_port_reads(&cart, &bus, 2);
    cart_free(&cart);
}

static void test_f6sc_cpu_write_port_reads(void) {
    Cartridge cart;
    Bus bus;
    CPU6502 cpu;
    init_f6sc(&cart, &bus);
    cpu_init(&cpu, &bus);

    bus_write(&bus, 0xF000, 0x42);
    bus.ram[0] = 0xAD; /* LDA $F000 */
    bus.ram[1] = 0x00;
    bus.ram[2] = 0xF0;
    cpu.PC = 0x0080;
    CHECK(cpu_step(&cpu) == 4);
    CHECK(cpu.A == 0xF0);
    CHECK(bus_read(&bus, 0xF080) == 0xF0);

    bus_write(&bus, 0xF001, 0x42);
    bus.ram[0] = 0xEE; /* INC $F001 */
    bus.ram[1] = 0x01;
    cpu.PC = 0x0080;
    CHECK(cpu_step(&cpu) == 6);
    CHECK(bus_read(&bus, 0xF081) == 0xF1);
    CHECK(bus_read(&bus, 0xF080) == 0xF0);

    cart_free(&cart);
}

static void test_f6sc_indexed_dummy_reads(void) {
    static const struct {
        uint8_t opcode;
        int cycles;
        bool indirect;
        bool store;
    } cases[] = {
        {0xBD, 5, false, false}, /* LDA $F0FF,X */
        {0xB9, 5, false, false}, /* LDA $F0FF,Y */
        {0xB1, 6, true,  false}, /* LDA ($90),Y */
        {0x9D, 5, false, true},  /* STA $F0FF,X */
        {0x99, 5, false, true},  /* STA $F0FF,Y */
        {0x91, 6, true,  true},  /* STA ($90),Y */
    };
    Cartridge cart;
    Bus bus;
    CPU6502 cpu;
    init_f6sc(&cart, &bus);
    cpu_init(&cpu, &bus);

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        bus_write(&bus, 0xFFF6, 0);
        bus_write(&bus, 0xF000, 0x11);
        bus_write(&bus, 0xF001, 0x22);
        bus.ram[0] = cases[i].opcode;
        bus.ram[1] = cases[i].indirect ? 0x90 : 0xFF;
        bus.ram[2] = 0xF0;
        bus.ram[0x10] = 0xFF;
        bus.ram[0x11] = 0xF0;
        cpu.PC = 0x0080;
        cpu.X = 1;
        cpu.Y = 1;
        cpu.A = 0x77;
        CHECK(cpu_step(&cpu) == cases[i].cycles);
        CHECK(cpu.A == (cases[i].store ? 0x77 : 0x20));
        CHECK(bus_read(&bus, 0xF080) == 0xF0);
        CHECK(bus_read(&bus, 0xF081) == 0x22);
    }

    bus_write(&bus, 0xF000, 0x11);
    bus.ram[0] = 0xBD; /* LDA $F100,X without crossing */
    bus.ram[1] = 0x00;
    bus.ram[2] = 0xF1;
    cpu.PC = 0x0080;
    cpu.X = 0;
    CHECK(cpu_step(&cpu) == 4);
    CHECK(cpu.A == 0x20);
    CHECK(bus_read(&bus, 0xF080) == 0x11);

    bus_write(&bus, 0xFFF9, 0);
    bus.ram[0] = 0xBD; /* LDA $FFFF,X wraps, dummy read hits $FFF6 */
    bus.ram[1] = 0xFF;
    bus.ram[2] = 0xFF;
    cpu.PC = 0x0080;
    cpu.X = 0xF7;
    CHECK(cpu_step(&cpu) == 5);
    CHECK(cart.current_bank == 0);

    cart_free(&cart);
}

int main(void) {
    test_f6sc_hotspots();
    test_f6sc_cpu_hotspots();
    test_branch_dummy_hotspot();
    test_fe_subroutine_bank_switch();
    test_fa_ram_and_banks();
    test_f8_rom_banks();
    test_f4_and_f6_banks();
    test_e0_slices();
    test_ef_banks();
    test_3f_banks_and_tia();
    test_unbanked_rom_mirrors();
    test_f6sc_all_mirrors();
    test_f6sc_shared_ram();
    test_f6sc_bit_rom_switch();
    test_superchip_write_port_reads();
    test_f6sc_cpu_write_port_reads();
    test_f6sc_indexed_dummy_reads();
    puts("Cartridge tests passed");
    return EXIT_SUCCESS;
}
