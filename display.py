import os
import pygame


class Display:
    SUPPORTED_EXTS = (".bin", ".a26", ".rom")

    def __init__(self, scale=4, rom_dir="roms"):
        pygame.mixer.pre_init(44100, -16, 1, 1024)
        pygame.init()
        self.debug_lines = []
        self.width = 160 * scale
        self.height = 192 * 3  # 640x576 (4:3 verhouding)
        self.screen = pygame.display.set_mode((self.width, self.height))
        pygame.display.set_caption("Atari 2600 Emulator")
        self.clock = pygame.time.Clock()
        self.font = pygame.font.SysFont("monospace", 16, bold=True)
        self.show_help = True

        self.rom_dir = rom_dir
        self.roms = []
        self.selected_rom_idx = 0
        self.show_rom_menu = False
        self.refresh_roms()

    def refresh_roms(self):
        if os.path.isdir(self.rom_dir):
            self.roms = sorted(
                f for f in os.listdir(self.rom_dir)
                if f.lower().endswith(self.SUPPORTED_EXTS)
            )
        else:
            self.roms = []
        self.selected_rom_idx = min(self.selected_rom_idx, max(0, len(self.roms) - 1))

    def set_debug_lines(self, debug):
        self.debug_lines = debug

    def render(self, framebuffer=None):
        """Geeft (running, chosen_rom_path) terug."""
        chosen_rom = None

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False, None
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    return False, None
                elif event.key == pygame.K_F1:
                    self.show_help = not self.show_help
                elif event.key == pygame.K_F2:
                    self.show_rom_menu = not self.show_rom_menu
                    if self.show_rom_menu:
                        self.refresh_roms()
                elif self.show_rom_menu and self.roms:
                    if event.key == pygame.K_UP:
                        self.selected_rom_idx = (self.selected_rom_idx - 1) % len(self.roms)
                    elif event.key == pygame.K_DOWN:
                        self.selected_rom_idx = (self.selected_rom_idx + 1) % len(self.roms)
                    elif event.key in (pygame.K_RETURN, pygame.K_KP_ENTER):
                        self.show_rom_menu = False
                        chosen_rom = os.path.join(self.rom_dir, self.roms[self.selected_rom_idx])

        if framebuffer is not None:
            frame_surface = pygame.image.frombuffer(framebuffer, (160, 200), "RGB")
            scaled_surface = pygame.transform.scale(frame_surface, (self.width, self.height))
            self.screen.blit(scaled_surface, (0, 0))
        else:
            self.screen.fill((0, 0, 0))

        if self.show_help and not self.show_rom_menu:
            self._draw_help()

        if self.show_rom_menu:
            self._draw_rom_menu()

        pygame.display.flip()
        self.clock.tick(0)
    
        return True, chosen_rom

    def _draw_help(self):
        overlay = pygame.Surface((510, 200))
        overlay.set_alpha(200)
        overlay.fill((20, 20, 20))
        self.screen.blit(overlay, (15, 15))
        self.screen.blit(self.font.render("F1: Help | F2: ROMs | ESC: Afsluiten", True, (0, 255, 120)), (25, 25))
        self.screen.blit(self.font.render("ASWD: Bewegen CTRL/SHIFT: action buttons", True, (0, 255, 120)), (25, 55))
        self.screen.blit(self.font.render(f"FPS: {self.clock.get_fps()}", True, (0, 255, 120)), (25, 85))

        for i, line in enumerate(self.debug_lines):
            surf = self.font.render(line, True, (0, 255, 120))
            self.screen.blit(surf, (25, 75 + i * 20))

    def _draw_rom_menu(self):
        overlay = pygame.Surface((self.width - 30, self.height - 30))
        overlay.set_alpha(225)
        overlay.fill((20, 20, 20))
        self.screen.blit(overlay, (15, 15))

        header = self.font.render(f"Selecteer ROM uit ./{self.rom_dir}/ (Enter = start, F2 = sluit)", True, (0, 255, 120))
        self.screen.blit(header, (25, 25))

        if not self.roms:
            msg = self.font.render("Geen .bin / .a26 / .rom bestanden in ./roms/", True, (255, 200, 0))
            self.screen.blit(msg, (25, 65))
            return

        max_lines = (self.height - 95) // 22
        start = max(0, self.selected_rom_idx - max_lines // 2)
        for i, filename in enumerate(self.roms[start:start + max_lines]):
            idx = start + i
            selected = idx == self.selected_rom_idx
            prefix = "> " if selected else "  "
            color = (255, 255, 100) if selected else (0, 255, 120)
            surf = self.font.render(f"{prefix}{filename}", True, color)
            self.screen.blit(surf, (25, 60 + i * 22))

    def close(self):
        pygame.quit()