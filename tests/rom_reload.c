#define main emulator_main
#include "../main.c"
#undef main
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", \
                __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void check_fresh_tia(const TIA *tia) {
    CHECK(!tia->vsync_on && tia->vsync == 0 && tia->vblank == 0);
    CHECK(!tia->wsync && tia->nusiz0 == 0 && tia->nusiz1 == 0);
    CHECK(tia->grp0 == 0 && tia->grp1 == 0 && tia->grp0_old == 0);
    CHECK(tia->enam0 == 0 && tia->enam1 == 0 && tia->vdelp0 == 0);
    CHECK(tia->p0_x == 0 && tia->cxp0fb == 0 && tia->audio_clock == 6);
    CHECK(tia->inpt4 == 0x80 && tia->inpt5 == 0x80);
}

int main(void) {
    char path[] = "/tmp/6502core-rom-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0);
    FILE *rom_file = fdopen(fd, "wb");
    CHECK(rom_file != NULL);
    uint8_t rom[4096] = {0};
    rom[0x0FFC] = 0x00;
    rom[0x0FFD] = 0xF1;
    CHECK(fwrite(rom, 1, sizeof(rom), rom_file) == sizeof(rom));
    CHECK(fclose(rom_file) == 0);

    Emulator emu = {0};
    CHECK(emu_load_rom(&emu, path));
    check_fresh_tia(&emu.tia);
    tia_write(&emu.tia, 0x00, 0x02);
    tia_write(&emu.tia, 0x01, 0x02);
    tia_write(&emu.tia, 0x04, 0x37);
    tia_write(&emu.tia, 0x05, 0x35);
    tia_write(&emu.tia, 0x1B, 0xFF);
    tia_write(&emu.tia, 0x1C, 0xFE);
    tia_write(&emu.tia, 0x1D, 0x02);
    tia_write(&emu.tia, 0x1E, 0x02);
    tia_write(&emu.tia, 0x25, 0x01);
    emu.tia.cxp0fb = 0xC0;
    emu.tia.p0_x = 25;
    emu.tia.audio_clock = 42;
    tia_step(&emu.tia, 228);
    CHECK(emu.tia.framebuffer != NULL);
    CHECK(emu_load_rom(&emu, path));
    check_fresh_tia(&emu.tia);
    emu_destroy(&emu);

    uint8_t dpc_rom[10240] = {0};
    const uint8_t program[] = {
        0xAD, 0x06, 0x10, 0x85, 0x19,
        0xAD, 0x84, 0x02, 0xD0, 0xF6,
        0xA9, 0x02, 0x85, 0x00, 0x4C, 0x0E, 0xF1
    };
    memcpy(dpc_rom + 0x1100, program, sizeof(program));
    dpc_rom[0x1FFC] = 0x00;
    dpc_rom[0x1FFD] = 0xF1;
    rom_file = fopen(path, "wb");
    CHECK(rom_file != NULL);
    CHECK(fwrite(dpc_rom, 1, sizeof(dpc_rom), rom_file) == sizeof(dpc_rom));
    CHECK(fclose(rom_file) == 0);
    for (int i = 0; i < 2; i++) {
        CHECK(emu_load_rom(&emu, path));
        CHECK(emu.cart.type == CART_TYPE_DPC);
        CHECK(bus_step_frame(&emu.bus, &emu.cpu));
        CHECK(emu.tia.completed_frame_height > 0);
    }
    emu_destroy(&emu);
    CHECK(unlink(path) == 0);
    puts("ROM reload test passed");
    return EXIT_SUCCESS;
}
