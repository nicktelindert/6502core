#define _POSIX_C_SOURCE 200809L
#include "../rom_browser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); \
} } while (0)

static void touch(const char *path) {
    FILE *f = fopen(path, "w");
    CHECK(f && fclose(f) == 0);
}

int main(void) {
    char tmp[] = "/tmp/6502core-browser-XXXXXX";
    CHECK(mkdtemp(tmp) != NULL);
    char config[4096], root[4096], sub[4096], hidden[4096], rom[4096], other[4096];
    CHECK(snprintf(config, sizeof(config), "%s/config/nested", tmp) < (int)sizeof(config));
    CHECK(snprintf(root, sizeof(root), "%s/roms space%%", tmp) < (int)sizeof(root));
    CHECK(snprintf(sub, sizeof(sub), "%s/sub", root) < (int)sizeof(sub));
    CHECK(snprintf(hidden, sizeof(hidden), "%s/.hidden", root) < (int)sizeof(hidden));
    CHECK(snprintf(rom, sizeof(rom), "%s/Game.Bin", root) < (int)sizeof(rom));
    CHECK(snprintf(other, sizeof(other), "%s/readme.txt", root) < (int)sizeof(other));
    CHECK(mkdir(root, 0700) == 0 && mkdir(sub, 0700) == 0 && mkdir(hidden, 0700) == 0);
    touch(rom);
    touch(other);
    CHECK(setenv("XDG_CONFIG_HOME", config, 1) == 0);

    char path[ROM_PATH_MAX], error[256];
    CHECK(rom_config_load(path, sizeof(path), error, sizeof(error)) == ROM_CONFIG_MISSING);
    CHECK(rom_config_save(root, error, sizeof(error)));
    CHECK(rom_config_load(path, sizeof(path), error, sizeof(error)) == ROM_CONFIG_OK);
    CHECK(strcmp(path, root) == 0);
    CHECK(!rom_config_save("/does-not-exist-6502core", error, sizeof(error)));
    CHECK(rom_config_load(path, sizeof(path), error, sizeof(error)) == ROM_CONFIG_OK);
    CHECK(strcmp(path, root) == 0);

    RomBrowser browser = {0};
    CHECK(rom_browser_open(&browser, root, false, error, sizeof(error)));
    CHECK(browser.count == 4);
    CHECK(browser.entries[0].is_dir && strcmp(browser.entries[0].name, "..") == 0);
    CHECK(browser.entries[1].is_dir && strcmp(browser.entries[1].name, ".hidden") == 0);
    CHECK(browser.entries[2].is_dir && strcmp(browser.entries[2].name, "sub") == 0);
    CHECK(!browser.entries[3].is_dir && strcmp(browser.entries[3].name, "Game.Bin") == 0);
    CHECK(rom_browser_path(&browser, 3, path, sizeof(path)));
    CHECK(strcmp(path, rom) == 0);
    CHECK(rom_browser_open(&browser, sub, true, error, sizeof(error)));
    CHECK(browser.count == 1 && strcmp(browser.entries[0].name, "..") == 0);
    CHECK(!rom_browser_open(&browser, "/does-not-exist-6502core", false, error, sizeof(error)));
    CHECK(strcmp(browser.dir, sub) == 0);
    rom_browser_free(&browser);

    CHECK(snprintf(path, sizeof(path), "%s/6502core/config.ini", config) < (int)sizeof(path));
    FILE *bad = fopen(path, "w");
    CHECK(bad && fputs("[6502core]\nrom_dir=%ZZ\n", bad) >= 0 && fclose(bad) == 0);
    CHECK(rom_config_load(path, sizeof(path), error, sizeof(error)) == ROM_CONFIG_ERROR);
    CHECK(unsetenv("XDG_CONFIG_HOME") == 0 && setenv("HOME", tmp, 1) == 0);
    CHECK(rom_config_load(path, sizeof(path), error, sizeof(error)) == ROM_CONFIG_MISSING);
    CHECK(rom_config_save(root, error, sizeof(error)));
    CHECK(rom_config_load(path, sizeof(path), error, sizeof(error)) == ROM_CONFIG_OK);
    CHECK(strcmp(path, root) == 0);
    CHECK(snprintf(path, sizeof(path), "%s/.config/6502core/config.ini", tmp) < (int)sizeof(path));
    CHECK(unlink(path) == 0);
    CHECK(snprintf(path, sizeof(path), "%s/.config/6502core", tmp) < (int)sizeof(path));
    CHECK(rmdir(path) == 0);
    CHECK(snprintf(path, sizeof(path), "%s/.config", tmp) < (int)sizeof(path));
    CHECK(rmdir(path) == 0);
    CHECK(unlink(rom) == 0 && unlink(other) == 0 && rmdir(sub) == 0 && rmdir(hidden) == 0 && rmdir(root) == 0);
    CHECK(snprintf(path, sizeof(path), "%s/6502core/config.ini", config) < (int)sizeof(path));
    CHECK(unlink(path) == 0);
    CHECK(snprintf(path, sizeof(path), "%s/6502core", config) < (int)sizeof(path));
    CHECK(rmdir(path) == 0 && rmdir(config) == 0);
    CHECK(snprintf(path, sizeof(path), "%s/config", tmp) < (int)sizeof(path));
    CHECK(rmdir(path) == 0 && rmdir(tmp) == 0);
    puts("ROM browser tests passed");
    return 0;
}
