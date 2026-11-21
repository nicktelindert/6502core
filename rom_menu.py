import os
import pygame


class RomMenu:
    SUPPORTED_EXTS = (".bin", ".a26", ".rom")

    def __init__(self, rom_dir="roms"):
        self.rom_dir = rom_dir
        os.makedirs(self.rom_dir, exist_ok=True)
        self.roms = []
        self.selected = 0
        self.visible = True
        self.refresh()

    def refresh(self):
        self.roms = sorted(
            f for f in os.listdir(self.rom_dir)
            if f.lower().endswith(self.SUPPORTED_EXTS)
        )
        self.selected = min(self.selected, max(0, len(self.roms) - 1))

    def handle_event(self, event):
        """Geeft het volledige pad naar de gekozen ROM terug bij Enter, anders None."""
        if event.type != pygame.KEYDOWN:
            return None

        if event.key == pygame.K_F2:
            self.visible = not self.visible
            if self.visible:
                self.refresh()
            return None

        if not self.visible or not self.roms:
            return None

        if event.key == pygame.K_UP:
            self.selected = (self.selected - 1) % len(self.roms)
        elif event.key == pygame.K_DOWN:
            self.selected = (self.selected + 1) % len(self.roms)
        elif event.key in (pygame.K_RETURN, pygame.K_KP_ENTER):
            self.visible = False
            return os.path.join(self.rom_dir, self.roms[self.selected])
        return None

    def draw(self, surface, font):
        if not self.visible:
            return

        w, h = surface.get_size()
        overlay = pygame.Surface((w - 80, h - 80), pygame.SRCALPHA)
        overlay.fill((10, 14, 24, 225))
        pygame.draw.rect(overlay, (0, 220, 120), overlay.get_rect(), 2)

        title = font.render(f"Kies ROM uit ./{self.rom_dir}/ (Enter = start, F2 = sluit)", True, (0, 255, 140))
        overlay.blit(title, (16, 14))

        if not self.roms:
            msg = font.render("Geen .bin / .a26 / .rom bestanden gevonden in ./roms/", True, (255, 180, 80))
            overlay.blit(msg, (16, 52))
        else:
            max_visible = (overlay.get_height() - 56) // 24
            start = max(0, self.selected - max_visible // 2)
            for i, name in enumerate(self.roms[start:start + max_visible]):
                idx = start + i
                is_sel = idx == self.selected
                prefix = "> " if is_sel else "  "
                color = (255, 255, 100) if is_sel else (210, 220, 230)
                line = font.render(f"{prefix}{name}", True, color)
                overlay.blit(line, (16, 48 + i * 24))

        surface.blit(overlay, (40, 40))