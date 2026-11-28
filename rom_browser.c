#define _XOPEN_SOURCE 700
#include "rom_browser.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void set_error(char *error, size_t size, const char *message) {
    snprintf(error, size, "%s: %s", message, strerror(errno));
}

static bool join_path(char *out, size_t size, const char *dir, const char *name) {
    int n = snprintf(out, size, "%s%s%s", dir, strcmp(dir, "/") == 0 ? "" : "/", name);
    if (n < 0 || (size_t)n >= size) {
        errno = ENAMETOOLONG;
        return false;
    }
    return true;
}

static bool config_dir(char *out, size_t size) {
    const char *base = getenv("XDG_CONFIG_HOME");
    char fallback[ROM_PATH_MAX];
    if (!base || base[0] != '/') {
        const char *home = getenv("HOME");
        if (!home || home[0] != '/') {
            errno = EINVAL;
            return false;
        }
        if (!join_path(fallback, sizeof(fallback), home, ".config")) return false;
        base = fallback;
    }
    return join_path(out, size, base, "6502core");
}

static bool ensure_dirs(char *path) {
    for (char *p = path + 1; ; p++) {
        if (*p != '/' && *p != '\0') continue;
        char saved = *p;
        *p = '\0';
        if (mkdir(path, 0700) != 0 && errno != EEXIST) {
            *p = saved;
            return false;
        }
        *p = saved;
        if (!saved) break;
    }
    return true;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static RomConfigResult config_load_key(const char *key, char *path, size_t size,
                                       char *error, size_t error_size) {
    char dir[ROM_PATH_MAX], filename[ROM_PATH_MAX];
    if (!config_dir(dir, sizeof(dir)) || !join_path(filename, sizeof(filename), dir, "config.ini")) {
        set_error(error, error_size, "Config path");
        return ROM_CONFIG_ERROR;
    }
    FILE *f = fopen(filename, "r");
    if (!f) {
        if (errno == ENOENT) return ROM_CONFIG_MISSING;
        set_error(error, error_size, "Open config");
        return ROM_CONFIG_ERROR;
    }
    char *line = NULL;
    size_t cap = 0;
    bool section = false, found = false, valid = true;
    while (getline(&line, &cap, f) >= 0) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (line[0] == '[') section = strcmp(line, "[6502core]") == 0;
        else if (section && strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == '=') {
            size_t j = 0;
            const char *value = line + strlen(key) + 1;
            for (size_t i = 0; value[i]; i++) {
                unsigned char c = (unsigned char)value[i];
                if (c == '%') {
                    if (!value[i + 1] || !value[i + 2]) { valid = false; break; }
                    int hi = hex_value(value[i + 1]), lo = hex_value(value[i + 2]);
                    if (hi < 0 || lo < 0 || (hi == 0 && lo == 0)) { valid = false; break; }
                    c = (unsigned char)((hi << 4) | lo);
                    i += 2;
                }
                if (j + 1 >= size) { valid = false; break; }
                path[j++] = (char)c;
            }
            if (size) path[j] = '\0';
            found = true;
            break;
        }
    }
    if (ferror(f)) valid = false;
    free(line);
    if (fclose(f) != 0) valid = false;
    if (!valid || (found && (!size || path[0] != '/'))) {
        snprintf(error, error_size, "Invalid path in config");
        return ROM_CONFIG_ERROR;
    }
    return found ? ROM_CONFIG_OK : ROM_CONFIG_MISSING;
}

RomConfigResult rom_config_load(char *path, size_t size, char *error, size_t error_size) {
    RomConfigResult result = config_load_key("rom_dir", path, size, error, error_size);
    if (result != ROM_CONFIG_OK) return result;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(error, error_size, "Invalid or unavailable ROM folder in config");
        return ROM_CONFIG_ERROR;
    }
    return ROM_CONFIG_OK;
}

RomConfigResult bios_config_load(char *path, size_t size, char *error, size_t error_size) {
    return config_load_key("bios_5200", path, size, error, error_size);
}

static bool write_config_entry(FILE *f, const char *key, const char *path) {
    bool ok = fprintf(f, "%s=", key) >= 0;
    for (const unsigned char *p = (const unsigned char *)path; *p && ok; p++) {
        if (*p == '%' || *p == '\n' || *p == '\r')
            ok = fprintf(f, "%%%02X", *p) >= 0;
        else
            ok = fputc(*p, f) != EOF;
    }
    return ok && fputc('\n', f) != EOF;
}

static bool config_save_key(const char *key, const char *path, char *error, size_t error_size) {
    char dir[ROM_PATH_MAX], filename[ROM_PATH_MAX], temp[ROM_PATH_MAX];
    if (!path || path[0] != '/') {
        snprintf(error, error_size, "Config path must be absolute");
        return false;
    }
    if (!config_dir(dir, sizeof(dir)) || !ensure_dirs(dir) ||
        !join_path(filename, sizeof(filename), dir, "config.ini") ||
        !join_path(temp, sizeof(temp), dir, ".config-XXXXXX")) {
        set_error(error, error_size, "Create config directory");
        return false;
    }
    FILE *old = fopen(filename, "r");
    if (!old && errno != ENOENT) {
        set_error(error, error_size, "Open config");
        return false;
    }
    int fd = mkstemp(temp);
    if (fd < 0) { set_error(error, error_size, "Create config"); if (old) fclose(old); return false; }
    FILE *f = fdopen(fd, "w");
    if (!f) {
        set_error(error, error_size, "Open config");
        close(fd);
        if (old) fclose(old);
        unlink(temp);
        return false;
    }
    bool ok = true, section = false, found_section = false, written = false, last_newline = true;
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while (old && ok && (len = getline(&line, &cap, old)) >= 0) {
        if (line[0] == '[') {
            if (section && !written) {
                if (!last_newline) ok = fputc('\n', f) != EOF;
                if (ok) ok = write_config_entry(f, key, path);
                written = true;
            }
            section = strncmp(line, "[6502core]", 10) == 0 &&
                      (line[10] == '\n' || line[10] == '\r' || line[10] == '\0');
            if (section) found_section = true;
        }
        if (section && strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == '=') {
            if (!written) ok = write_config_entry(f, key, path);
            written = true;
            last_newline = true;
        } else {
            if (ok) ok = fwrite(line, 1, (size_t)len, f) == (size_t)len;
            last_newline = len > 0 && line[len - 1] == '\n';
        }
    }
    if (old && ferror(old)) ok = false;
    if (old && fclose(old) != 0) ok = false;
    free(line);
    if (ok && !written) {
        if (!last_newline) ok = fputc('\n', f) != EOF;
        if (ok && !found_section) ok = fputs("[6502core]\n", f) >= 0;
        if (ok) ok = write_config_entry(f, key, path);
    }
    if (fclose(f) != 0) ok = false;
    if (ok && rename(temp, filename) != 0) ok = false;
    if (!ok) {
        set_error(error, error_size, "Save config");
        unlink(temp);
    }
    return ok;
}

bool rom_config_save(const char *path, char *error, size_t error_size) {
    struct stat st;
    if (!path || path[0] != '/' || stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(error, error_size, "ROM folder is not available");
        return false;
    }
    return config_save_key("rom_dir", path, error, error_size);
}

bool bios_config_save(const char *path, char *error, size_t error_size) {
    return config_save_key("bios_5200", path, error, error_size);
}

void rom_browser_free(RomBrowser *browser) {
    for (size_t i = 0; i < browser->count; i++) free(browser->entries[i].name);
    free(browser->entries);
    memset(browser, 0, sizeof(*browser));
}

static bool is_rom(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return false;
    char lower[5];
    size_t n = strlen(ext);
    if (n >= sizeof(lower)) return false;
    for (size_t i = 0; i <= n; i++) lower[i] = (char)tolower((unsigned char)ext[i]);
    return strcmp(lower, ".bin") == 0 || strcmp(lower, ".a26") == 0 ||
           strcmp(lower, ".a52") == 0 || strcmp(lower, ".rom") == 0;
}

static int compare_entries(const void *a, const void *b) {
    const RomBrowserEntry *x = a, *y = b;
    if (strcmp(x->name, "..") == 0) return strcmp(y->name, "..") == 0 ? 0 : -1;
    if (strcmp(y->name, "..") == 0) return 1;
    if (x->is_dir != y->is_dir) return x->is_dir ? -1 : 1;
    return strcmp(x->name, y->name);
}

bool rom_browser_path(const RomBrowser *browser, size_t index, char *path, size_t size) {
    return index < browser->count && join_path(path, size, browser->dir, browser->entries[index].name);
}

bool rom_browser_open(RomBrowser *browser, const char *path, bool directories_only,
                      char *error, size_t error_size) {
    return rom_browser_open_filtered(browser, path, directories_only ? BROWSER_DIRECTORIES : BROWSER_ROMS,
                                     error, error_size);
}

bool rom_browser_open_filtered(RomBrowser *browser, const char *path, BrowserFilter filter,
                               char *error, size_t error_size) {
    RomBrowser next = {0};
    char *resolved = realpath(path, NULL);
    if (!resolved) { set_error(error, error_size, "Open folder"); return false; }
    if (strlen(resolved) >= sizeof(next.dir)) {
        free(resolved);
        errno = ENAMETOOLONG;
        set_error(error, error_size, "Open folder");
        return false;
    }
    strcpy(next.dir, resolved);
    free(resolved);
    DIR *dir = opendir(next.dir);
    if (!dir) { set_error(error, error_size, "Open folder"); return false; }
    bool ok = true;
    size_t capacity = 0;
    struct dirent *entry;
    for (;;) {
        errno = 0;
        entry = readdir(dir);
        if (!entry) { if (errno) ok = false; break; }
        if (strcmp(entry->d_name, ".") == 0) continue;
        if (strcmp(next.dir, "/") == 0 && strcmp(entry->d_name, "..") == 0) continue;
        char full[ROM_PATH_MAX];
        struct stat st;
        if (!join_path(full, sizeof(full), next.dir, entry->d_name)) { ok = false; break; }
        if (stat(full, &st) != 0) {
            fprintf(stderr, "Cannot inspect %s: %s\n", full, strerror(errno));
            continue;
        }
        bool is_dir = S_ISDIR(st.st_mode);
        if (!is_dir && (!S_ISREG(st.st_mode) || filter == BROWSER_DIRECTORIES ||
                        (filter == BROWSER_ROMS && !is_rom(entry->d_name)))) continue;
        if (next.count == capacity) {
            size_t new_capacity = capacity ? capacity * 2 : 16;
            RomBrowserEntry *items = realloc(next.entries, new_capacity * sizeof(*items));
            if (!items) { ok = false; break; }
            next.entries = items;
            capacity = new_capacity;
        }
        char *name = strdup(entry->d_name);
        if (!name) { ok = false; break; }
        next.entries[next.count++] = (RomBrowserEntry){name, is_dir};
    }
    int saved_errno = errno;
    if (closedir(dir) != 0 && ok) { ok = false; saved_errno = errno; }
    if (!ok) {
        errno = saved_errno;
        set_error(error, error_size, "Read folder");
        rom_browser_free(&next);
        return false;
    }
    if (next.count > 1) qsort(next.entries, next.count, sizeof(*next.entries), compare_entries);
    rom_browser_free(browser);
    *browser = next;
    return true;
}
