#include "demo2600_pattern.h"
#include <SDL2/SDL.h>

static bool verify_pixels(SDL_Renderer *renderer, const Demo2600 *demo,
                          VideoStandard standard, int width, int height) {
    int first = standard == VIDEO_PAL ? 47 : 39;
    int band = standard == VIDEO_PAL ? 76 : 64;
    int total = demo->tia.completed_frame_height;
    const struct { int x, y; uint8_t color; } samples[] = {
        {60, first - 1, 0x00},
        {60, first, 0x26},
        {60, first + band, 0x46},
        {60, first + 2 * band, 0x86},
        {60, first + 3 * band - 1, 0x86},
        {60, first + 3 * band, 0x00},
        {0, first, 0x0a},
        {demo->tia.p0_x, first, 0x0e}
    };
    for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        if (samples[i].x < 0 || samples[i].x >= TIA_FB_WIDTH) {
            fprintf(stderr, "Video demo player is outside the frame\n");
            return false;
        }
        SDL_Rect point = {
            (2 * samples[i].x + 1) * width / (2 * TIA_FB_WIDTH),
            (2 * samples[i].y + 1) * height / (2 * total), 1, 1
        };
        uint8_t pixel[3];
        if (SDL_RenderReadPixels(renderer, &point, SDL_PIXELFORMAT_RGB24, pixel, 3) != 0) {
            fprintf(stderr, "Video readback failed: %s\n", SDL_GetError());
            return false;
        }
        if (memcmp(pixel, demo->tia.color_lut[samples[i].color], 3) != 0) {
            fprintf(stderr, "Video demo pixel mismatch at (%d,%d)\n", samples[i].x, samples[i].y);
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv) {
    bool once = false;
    VideoStandard standard = VIDEO_NTSC;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--once") == 0) once = true;
        else if (strcmp(argv[i], "--pal") == 0) standard = VIDEO_PAL;
        else {
            fprintf(stderr, "Usage: %s [--pal] [--once]\n", argv[0]);
            return 1;
        }
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    int result = 1;
    SDL_Window *window = SDL_CreateWindow("6502core - synthetic Atari 2600 video demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 720, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    int total = standard == VIDEO_PAL ? 312 : 262;
    SDL_Texture *texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING, TIA_FB_WIDTH, total) : NULL;
    if (!renderer || !texture) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        goto cleanup_sdl;
    }
    Demo2600 demo;
    if (!demo2600_init(&demo, standard)) goto cleanup_sdl;
    if (!system2600_step_frame(&demo.system, &demo.cpu)) {
        fprintf(stderr, "Video demo could not synchronize its first frame\n");
        goto cleanup_demo;
    }
    result = 0;
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
                running = false;
        }
        if (!running) break;
        if (!system2600_step_frame(&demo.system, &demo.cpu) ||
            demo.tia.completed_frame_height != total) {
            fprintf(stderr, "Video demo frame synchronization failed\n");
            result = 1;
            break;
        }
        if (SDL_UpdateTexture(texture, NULL, demo.tia.framebuffer, TIA_FB_WIDTH * 3) != 0 ||
            SDL_RenderClear(renderer) != 0 || SDL_RenderCopy(renderer, texture, NULL, NULL) != 0) {
            fprintf(stderr, "Video rendering failed: %s\n", SDL_GetError());
            result = 1;
            break;
        }
        if (once) {
            int width, height;
            if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0) {
                fprintf(stderr, "Unable to get renderer size: %s\n", SDL_GetError());
                result = 1;
            } else if (!verify_pixels(renderer, &demo, standard, width, height)) {
                result = 1;
            }
        }
        SDL_RenderPresent(renderer);
        if (once) break;
        SDL_Delay(standard == VIDEO_PAL ? 20 : 16);
    }
cleanup_demo:
    demo2600_destroy(&demo);
cleanup_sdl:
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
