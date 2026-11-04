"""Recorta os oito livros animados enviados pelo criador em uma spritesheet."""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
source = Image.open(ROOT / "design/biblia_frames_referencia.png").convert("RGBA")
if source.size != (2172, 724):
    raise ValueError("A folha da Biblia precisa ter 2172 x 724 pixels")

cell = 80
sheet = Image.new("RGBA", (8 * cell, cell))
for frame in range(8):
    x0 = round(frame * source.width / 8)
    x1 = round((frame + 1) * source.width / 8)
    crop = source.crop((x0, 185, x1, 460))
    crop = crop.resize((cell, cell), Image.Resampling.NEAREST)
    # O fundo da folha ja e transparente; descarta residuos muito fracos.
    pixels = crop.load()
    for y in range(cell):
        for x in range(cell):
            r, g, b, a = pixels[x, y]
            if a < 12:
                pixels[x, y] = (r, g, b, 0)
    sheet.alpha_composite(crop, (frame * cell, 0))

sheet.save(ROOT / "assets/biblia_orbita.png")
