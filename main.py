import sys
import os
from tia import TIA
from riot import RIOT
from cartridge import create_cartridge
from bus import Bus
from cpu6507 import CPU6507
from display import Display
import pygame


def handle_input(cpu, tia, riot):
    keys = pygame.key.get_pressed()

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

    if keys[pygame.K_LCTRL] or keys[pygame.K_RCTRL]:
        tia.inpt4 = 0x00
    else:
        tia.inpt4 = 0x80

    swchb = 0x0B
    if keys[pygame.K_LSHIFT] or keys[pygame.K_RSHIFT]:
        swchb &= ~0x01  # Reset
    riot.swchb = swchb


def check_rom_folder():
    check_path = os.path.join(os.path.curdir, "roms")
    if not os.path.isdir(check_path):
        os.mkdir(check_path)


def load_rom(path):
    with open(path, "rb") as f:
        rom_data = f.read()
    tia = TIA()
    riot = RIOT()
    cart = create_cartridge(rom_data, os.path.basename(path))
    bus = Bus(tia, riot, cart)
    cpu = CPU6507(bus)
    return tia, riot, cart, bus, cpu


def main():
    check_rom_folder()
    display = Display(scale=4, rom_dir="roms")

    if len(sys.argv) > 1:
        tia, riot, cart, bus, cpu = load_rom(sys.argv[1])
    else:
        tia = riot = cart = bus = cpu = None
        display.show_rom_menu = True

    running = True
    while running:
        if bus is not None and not display.show_rom_menu:
            handle_input(cpu, tia, riot)
            bus.step_frame(cpu)

        framebuffer = tia.framebuffer if tia is not None else None
        running, chosen_rom = display.render(framebuffer)
        if chosen_rom:
            tia, riot, cart, bus, cpu = load_rom(chosen_rom)

    display.close()


if __name__ == "__main__":
    main()