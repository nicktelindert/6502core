#include "../antic5200.h"
#include "../system5200.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); \
} } while (0)

static void test_registers_and_timing(void) {
    Antic5200 antic;
    antic5200_init(&antic);
    CHECK(antic5200_read(&antic, 0xd40b) == 0);
    CHECK(antic5200_read(&antic, 0xd41f) == 0x1f);
    antic5200_write(&antic, 0xd410, 0x20);
    CHECK(antic.dmactl == 0x20);
    antic5200_write(&antic, 0xd412, 0xff);
    antic5200_write(&antic, 0xd413, 0x23);
    CHECK(antic.dlist == 0x23ff);
    for (int i = 0; i < 114 * 2; i++) antic5200_tick(&antic);
    CHECK(antic.scanline == 2 && antic.cycle == 0);
    CHECK(antic5200_read(&antic, 0xd42b) == 1);
    antic5200_write(&antic, 0xd40f, 0);
    CHECK(antic5200_read(&antic, 0xd40f) == 0x1f);
    for (int i = 0; i < 114 * 260; i++) antic5200_tick(&antic);
    CHECK(antic.scanline == 0 && antic.cycle == 0);
}

static void test_display_dma_and_cpu_wait(void) {
    System5200 system;
    Antic5200 antic;
    CPU6502 cpu;
    system5200_init(&system);
    antic5200_init(&antic);
    system5200_attach_antic(&system, &antic, &cpu);
    system.ram[0x03ff] = 0x70; /* eight blank lines */
    system.ram[0x0000] = 0x41; /* JVB $0400, wraps within 1 KB */
    system.ram[0x0001] = 0x00;
    system.ram[0x0002] = 0x04;
    system.ram[0x0200] = 0xea; /* NOP */
    system.bios_loaded = true;
    system.bios[0x7fc] = 0;
    system.bios[0x7fd] = 2;
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    system5200_write(&system, 0xd402, 0xff);
    system5200_write(&system, 0xd403, 0x03);
    system5200_write(&system, 0xd400, 0x20);
    while (antic.scanline < 8 || antic.cycle < 1) system5200_tick(&system, 1);
    CHECK(antic.dma_pending);
    uint64_t write_before = system.elapsed_cycles;
    system5200_write(&system, 0x0300, 0x55);
    CHECK(system.elapsed_cycles == write_before + 1 && antic.dma_pending);
    CHECK(system.ram[0x0300] == 0x55);
    uint64_t before = system.elapsed_cycles;
    CHECK(cpu_step(&cpu) == 2);
    CHECK(system.elapsed_cycles - before == 3);
    CHECK(antic.dlist == 0x0000 && antic.instruction == 0x70);
    CHECK(cpu.PC == 0x0201);
    while (antic.scanline < 16 || antic.cycle < 1) system5200_tick(&system, 1);
    CHECK(antic.dma_pending);
    before = system.elapsed_cycles;
    system.ram[0x0201] = 0xea;
    CHECK(cpu_step(&cpu) == 2);
    CHECK(system.elapsed_cycles - before == 3);
    while (antic.cycle < 6) system5200_tick(&system, 1);
    before = system.elapsed_cycles;
    system.ram[0x0202] = 0xea;
    CHECK(cpu_step(&cpu) == 2);
    CHECK(system.elapsed_cycles - before == 4);
    CHECK(antic.dlist == 0x0400 && antic.wait_vblank);
}

static void test_vblank_nmi(void) {
    System5200 system;
    Antic5200 antic;
    CPU6502 cpu;
    system5200_init(&system);
    antic5200_init(&antic);
    system5200_attach_antic(&system, &antic, &cpu);
    system.bios_loaded = true;
    system.bios[0x7fc] = 0;
    system.bios[0x7fd] = 2;
    system.bios[0x7fa] = 0x00;
    system.bios[0x7fb] = 0xf8;
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    system5200_write(&system, 0xd40e, 0x40);
    while (antic.scanline < 248) system5200_tick(&system, 1);
    while (antic.cycle < 6) system5200_tick(&system, 1);
    CHECK(antic.nmist == 0x1f && !cpu.nmi_pending);
    system5200_tick(&system, 1);
    CHECK(antic.nmist == 0x5f && !cpu.nmi_pending);
    system5200_tick(&system, 1);
    CHECK(cpu.nmi_pending && !cpu.nmi_asserted);
    CHECK(cpu_step(&cpu) == 7 && cpu.PC == 0xf800);
    system5200_write(&system, 0xd40f, 0);
    CHECK(antic.nmist == 0x1f);
    system5200_write(&system, 0xd40e, 0);
    while (antic.scanline != 0) system5200_tick(&system, 1);
    while (antic.scanline < 248 || antic.cycle < 8) system5200_tick(&system, 1);
    CHECK(antic.nmist == 0x5f && !cpu.nmi_pending);
}

static void test_wsync(void) {
    System5200 system;
    Antic5200 antic;
    CPU6502 cpu;
    system5200_init(&system);
    antic5200_init(&antic);
    system5200_attach_antic(&system, &antic, &cpu);
    system.bios_loaded = true;
    system.bios[0x7fc] = 0;
    system.bios[0x7fd] = 2;
    system.ram[0x200] = 0xea;
    system.ram[0x201] = 0xea;
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    system5200_write(&system, 0xd40a, 0);
    uint64_t before = system.elapsed_cycles;
    CHECK(cpu_step(&cpu) == 2);
    CHECK(system.elapsed_cycles - before > 2);
    CHECK(antic.cycle == 108);
    system5200_tick(&system, 1);
    system5200_write(&system, 0xd40a, 0);
    before = system.elapsed_cycles;
    CHECK(cpu_step(&cpu) == 2);
    CHECK(system.elapsed_cycles - before > 100 && antic.cycle == 108);
}

static void test_wsync_allows_cpu_writes(void) {
    System5200 system;
    Antic5200 antic;
    CPU6502 cpu;
    system5200_init(&system);
    antic5200_init(&antic);
    system5200_attach_antic(&system, &antic, &cpu);
    CpuBus bus = system5200_cpu_bus(&system);
    bus.write(bus.context, 0xd40a, 0);
    uint64_t before = system.elapsed_cycles;
    bus.write(bus.context, 0x300, 0x5a);
    CHECK(system.elapsed_cycles == before + 1 && antic.wsync);
    CHECK(system.ram[0x300] == 0x5a);
    (void)bus.read(bus.context, 0x300);
    CHECK(!antic.wsync && antic.cycle == 107);
}

static void test_dma_disabled_and_scrolling_instruction(void) {
    Antic5200 antic;
    antic5200_init(&antic);
    antic5200_write(&antic, 0xd400, 0x20);
    for (int i = 0; i < 8 * 114 + 1; i++) antic5200_tick(&antic);
    CHECK(antic.dma_pending);
    antic5200_write(&antic, 0xd400, 0);
    CHECK(!antic.dma_pending);
    antic5200_write(&antic, 0xd400, 0x20);
    for (int i = 0; i < 114; i++) antic5200_tick(&antic);
    CHECK(antic.dma_pending);
    antic5200_write(&antic, 0xd400, 0x22);
    antic5200_dma_byte(&antic, 0x12);
    CHECK(!antic.unsupported && !antic.dma_pending && antic.instruction == 0x12);
}

static void test_bitmap_dma_timing(void) {
    static const struct { uint8_t mode, height, interval; } modes[] = {
        {8, 8, 8}, {9, 4, 8}, {10, 4, 4}, {11, 2, 4},
        {12, 1, 4}, {13, 2, 2}, {14, 1, 2}, {15, 1, 2}
    };
    for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    for (unsigned width = 1; width <= 2; width++) {
        Antic5200 antic;
        antic5200_init(&antic);
        antic5200_write(&antic, 0xd400, (uint8_t)(0x20 | width));
        while (antic.scanline != 8 || antic.cycle != 1) antic5200_tick(&antic);
        antic5200_dma_byte(&antic, modes[i].mode);
        CHECK(!antic.unsupported && antic.lines_left == modes[i].height);
        unsigned fetches = 0;
        while (antic.scanline < (unsigned)(8 + modes[i].height)) {
            antic5200_tick(&antic);
            if (antic.playfield_dma_pending) {
                CHECK(antic.scanline == 8);
                CHECK(antic.cycle == (width == 1 ? 28u : 20u) + fetches * modes[i].interval);
                CHECK(antic.playfield_column == fetches);
                antic5200_playfield_byte(&antic, (uint8_t)fetches);
                fetches++;
            }
        }
        CHECK(fetches == (width == 1 ? 64u : 80u) / modes[i].interval);
        CHECK(antic.screen_address == fetches);
    }
}

static void test_horizontal_dma_timing(void) {
    for (unsigned mode = 2; mode <= 15; mode++)
    for (unsigned width = 1; width <= 3; width++)
    for (unsigned scroll = 0; scroll <= 1; scroll++)
    for (unsigned h = 0; h < 16; h++) {
        Antic5200 antic;
        antic5200_init(&antic);
        antic.dmactl = (uint8_t)(0x20 | width);
        antic5200_write(&antic, 0xd404, (uint8_t)(0xf0 | h));
        CHECK(antic.hscrol == h);
        while (antic.scanline != 8 || antic.cycle != 1) antic5200_tick(&antic);
        antic5200_dma_byte(&antic, (uint8_t)(mode | (scroll ? 0x10 : 0)));
        unsigned fetch = width + (scroll && width < 3);
        unsigned interval = mode < 6 || mode >= 13 ? 2 : mode <= 7 || mode >= 10 ? 4 : 8;
        unsigned span = fetch == 1 ? 64 : fetch == 2 ? 80 : 96;
        unsigned start = (mode < 8 ? (fetch == 1 ? 26 : fetch == 2 ? 18 : 10) :
                                    (fetch == 1 ? 28 : fetch == 2 ? 20 : 12)) +
                         (scroll ? h / 2 : 0);
        unsigned names = 0, glyphs = 0;
        while (antic.scanline == 8) {
            antic5200_tick(&antic);
            if (!antic.playfield_dma_pending) continue;
            unsigned cycle = antic.scanline == 9 ? 114 : antic.cycle;
            if (antic.character_dma) {
                CHECK(mode < 8 && cycle == start + 3 + glyphs * interval);
                CHECK(antic.playfield_column == glyphs++);
            } else {
                CHECK(cycle == start + names * interval);
                CHECK(antic.playfield_column == names);
                CHECK(antic5200_playfield_address(&antic) == names++);
            }
            CHECK(antic.playfield_dma_virtual == (cycle >= 106));
            antic5200_playfield_byte(&antic, 0);
        }
        CHECK(!antic.unsupported && names == span / interval);
        CHECK(glyphs == (mode < 8 ? names : 0));
        CHECK(antic.screen_address == names);
    }
}

static void test_virtual_dma_cpu_bus(void) {
    for (unsigned write = 0; write < 2; write++) {
        System5200 system;
        Antic5200 antic;
        CPU6502 cpu;
        system5200_init(&system);
        antic5200_init(&antic);
        system5200_attach_antic(&system, &antic, &cpu);
        antic.scanline = 32;
        antic.cycle = 105;
        antic.instruction = 0x1f;
        antic.lines_left = 1;
        antic.first_mode_line = true;
        antic.dmactl = 3;
        antic.hscrol = 4;
        antic.screen_address = 0x1000;
        system.ram[0x300] = 0xa5;
        system.data_bus = 0x11;
        CpuBus bus = system5200_cpu_bus(&system);
        uint64_t before = system.elapsed_cycles;
        if (write) bus.write(bus.context, 0x300, 0x5a);
        else CHECK(bus.read(bus.context, 0x300) == 0xa5);
        CHECK(system.elapsed_cycles == before + 1);
        CHECK(!antic.playfield_dma_pending && !antic.unsupported);
        CHECK(antic.screen_address == 0x1001);
        CHECK(antic.line_buffer[46] == (write ? 0x5a : 0xa5));
    }
}

static void test_character_dma_timing(void) {
    for (unsigned mode = 2; mode <= 7; mode++)
    for (unsigned width = 1; width <= 2; width++) {
        Antic5200 antic;
        antic5200_init(&antic);
        antic5200_write(&antic, 0xd400, (uint8_t)(0x20 | width));
        antic5200_write(&antic, 0xd409, 0x13);
        while (antic.scanline != 8 || antic.cycle != 1) antic5200_tick(&antic);
        antic5200_dma_byte(&antic, (uint8_t)mode);
        CHECK(!antic.unsupported);
        unsigned interval = mode < 6 ? 2 : 4;
        unsigned count = (width == 1 ? 64 : 80) / interval;
        unsigned start = width == 1 ? 26 : 18;
        unsigned names = 0, glyphs = 0;
        while (antic.scanline < (unsigned)(8 + (mode == 3 ? 10 : mode == 5 || mode == 7 ? 16 : 8))) {
            antic5200_tick(&antic);
            if (antic.playfield_dma_pending) {
                if (!antic.character_dma) {
                    CHECK(antic.scanline == 8);
                    CHECK(antic.cycle == start + names * interval);
                    CHECK(antic5200_playfield_address(&antic) == names);
                    antic5200_playfield_byte(&antic, 1);
                    names++;
                } else {
                    unsigned row = antic.scanline - 8;
                    if (mode == 5 || mode == 7) row /= 2;
                    unsigned base = mode < 6 ? 0x1000 : 0x1200;
                    CHECK(antic.cycle == start + 3 + (glyphs % count) * interval);
                    CHECK(antic5200_playfield_address(&antic) == base + 8 + (row & 7));
                    antic5200_playfield_byte(&antic, 0x80);
                    glyphs++;
                }
            }
        }
        if (names != count) fprintf(stderr, "mode %u width %u: %u names, expected %u\n", mode, width, names, count);
        CHECK(names == count);
        CHECK(glyphs == count * (mode == 3 ? 10u : mode == 5 || mode == 7 ? 16u : 8u));
        CHECK(antic.screen_address == count);
    }
}

static void test_character_dma_overrun(void) {
    Antic5200 antic;
    antic5200_init(&antic);
    antic5200_write(&antic, 0xd400, 0x22);
    while (antic.scanline != 8 || antic.cycle != 1) antic5200_tick(&antic);
    antic5200_dma_byte(&antic, 2);
    while (antic.cycle < 18) antic5200_tick(&antic);
    CHECK(antic.playfield_dma_pending && !antic.character_dma);
    antic5200_tick(&antic);
    antic5200_tick(&antic);
    CHECK(antic.unsupported && !antic.playfield_dma_pending);
}

typedef struct {
    Antic5200 *antic;
    uint16_t address[2];
    uint8_t cycle[2];
    unsigned count;
} GlyphTrace;

static uint8_t trace_glyph_read(void *context, uint16_t address) {
    GlyphTrace *trace = context;
    CHECK(trace->count < 2);
    trace->address[trace->count] = address;
    trace->cycle[trace->count++] = trace->antic->cycle;
    return 0x80;
}

static void test_character_dma_arrives_on_cpu_read(void) {
    System5200 system;
    Antic5200 antic;
    CPU6502 cpu;
    system5200_init(&system);
    antic5200_init(&antic);
    system5200_attach_antic(&system, &antic, &cpu);
    system.bios_loaded = true;
    system.bios[0x7fd] = 2;
    system.ram[0x200] = 0x8d;
    system.ram[0x201] = 0x07;
    system.ram[0x202] = 0x02;
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    cpu.A = 0x55;
    antic.scanline = 28;
    antic.cycle = 24;
    antic.instruction = 7;
    antic.lines_left = 16;
    antic.first_mode_line = true;
    antic.mode_last_row = 15;
    antic.dmactl = 0x22;
    antic.chbase = 0xc0;
    antic.line_buffer[1] = 1;
    antic.screen_address = 0x1002;
    system.ram[0x1002] = 2;
    system.ram[0x1003] = 3;
    GlyphTrace trace = {.antic = &antic};
    system.gtia = (System5200Device){.context = &trace, .read = trace_glyph_read};
    uint64_t before = system.elapsed_cycles;
    CHECK(cpu_step(&cpu) == 4);
    CHECK(system.ram[0x207] == 0x55);
    CHECK(!antic.unsupported && system.elapsed_cycles - before == 8);
    CHECK(antic.cycle == 32 && antic.screen_address == 0x1004);
    CHECK(antic.line_buffer[2] == 2 && antic.line_buffer[3] == 3);
    CHECK(trace.count == 2);
    CHECK(trace.cycle[0] == 25 && trace.address[0] == 0xc008);
    CHECK(trace.cycle[1] == 29 && trace.address[1] == 0xc010);
}

typedef struct {
    Antic5200 *antic;
    uint8_t read_cycle;
    uint8_t write_cycle[2];
    uint8_t value[2];
    unsigned writes;
} RmwTrace;

static uint8_t trace_rmw_read(void *context, uint16_t address) {
    RmwTrace *trace = context;
    CHECK(address == 0xc100);
    trace->read_cycle = trace->antic->cycle;
    return 0x7f;
}

static void trace_rmw_write(void *context, uint16_t address, uint8_t value) {
    RmwTrace *trace = context;
    CHECK(address == 0xc100 && trace->writes < 2);
    trace->write_cycle[trace->writes] = trace->antic->cycle;
    trace->value[trace->writes++] = value;
}

static void test_character_dma_during_rmw_writes(void) {
    System5200 system;
    Antic5200 antic;
    CPU6502 cpu;
    system5200_init(&system);
    antic5200_init(&antic);
    system5200_attach_antic(&system, &antic, &cpu);
    system.bios_loaded = true;
    system.bios[0x7fd] = 2;
    system.ram[0x200] = 0xee;
    system.ram[0x201] = 0x00;
    system.ram[0x202] = 0xc1;
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    antic.scanline = 28;
    antic.cycle = 14;
    antic.instruction = 2;
    antic.lines_left = 8;
    antic.first_mode_line = true;
    antic.mode_last_row = 7;
    antic.dmactl = 0x22;
    antic.screen_address = 0x1000;
    RmwTrace trace = {.antic = &antic};
    system.gtia = (System5200Device){&trace, trace_rmw_read, trace_rmw_write};
    uint64_t before = system.elapsed_cycles;
    CHECK(cpu_step(&cpu) == 6);
    CHECK(!antic.unsupported && !antic.playfield_dma_pending);
    CHECK(system.elapsed_cycles - before == 86 && antic.cycle == 100);
    CHECK(antic.screen_address == 0x1028);
    CHECK(trace.read_cycle == 19 && trace.writes == 2);
    CHECK(trace.write_cycle[0] == 98 && trace.value[0] == 0x7f);
    CHECK(trace.write_cycle[1] == 100 && trace.value[1] == 0x80);
}

static void test_character_dma_during_nmi_stack_writes(void) {
    System5200 system;
    Antic5200 antic;
    CPU6502 cpu;
    system5200_init(&system);
    antic5200_init(&antic);
    system5200_attach_antic(&system, &antic, &cpu);
    system.bios_loaded = true;
    system.bios[0x7fd] = 2;
    system.bios[0x7fa] = 0x34;
    system.bios[0x7fb] = 0x12;
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    antic.scanline = 28;
    antic.cycle = 15;
    antic.instruction = 2;
    antic.lines_left = 8;
    antic.first_mode_line = true;
    antic.mode_last_row = 7;
    antic.dmactl = 0x22;
    antic.screen_address = 0x1000;
    uint8_t stack = cpu.SP;
    uint8_t status = (uint8_t)((cpu.status | CPU_FLAG_U) & ~CPU_FLAG_B);
    cpu_set_nmi(&cpu, true);
    uint64_t before = system.elapsed_cycles;
    CHECK(cpu_step(&cpu) == 7);
    CHECK(!antic.unsupported && !antic.playfield_dma_pending);
    CHECK(system.elapsed_cycles - before == 87 && antic.cycle == 102);
    CHECK(antic.screen_address == 0x1028 && cpu.PC == 0x1234);
    CHECK(cpu.SP == (uint8_t)(stack - 3));
    CHECK(system.ram[0x100 | stack] == 0x02);
    CHECK(system.ram[0x100 | (uint8_t)(stack - 1)] == 0x00);
    CHECK(system.ram[0x100 | (uint8_t)(stack - 2)] == status);
}

static void test_pm_dma_requests(void) {
    for (unsigned single = 0; single < 2; single++)
    for (unsigned enabled = 4; enabled <= 12; enabled += 4) {
        Antic5200 antic;
        antic5200_init(&antic);
        antic5200_write(&antic, 0xd407, 0x1f);
        antic5200_write(&antic, 0xd400, (uint8_t)(enabled | (single ? 0x10 : 0)));
        while (antic.scanline != 8 || antic.cycle != 0) {
            antic5200_tick(&antic);
            if (antic.scanline < 8) CHECK(antic.pm_pending == 0);
        }
        unsigned base = single ? 0x1800 : 0x1c00;
        unsigned stride = single ? 0x100 : 0x80;
        unsigned line = single ? 8 : 4;
        CHECK(antic.pm_pending == 1);
        CHECK(antic.pm_address[0] == base + 3 * stride + line);
        antic5200_pm_byte(&antic, 0, 0);
        for (unsigned cycle = 1; cycle <= 6; cycle++) {
            antic5200_tick(&antic);
            if ((enabled & 8) && cycle >= 2 && cycle <= 5) {
                unsigned slot = cycle - 1;
                CHECK(antic.pm_pending == (1u << slot));
                CHECK(antic.pm_address[slot] == base + (slot + 3) * stride + line);
                antic5200_pm_byte(&antic, slot, 0);
            } else CHECK(antic.pm_pending == 0);
        }
        while (antic.scanline != 9 || antic.cycle != 0) antic5200_tick(&antic);
        CHECK(antic.pm_address[0] == base + 3 * stride + (single ? 9 : 4));
    }
}

static void test_pm_dma_stalls_cpu(void) {
    System5200 system;
    Antic5200 antic;
    CPU6502 cpu;
    system5200_init(&system);
    antic5200_init(&antic);
    system5200_attach_antic(&system, &antic, &cpu);
    system.bios_loaded = true;
    system.bios[0x7fc] = 0;
    system.bios[0x7fd] = 2;
    system.ram[0x200] = 0xea;
    system.ram[0x300] = 0x70;
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    system5200_write(&system, 0xd402, 0);
    system5200_write(&system, 0xd403, 3);
    system5200_write(&system, 0xd400, 0x28);
    while (antic.scanline != 8 || antic.cycle != 0) system5200_tick(&system, 1);
    CHECK(antic.pm_pending == 1);
    uint64_t before = system.elapsed_cycles;
    CHECK(cpu_step(&cpu) == 2);
    CHECK(system.elapsed_cycles - before == 8);
    CHECK(antic.pm_pending == 0 && !antic.dma_pending && antic.dlist == 0x301);
}

static void test_cpu_write_at_dma_request(void) {
    System5200 system;
    Antic5200 antic;
    CPU6502 cpu;
    system5200_init(&system);
    antic5200_init(&antic);
    system5200_attach_antic(&system, &antic, &cpu);
    system.bios_loaded = true;
    system.bios[0x7fc] = 0;
    system.bios[0x7fd] = 2;
    system.ram[0x200] = 0x85;
    system.ram[0x201] = 0x80;
    system.ram[0x202] = 0xea;
    system.ram[0x300] = 0x00;
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    cpu.A = 0x5a;
    system5200_write(&system, 0xd402, 0x00);
    system5200_write(&system, 0xd403, 0x03);
    system5200_write(&system, 0xd400, 0x20);
    while (antic.scanline < 7 || antic.cycle < 113) system5200_tick(&system, 1);
    uint64_t before = system.elapsed_cycles;
    CHECK(cpu_step(&cpu) == 3 && system.elapsed_cycles - before == 4);
    CHECK(system.ram[0x80] == 0x5a && !antic.dma_pending);
    before = system.elapsed_cycles;
    CHECK(cpu_step(&cpu) == 2 && system.elapsed_cycles - before == 2);
    CHECK(!antic.dma_pending && antic.dlist == 0x0301);
}

static void advance_to(Antic5200 *antic, unsigned line, unsigned cycle) {
    while (antic->scanline != line || antic->cycle != cycle) antic5200_tick(antic);
}

static void test_vertical_scroll_counter(void) {
    for (unsigned offset = 0; offset < 16; offset++) {
        Antic5200 antic;
        antic5200_init(&antic);
        antic.dmactl = 0x22;
        antic5200_write(&antic, 0xd405, (uint8_t)(0xf0 | offset));
        CHECK(antic.vscrol == offset);
        advance_to(&antic, 8, 1);
        antic5200_dma_byte(&antic, 0x22);
        unsigned entry_lines = ((7 - offset) & 15) + 1;
        CHECK(antic.mode_row == offset && antic.lines_left == entry_lines);
        antic.dmactl = 0;
        for (unsigned row = 0; row < entry_lines; row++) {
            CHECK(antic.mode_row == ((offset + row) & 15));
            advance_to(&antic, 9 + row, 0);
        }
        antic5200_dma_byte(&antic, 0x22);
        CHECK(antic.mode_row == 0 && antic.lines_left == 8);
        advance_to(&antic, 16 + entry_lines, 0);
        antic5200_dma_byte(&antic, 0x02);
        CHECK(antic.scroll_ending && antic.lines_left == offset + 1);
        for (unsigned row = 0; row <= offset; row++) {
            CHECK(antic.mode_row == row);
            advance_to(&antic, 17 + entry_lines + row, 0);
        }
        CHECK(!antic.lines_left && !antic.unsupported);
    }
}

static void test_scroll_end_dli_timing(void) {
    Antic5200 antic;
    CPU6502 cpu = {0};
    antic5200_init(&antic);
    antic.cpu = &cpu;
    antic5200_write(&antic, 0xd40e, 0x80);
    antic5200_write(&antic, 0xd405, 2);
    advance_to(&antic, 8, 1);
    antic5200_dma_byte(&antic, 0x22);
    advance_to(&antic, 14, 1);
    antic5200_dma_byte(&antic, 0x80);
    CHECK(antic.scroll_ending && antic.lines_left == 3);
    advance_to(&antic, 15, 5);
    antic5200_write(&antic, 0xd405, 1);
    advance_to(&antic, 15, 6);
    antic5200_write(&antic, 0xd405, 2);
    advance_to(&antic, 15, 8);
    CHECK(cpu.nmi_pending && antic.nmist == 0x9f);
    advance_to(&antic, 16, 0);
    CHECK(antic.lines_left && antic.mode_row == 2);
    advance_to(&antic, 17, 0);
    CHECK(!antic.lines_left);
}

static void test_scroll_latch_deadlines(void) {
    Antic5200 antic;
    antic5200_init(&antic);
    antic5200_write(&antic, 0xd405, 2);
    advance_to(&antic, 8, 1);
    antic5200_write(&antic, 0xd405, 5);
    antic5200_dma_byte(&antic, 0x22);
    CHECK(antic.mode_row == 2 && antic.lines_left == 6);
    advance_to(&antic, 14, 1);
    antic5200_dma_byte(&antic, 0x00);
    CHECK(antic.scroll_ending && antic.mode_row == 0);
    advance_to(&antic, 14, 108);
    antic5200_write(&antic, 0xd405, 0);
    antic5200_tick(&antic);
    antic5200_write(&antic, 0xd405, 5);
    advance_to(&antic, 15, 0);
    CHECK(!antic.lines_left);
}

static void test_scroll_end_jvb(void) {
    Antic5200 antic;
    CPU6502 cpu = {0};
    antic5200_init(&antic);
    antic.cpu = &cpu;
    antic5200_write(&antic, 0xd40e, 0x80);
    antic5200_write(&antic, 0xd405, 2);
    advance_to(&antic, 8, 1);
    antic5200_dma_byte(&antic, 0x22);
    advance_to(&antic, 14, 1);
    antic5200_dma_byte(&antic, 0xe1);
    antic5200_dma_byte(&antic, 0x34);
    antic5200_dma_byte(&antic, 0x12);
    CHECK(antic.wait_vblank && antic.scroll_ending && antic.lines_left == 3);
    advance_to(&antic, 16, 8);
    CHECK(cpu.nmi_pending && antic.nmist == 0x9f);
    cpu.nmi_pending = false;
    advance_to(&antic, 17, 8);
    CHECK(cpu.nmi_pending && !antic.scroll_ending && antic.dlist == 0x1234);
}

static void test_blank_dli_and_jvb(void) {
    Antic5200 antic;
    CPU6502 cpu = {0};
    antic5200_init(&antic);
    antic.cpu = &cpu;
    antic5200_write(&antic, 0xd40e, 0x80);
    antic5200_write(&antic, 0xd400, 0x20);
    advance_to(&antic, 8, 1);
    antic5200_dma_byte(&antic, 0x90); /* two blank lines with DLI */
    CHECK(!antic.unsupported && antic.lines_left == 2 && !cpu.nmi_pending);
    advance_to(&antic, 9, 7);
    CHECK(antic.nmist == 0x9f && !cpu.nmi_pending);
    antic5200_write(&antic, 0xd40f, 0);
    CHECK(antic.nmist == 0x1f);
    advance_to(&antic, 9, 8);
    CHECK(cpu.nmi_pending);
    cpu.nmi_pending = false;
    advance_to(&antic, 10, 1);
    antic5200_dma_byte(&antic, 0xc1); /* JVB with repeated DLI */
    antic5200_dma_byte(&antic, 0x00);
    antic5200_dma_byte(&antic, 0x03);
    CHECK(antic.wait_vblank);
    advance_to(&antic, 11, 7);
    CHECK(antic.nmist == 0x9f);
    advance_to(&antic, 11, 8);
    CHECK(cpu.nmi_pending);
    cpu.nmi_pending = false;
    advance_to(&antic, 12, 8);
    CHECK(cpu.nmi_pending);
}

static void test_dli_mask_and_status(void) {
    Antic5200 antic;
    CPU6502 cpu = {0};
    antic5200_init(&antic);
    antic.cpu = &cpu;
    antic5200_write(&antic, 0xd400, 0x20);
    advance_to(&antic, 8, 1);
    antic5200_dma_byte(&antic, 0x80); /* one line, disabled DLI */
    advance_to(&antic, 8, 8);
    CHECK(antic.nmist == 0x9f && !cpu.nmi_pending);
}

static void test_lms_and_mode_heights(void) {
    static const uint8_t heights[] = {8, 10, 8, 16, 8, 16, 8, 4, 4, 2, 1, 2, 1, 1};
    for (unsigned mode = 2; mode <= 15; mode++) {
        Antic5200 antic;
        antic5200_init(&antic);
        antic.dlist = 0x03ff;
        antic5200_dma_byte(&antic, (uint8_t)(0x40 | mode));
        CHECK(!antic.unsupported && antic.dma_stage == 1 && antic.dlist == 0x0000);
        CHECK(antic.lines_left == heights[mode - 2]);
        antic5200_dma_byte(&antic, 0x34);
        antic5200_dma_byte(&antic, 0x12);
        CHECK(antic.screen_address == 0x1234 && antic.dlist == 2);
        CHECK(antic.dma_stage == 0 && !antic.dma_pending);
        for (int i = 0; i < heights[mode - 2] * 114; i++) antic5200_tick(&antic);
        CHECK(antic.lines_left == 0);
    }
}

static void test_late_address_fetches(void) {
    Antic5200 antic;
    antic5200_init(&antic);
    antic.cycle = 9;
    antic.dma_pending = true;
    antic5200_dma_byte(&antic, 0x41);
    CHECK(antic.dma_pending && antic.dma_stage == 1);
    antic5200_tick(&antic);
    antic5200_dma_byte(&antic, 0x34);
    CHECK(antic.dma_pending && antic.dma_stage == 2);
    antic5200_tick(&antic);
    antic5200_dma_byte(&antic, 0x12);
    CHECK(!antic.dma_pending && antic.dma_stage == 0 && antic.dlist == 0x1234);
}

static void test_display_dma_enable_latch(void) {
    Antic5200 antic;
    antic5200_init(&antic);
    advance_to(&antic, 7, 113);
    antic5200_write(&antic, 0xd400, 0x20);
    antic5200_tick(&antic);
    antic5200_tick(&antic);
    CHECK(!antic.dma_pending);
    advance_to(&antic, 9, 1);
    CHECK(antic.dma_pending);
}

int main(void) {
    test_registers_and_timing();
    test_display_dma_and_cpu_wait();
    test_wsync();
    test_wsync_allows_cpu_writes();
    test_vblank_nmi();
    test_dma_disabled_and_scrolling_instruction();
    test_bitmap_dma_timing();
    test_horizontal_dma_timing();
    test_virtual_dma_cpu_bus();
    test_character_dma_timing();
    test_character_dma_overrun();
    test_character_dma_arrives_on_cpu_read();
    test_character_dma_during_rmw_writes();
    test_character_dma_during_nmi_stack_writes();
    test_pm_dma_requests();
    test_pm_dma_stalls_cpu();
    test_cpu_write_at_dma_request();
    test_blank_dli_and_jvb();
    test_vertical_scroll_counter();
    test_scroll_end_dli_timing();
    test_scroll_latch_deadlines();
    test_scroll_end_jvb();
    test_dli_mask_and_status();
    test_lms_and_mode_heights();
    test_late_address_fetches();
    test_display_dma_enable_latch();
    puts("ANTIC 5200 timing and DMA tests passed");
    return 0;
}
