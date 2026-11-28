#include "../antic5200.h"
#include "../gtia5200.h"
#include "../system5200.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    bool once = argc == 2 && strcmp(argv[1], "--once") == 0;
    if (argc > 2 || (argc == 2 && !once)) {
        fprintf(stderr, "Usage: %s [--once]\n", argv[0]);
        return 1;
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow("6502core - synthetic Atari 5200 video demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 720, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture *texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING, GTIA5200_WIDTH, GTIA5200_HEIGHT) : NULL;
    if (!renderer || !texture) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        if (renderer) SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    System5200 system;
    Antic5200 antic;
    GTIA5200 gtia;
    CPU6502 cpu;
    system5200_init(&system);
    antic5200_init(&antic);
    gtia5200_init(&gtia);
    system5200_attach_antic(&system, &antic, &cpu);
    system5200_attach_gtia(&system, &gtia);
    system.bios_loaded = true;
    system.bios[0x7fc] = 0;
    system.bios[0x7fd] = 2;
    system.ram[0x200] = 0x4c;
    system.ram[0x201] = 0;
    system.ram[0x202] = 2;
    size_t list = 0x300;
    for (int i = 0; i < 3; i++) system.ram[list++] = 0x70;
    for (int y = 0; y < 192; y++) {
        if (y == 0 || y == 100) {
            system.ram[list++] = 0x4f;
            system.ram[list++] = 0x00;
            system.ram[list++] = y == 0 ? 0x10 : 0x20;
        } else system.ram[list++] = 0x0f;
        size_t row = y < 100 ? 0x1000 + (size_t)y * 40 : 0x2000 + (size_t)(y - 100) * 40;
        for (int x = 0; x < 40; x++)
            system.ram[row + (size_t)x] = ((x / 4 + y / 16) & 1) ? 0xf0 : 0x0f;
    }
    system.ram[list++] = 0x41;
    system.ram[list++] = 0;
    system.ram[list++] = 3;
    for (unsigned line = 32; line < 224; line++)
        system.ram[0x3c00 + line] = 0xff;
    system5200_write(&system, 0xc018, 0x02);
    system5200_write(&system, 0xc017, 0x0e);
    system5200_write(&system, 0xc01a, 0x00);
    system5200_write(&system, 0xc000, 0x80);
    system5200_write(&system, 0xc012, 0x0e);
    system5200_write(&system, 0xc01b, 0x01);
    system5200_write(&system, 0xc01d, 0x02);
    system5200_write(&system, 0xd402, 0x00);
    system5200_write(&system, 0xd403, 0x03);
    system5200_write(&system, 0xd407, 0x38);
    system5200_write(&system, 0xd400, 0x3a);
    cpu_init_with_bus(&cpu, system5200_cpu_bus(&system));
    bool running = true;
    int result = 0;
    uint8_t rgb[GTIA5200_HEIGHT][GTIA5200_WIDTH][3];
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
                running = false;
        }
        uint64_t deadline = system.elapsed_cycles + 114u * 263u;
        bool reached_end = false;
        while (system.elapsed_cycles < deadline) {
            cpu_step(&cpu);
            if (antic.unsupported) {
                fprintf(stderr, "Unsupported ANTIC mode in demo\n");
                result = 1;
                running = false;
                break;
            }
            if (antic.scanline >= 248) reached_end = true;
            if (reached_end && antic.scanline == 0) break;
        }
        if (system.elapsed_cycles >= deadline) {
            fprintf(stderr, "Video frame did not finish\n");
            result = 1;
            break;
        }
        for (int y = 0; y < GTIA5200_HEIGHT; y++)
            for (int x = 0; x < GTIA5200_WIDTH; x++) {
                uint8_t gray = (uint8_t)((gtia.framebuffer[y][x] & 0x0e) * 255 / 14);
                rgb[y][x][0] = rgb[y][x][1] = rgb[y][x][2] = gray;
            }
        if (SDL_UpdateTexture(texture, NULL, rgb, GTIA5200_WIDTH * 3) != 0 ||
            SDL_RenderClear(renderer) != 0 || SDL_RenderCopy(renderer, texture, NULL, NULL) != 0) {
            fprintf(stderr, "Video rendering failed: %s\n", SDL_GetError());
            result = 1;
            break;
        }
        if (once) {
            uint8_t dark[3], light[3], sprite[3];
            int y = (32 - GTIA5200_FIRST_SCANLINE) * 3;
            SDL_Rect first = {0, y, 1, 1}, second = {18, y, 1, 1}, player = {480, y, 1, 1};
            if (SDL_RenderReadPixels(renderer, &first, SDL_PIXELFORMAT_RGB24, dark, 3) != 0 ||
                SDL_RenderReadPixels(renderer, &second, SDL_PIXELFORMAT_RGB24, light, 3) != 0 ||
                SDL_RenderReadPixels(renderer, &player, SDL_PIXELFORMAT_RGB24, sprite, 3) != 0) {
                fprintf(stderr, "Video readback failed: %s\n", SDL_GetError());
                result = 1;
                break;
            }
            if (dark[0] >= light[0] || sprite[0] != light[0]) {
                fprintf(stderr, "Video demo did not produce the expected pixels\n");
                result = 1;
                break;
            }
        }
        SDL_RenderPresent(renderer);
        if (once) break;
        SDL_Delay(16);
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
