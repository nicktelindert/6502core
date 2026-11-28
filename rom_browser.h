#ifndef ROM_BROWSER_H
#define ROM_BROWSER_H

#include <stdbool.h>
#include <stddef.h>

#define ROM_PATH_MAX 4096

typedef struct {
    char *name;
    bool is_dir;
} RomBrowserEntry;

typedef struct {
    char dir[ROM_PATH_MAX];
    RomBrowserEntry *entries;
    size_t count;
} RomBrowser;

typedef enum {
    ROM_CONFIG_OK,
    ROM_CONFIG_MISSING,
    ROM_CONFIG_ERROR
} RomConfigResult;

RomConfigResult rom_config_load(char *path, size_t size, char *error, size_t error_size);
bool rom_config_save(const char *path, char *error, size_t error_size);
RomConfigResult bios_config_load(char *path, size_t size, char *error, size_t error_size);
bool bios_config_save(const char *path, char *error, size_t error_size);
typedef enum {
    BROWSER_ROMS,
    BROWSER_DIRECTORIES,
    BROWSER_ALL_FILES
} BrowserFilter;
bool rom_browser_open(RomBrowser *browser, const char *path, bool directories_only,
                      char *error, size_t error_size);
bool rom_browser_open_filtered(RomBrowser *browser, const char *path, BrowserFilter filter,
                               char *error, size_t error_size);
bool rom_browser_path(const RomBrowser *browser, size_t index, char *path, size_t size);
void rom_browser_free(RomBrowser *browser);

#endif
