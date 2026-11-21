import sys
from tia import TIA
from riot import RIOT
from cartridge import create_cartridge
from bus import Bus
from cpu6507 import CPU6507
from display import Display
import pygame

def handle_input(cpu, tia, riot):
    keys = pygame.key.get_pressed()

    # Joystick Player 0 (SWCHA bits 4..7, 0 = ingedrukt)
    swcha = 0xFF
    if keys[pygame.K_w]:
        swcha &= ~0x10  # Omhoog
    if keys[pygame.K_s]:
        swcha &= ~0x20  # Omlaag
    if keys[pygame.K_a]:
        swcha &= ~0x40  # Links
    if keys[pygame.K_d]:
        swcha &= ~0x80  # Rechts
    riot.swcha = swcha

    # Vuurknop Player 0 (TIA INPT4 bit 7, 0 = ingedrukt)
    if keys[pygame.K_LCTRL] or keys[pygame.K_RCTRL]:
        tia.inpt4 = 0x00
    else:
        tia.inpt4 = 0x80

    # Tweede knop: Shift als Game Reset (SWCHB bit 0, 0 = ingedrukt)
    # (Veel Atari 2600 games gebruiken een 1-knops joystick + Reset op de console)
    swchb = 0x0B
    if keys[pygame.K_LSHIFT] or keys[pygame.K_RSHIFT]:
        swchb &= ~0x01  # Reset
    riot.swchb = swchb

def main():

    rom_path = sys.argv[1]

    with open(rom_path, "rb") as f:
        rom_data = f.read()

    tia = TIA()
    riot = RIOT()
    cart = create_cartridge(rom_data)
    bus = Bus(tia, riot, cart)
    cpu = CPU6507(bus)
    display = Display(scale=4)

    running = True
    while running:
        handle_input(cpu, tia, riot)
        bus.step_frame(cpu)
        running = display.render(tia.framebuffer)
        # Voorbeeld van compacte debug-regels in je F1-overlay:
        display.set_debug_lines([
            f"SL:{tia.scanline:3d}  CLK:{tia.clock:3d}",
            f"P0:{tia.debug_p0_x:3d}, (HM:{tia.hmp0 >> 4:X}) | P1:{tia.debug_p1_x:3d} (HM:{tia.hmp1 >> 4:X})",
            f"M0:{tia.m0_x:3d} (HM:{tia.hmm0 >> 4:X}) | M1:{tia.m1_x:3d} (HM:{tia.hmm1 >> 4:X}) | BL:{tia.bl_x:3d} (HM:{tia.hmbl >> 4:X})",
        ])

    display.close()


if __name__ == "__main__":
    main()
