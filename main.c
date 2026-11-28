#define _POSIX_C_SOURCE 200809L
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

#include "tia.h"
#include "riot.h"
#include "cartridge.h"
#include "system2600.h"
#include "cpu6502.h"
#include "audio.h"
#include "rom_browser.h"
#include "system5200.h"
#include "antic5200.h"
#include "gtia5200.h"
#include "pokey5200.h"


typedef struct {
    TIAAudio audio;
    TIA tia;
    RIOT riot;
    Cartridge cart;
    System2600 system2600;
    CPU6502 cpu;
    bool loaded;
    VideoStandard standard;
    bool is_5200;
    bool warned_5200;
    System5200 system5200;
    Antic5200 antic5200;
    GTIA5200 gtia5200;
    Pokey5200 pokey5200;
    char load_error[256];
} Emulator;

typedef struct {
    bool show_help;
    bool show_rom_menu;
    bool selecting_folder;
    bool selecting_bios;
    char rom_dir[ROM_PATH_MAX];
    char bios_path[ROM_PATH_MAX];
    uint32_t bios_notice_until;
    RomBrowser browser;
    size_t selected_rom_idx;
    char browser_error[256];
    char debug_lines[8][128];
    int debug_line_count;
} OverlayState;

/* Minimale 5x7 bitmap glyphs (bovenste 5 bits per rij gebruikt) */
static const uint8_t FONT_5X7[128][7] = {
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    ['!'] = {0x04,0x04,0x04,0x04,0x04,0x00,0x04},
    ['"'] = {0x0A,0x0A,0x0A,0x00,0x00,0x00,0x00},
    ['#'] = {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00},
    ['%'] = {0x19,0x19,0x02,0x04,0x08,0x13,0x13},
    ['&'] = {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D},
    ['\''] = {0x04,0x04,0x08,0x00,0x00,0x00,0x00},
    ['('] = {0x02,0x04,0x08,0x08,0x08,0x04,0x02},
    [')'] = {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
    ['*'] = {0x00,0x0A,0x04,0x1F,0x04,0x0A,0x00},
    ['+'] = {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
    [','] = {0x00,0x00,0x00,0x00,0x00,0x0C,0x04},
    ['-'] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
    ['.'] = {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},
    ['?'] = {0x0E,0x11,0x01,0x02,0x04,0x00,0x04},
    ['/'] = {0x01,0x02,0x04,0x08,0x10,0x00,0x00},
    ['0'] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    ['1'] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    ['2'] = {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
    ['3'] = {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
    ['4'] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    ['5'] = {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    ['6'] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    ['7'] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    ['8'] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    ['9'] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    [':'] = {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00},
    ['='] = {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},
    ['>'] = {0x10,0x08,0x04,0x02,0x04,0x08,0x10},
    ['A'] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    ['B'] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    ['C'] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    ['D'] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    ['E'] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    ['F'] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    ['G'] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
    ['H'] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    ['I'] = {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    ['J'] = {0x07,0x02,0x02,0x02,0x12,0x12,0x0C},
    ['K'] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    ['L'] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    ['M'] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    ['N'] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    ['O'] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    ['P'] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    ['Q'] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    ['R'] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    ['S'] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    ['T'] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    ['U'] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    ['V'] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    ['W'] = {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
    ['X'] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    ['Y'] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    ['Z'] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    ['['] = {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E},
    [']'] = {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E},
    ['_'] = {0x00,0x00,0x00,0x00,0x00,0x00,0x1F},
    ['a'] = {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F},
    ['b'] = {0x10,0x10,0x1E,0x11,0x11,0x11,0x1E},
    ['c'] = {0x00,0x00,0x0E,0x11,0x10,0x11,0x0E},
    ['d'] = {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F},
    ['e'] = {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E},
    ['f'] = {0x06,0x08,0x1E,0x08,0x08,0x08,0x08},
    ['g'] = {0x00,0x00,0x0F,0x11,0x11,0x0F,0x01},
    ['h'] = {0x10,0x10,0x1E,0x11,0x11,0x11,0x11},
    ['i'] = {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E},
    ['j'] = {0x02,0x00,0x06,0x02,0x02,0x12,0x0C},
    ['k'] = {0x10,0x10,0x12,0x14,0x18,0x14,0x12},
    ['l'] = {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E},
    ['m'] = {0x00,0x00,0x1A,0x15,0x15,0x11,0x11},
    ['n'] = {0x00,0x00,0x1E,0x11,0x11,0x11,0x11},
    ['o'] = {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E},
    ['p'] = {0x00,0x00,0x1E,0x11,0x11,0x1E,0x10},
    ['q'] = {0x00,0x00,0x0F,0x11,0x11,0x0F,0x01},
    ['r'] = {0x00,0x00,0x16,0x19,0x10,0x10,0x10},
    ['s'] = {0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E},
    ['t'] = {0x08,0x08,0x1E,0x08,0x08,0x09,0x06},
    ['u'] = {0x00,0x00,0x11,0x11,0x11,0x13,0x0D},
    ['v'] = {0x00,0x00,0x11,0x11,0x11,0x0A,0x04},
    ['w'] = {0x00,0x00,0x11,0x11,0x15,0x15,0x0A},
    ['x'] = {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11},
    ['y'] = {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E},
    ['z'] = {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F},
};

static void draw_text(SDL_Renderer *renderer, int x, int y, const char *text,
                      SDL_Color color, int scale) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    int cursor_x = x;
    for (const char *p = text; *p; ++p) {
        unsigned char ch = (unsigned char)*p;
        if (ch >= 128) ch = '?';
        const uint8_t *glyph = FONT_5X7[ch];
        for (int row = 0; row < 7; ++row) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (1u << (4 - col))) {
                    SDL_Rect px = {
                        cursor_x + col * scale,
                        y + row * scale,
                        scale,
                        scale
                    };
                    SDL_RenderFillRect(renderer, &px);
                }
            }
        }
        cursor_x += (5 + 1) * scale;
    }
}

static void draw_overlay_box(SDL_Renderer *renderer, int x, int y, int w, int h, uint8_t alpha) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, alpha);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(renderer, &rect);
}


static void emu_destroy(Emulator *emu) {
    if (!emu) {
        return;
    }
    if (emu->loaded) {
        if (!emu->is_5200) {
            cart_free(&emu->cart);
            tia_destroy(&emu->tia);
        }
        emu->loaded = false;
    }
}

static VideoStandard video_standard_from_filename(const char *path) {
    const char *name = strrchr(path, '/');
#ifdef _WIN32
    const char *windows_name = strrchr(path, '\\');
    if (!name || (windows_name && windows_name > name)) name = windows_name;
#endif
    name = name ? name + 1 : path;
    for (const char *p = name; p[0] && p[1] && p[2]; p++) {
        if (tolower((unsigned char)p[0]) == 'p' &&
            tolower((unsigned char)p[1]) == 'a' &&
            tolower((unsigned char)p[2]) == 'l') return VIDEO_PAL;
    }
    return VIDEO_NTSC;
}

static void emu_set_video_standard(Emulator *emu, VideoStandard standard) {
    emu->standard = standard;
    tia_set_video_standard(&emu->tia, standard);
    tia_audio_set_video_standard(&emu->audio, standard);
    cart_set_video_standard(&emu->cart, standard);
}

typedef enum {
    ROM_SYSTEM_UNKNOWN,
    ROM_SYSTEM_2600,
    ROM_SYSTEM_5200,
    ROM_SYSTEM_ERROR
} RomSystem;

typedef enum {
    ROM_OPEN_OK,
    ROM_OPEN_CANCELLED,
    ROM_OPEN_ERROR
} RomOpenResult;

static RomSystem detect_rom_system(const uint8_t *data, size_t size) {
    bool size5200 = size == 0x1000 || size == 0x2000 ||
                    size == 0x4000 || size == 0x8000;
    if (size < 6) return ROM_SYSTEM_UNKNOWN;
    uint16_t start5200 = (uint16_t)(data[size - 2] | ((uint16_t)data[size - 1] << 8));
    uint16_t reset2600 = (uint16_t)(data[size - 4] | ((uint16_t)data[size - 3] << 8));
    bool possible5200 = size5200 && start5200 >= 0x4000 && start5200 < 0xc000;
    unsigned io_count = 0, io_chips = 0;
    /* A plausible vector alone is ambiguous; require references to multiple 5200 chips. */
    if (possible5200) {
        for (size_t i = 0; i + 2 < size; i++) {
            switch (data[i]) {
                case 0x8d: case 0x8e: case 0x8c: case 0xad: case 0xae:
                case 0xac: case 0x2c: case 0x9d: case 0x99: case 0xbd: case 0xb9:
                    break;
                default: continue;
            }
            uint16_t address = (uint16_t)(data[i + 1] | ((uint16_t)data[i + 2] << 8));
            unsigned chip = address >= 0xc000 && address <= 0xc01f ? 1u :
                            address >= 0xd400 && address <= 0xd40f ? 2u :
                            address >= 0xe800 && address <= 0xe80f ? 4u : 0u;
            if (chip) { io_count++; io_chips |= chip; }
        }
        if (io_count >= 4 && (io_chips & (io_chips - 1u)) != 0)
            return ROM_SYSTEM_5200;
        return ROM_SYSTEM_UNKNOWN;
    }
    if (size >= 0x800 && reset2600 != 0xffffu && (reset2600 & 0x1000u) != 0)
        return ROM_SYSTEM_2600;
    return ROM_SYSTEM_UNKNOWN;
}

static bool detect_rom_file(const char *path, RomSystem *system, char *error, size_t error_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(error, error_size, "Unable to open ROM");
        return false;
    }
    uint8_t data[0x8001];
    size_t size = fread(data, 1, sizeof(data), f);
    bool read_error = ferror(f) != 0;
    int close_result = fclose(f);
    if (read_error || close_result != 0) {
        snprintf(error, error_size, "Unable to read ROM");
        return false;
    }
    if (size == 0) {
        snprintf(error, error_size, "ROM is empty");
        return false;
    }
    *system = size == sizeof(data) ? ROM_SYSTEM_UNKNOWN : detect_rom_system(data, size);
    return true;
}

static RomSystem choose_rom_system(SDL_Window *window) {
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
        {0, 1, "Atari 2600"},
        {0, 2, "Atari 5200"}
    };
    const SDL_MessageBoxData dialog = {
        SDL_MESSAGEBOX_INFORMATION, window, "Choose system",
        "Cannot reliably identify this ROM. Which system is it for?",
        3, buttons, NULL
    };
    int selected = 0;
    if (SDL_ShowMessageBox(&dialog, &selected) < 0) {
        fprintf(stderr, "Unable to show system selection: %s\n", SDL_GetError());
        return ROM_SYSTEM_ERROR;
    }
    return selected == 1 ? ROM_SYSTEM_2600 :
           selected == 2 ? ROM_SYSTEM_5200 : ROM_SYSTEM_UNKNOWN;
}

static bool emu_load_5200(Emulator *emu, const char *path) {
    char bios_path[ROM_PATH_MAX];
    char error[256] = {0};
    RomConfigResult config = bios_config_load(bios_path, sizeof(bios_path), error, sizeof(error));
    if (config != ROM_CONFIG_OK) {
        snprintf(emu->load_error, sizeof(emu->load_error), "%s",
                 config == ROM_CONFIG_MISSING ? "Select a 5200 BIOS with F6 first" : error);
        fprintf(stderr, "%s\n", emu->load_error);
        return false;
    }
    System5200 candidate;
    system5200_init(&candidate);
    if (!system5200_load_bios_file(&candidate, bios_path, error, sizeof(error))) {
        snprintf(emu->load_error, sizeof(emu->load_error), "5200 BIOS: %.230s", error);
        fprintf(stderr, "%s\n", emu->load_error);
        return false;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(emu->load_error, sizeof(emu->load_error), "Unable to open 5200 ROM");
        fprintf(stderr, "%s: %s\n", emu->load_error, path);
        return false;
    }
    uint8_t bytes[0x8001];
    size_t size = fread(bytes, 1, sizeof(bytes), f);
    bool read_error = ferror(f) != 0;
    int close_result = fclose(f);
    if (read_error || close_result != 0) {
        snprintf(emu->load_error, sizeof(emu->load_error), "Unable to read 5200 ROM");
        fprintf(stderr, "%s\n", emu->load_error);
        return false;
    }
    System5200CartType type;
    switch (size) {
        case 0x1000: type = SYSTEM5200_CART_4K; break;
        case 0x2000: type = SYSTEM5200_CART_8K; break;
        case 0x4000: {
            uint16_t entry = (uint16_t)(bytes[0x3ffe] | ((uint16_t)bytes[0x3fff] << 8));
            type = entry >= 0x4000 && entry < 0x8000
                ? SYSTEM5200_CART_16K_EE : SYSTEM5200_CART_16K;
            break;
        }
        case 0x8000: type = SYSTEM5200_CART_32K; break;
        default:
            snprintf(emu->load_error, sizeof(emu->load_error), "Unsupported 5200 ROM size");
            fprintf(stderr, "%s: %zu bytes\n", emu->load_error, size);
            return false;
    }
    if (!system5200_load_cartridge(&candidate, type, bytes, size)) {
        snprintf(emu->load_error, sizeof(emu->load_error), "Unable to load 5200 cartridge");
        return false;
    }
    emu_destroy(emu);
    emu->system5200 = candidate;
    antic5200_init(&emu->antic5200);
    gtia5200_init(&emu->gtia5200);
    pokey5200_init(&emu->pokey5200);
    pokey5200_capture_audio(&emu->pokey5200, true);
    pokey5200_set_pot(&emu->pokey5200, 0, 114);
    pokey5200_set_pot(&emu->pokey5200, 1, 114);
    system5200_attach_antic(&emu->system5200, &emu->antic5200, &emu->cpu);
    system5200_attach_gtia(&emu->system5200, &emu->gtia5200);
    system5200_attach_pokey(&emu->system5200, &emu->pokey5200);
    cpu_init_with_bus(&emu->cpu, system5200_cpu_bus(&emu->system5200));
    tia_set_video_standard(&emu->tia, VIDEO_NTSC);
    emu->standard = VIDEO_NTSC;
    emu->is_5200 = true;
    emu->warned_5200 = false;
    emu->loaded = true;
    if (size == 0x4000)
        fprintf(stderr, "5200: using %s 16K layout (selected from entry address)\n",
                type == SYSTEM5200_CART_16K_EE ? "two-chip" : "standard");
    printf("Loaded experimental 5200 ROM: %s\n", path);
    return true;
}

static bool emu_load_rom(Emulator *emu, const char *path, RomSystem system) {
    emu->load_error[0] = '\0';
    if (system == ROM_SYSTEM_5200) return emu_load_5200(emu, path);
    if (system != ROM_SYSTEM_2600) {
        snprintf(emu->load_error, sizeof(emu->load_error), "ROM system not selected");
        fprintf(stderr, "%s\n", emu->load_error);
        return false;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Unable to open rom: %s\n", path);
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long sz = ftell(f);
    if (sz <= 0) {
        fclose(f);
        return false;
    }
    rewind(f);

    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t read_n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (read_n != (size_t)sz) {
        free(buf);
        return false;
    }

    emu_destroy(emu);
    emu->is_5200 = false;

    const char *base = strrchr(path, '/');
#ifdef _WIN32
    const char *base_win = strrchr(path, '\\');
    if (!base || (base_win && base_win > base)) {
        base = base_win;
    }
#endif
    base = base ? (base + 1) : path;

    if (!cart_init_from_bytes(&emu->cart, buf, (size_t)sz, base)) {
        free(buf);
        return false;
    }
    free(buf);

    tia_audio_init(&emu->audio);
    tia_init(&emu->tia, &emu->audio);
    emu_set_video_standard(emu, video_standard_from_filename(base));
    riot_init(&emu->riot);
    system2600_init(&emu->system2600, &emu->tia, &emu->riot, &emu->cart, tia_step, tia_read, tia_write);
    cpu_init_with_bus(&emu->cpu, system2600_cpu_bus(&emu->system2600));
    emu->loaded = true;

    printf("Geladen: %s [%s]\n", emu->cart.name,
           emu->standard == VIDEO_PAL ? "PAL" : "NTSC");
    return true;
}

static RomOpenResult emu_open_rom(Emulator *emu, const char *path, SDL_Window *window,
                                 RomSystem (*choose)(SDL_Window *)) {
    RomSystem system;
    emu->load_error[0] = '\0';
    if (!detect_rom_file(path, &system, emu->load_error, sizeof(emu->load_error))) {
        fprintf(stderr, "%s: %s\n", emu->load_error, path);
        return ROM_OPEN_ERROR;
    }
    if (system == ROM_SYSTEM_UNKNOWN) system = choose(window);
    if (system == ROM_SYSTEM_ERROR) {
        snprintf(emu->load_error, sizeof(emu->load_error), "Unable to show system selection");
        return ROM_OPEN_ERROR;
    }
    if (system == ROM_SYSTEM_UNKNOWN) return ROM_OPEN_CANCELLED;
    return emu_load_rom(emu, path, system) ? ROM_OPEN_OK : ROM_OPEN_ERROR;
}

static bool emu_step_5200_frame(Emulator *emu) {
    uint64_t deadline = emu->system5200.elapsed_cycles + 114u * 263u;
    bool reached_end = false;
    while (emu->system5200.elapsed_cycles < deadline) {
        cpu_step(&emu->cpu);
        if (emu->antic5200.unsupported) {
            fprintf(stderr, "5200: unsupported ANTIC operation; emulation paused\n");
            emu->warned_5200 = true;
            return true;
        }
        if (emu->antic5200.scanline >= 248) reached_end = true;
        if (reached_end && emu->antic5200.scanline == 0) return true;
    }
    fprintf(stderr, "5200 video frame did not finish\n");
    return false;
}

static bool overlay_open_filtered(OverlayState *ov, const char *path, BrowserFilter filter) {
    ov->show_help = false;
    ov->show_rom_menu = true;
    ov->browser_error[0] = '\0';
    if (!rom_browser_open_filtered(&ov->browser, path, filter, ov->browser_error, sizeof(ov->browser_error))) {
        fprintf(stderr, "%s\n", ov->browser_error);
        return false;
    }
    ov->selecting_folder = filter == BROWSER_DIRECTORIES;
    ov->selecting_bios = filter == BROWSER_ALL_FILES;
    ov->selected_rom_idx = 0;
    return true;
}

static bool overlay_open_browser(OverlayState *ov, const char *path, bool selecting_folder) {
    return overlay_open_filtered(ov, path, selecting_folder ? BROWSER_DIRECTORIES : BROWSER_ROMS);
}

static bool overlay_select_bios(OverlayState *ov, const char *path) {
    System5200 system;
    system5200_init(&system);
    if (!system5200_load_bios_file(&system, path, ov->browser_error, sizeof(ov->browser_error)) ||
        !bios_config_save(path, ov->browser_error, sizeof(ov->browser_error))) {
        fprintf(stderr, "%s\n", ov->browser_error);
        return false;
    }
    snprintf(ov->bios_path, sizeof(ov->bios_path), "%s", path);
    ov->show_rom_menu = false;
    ov->bios_notice_until = SDL_GetTicks() + 5000;
    return true;
}

static bool overlay_browser_key(OverlayState *ov, SDL_Keycode key, char *rom_path, size_t path_size) {
    RomBrowser *browser = &ov->browser;
    if (key == SDLK_ESCAPE || key == SDLK_F2) {
        ov->show_rom_menu = false;
    } else if (key == SDLK_UP && browser->count) {
        ov->selected_rom_idx = ov->selected_rom_idx ? ov->selected_rom_idx - 1 : browser->count - 1;
    } else if (key == SDLK_DOWN && browser->count) {
        ov->selected_rom_idx = (ov->selected_rom_idx + 1) % browser->count;
    } else if (key == SDLK_s && ov->selecting_folder && browser->dir[0]) {
        if (rom_config_save(browser->dir, ov->browser_error, sizeof(ov->browser_error))) {
            strcpy(ov->rom_dir, browser->dir);
            overlay_open_browser(ov, ov->rom_dir, false);
        } else {
            fprintf(stderr, "%s\n", ov->browser_error);
        }
    } else if (key == SDLK_BACKSPACE || ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && browser->count)) {
        char path[ROM_PATH_MAX];
        bool parent = key == SDLK_BACKSPACE;
        if (parent) {
            int n = snprintf(path, sizeof(path), "%s/..", browser->dir);
            if (n < 0 || (size_t)n >= sizeof(path)) {
                snprintf(ov->browser_error, sizeof(ov->browser_error), "Path is too long");
                return false;
            }
        } else if (!rom_browser_path(browser, ov->selected_rom_idx, path, sizeof(path))) {
            snprintf(ov->browser_error, sizeof(ov->browser_error), "Path is too long");
            return false;
        }
        if (parent || browser->entries[ov->selected_rom_idx].is_dir) {
            BrowserFilter filter = ov->selecting_folder ? BROWSER_DIRECTORIES :
                                   ov->selecting_bios ? BROWSER_ALL_FILES : BROWSER_ROMS;
            overlay_open_filtered(ov, path, filter);
        } else if (snprintf(rom_path, path_size, "%s", path) >= (int)path_size) {
            snprintf(ov->browser_error, sizeof(ov->browser_error), "Path is too long");
        } else {
            return true;
        }
    }
    return false;
}

static void overlay_draw_fps(SDL_Renderer *renderer, double fps, int win_w, VideoStandard standard) {
    SDL_Color green = { 0, 255, 120, 255 };
    char buf[160];
    snprintf(buf, sizeof(buf), "%s FPS: %.1f", standard == VIDEO_PAL ? "PAL" : "NTSC", fps);
    draw_text(renderer, win_w - 220, 25, buf, green, 2);
}

static void overlay_draw_help(OverlayState *ov, SDL_Renderer *renderer, int win_w, bool is_5200) {
    SDL_Color green = { 0, 255, 120, 255 };

    draw_overlay_box(renderer, 15, 15, win_w - 30, is_5200 ? 145 : 95, 225);
    draw_text(renderer, 25, 25, "F1: Help F2: ROMs F3: Frame limit F4: PAL/NTSC", green, 2);
    draw_text(renderer, 25, 50, "F5: ROM folder F6: 5200 BIOS", green, 2);
    draw_text(renderer, 25, 75, is_5200 ? "WASD/ARROWS: Move  CTRL: Fire  SPACE: Top fire" :
              "ASWD: Movement CTRL/SHIFT: Fire/Reset", green, 2);
    if (is_5200) {
        draw_text(renderer, 25, 100, "ENTER: Start  P: Pause  R: Reset", green, 2);
        draw_text(renderer, 25, 125, "0-9, NUMPAD */: Keypad *#", green, 2);
    }



    for (int i = 0; i < ov->debug_line_count && i < 8; ++i) {
        draw_text(renderer, 25, 75 + i * 20, ov->debug_lines[i], green, 2);
    }
}

static void overlay_draw_rom_menu(OverlayState *ov, SDL_Renderer *renderer, int win_w, int win_h) {
    SDL_Color green = { 0, 255, 120, 255 };
    SDL_Color yellow = { 255, 255, 100, 255 };
    SDL_Color warn = { 255, 200, 0, 255 };
    draw_overlay_box(renderer, 15, 15, win_w - 30, win_h - 30, 225);
    draw_text(renderer, 25, 25, ov->selecting_folder ? "Select ROM folder" :
              ov->selecting_bios ? "Select 5200 BIOS file" : "Select ROM", green, 2);
    size_t visible = win_w > 50 ? (size_t)(win_w - 50) / 12 : 1;
    const char *dir = ov->browser.dir;
    size_t len = strlen(dir);
    if (len > visible) dir += len - visible;
    draw_text(renderer, 25, 48, dir, green, 2);
    draw_text(renderer, 25, win_h - 80,
              ov->selecting_folder ? "ENTER: open  S: select folder  BACKSPACE: up" :
                                     "ENTER: open  BACKSPACE: up  F5: ROM folder", green, 2);
    draw_text(renderer, 25, win_h - 58, "ESC: close", green, 2);
    if (ov->browser_error[0]) draw_text(renderer, 25, win_h - 36, ov->browser_error, warn, 1);
    if (!ov->browser.count) {
        draw_text(renderer, 25, 80, "No entries", warn, 2);
        return;
    }
    int max_lines = (win_h - 170) / 22;
    if (max_lines < 1) max_lines = 1;
    int start = (int)ov->selected_rom_idx - max_lines / 2;
    if (start < 0) start = 0;
    if ((size_t)(start + max_lines) > ov->browser.count) start = (int)ov->browser.count - max_lines;
    if (start < 0) start = 0;

    for (int i = 0; i < max_lines && (size_t)(start + i) < ov->browser.count; ++i) {
        size_t idx = (size_t)(start + i);
        bool selected = (idx == ov->selected_rom_idx);
        char line[ROM_PATH_MAX + 8];
        snprintf(line, sizeof(line), "%s%s%s", selected ? "> " : "  ",
                 ov->browser.entries[idx].is_dir ? "/ " : "  ", ov->browser.entries[idx].name);
        if (visible < strlen(line)) line[visible] = '\0';
        draw_text(renderer, 25, 80 + i * 22, line, selected ? yellow : green, 2);
    }
}

static void overlay_render(OverlayState *ov, SDL_Renderer *renderer, int win_w, int win_h, double fps, VideoStandard standard, bool is_5200) {
    if (ov->show_rom_menu) {
        overlay_draw_rom_menu(ov, renderer, win_w, win_h);
    } else if (ov->show_help) {
        overlay_draw_help(ov, renderer, win_w, is_5200);
    }

    if (ov->show_help == false && ov->show_rom_menu == false) {
        overlay_draw_fps(renderer, fps, win_w, standard);
    }
    if (ov->bios_notice_until && (int32_t)(ov->bios_notice_until - SDL_GetTicks()) > 0) {
        SDL_Color green = { 0, 255, 120, 255 };
        draw_overlay_box(renderer, 15, 85, 400, 35, 225);
        draw_text(renderer, 25, 95, "5200 BIOS saved", green, 2);
    }
}

static void handle_5200_input(Emulator *emu, const uint8_t *keys, bool active) {
    bool left = active && (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]);
    bool right = active && (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]);
    bool up = active && (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]);
    bool down = active && (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]);
    unsigned x = left == right ? 114 : left ? 6 : 220;
    unsigned y = up == down ? 114 : up ? 6 : 220;
    pokey5200_set_pot(&emu->pokey5200, 0, x);
    pokey5200_set_pot(&emu->pokey5200, 1, y);
    gtia5200_set_trigger(&emu->gtia5200, 0, active &&
                         (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]));
    static const struct { SDL_Scancode key; uint8_t code; } keypad[] = {
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
    uint16_t buttons = 0;
    if (active) {
        for (unsigned i = 0; i < sizeof(keypad) / sizeof(keypad[0]); i++)
            if (keys[keypad[i].key]) buttons |= (uint16_t)(1u << keypad[i].code);
        if (keys[SDL_SCANCODE_8] && (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]))
            buttons = (uint16_t)((buttons & ~(1u << 6)) | (1u << 3));
        if (keys[SDL_SCANCODE_3] && (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]))
            buttons = (uint16_t)((buttons & ~(1u << 13)) | (1u << 1));
    }
    pokey5200_set_controller(&emu->pokey5200, 0, buttons, active && keys[SDL_SCANCODE_SPACE]);
}


static void handle_input(Emulator *emu, bool active) {
    if (!emu || !emu->loaded) {
        return;
    }

    if (!active) {
        emu->riot.swcha = 0xFFu;
        emu->riot.swchb = 0x0Bu;
        emu->tia.inpt4 = 0x80u;
        return;
    }
    const uint8_t *keys = SDL_GetKeyboardState(NULL);

    uint8_t swcha = 0xFFu;
    if (keys[SDL_SCANCODE_W]) swcha &= (uint8_t)~0x10u; /* Omhoog */
    if (keys[SDL_SCANCODE_S]) swcha &= (uint8_t)~0x20u; /* Omlaag */
    if (keys[SDL_SCANCODE_A]) swcha &= (uint8_t)~0x40u; /* Links */
    if (keys[SDL_SCANCODE_D]) swcha &= (uint8_t)~0x80u; /* Rechts */
    emu->riot.swcha = swcha;

    if (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]) {
        emu->tia.inpt4 = 0x00u;
    } else {
        emu->tia.inpt4 = 0x80u;
    }

    uint8_t swchb = 0x0Bu;
    if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
        swchb &= (uint8_t)~0x01u; /* Reset */
    }
    emu->riot.swchb = swchb;
}

typedef struct {
    double deadline;
} FramePacer;

static void frame_pacer_wait(FramePacer *pacer, uint64_t frame_start,
                             uint64_t frequency, double fps) {
    double period = (double)frequency / fps;
    if (pacer->deadline == 0.0 || (double)frame_start > pacer->deadline + period) {
        pacer->deadline = (double)frame_start + period;
    } else {
        pacer->deadline += period;
    }

    uint64_t now = SDL_GetPerformanceCounter();
    while ((double)now < pacer->deadline) {
        double remaining_ms = (pacer->deadline - (double)now) * 1000.0 / (double)frequency;
        uint32_t delay_ms = (uint32_t)remaining_ms;
        SDL_Delay(delay_ms ? delay_ms : 1);
        now = SDL_GetPerformanceCounter();
    }
}

int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Init fout: %s\n", SDL_GetError());
        return 1;
    }

    int scale = 6;
    int win_w = (TIA_FB_HEIGHT / 3 * 4) * scale;
    int win_h = TIA_FB_HEIGHT * scale;

    SDL_Window *window = SDL_CreateWindow(
        "6502core (Press F1 to open the menu)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        win_w,
        win_h,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow fout: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED
    );
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer fout: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        TIA_FB_WIDTH,
        TIA_FB_HEIGHT
    );
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture fout: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_Texture *texture5200 = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
                                                SDL_TEXTUREACCESS_STREAMING,
                                                GTIA5200_WIDTH, GTIA5200_HEIGHT);
    if (!texture5200) {
        fprintf(stderr, "SDL 5200 texture failed: %s\n", SDL_GetError());
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    int texture_height = TIA_FB_HEIGHT;
    int display_height = TIA_FB_HEIGHT;

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = (int)TIA_OUT_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    static double current_fps = 60.0;
    uint32_t max_queued_bytes  = 0;
    if (audio_dev > 0) {
        SDL_PauseAudioDevice(audio_dev, 0);
    } else {
        fprintf(stderr, "Audio device unavailable: %s\n", SDL_GetError());
    }

    Emulator emu;
    memset(&emu, 0, sizeof(emu));


  
    int16_t audio_tmp[4096];
    bool running = true;
    const uint64_t perf_freq = SDL_GetPerformanceFrequency();
    OverlayState overlay = {0};
    bool uncapped = false;
    FramePacer pacer = {0};

    if (argc == 2) {
        if (emu_open_rom(&emu, argv[1], window, choose_rom_system) == ROM_OPEN_ERROR)
            fprintf(stderr, "Unable to load ROM: %s\n", argv[1]);
    }
    char config_error[256] = {0};
    RomConfigResult config_result = rom_config_load(overlay.rom_dir, sizeof(overlay.rom_dir),
                                                    config_error, sizeof(config_error));
    if (config_result != ROM_CONFIG_OK) {
        overlay.rom_dir[0] = '\0';
        const char *home = getenv("HOME");
        if (!home || home[0] != '/') home = ".";
        if (!overlay_open_browser(&overlay, home, true)) {
            overlay_open_browser(&overlay, "/", true);
        }
        if (config_result == ROM_CONFIG_ERROR) {
            fprintf(stderr, "%s\n", config_error);
            snprintf(overlay.browser_error, sizeof(overlay.browser_error), "%s", config_error);
        }
    }
    config_error[0] = '\0';
    RomConfigResult bios_result = bios_config_load(overlay.bios_path, sizeof(overlay.bios_path),
                                                    config_error, sizeof(config_error));
    if (bios_result == ROM_CONFIG_ERROR) {
        fprintf(stderr, "5200 BIOS config: %s\n", config_error);
        overlay.bios_path[0] = '\0';
    }
    
    while (running) {
        uint64_t frame_start = SDL_GetPerformanceCounter();
        SDL_Event ev;

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN) {
                SDL_Keycode key = ev.key.keysym.sym;

                if (key == SDLK_F4 && !ev.key.repeat && emu.loaded && !emu.is_5200) {
                    emu_set_video_standard(&emu, emu.standard == VIDEO_NTSC ? VIDEO_PAL : VIDEO_NTSC);
                    pacer.deadline = 0.0;
                    if (audio_dev > 0) SDL_ClearQueuedAudio(audio_dev);
                    printf("Video: %s\n", emu.standard == VIDEO_PAL ? "PAL" : "NTSC");
                }

                if (key == SDLK_F3 && uncapped == false) {
                    uncapped = true;
                } else if (key == SDLK_F3 && uncapped == true) {
                    uncapped = false;
                }
                if (key == SDLK_F3) pacer.deadline = 0.0;
                if (key == SDLK_F6 && !ev.key.repeat) {
                    char dir[ROM_PATH_MAX];
                    snprintf(dir, sizeof(dir), "%s", overlay.bios_path);
                    char *slash = strrchr(dir, '/');
                    if (slash) {
                        if (slash == dir) slash[1] = '\0'; else *slash = '\0';
                    } else {
                        snprintf(dir, sizeof(dir), "%s", overlay.rom_dir[0] ? overlay.rom_dir : "/");
                    }
                    if (!overlay_open_filtered(&overlay, dir, BROWSER_ALL_FILES) && strcmp(dir, "/") != 0)
                        overlay_open_filtered(&overlay, "/", BROWSER_ALL_FILES);
                } else if (key == SDLK_F5 && !ev.key.repeat) {
                    const char *dir = overlay.rom_dir[0] ? overlay.rom_dir : overlay.browser.dir;
                    if (!dir[0]) dir = "/";
                    overlay_open_browser(&overlay, dir, true);
                } else if (overlay.show_rom_menu) {
                    char rom_path[ROM_PATH_MAX];
                    if (overlay_browser_key(&overlay, key, rom_path, sizeof(rom_path))) {
                        if (overlay.selecting_bios) {
                            overlay_select_bios(&overlay, rom_path);
                        } else {
                            RomOpenResult result = emu_open_rom(&emu, rom_path, window, choose_rom_system);
                            if (result == ROM_OPEN_OK) {
                                pacer.deadline = 0.0;
                                if (audio_dev > 0) SDL_ClearQueuedAudio(audio_dev);
                                overlay.show_rom_menu = false;
                            } else if (result == ROM_OPEN_ERROR) {
                                snprintf(overlay.browser_error, sizeof(overlay.browser_error), "%s",
                                         emu.load_error[0] ? emu.load_error : "Unable to load ROM");
                                fprintf(stderr, "Unable to load ROM: %s\n", rom_path);
                            }
                        }
                    }
                } else {
                    
                    if (key == SDLK_ESCAPE) {
                        running = false;
                    } 
                    
                    else if (key == SDLK_F1) {
                        overlay.show_help = !overlay.show_help;
                        if (overlay.show_help) {
                            overlay.show_rom_menu = false;
                        } 
                    
                    }
                    else if (key == SDLK_F2) {
                        const char *dir = overlay.rom_dir[0] ? overlay.rom_dir : "/";
                        overlay_open_browser(&overlay, dir, false);
                    }

                }
                  
            
            }
        }


        if (emu.loaded && emu.is_5200) {
            handle_5200_input(&emu, SDL_GetKeyboardState(NULL),
                              !overlay.show_rom_menu && !overlay.show_help &&
                              (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS));
            if (!emu.warned_5200 && !emu_step_5200_frame(&emu)) {
                emu_destroy(&emu);
                continue;
            }
            uint8_t rgb[GTIA5200_HEIGHT][GTIA5200_WIDTH][3];
            for (unsigned y = 0; y < GTIA5200_HEIGHT; y++)
                for (unsigned x = 0; x < GTIA5200_WIDTH; x++)
                    memcpy(rgb[y][x], emu.tia.color_lut[emu.gtia5200.framebuffer[y][x]], 3);
            if (SDL_UpdateTexture(texture5200, NULL, rgb, GTIA5200_WIDTH * 3) != 0)
                fprintf(stderr, "5200 texture update failed: %s\n", SDL_GetError());
        } else if (emu.loaded) {
            handle_input(&emu, !overlay.show_rom_menu);
            if (!system2600_step_frame(&emu.system2600, &emu.cpu)) {
                fprintf(stderr, "Frame synchronization failed for %s (PC=%04X, scanline=%d)\n",
                        emu.cart.name, emu.cpu.PC, emu.tia.scanline);
                emu_destroy(&emu);
                rom_browser_free(&overlay.browser);
                if (audio_dev > 0) SDL_ClearQueuedAudio(audio_dev);
                continue;
            }

            int completed_height = emu.tia.completed_frame_height;

            if (completed_height > 0) {
                if (completed_height > texture_height) {
                    SDL_Texture *larger = SDL_CreateTexture(
                        renderer,
                        SDL_PIXELFORMAT_RGB24,
                        SDL_TEXTUREACCESS_STREAMING,
                        TIA_FB_WIDTH,
                        completed_height
                    );

                    if (larger) {
                        SDL_DestroyTexture(texture);
                        texture = larger;
                        texture_height = completed_height;
                    } else {
                        fprintf(stderr, "SDL texture vergroten mislukt: %s\n", SDL_GetError());
                    }
                }

                if (completed_height <= texture_height) {
                    SDL_Rect update_rect = {0, 0, TIA_FB_WIDTH, completed_height};
                    SDL_UpdateTexture(texture, &update_rect, emu.tia.framebuffer,
                                    TIA_FB_WIDTH * 3);
                    display_height = completed_height;
                }
            }

        } else {
            /* Leeg/donker scherm zolang er nog geen ROM actief is */
            uint8_t blank[TIA_FB_SIZE];
            memset(blank, 0x18, sizeof(blank));
            SDL_UpdateTexture(texture, NULL, blank, TIA_FB_WIDTH * 3);
        }
       
      

        if (emu.loaded && (emu.is_5200 || audio_dev > 0)) {
            size_t n = emu.is_5200 ?
                pokey5200_take_samples(&emu.pokey5200, audio_tmp, 4096) :
                tia_audio_take_samples(&emu.audio, audio_tmp, 4096);
            if (audio_dev > 0 && n > 0) {
                max_queued_bytes = (uint32_t)(((double)have.freq /
                    video_frame_rate(emu.standard)) * 3.0 * sizeof(int16_t) * have.channels);
                uint32_t bytes = (uint32_t)(n * sizeof(int16_t));
                uint32_t queued = SDL_GetQueuedAudioSize(audio_dev);
                if (queued > max_queued_bytes) {
                    SDL_ClearQueuedAudio(audio_dev);
                    queued = 0;
                }
                if (queued + bytes <= max_queued_bytes &&
                    SDL_QueueAudio(audio_dev, audio_tmp, bytes) != 0)
                    fprintf(stderr, "Audio queue failed: %s\n", SDL_GetError());
            }
        }
        SDL_GetRendererOutputSize(renderer, &win_w, &win_h);

        int src_height = display_height;
        if (src_height > texture_height) {
            src_height = texture_height;
        }

        SDL_Rect src_rect = emu.loaded && emu.is_5200 ?
            (SDL_Rect){0, 0, GTIA5200_WIDTH, GTIA5200_HEIGHT} :
            (SDL_Rect){0, 0, TIA_FB_WIDTH, src_height};
        SDL_Rect dst_rect = {0, 0, win_w, win_h};

        SDL_RenderClear(renderer);
        if (src_height > 0 &&
            SDL_RenderCopy(renderer, emu.loaded && emu.is_5200 ? texture5200 : texture, &src_rect, &dst_rect) < 0) {
            fprintf(stderr, "SDL_RenderCopy fout: %s\n", SDL_GetError());
        }

        overlay_render(&overlay, renderer, win_w, win_h, current_fps, emu.standard, emu.loaded && emu.is_5200);
        if (emu.loaded && emu.is_5200 && !overlay.show_rom_menu && !overlay.show_help) {
            SDL_Color yellow = {255, 255, 100, 255};
            draw_text(renderer, 20, 20, emu.warned_5200 ?
                      "5200 PAUSED - VIDEO NOT SUPPORTED" : "5200 SUPPORT IS STILL EXPERIMENTAL",
                      yellow, 1);
        }
        SDL_RenderPresent(renderer);
        if (!uncapped) {
            frame_pacer_wait(&pacer, frame_start, perf_freq, video_frame_rate(emu.standard));
        }  

        uint64_t now = SDL_GetPerformanceCounter();
        double frame_dt = (double)(now - frame_start) / (double)perf_freq;
        static double accum_time = 0.0;
        static int accum_frames = 0;

        accum_time += frame_dt;
        accum_frames++;
        if (accum_time >= 0.5) {
            current_fps = (double)accum_frames / accum_time;
            accum_time = 0.0;
            accum_frames = 0;
        }  

    }

    if (audio_dev > 0) {
        SDL_CloseAudioDevice(audio_dev);
    }
    emu_destroy(&emu);
    SDL_DestroyTexture(texture5200);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}