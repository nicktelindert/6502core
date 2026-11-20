import pygame

class Display:
    def __init__(self, scale=4):
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

    def set_debug_lines(self, debug):
        self.debug_lines = debug

    def render(self, framebuffer):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    return False
                elif event.key == pygame.K_F1:
                    self.show_help = not self.show_help

        frame_surface = pygame.image.frombuffer(framebuffer, (160, 192), "RGB")
        scaled_surface = pygame.transform.scale(frame_surface, (self.width, self.height))
        self.screen.blit(scaled_surface, (0, 0))
        if self.show_help:
            self._draw_help()

        pygame.display.flip()
        self.clock.tick(60)
        return True

    def _draw_help(self):
        overlay = pygame.Surface((310, 100))
        overlay.set_alpha(200)
        overlay.fill((20, 20, 20))
        self.screen.blit(overlay, (15, 15))
        self.screen.blit(self.font.render("F1: Help | ESC: Afsluiten", True, (0, 255, 120)), (25, 25))
        self.screen.blit(self.font.render("ASWD: Bewegen CTRL/SHIFT: action buttons", True, (0, 255, 120)), (25, 55))
        for i, line in enumerate(self.debug_lines):
            surf = self.font.render(line, True, (0, 255, 120))
            self.screen.blit(surf, (25, 75 + i * 20))


    def close(self):
        pygame.quit()
