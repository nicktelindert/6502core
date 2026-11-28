#include "../pokey5200.h"
#include "../system5200.h"
#include "../antic5200.h"
#include "../gtia5200.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); \
} } while (0)

static void test_controller(void) {
    System5200 system;
    Pokey5200 pokey;
    GTIA5200 gtia;
    Antic5200 antic;
    CPU6502 cpu = {0};
    system5200_init(&system);
    pokey5200_init(&pokey);
    gtia5200_init(&gtia);
    antic5200_init(&antic);
    system5200_attach_pokey(&system, &pokey);
    system5200_attach_gtia(&system, &gtia);
    system5200_attach_antic(&system, &antic, &cpu);
    system5200_write(&system, 0xc01f, 4);
    system5200_write(&system, 0xe80f, 2);
    system5200_write(&system, 0xe80e, 0xc0);
    pokey5200_set_controller(&pokey, 0, 1u << 12, false);
    system5200_tick(&system, 64 * 114);
    CHECK((pokey.kbcode & 0x1e) == 24);
    CHECK(!(system5200_read(&system, 0xe80e) & 0x40) && cpu.irq_asserted);
    system5200_write(&system, 0xe80e, 0x80);
    CHECK(!cpu.irq_asserted && (pokey.irqst & 0x40));
    pokey5200_set_controller(&pokey, 0, 0, true);
    system5200_tick(&system, 64 * 114);
    CHECK(!(pokey.irqst & 0x80) && !(pokey.skstat & 8) && cpu.irq_asserted);
    system5200_write(&system, 0xe80e, 0);
    CHECK(!cpu.irq_asserted && (pokey.irqst & 0x80));
    system5200_write(&system, 0xe80e, 0x80);
    system5200_tick(&system, 64 * 114);
    CHECK(!cpu.irq_asserted);
    system5200_write(&system, 0xc01f, 5);
    system5200_tick(&system, 64 * 114);
    CHECK(pokey.skstat & 8);
    gtia5200_set_trigger(&gtia, 0, true);
    CHECK(system5200_read(&system, 0xc010) == 0);
    system5200_write(&system, 0xc01d, 4);
    gtia5200_set_trigger(&gtia, 0, false);
    CHECK(system5200_read(&system, 0xc010) == 0);
    system5200_write(&system, 0xc01d, 0);
    CHECK(system5200_read(&system, 0xc010) == 1);
}

static void test_keypad_scanning(void) {
    for (unsigned code = 1; code <= 15; code++) {
        Pokey5200 pokey;
        pokey5200_init(&pokey);
        pokey5200_write(&pokey, 0xe80f, 2);
        pokey5200_write(&pokey, 0xe80e, 0x40);
        pokey5200_set_controller(&pokey, 0, (uint16_t)(1u << code), false);
        for (unsigned cycle = 0; cycle < 64 * 114; cycle++) pokey5200_tick(&pokey);
        CHECK((pokey.kbcode & 0x1f) == (code * 2 + 1));
        CHECK(!(pokey.irqst & 0x40));
        pokey5200_write(&pokey, 0xe80e, 0);
        pokey5200_write(&pokey, 0xe80e, 0x40);
        for (unsigned cycle = 0; cycle < 64 * 114; cycle++) pokey5200_tick(&pokey);
        CHECK(!(pokey.irqst & 0x40));
        pokey5200_set_controller(&pokey, 0, 0, false);
        pokey5200_write(&pokey, 0xe80e, 0);
        pokey5200_write(&pokey, 0xe80e, 0x40);
        for (unsigned cycle = 0; cycle < 64 * 114; cycle++) pokey5200_tick(&pokey);
        CHECK((pokey.irqst & 0x40) && (pokey.skstat & 4));
    }
    Pokey5200 pokey;
    pokey5200_init(&pokey);
    pokey5200_write(&pokey, 0xe80f, 3);
    pokey5200_write(&pokey, 0xe80e, 0x40);
    pokey5200_set_controller(&pokey, 0, 1u << 12, false);
    for (unsigned cycle = 0; cycle < 256 * 114; cycle++) pokey5200_tick(&pokey);
    CHECK((pokey.irqst & 0x40) && pokey.kbcode == 0xff);
}

static void test_controller_ports(void) {
    for (unsigned port = 0; port < 4; port++) {
        Pokey5200 pokey;
        pokey5200_init(&pokey);
        pokey5200_set_controller(&pokey, port, 1u << 12, true);
        pokey5200_set_controller_lines(&pokey, (uint8_t)((port + 1) & 3));
        pokey5200_write(&pokey, 0xe80f, 2);
        pokey5200_write(&pokey, 0xe80e, 0xc0);
        for (unsigned cycle = 0; cycle < 64 * 114; cycle++) pokey5200_tick(&pokey);
        CHECK(pokey.kbcode == 0xff && pokey.irqst == 0xff);
        pokey5200_set_controller_lines(&pokey, (uint8_t)port);
        for (unsigned cycle = 0; cycle < 64 * 114; cycle++) pokey5200_tick(&pokey);
        CHECK((pokey.kbcode & 0x1e) == 24);
        CHECK(!(pokey.irqst & 0xc0));
    }
}

static void test_pot_power(void) {
    Pokey5200 pokey;
    pokey5200_init(&pokey);
    CHECK(pokey5200_set_pot(&pokey, 0, 6));
    pokey5200_set_controller_lines(&pokey, 0);
    pokey5200_write(&pokey, 0xe80f, 4);
    pokey5200_write(&pokey, 0xe80b, 0);
    for (int i = 0; i < 229; i++) pokey5200_tick(&pokey);
    CHECK(pokey.pot_value[0] == 228);
    pokey5200_set_controller_lines(&pokey, 4);
    pokey5200_write(&pokey, 0xe80b, 0);
    for (int i = 0; i < 7; i++) pokey5200_tick(&pokey);
    CHECK(pokey.pot_value[0] == 6);
}

static void tick(Pokey5200 *pokey, unsigned cycles) {
    while (cycles--) pokey5200_tick(pokey);
}

static void test_audio_timers(void) {
    static const struct { uint8_t audctl; unsigned channel, period; } cases[] = {
        {0x00, 0, 84}, {0x01, 0, 342}, {0x40, 0, 6}, {0x20, 2, 6},
        {0x10, 1, (258 + 1) * 28}, {0x50, 1, 258 + 7},
        {0x08, 3, (258 + 1) * 28}, {0x28, 3, 258 + 7}
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Pokey5200 pokey;
        pokey5200_init(&pokey);
        unsigned low = cases[i].channel & ~1u;
        pokey5200_write(&pokey, 0xe800 + low * 2, 2);
        pokey5200_write(&pokey, 0xe800 + (low + 1) * 2, 1);
        pokey5200_write(&pokey, 0xe801 + cases[i].channel * 2, 0xaf);
        pokey5200_write(&pokey, 0xe808, cases[i].audctl);
        pokey5200_write(&pokey, 0xe80f, 3);
        pokey5200_write(&pokey, 0xe809, 0);
        bool initial = pokey.audio_output[cases[i].channel];
        tick(&pokey, cases[i].period - 1);
        CHECK(pokey.audio_output[cases[i].channel] == initial);
        tick(&pokey, 1);
        CHECK(pokey.audio_output[cases[i].channel] != initial);
        tick(&pokey, cases[i].period);
        CHECK(pokey.audio_output[cases[i].channel] == initial);
        pokey5200_write(&pokey, 0xe809, 0);
        CHECK(pokey.audio_output[cases[i].channel] == (cases[i].channel >= 2));
    }
}

static void test_audio_samples(void) {
    Pokey5200 pokey;
    pokey5200_init(&pokey);
    pokey5200_capture_audio(&pokey, true);
    pokey5200_write(&pokey, 0xe80f, 3);
    tick(&pokey, 29868);
    int16_t samples[1024];
    size_t n = pokey5200_take_samples(&pokey, samples, 1024);
    CHECK(n == (size_t)(29868u * 44100ull / 1789773u));
    for (size_t i = 0; i < n; i++) CHECK(samples[i] == 0);
    pokey5200_write(&pokey, 0xe800, 8);
    pokey5200_write(&pokey, 0xe801, 0xaf);
    pokey5200_write(&pokey, 0xe809, 0);
    tick(&pokey, 29868);
    n = pokey5200_take_samples(&pokey, samples, 1024);
    int minimum = 0, maximum = 0;
    for (size_t i = 0; i < n; i++) {
        if (samples[i] < minimum) minimum = samples[i];
        if (samples[i] > maximum) maximum = samples[i];
    }
    CHECK(n >= 735 && n <= 737 && minimum < -500 && maximum > 500);
    CHECK(pokey5200_take_samples(&pokey, samples, 1024) == 0);
    pokey5200_capture_audio(&pokey, false);
    tick(&pokey, 29868);
    CHECK(pokey.audio_count == 0);
}

static void test_audio_waveforms(void) {
    Pokey5200 pokey;
    pokey5200_init(&pokey);
    pokey5200_write(&pokey, 0xe80f, 3);
    tick(&pokey, 15);
    CHECK(pokey.poly4 == 0);
    tick(&pokey, 16);
    CHECK(pokey.poly5 == 0);
    for (unsigned mode = 0; mode < 4; mode++) {
        pokey5200_init(&pokey);
        pokey5200_write(&pokey, 0xe80f, 3);
        pokey5200_write(&pokey, 0xe808, (mode & 1 ? 0x80 : 0) | 0x40);
        pokey5200_write(&pokey, 0xe801, mode & 2 ? 0xcf : 0x8f);
        pokey5200_write(&pokey, 0xe809, 0);
        bool seen_low = false, seen_high = false;
        for (unsigned i = 0; i < 2048; i++) {
            pokey5200_tick(&pokey);
            if (pokey.audio_output[0]) seen_high = true;
            else seen_low = true;
        }
        CHECK(seen_low && seen_high);
    }

    pokey5200_init(&pokey);
    pokey5200_capture_audio(&pokey, true);
    for (unsigned channel = 0; channel < 4; channel++)
        pokey5200_write(&pokey, 0xe801 + 2 * channel, 0x1f);
    tick(&pokey, 42);
    int16_t sample[2];
    CHECK(pokey5200_take_samples(&pokey, sample, 2) == 1);
    CHECK(sample[0] > 29000 && sample[0] < 30000);
    pokey5200_capture_audio(&pokey, true);
    CHECK(pokey.audio_count == 0 && pokey.audio_phase == 0 && pokey.audio_dc == 0);
}

static void test_audio_frequency(void) {
    Pokey5200 pokey;
    pokey5200_init(&pokey);
    pokey5200_write(&pokey, 0xe80f, 3);
    pokey5200_write(&pokey, 0xe800, 72);
    pokey5200_write(&pokey, 0xe801, 0xaf);
    pokey5200_write(&pokey, 0xe809, 0);
    unsigned transitions = 0;
    bool previous = pokey.audio_output[0];
    for (unsigned i = 0; i < 1789773; i++) {
        pokey5200_tick(&pokey);
        if (previous != pokey.audio_output[0]) transitions++;
        previous = pokey.audio_output[0];
    }
    CHECK(transitions == 875);
}

static void test_audio_system_bus(void) {
    System5200 system;
    Pokey5200 pokey;
    system5200_init(&system);
    pokey5200_init(&pokey);
    system5200_attach_pokey(&system, &pokey);
    pokey5200_capture_audio(&pokey, true);
    system5200_write(&system, 0xe80f, 3);
    system5200_write(&system, 0xe800, 8);
    system5200_write(&system, 0xe801, 0xaf);
    system5200_write(&system, 0xe809, 0);
    system5200_tick(&system, 29868);
    int16_t samples[1024];
    size_t n = pokey5200_take_samples(&pokey, samples, 1024);
    CHECK(n >= 735 && n <= 737);
    bool nonzero = false;
    for (size_t i = 0; i < n; i++) nonzero |= samples[i] != 0;
    CHECK(nonzero);
}

int main(void) {
    test_controller();
    test_keypad_scanning();
    test_controller_ports();
    test_pot_power();
    test_audio_timers();
    test_audio_samples();
    test_audio_waveforms();
    test_audio_frequency();
    test_audio_system_bus();
    Pokey5200 pokey;
    System5200 system;
    pokey5200_init(&pokey);
    system5200_init(&system);
    system5200_attach_pokey(&system, &pokey);
    pokey5200_set_controller_lines(&pokey, 4);
    CHECK(pokey5200_set_pot(&pokey, 0, 2));
    CHECK(pokey5200_set_pot(&pokey, 1, 4));
    CHECK(pokey5200_set_pot(&pokey, 2, 0));
    CHECK(!pokey5200_set_pot(&pokey, 8, 1));
    CHECK(!pokey5200_set_pot(&pokey, 1, 229));
    system5200_write(&system, 0xe820, 0x37);
    system5200_write(&system, 0xe8f1, 0xa5);
    system5200_write(&system, 0xe8f8, 0x41);
    CHECK(pokey.audf[0] == 0x37 && pokey.audc[0] == 0xa5 && pokey.audctl == 0x41);
    system5200_write(&system, 0xe80b, 0);
    CHECK(pokey.allpot == 0xff && pokey.pot_value[0] == 0);
    system5200_tick(&system, 400);
    CHECK(pokey.pot_value[0] == 0);
    system5200_write(&system, 0xe80f, 3);
    system5200_tick(&system, 113);
    CHECK(pokey.pot_value[0] == 0);
    system5200_tick(&system, 1);
    CHECK(pokey.pot_value[0] == 1 && pokey.pot_value[2] == 0 && pokey.allpot == 0xfb);
    system5200_tick(&system, 114);
    CHECK(pokey.pot_value[0] == 2 && !(pokey.allpot & 1));
    system5200_tick(&system, 228);
    CHECK(pokey.pot_value[0] == 2 && pokey.pot_value[1] == 4 && !(pokey.allpot & 3));
    system5200_tick(&system, 224 * 114);
    CHECK(!pokey.pot_scanning && pokey.allpot == 0 && pokey.pot_value[7] == 228);
    system5200_write(&system, 0xe80f, 4);
    system5200_write(&system, 0xe80b, 0);
    system5200_tick(&system, 2);
    CHECK(pokey.pot_value[0] == 2 && !(pokey.allpot & 1));
    CHECK(system5200_read(&system, 0xeff0) == 2);
    puts("POKEY 5200 register and pot tests passed");
    return 0;
}
