"""Recorta poses de disparo e efeitos da folha enviada para a arma de fogo."""
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
source = np.asarray(Image.open(ROOT / "design/arma_fogo_referencia.png").convert("RGB"))
if source.shape[:2] != (1254, 1254):
    raise ValueError("A referencia da arma de fogo precisa ter 1254 x 1254 pixels")


def transparent(box):
    x0, y0, x1, y1 = box
    rgb = source[y0:y1, x0:x1]
    alpha = np.clip((rgb.max(axis=2).astype(np.float32) - 9) * 18, 0, 255).astype(np.uint8)
    art = Image.fromarray(np.dstack((rgb, alpha)), "RGBA")
    bounds = art.getbbox()
    if bounds is None:
        raise ValueError(f"Recorte vazio: {box}")
    return art.crop(bounds)


effects = {
    "projetil": ((370, 818, 435, 855), (52, 28)),
    "clarao": ((940, 810, 1004, 865), (52, 52)),
    "impacto": ((256, 913, 315, 971), (56, 56)),
}
for name, (box, size) in effects.items():
    art = transparent(box)
    art.thumbnail(size, Image.Resampling.NEAREST)
    canvas = Image.new("RGBA", size)
    canvas.alpha_composite(art, ((size[0]-art.width)//2, (size[1]-art.height)//2))
    canvas.save(ROOT / "assets" / f"arma_fogo_{name}.png")

# Quatro quadros de postura por direcao. A folha original tem oito angulos;
# usamos o angulo diagonal para a frente, pois o painel marcado "Baixo"
# mostra as costas e nao funcionaria como pose frontal.
pose_centers = {
    0: (665, 620, 575, 575),   # frente: baixo-direita
    1: (255, 210, 170, 170),   # costas
    2: (1090, 1035, 995, 995),  # esquerda
    3: (530, 485, 445, 445),  # direita
}
poses = Image.new("RGBA", (4 * 64, 4 * 80))
for direction, centers in pose_centers.items():
    for frame, center in enumerate(centers):
        art = transparent((center-22, 292, center+22, 382))
        scale = min(58/art.width, 70/art.height)
        art = art.resize((round(art.width*scale), round(art.height*scale)),
                         Image.Resampling.NEAREST)
        poses.alpha_composite(art, (frame*64+(64-art.width)//2,
                                    direction*80+77-art.height))
poses.save(ROOT / "assets/arma_fogo_corpo.png")

