"""Recorta os oito livros animados enviados pelo criador em uma spritesheet."""
from pathlib import Path

from PIL import Image
import numpy as np

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

# Pose de ativacao: seis quadros frontais da folha de conceito. Nas demais
# direcoes mantemos o rosto correto e animamos o livro junto ao corpo.
concept = np.asarray(Image.open(ROOT / "design/biblia_ataque_referencia.png").convert("RGB"))
normal = Image.open(ROOT / "assets/protagonista.png").convert("RGBA")
activation = Image.new("RGBA", (6 * 64, 4 * 80))
centers = (295, 455, 625, 800, 980, 1160)
book = sheet.crop((0, 0, 80, 80))
for facing in range(4):
    for frame, center in enumerate(centers):
        if facing == 0:
            rgb = concept[109:255, center-65:center+65]
            alpha = np.clip((rgb.max(axis=2).astype(float)-8)*18, 0, 255).astype(np.uint8)
            art = Image.fromarray(np.dstack((rgb, alpha)), "RGBA")
            art = art.crop(art.getbbox())
            scale = min(61/art.width, 72/art.height)
            art = art.resize((round(art.width*scale), round(art.height*scale)),
                             Image.Resampling.NEAREST)
            activation.alpha_composite(art, (frame*64+(64-art.width)//2,
                                             77-art.height))
        else:
            body = normal.crop((0, facing*80, 64, (facing+1)*80))
            lift = (0, 1, 3, 4, 2, 0)[frame]
            activation.alpha_composite(body, (frame*64, facing*80-lift))
            prop = book.resize((29, 29), Image.Resampling.NEAREST)
            x = frame*64 + (8 if facing==2 else 30)
            y = facing*80 + 35 - lift - (4 if frame in (2,3,4) else 0)
            activation.alpha_composite(prop, (x, y))
activation.save(ROOT / "assets/biblia_ativacao.png")
