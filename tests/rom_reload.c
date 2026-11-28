#define main emulator_main
#include "../main.c"
#undef main
#include <sys/stat.h>
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

static void test_5200_keyboard_mapping(void) {
    Emulator emu = {0};
    uint8_t keys[SDL_NUM_SCANCODES] = {0};
    pokey5200_init(&emu.pokey5200);
    gtia5200_init(&emu.gtia5200);
    handle_5200_input(&emu, keys, true);
    CHECK(emu.pokey5200.pot_input[0] == 114 && emu.pokey5200.pot_input[1] == 114);
    keys[SDL_SCANCODE_LEFT] = keys[SDL_SCANCODE_W] = 1;
    keys[SDL_SCANCODE_LCTRL] = keys[SDL_SCANCODE_SPACE] = keys[SDL_SCANCODE_RETURN] = 1;
    handle_5200_input(&emu, keys, true);
    CHECK(emu.pokey5200.pot_input[0] == 6 && emu.pokey5200.pot_input[1] == 6);
    CHECK(!emu.gtia5200.trigger[0] && emu.pokey5200.upper_trigger[0]);
    CHECK(emu.pokey5200.keypad[0] == (1u << 12));
    keys[SDL_SCANCODE_RIGHT] = 1;
    handle_5200_input(&emu, keys, true);
    CHECK(emu.pokey5200.pot_input[0] == 114);
    handle_5200_input(&emu, keys, false);
    CHECK(emu.pokey5200.pot_input[0] == 114 && emu.pokey5200.pot_input[1] == 114);
    CHECK(emu.gtia5200.trigger[0] && !emu.pokey5200.upper_trigger[0] && !emu.pokey5200.keypad[0]);
    memset(keys, 0, sizeof(keys));
    keys[SDL_SCANCODE_LSHIFT] = keys[SDL_SCANCODE_8] = 1;
    handle_5200_input(&emu, keys, true);
    CHECK(emu.pokey5200.keypad[0] == (1u << 3));
    keys[SDL_SCANCODE_8] = 0;
    keys[SDL_SCANCODE_3] = 1;
    handle_5200_input(&emu, keys, true);
    CHECK(emu.pokey5200.keypad[0] == (1u << 1));
    static const struct { SDL_Scancode key; unsigned code; } cases[] = {
        {SDL_SCANCODE_1, 15}, {SDL_SCANCODE_2, 14}, {SDL_SCANCODE_3, 13},
        {SDL_SCANCODE_4, 11}, {SDL_SCANCODE_5, 10}, {SDL_SCANCODE_6, 9},
        {SDL_SCANCODE_7, 7}, {SDL_SCANCODE_8, 6}, {SDL_SCANCODE_9, 5},
        {SDL_SCANCODE_0, 2}, {SDL_SCANCODE_KP_MULTIPLY, 3}, {SDL_SCANCODE_KP_DIVIDE, 1},
        {SDL_SCANCODE_KP_1, 15}, {SDL_SCANCODE_KP_2, 14}, {SDL_SCANCODE_KP_3, 13},
        {SDL_SCANCODE_KP_4, 11}, {SDL_SCANCODE_KP_5, 10}, {SDL_SCANCODE_KP_6, 9},
        {SDL_SCANCODE_KP_7, 7}, {SDL_SCANCODE_KP_8, 6}, {SDL_SCANCODE_KP_9, 5},
        {SDL_SCANCODE_KP_0, 2}, {SDL_SCANCODE_RETURN, 12}, {SDL_SCANCODE_KP_ENTER, 12},
        {SDL_SCANCODE_P, 8}, {SDL_SCANCODE_R, 4}
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        memset(keys, 0, sizeof(keys));
        keys[cases[i].key] = 1;
        handle_5200_input(&emu, keys, true);
        CHECK(emu.pokey5200.keypad[0] == (1u << cases[i].code));
    }
    memset(keys, 0, sizeof(keys));
    keys[SDL_SCANCODE_D] = keys[SDL_SCANCODE_DOWN] = 1;
    keys[SDL_SCANCODE_RCTRL] = 1;
    handle_5200_input(&emu, keys, true);
    CHECK(emu.pokey5200.pot_input[0] == 220 && emu.pokey5200.pot_input[1] == 220);
    CHECK(!emu.gtia5200.trigger[0] && !emu.pokey5200.upper_trigger[0]);
    memset(keys, 0, sizeof(keys));
    handle_5200_input(&emu, keys, true);
    CHECK(emu.gtia5200.trigger[0] && emu.pokey5200.pot_input[0] == 114);
}

static void test_frame_pacer(void) {
    uint64_t frequency = SDL_GetPerformanceFrequency();
    for (int standard = VIDEO_NTSC; standard <= VIDEO_PAL; standard++) {
        FramePacer pacer = {0};
        uint64_t start = SDL_GetPerformanceCounter();
        for (int frame = 0; frame < 10; frame++) {
            uint64_t frame_start = SDL_GetPerformanceCounter();
            frame_pacer_wait(&pacer, frame_start, frequency,
                             video_frame_rate((VideoStandard)standard));
        }
        double elapsed = (double)(SDL_GetPerformanceCounter() - start) / frequency;
        double target = 10.0 / video_frame_rate((VideoStandard)standard);
        CHECK(elapsed >= target);
        CHECK(elapsed < target + 0.5);
    }
}

static void test_browser_keys(void) {
    char tmp[] = "/tmp/6502core-ui-XXXXXX";
    CHECK(mkdtemp(tmp) != NULL);
    char config[ROM_PATH_MAX], folder[ROM_PATH_MAX], rom[ROM_PATH_MAX], path[ROM_PATH_MAX];
    CHECK(snprintf(config, sizeof(config), "%s/config", tmp) < (int)sizeof(config));
    CHECK(snprintf(folder, sizeof(folder), "%s/games", tmp) < (int)sizeof(folder));
    CHECK(snprintf(rom, sizeof(rom), "%s/game.bin", folder) < (int)sizeof(rom));
    CHECK(mkdir(folder, 0700) == 0);
    FILE *f = fopen(rom, "w");
    CHECK(f && fclose(f) == 0);
    CHECK(setenv("XDG_CONFIG_HOME", config, 1) == 0);

    OverlayState ov = {0};
    CHECK(overlay_open_browser(&ov, tmp, true));
    CHECK(ov.show_rom_menu && ov.selecting_folder);
    CHECK(ov.browser.count == 2);
    CHECK(!overlay_browser_key(&ov, SDLK_DOWN, path, sizeof(path)));
    CHECK(!overlay_browser_key(&ov, SDLK_RETURN, path, sizeof(path)));
    CHECK(strcmp(ov.browser.dir, folder) == 0);
    CHECK(!overlay_browser_key(&ov, SDLK_s, path, sizeof(path)));
    CHECK(!ov.selecting_folder && strcmp(ov.rom_dir, folder) == 0);
    char error[256];
    CHECK(rom_config_load(path, sizeof(path), error, sizeof(error)) == ROM_CONFIG_OK);
    CHECK(strcmp(path, folder) == 0);
    CHECK(!overlay_browser_key(&ov, SDLK_DOWN, path, sizeof(path)));
    CHECK(overlay_browser_key(&ov, SDLK_RETURN, path, sizeof(path)));
    CHECK(strcmp(path, rom) == 0);
    CHECK(!overlay_browser_key(&ov, SDLK_BACKSPACE, path, sizeof(path)));
    CHECK(strcmp(ov.browser.dir, tmp) == 0);
    CHECK(overlay_open_browser(&ov, tmp, true));
    CHECK(setenv("XDG_CONFIG_HOME", rom, 1) == 0);
    CHECK(!overlay_browser_key(&ov, SDLK_s, path, sizeof(path)));
    CHECK(ov.selecting_folder && strcmp(ov.rom_dir, folder) == 0 && ov.browser_error[0]);
    CHECK(setenv("XDG_CONFIG_HOME", config, 1) == 0);
    CHECK(overlay_open_browser(&ov, folder, true));
    CHECK(!overlay_browser_key(&ov, SDLK_ESCAPE, path, sizeof(path)));
    CHECK(!ov.show_rom_menu && strcmp(ov.rom_dir, folder) == 0);
    char bios[ROM_PATH_MAX];
    CHECK(snprintf(bios, sizeof(bios), "%s/5200bios.dat", folder) < (int)sizeof(bios));
    f = fopen(bios, "wb");
    CHECK(f);
    uint8_t bytes[2048] = {0};
    CHECK(fwrite(bytes, 1, sizeof(bytes), f) == sizeof(bytes) && fclose(f) == 0);
    CHECK(overlay_open_filtered(&ov, folder, BROWSER_ALL_FILES));
    CHECK(ov.selecting_bios && ov.browser.count == 3);
    CHECK(!overlay_browser_key(&ov, SDLK_DOWN, path, sizeof(path)));
    CHECK(overlay_browser_key(&ov, SDLK_RETURN, path, sizeof(path)));
    CHECK(strcmp(path, bios) == 0);
    CHECK(overlay_select_bios(&ov, path));
    CHECK(!ov.show_rom_menu && strcmp(ov.bios_path, bios) == 0);
    CHECK(bios_config_load(path, sizeof(path), error, sizeof(error)) == ROM_CONFIG_OK);
    CHECK(strcmp(path, bios) == 0);
    CHECK(!overlay_select_bios(&ov, rom));
    CHECK(strcmp(ov.bios_path, bios) == 0);
    CHECK(bios_config_load(path, sizeof(path), error, sizeof(error)) == ROM_CONFIG_OK);
    CHECK(strcmp(path, bios) == 0);
    CHECK(setenv("XDG_CONFIG_HOME", rom, 1) == 0);
    CHECK(!overlay_select_bios(&ov, bios));
    CHECK(strcmp(ov.bios_path, bios) == 0);
    CHECK(setenv("XDG_CONFIG_HOME", config, 1) == 0);
    CHECK(bios_config_load(path, sizeof(path), error, sizeof(error)) == ROM_CONFIG_OK);
    CHECK(strcmp(path, bios) == 0);
    rom_browser_free(&ov.browser);
    CHECK(unlink(bios) == 0 && unlink(rom) == 0 && rmdir(folder) == 0);
    CHECK(snprintf(path, sizeof(path), "%s/6502core/config.ini", config) < (int)sizeof(path));
    CHECK(unlink(path) == 0);
    CHECK(snprintf(path, sizeof(path), "%s/6502core", config) < (int)sizeof(path));
    CHECK(rmdir(path) == 0 && rmdir(config) == 0 && rmdir(tmp) == 0);
}

static void test_experimental_5200(void) {
    char tmp[] = "/tmp/6502core-5200-XXXXXX";
    CHECK(mkdtemp(tmp) != NULL);
    char config[ROM_PATH_MAX], bios[ROM_PATH_MAX], rompath[ROM_PATH_MAX], path[ROM_PATH_MAX];
    CHECK(snprintf(config, sizeof(config), "%s/config", tmp) < (int)sizeof(config));
    CHECK(snprintf(bios, sizeof(bios), "%s/bios.dat", tmp) < (int)sizeof(bios));
    CHECK(snprintf(rompath, sizeof(rompath), "%s/game.A52", tmp) < (int)sizeof(rompath));
    CHECK(setenv("XDG_CONFIG_HOME", config, 1) == 0);
    uint8_t firmware[2048] = {0};
    firmware[0] = 0xa9; firmware[1] = 0x4e;
    firmware[2] = 0x8d; firmware[3] = 0x1a; firmware[4] = 0xc0;
    firmware[5] = 0x4c; firmware[6] = 0x05; firmware[7] = 0xf8;
    firmware[0x7fc] = 0; firmware[0x7fd] = 0xf8;
    FILE *f = fopen(bios, "wb");
    CHECK(f && fwrite(firmware, 1, sizeof(firmware), f) == sizeof(firmware) && fclose(f) == 0);
    uint8_t data[0x8000] = {0};
    f = fopen(rompath, "wb");
    CHECK(f && fwrite(data, 1, 0x2000, f) == 0x2000 && fclose(f) == 0);
    Emulator emu = {0};
    CHECK(!emu_load_rom(&emu, rompath) && !emu.loaded);
    CHECK(strstr(emu.load_error, "BIOS") != NULL);
    char error[256];
    CHECK(bios_config_save(bios, error, sizeof(error)));
    CHECK(emu_load_rom(&emu, rompath));
    CHECK(emu.loaded && emu.is_5200 && emu.system5200.cart_type == SYSTEM5200_CART_8K);
    CHECK(emu.cpu.PC == 0xf800);
    CHECK(emu_step_5200_frame(&emu) && !emu.warned_5200);
    CHECK(emu.gtia5200.framebuffer[0][0] == 0x4e);
    CHECK(!emu_load_rom(&emu, "/missing.a52") && emu.loaded && emu.is_5200);
    CHECK(snprintf(path, sizeof(path), "%s/game.a26", tmp) < (int)sizeof(path));
    data[0xffc] = 0; data[0xffd] = 0xf1;
    f = fopen(path, "wb");
    CHECK(f && fwrite(data, 1, 0x1000, f) == 0x1000 && fclose(f) == 0);
    CHECK(emu_load_rom(&emu, path) && emu.loaded && !emu.is_5200);
    CHECK(emu_load_rom(&emu, rompath) && emu.loaded && emu.is_5200);
    CHECK(unlink(path) == 0);
    for (unsigned i = 0; i < 3; i++) {
        static const size_t sizes[] = {0x1000, 0x4000, 0x8000};
        static const System5200CartType types[] = {SYSTEM5200_CART_4K, SYSTEM5200_CART_16K, SYSTEM5200_CART_32K};
        f = fopen(rompath, "wb");
        CHECK(f && fwrite(data, 1, sizes[i], f) == sizes[i] && fclose(f) == 0);
        CHECK(emu_load_rom(&emu, rompath) && emu.system5200.cart_type == types[i]);
    }
    f = fopen(rompath, "wb");
    CHECK(f && fwrite(data, 1, 512, f) == 512 && fclose(f) == 0);
    CHECK(!emu_load_rom(&emu, rompath) && emu.loaded && emu.is_5200);
    emu.antic5200.unsupported = true;
    CHECK(emu_step_5200_frame(&emu) && emu.warned_5200);
    emu_destroy(&emu);
    CHECK(unlink(bios) == 0 && unlink(rompath) == 0);
    CHECK(snprintf(path, sizeof(path), "%s/6502core/config.ini", config) < (int)sizeof(path));
    CHECK(unlink(path) == 0);
    CHECK(snprintf(path, sizeof(path), "%s/6502core", config) < (int)sizeof(path));
    CHECK(rmdir(path) == 0 && rmdir(config) == 0 && rmdir(tmp) == 0);
}

int main(void) {
    test_5200_keyboard_mapping();
    test_frame_pacer();
    test_browser_keys();
    test_experimental_5200();
    CHECK(video_standard_from_filename("/tmp/PAL_folder/game.bin") == VIDEO_NTSC);
    CHECK(video_standard_from_filename("/tmp/game (PAL).bin") == VIDEO_PAL);
    CHECK(video_standard_from_filename("/tmp/game-pal.a26") == VIDEO_PAL);
    CHECK(video_standard_from_filename("/tmp/game.bin") == VIDEO_NTSC);
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
    CHECK(is_5200_rom("game.a52") && is_5200_rom("game.A52"));
    CHECK(!is_5200_rom("game.a26") && !is_5200_rom("game.a52.bin"));
    CHECK(!emu_load_rom(&emu, "game.a52") && emu.loaded);
    check_fresh_tia(&emu.tia);
    CHECK(emu.standard == VIDEO_NTSC);
    CHECK(emu.audio.step == TIA_AUDIO_RATE / TIA_OUT_RATE);
    CHECK(video_frame_rate(VIDEO_NTSC) == 60.0);
    CHECK(video_frame_rate(VIDEO_PAL) == 50.0);
    uint8_t ntsc_color[3];
    memcpy(ntsc_color, emu.tia.color_lut[0x38], 3);
    emu_set_video_standard(&emu, VIDEO_PAL);
    CHECK(emu.standard == VIDEO_PAL);
    CHECK(emu.audio.step < TIA_AUDIO_RATE / TIA_OUT_RATE);
    CHECK(memcmp(ntsc_color, emu.tia.color_lut[0x38], 3) != 0);
    CHECK(memcmp(emu.tia.color_lut[0], emu.tia.color_lut[0x10], 3) == 0);
    emu_set_video_standard(&emu, VIDEO_NTSC);
    CHECK(memcmp(ntsc_color, emu.tia.color_lut[0x38], 3) == 0);
    CHECK(emu.audio.step == TIA_AUDIO_RATE / TIA_OUT_RATE);
    for (int i = 0; i < 256; i += 2)
        CHECK(memcmp(emu.tia.color_lut[i], emu.tia.color_lut[i + 1], 3) == 0);
    char pal_path[] = "/tmp/6502core-pal-XXXXXX";
    int pal_fd = mkstemp(pal_path);
    CHECK(pal_fd >= 0);
    FILE *pal_file = fdopen(pal_fd, "wb");
    CHECK(pal_file != NULL);
    CHECK(fwrite(rom, 1, sizeof(rom), pal_file) == sizeof(rom));
    CHECK(fclose(pal_file) == 0);
    CHECK(emu_load_rom(&emu, pal_path));
    CHECK(emu.standard == VIDEO_PAL);
    CHECK(emu.audio.step < TIA_AUDIO_RATE / TIA_OUT_RATE);
    CHECK(emu_load_rom(&emu, path));
    CHECK(emu.standard == VIDEO_NTSC);
    CHECK(unlink(pal_path) == 0);
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
        CHECK(system2600_step_frame(&emu.system2600, &emu.cpu));
        CHECK(emu.tia.completed_frame_height > 0);
    }
    emu_destroy(&emu);
    CHECK(unlink(path) == 0);
    puts("ROM reload test passed");
    return EXIT_SUCCESS;
}
