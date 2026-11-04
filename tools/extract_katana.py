"""Extrai quatro intensidades do corte e orienta para as quatro direcoes."""
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
source = np.asarray(Image.open(ROOT / "design/katana_ataque_referencia.png").convert("RGB"))
if source.shape[:2] != (1254, 1254):
    raise ValueError("A referencia da katana precisa ter 1254 x 1254 pixels")

# Apenas os efeitos, sem as legendas ou o fundo preto da folha de conceito.
boxes = ((307, 836, 466, 936), (480, 817, 705, 959),
         (695, 809, 964, 962), (970, 803, 1244, 965))
cell = 192
sheet = Image.new("RGBA", (cell * 4, cell * 4))

for frame, box in enumerate(boxes):
    crop = source[box[1]:box[3], box[0]:box[2]].copy()
    brightness = crop.max(axis=2)
    alpha = np.where(brightness < 7, 0,
                     np.clip((brightness.astype(np.float32) - 7) * 18, 0, 255)).astype(np.uint8)
    rgba = np.dstack((crop, alpha))
    effect = Image.fromarray(rgba, "RGBA")
    bbox = effect.getbbox()
    if bbox is None:
        raise ValueError(f"Efeito {frame} vazio")
    effect = effect.crop(bbox)
    scale = min(172 / effect.width, 125 / effect.height)
    effect = effect.resize((round(effect.width * scale), round(effect.height * scale)),
                           Image.Resampling.NEAREST)

    for facing in range(4):  # baixo, cima, esquerda, direita
        oriented = effect
        if facing == 2:
            oriented = effect.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        elif facing == 0:
            oriented = effect.rotate(-90, expand=True)
        elif facing == 1:
            oriented = effect.rotate(90, expand=True)
        # O centro coincide em todos os quadros: o personagem nao muda de tamanho.
        x = frame * cell + (cell - oriented.width) // 2
        y = facing * cell + (cell - oriented.height) // 2
        sheet.alpha_composite(oriented, (x, y))

sheet.save(ROOT / "assets/katana_corte.png")


def body_crop(box):
    x0, y0, x1, y1 = box
    rgb = source[y0:y1, x0:x1]
    alpha = np.clip((rgb.max(axis=2).astype(np.float32) - 7) * 18, 0, 255).astype(np.uint8)
    art = Image.fromarray(np.dstack((rgb, alpha)), "RGBA")
    bounds = art.getbbox()
    if bounds is None:
        raise ValueError(f"Pose da katana vazia: {box}")
    return art.crop(bounds)


normal = Image.open(ROOT / "assets/protagonista.png").convert("RGBA")
body_sheet = Image.new("RGBA", (4 * 64, 4 * 80))
right_boxes = ((226, 109, 310, 199), (365, 109, 449, 199),
               (525, 109, 609, 199), (1138, 109, 1222, 199))
vertical_boxes = {
    0: (525, 635, 618, 760),  # baixo
    1: (282, 645, 374, 760),  # cima
}

for facing in range(4):
    for frame in range(4):
        if facing in (2, 3):
            art = body_crop(right_boxes[frame])
            if facing == 2:
                art = art.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        elif frame in (0, 3):
            art = normal.crop((0, facing * 80, 64, (facing + 1) * 80))
        else:
            art = body_crop(vertical_boxes[facing])
        if facing in (0, 1) and frame in (0, 3):
            body_sheet.alpha_composite(art, (frame * 64, facing * 80))
            continue
        scale = min(60 / art.width, 70 / art.height)
        art = art.resize((max(1, round(art.width * scale)),
                          max(1, round(art.height * scale))), Image.Resampling.NEAREST)
        shift = 2 if frame == 2 else 0
        x = frame * 64 + (64 - art.width) // 2
        y = facing * 80 + 77 - art.height - shift
        body_sheet.alpha_composite(art, (x, y))

body_sheet.save(ROOT / "assets/katana_corpo.png")
