import sys
from cpu6507 import CPU6507
from display import Display

def main():
    rom_path = sys.argv[1]

    with open(rom_path, "rb") as f:
        rom_data = f.read()

    cpu = CPU6507(rom_data)
    display = Display(scale=4)

    running = True
    while running:
        cpu.step_frame()
        running = display.render(cpu.tia.framebuffer)

    display.close()

if __name__ == "__main__":
    main()
