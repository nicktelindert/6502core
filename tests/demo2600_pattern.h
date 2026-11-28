#ifndef DEMO2600_PATTERN_H
#define DEMO2600_PATTERN_H

#include "../system2600.h"
#include "../cpu6502.h"
#include "../tia.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    Cartridge cart;
    TIA tia;
    RIOT riot;
    System2600 system;
    CPU6502 cpu;
} Demo2600;

static bool demo2600_init(Demo2600 *demo, VideoStandard standard) {
    uint8_t rom[4096] = {0};
    const uint8_t program[] = {
        0xa9, 0x02, 0x85, 0x01,       /* VBLANK */
        0xa9, 0x0e, 0x85, 0x06,       /* player color */
        0xa9, 0x0a, 0x85, 0x08,       /* playfield color */
        0xa9, 0x10, 0x85, 0x0d,       /* playfield pattern */
        0xa9, 0xff, 0x85, 0x1b,       /* player shape */
        0xa9, 0x00, 0x85, 0x10,       /* player position */
        0xa9, 0x02, 0x85, 0x00,       /* frame: VSYNC on */
        0xa2, 0x03, 0x85, 0x02, 0xca, 0xd0, 0xfb,
        0xa9, 0x00, 0x85, 0x00,       /* VSYNC off */
        0xa2, standard == VIDEO_PAL ? 45 : 37, 0x85, 0x02, 0xca, 0xd0, 0xfb,
        0xa9, 0x00, 0x85, 0x01,       /* VBLANK off */
        0xa9, 0x26, 0x85, 0x09,
        0xa2, standard == VIDEO_PAL ? 76 : 64, 0x85, 0x02, 0xca, 0xd0, 0xfb,
        0xa9, 0x46, 0x85, 0x09,
        0xa2, standard == VIDEO_PAL ? 76 : 64, 0x85, 0x02, 0xca, 0xd0, 0xfb,
        0xa9, 0x86, 0x85, 0x09,
        0xa2, standard == VIDEO_PAL ? 76 : 64, 0x85, 0x02, 0xca, 0xd0, 0xfb,
        0xa9, 0x02, 0x85, 0x01,       /* overscan */
        0xa2, standard == VIDEO_PAL ? 36 : 30, 0x85, 0x02, 0xca, 0xd0, 0xfb,
        0x4c, 0x18, 0xf1
    };
    memcpy(rom + 0x100, program, sizeof(program));
    rom[0xffc] = 0x00;
    rom[0xffd] = 0xf1;
    memset(demo, 0, sizeof(*demo));
    if (!cart_init_from_bytes(&demo->cart, rom, sizeof(rom), "synthetic 2600 demo")) {
        fprintf(stderr, "Unable to initialize synthetic 2600 cartridge\n");
        return false;
    }
    tia_init(&demo->tia, NULL);
    tia_set_video_standard(&demo->tia, standard);
    riot_init(&demo->riot);
    system2600_init(&demo->system, &demo->tia, &demo->riot, &demo->cart,
                    tia_step, tia_read, tia_write);
    cpu_init_with_bus(&demo->cpu, system2600_cpu_bus(&demo->system));
    return true;
}

static void demo2600_destroy(Demo2600 *demo) {
    tia_destroy(&demo->tia);
    cart_free(&demo->cart);
}

#endif
