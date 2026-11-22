#define _POSIX_C_SOURCE 200809L
#include <SDL2/SDL.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "tia.h"
#include "riot.h"
#include "cartridge.h"
#include "bus.h"
#include "cpu6507.h"
#include "audio.h"

#define ROM_DIR     "roms"
#define MAX_ROMS    128
#define MAX_PATH_SZ 512

typedef struct {
    TIAAudio audio;
    TIA tia;
    RIOT riot;
    Cartridge cart;
    Bus bus;
    CPU6507 cpu;
    bool loaded;
} Emulator;

typedef struct {
    bool show_help;
    bool show_rom_menu;
    char rom_dir[256];
    char **roms;
    int rom_count;
    int selected_rom_idx;
    char debug_lines[8][128];
    int debug_line_count;
} OverlayState;


/* Minimale 5x7 bitmap glyphs (bovenste 5 bits per rij gebruikt) */
static const uint8_t FONT_5X7[128][7] = {
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    ['!'] = {0x04,0x04,0x04,0x04,0x04,0x00,0x04},
    ['"'] = {0x0A,0x0A,0x0A,0x00,0x00,0x00,0x00},
    ['('] = {0x02,0x04,0x08,0x08,0x08,0x04,0x02},
    [')'] = {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
    ['*'] = {0x00,0x0A,0x04,0x1F,0x04,0x0A,0x00},
    ['+'] = {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
    [','] = {0x00,0x00,0x00,0x00,0x00,0x0C,0x04},
    ['-'] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
    ['.'] = {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},
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
        cart_free(&emu->cart);
        emu->loaded = false;
    }
}

static bool emu_load_rom(Emulator *emu, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Kan ROM niet openen: %s\n", path);
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
    riot_init(&emu->riot);
    bus_init(&emu->bus, &emu->tia, &emu->riot, &emu->cart, tia_step, tia_read, tia_write);
    cpu_init(&emu->cpu, &emu->bus);
    emu->loaded = true;

    printf("Geladen: %s\n", emu->cart.name);
    return true;
}

static bool has_rom_ext(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return (strcmp(dot, ".bin") == 0 || strcmp(dot, ".BIN") == 0 ||
            strcmp(dot, ".a26") == 0 || strcmp(dot, ".A26") == 0 ||
            strcmp(dot, ".rom") == 0 || strcmp(dot, ".ROM") == 0);
}

static int cmp_str(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

static void overlay_scan_roms(OverlayState *ov) {
    for (int i = 0; i < ov->rom_count; ++i) free(ov->roms[i]);
    free(ov->roms);
    ov->roms = NULL;
    ov->rom_count = 0;
    ov->selected_rom_idx = 0;

    DIR *dir = opendir(ov->rom_dir);
    if (!dir) return;

    struct dirent *ent;
    int cap = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (!has_rom_ext(ent->d_name)) continue;

        if (ov->rom_count >= cap) {
            cap = cap == 0 ? 16 : cap * 2;
            char **tmp = realloc(ov->roms, (size_t)cap * sizeof(char *));
            if (!tmp) break;
            ov->roms = tmp;
        }
        ov->roms[ov->rom_count++] = strdup(ent->d_name);
    }
    closedir(dir);

    if (ov->rom_count > 1) {
        qsort(ov->roms, (size_t)ov->rom_count, sizeof(char *), cmp_str);
    }
}

static void overlay_draw_help(OverlayState *ov, SDL_Renderer *renderer, double fps) {
    SDL_Color green = { 0, 255, 120, 255 };
    char buf[160];

    draw_overlay_box(renderer, 15, 15, 510, 200, 200);
    draw_text(renderer, 25, 25, "F1: Help | F2: ROMs | ESC: Afsluiten", green, 2);
    draw_text(renderer, 25, 55, "ASWD: Bewegen CTRL/SHIFT: action buttons", green, 2);

    snprintf(buf, sizeof(buf), "FPS: %.1f", fps);
    draw_text(renderer, 25, 85, buf, green, 2);

    for (int i = 0; i < ov->debug_line_count && i < 8; ++i) {
        draw_text(renderer, 25, 75 + i * 20, ov->debug_lines[i], green, 2);
    }
}

static void overlay_draw_rom_menu(OverlayState *ov, SDL_Renderer *renderer, int win_w, int win_h) {
    SDL_Color green = { 0, 255, 120, 255 };
    SDL_Color yellow = { 255, 255, 100, 255 };
    SDL_Color warn = { 255, 200, 0, 255 };
    char header[256];

    draw_overlay_box(renderer, 15, 15, win_w - 30, win_h - 30, 225);
    snprintf(header, sizeof(header), "Selecteer ROM uit ./%s/ (Enter = start, F2 = sluit)", ov->rom_dir);
    draw_text(renderer, 25, 25, header, green, 2);

    if (ov->rom_count == 0) {
        draw_text(renderer, 25, 65, "Geen .bin / .a26 / .rom bestanden in ./roms/", warn, 2);
        return;
    }

    int max_lines = (win_h - 95) / 22;
    if (max_lines < 1) max_lines = 1;
    int start = ov->selected_rom_idx - max_lines / 2;
    if (start < 0) start = 0;
    if (start > ov->rom_count - max_lines) start = ov->rom_count - max_lines;
    if (start < 0) start = 0;

    for (int i = 0; i < max_lines && (start + i) < ov->rom_count; ++i) {
        int idx = start + i;
        bool selected = (idx == ov->selected_rom_idx);
        char line[300];
        snprintf(line, sizeof(line), "%s%s", selected ? "> " : "  ", ov->roms[idx]);
        draw_text(renderer, 25, 60 + i * 22, line, selected ? yellow : green, 2);
    }
}

static void overlay_render(OverlayState *ov, SDL_Renderer *renderer, int win_w, int win_h, double fps) {
    if (ov->show_rom_menu) {
        overlay_draw_rom_menu(ov, renderer, win_w, win_h);
    } else if (ov->show_help) {
        overlay_draw_help(ov, renderer, fps);
    }
}


static void handle_input(Emulator *emu) {
    if (!emu || !emu->loaded) {
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

static void ensure_rom_folder(void) {
#ifdef _WIN32
    _mkdir(ROM_DIR);
#else
    mkdir(ROM_DIR, 0755);
#endif
}

int main(int argc, char **argv) {
    ensure_rom_folder();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Init fout: %s\n", SDL_GetError());
        return 1;
    }

    int scale = 6;
    int win_w = TIA_FB_WIDTH * scale;
    int win_h = TIA_FB_HEIGHT * scale;

    SDL_Window *window = SDL_CreateWindow(
        "Atari 2600 Emulator",
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
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
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

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = (int)TIA_OUT_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_dev > 0) {
        SDL_PauseAudioDevice(audio_dev, 0);
    }

    Emulator emu;
    memset(&emu, 0, sizeof(emu));


  
    int16_t audio_tmp[4096];
    bool running = true;
    const uint64_t perf_freq = SDL_GetPerformanceFrequency();
    const double target_frame_ms = 1000.0 / 60.0; /* ~16.6667 ms */
    OverlayState overlay = {0};
    snprintf(overlay.rom_dir, sizeof(overlay.rom_dir), "roms");
    overlay.show_help = true;
    overlay.show_rom_menu = false;
    overlay.selected_rom_idx = 0;
    
    if (argc > 1) {
        if (emu_load_rom(&emu, argv[1])) {
            overlay.show_rom_menu = false;
        }
    }
    while (running) {
        uint64_t frame_start = SDL_GetPerformanceCounter();
        SDL_Event ev;

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_KEYDOWN) {
                SDL_Keycode key = ev.key.keysym.sym;

                if (overlay.show_rom_menu) {
                    if (key == SDLK_UP) {
                        if (overlay.rom_count > 0) {
                            printf("UP/DOWN: count=%d idx=%d\n", overlay.rom_count, overlay.selected_rom_idx);
                            overlay.selected_rom_idx--;
                            if (overlay.selected_rom_idx < 0) {
                                overlay.selected_rom_idx = overlay.rom_count - 1;
                            }
                        }
                    } else if (key == SDLK_DOWN) {
                        printf("UP/DOWN: count=%d idx=%d\n", overlay.rom_count, overlay.selected_rom_idx);
                        if (overlay.rom_count > 0) {
                            overlay.selected_rom_idx = (overlay.selected_rom_idx +1) % overlay.rom_count;
                            printf("UP/DOWN: count=%d idx=%d\n", overlay.rom_count, overlay.selected_rom_idx);
                        }
                    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                        if (overlay.rom_count > 0) {
                            char rom_path[520];
                            snprintf(rom_path, sizeof(rom_path), "%s/%s",
                                    overlay.rom_dir, overlay.roms[overlay.selected_rom_idx]);
                            if (emu_load_rom(&emu, rom_path)) {
                                overlay.show_rom_menu = false;
                            }
                        }
                    } else if (key == SDLK_F2 || key == SDLK_ESCAPE) {
                        overlay.show_rom_menu = false;
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
                        overlay.show_help = false;
                        overlay.show_rom_menu = true;
                        overlay_scan_roms(&overlay);
                    }

                }
                  
            
            }
        }


        if (emu.loaded) {
            handle_input(&emu);
            bus_step_frame(&emu.bus, &emu.cpu);

            if (audio_dev > 0) {
                size_t n = tia_audio_take_samples(&emu.audio, audio_tmp, 4096);
                if (n > 0) {
                    SDL_QueueAudio(audio_dev, audio_tmp, (uint32_t)(n * sizeof(int16_t)));
                }
            }

            SDL_UpdateTexture(texture, NULL, emu.tia.framebuffer, TIA_FB_WIDTH * 3);
        } else {
            /* Leeg/donker scherm zolang er nog geen ROM actief is */
            uint8_t blank[TIA_FB_SIZE];
            memset(blank, 0x18, sizeof(blank));
            SDL_UpdateTexture(texture, NULL, blank, TIA_FB_WIDTH * 3);
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_GetRendererOutputSize(renderer, &win_w, &win_h);
        overlay_render(&overlay, renderer, win_w, win_h, 60.00);
        SDL_RenderPresent(renderer);

        uint64_t frame_end = SDL_GetPerformanceCounter();
        double elapsed_ms = ((double)(frame_end - frame_start) * 1000.0) / (double)perf_freq;
        if (elapsed_ms < target_frame_ms) {
            double remaining_ms = target_frame_ms - elapsed_ms;
            if (remaining_ms > 2.0) {
                SDL_Delay((uint32_t)(remaining_ms - 1.0));
            }
            while ((((double)(SDL_GetPerformanceCounter() - frame_start) * 1000.0) / (double)perf_freq) < target_frame_ms) {
                /* Fijnafstemming tot exact 16.67 ms */
            }
        }
    }

    if (audio_dev > 0) {
        SDL_CloseAudioDevice(audio_dev);
    }
    emu_destroy(&emu);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}