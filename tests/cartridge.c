#include "../bus.h"
#include "../cpu6507.h"
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
    CPU6507 cpu;
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
    CPU6507 cpu;
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
    CPU6507 cpu;
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
    CPU6507 cpu;
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
    CPU6507 cpu;
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
    test_f6sc_all_mirrors();
    test_f6sc_shared_ram();
    test_f6sc_bit_rom_switch();
    test_superchip_write_port_reads();
    test_f6sc_cpu_write_port_reads();
    test_f6sc_indexed_dummy_reads();
    puts("F6SC cartridge tests passed");
    return EXIT_SUCCESS;
}
