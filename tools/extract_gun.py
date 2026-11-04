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

