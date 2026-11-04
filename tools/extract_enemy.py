"""Prepara os sprites do primeiro inimigo a partir da arte do criador."""
from pathlib import Path
import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
source = np.asarray(Image.open(ROOT / "design/inimigo_encapuzado_referencia.png").convert("RGB"))
back = Image.open(ROOT / "design/inimigo_encapuzado_costas_gerado.png").convert("RGBA")
front = Image.open(ROOT / "design/inimigo_encapuzado_frente_gerado.png").convert("RGBA")
CELL = 80


def place(sheet, art, column, row):
    alpha = np.asarray(art.getchannel("A"))
    ys, xs = np.where(alpha > 64)
    if not len(xs):
        raise ValueError(f"Sprite vazio em {row}, {column}")
    art = art.crop((int(xs.min()), int(ys.min()), int(xs.max()+1), int(ys.max()+1)))
    scale = min(72/art.height, 72/art.width)
    art = art.resize((max(1, round(art.width*scale)), max(1, round(art.height*scale))),
                     Image.Resampling.NEAREST)
    sheet.alpha_composite(art, (column*CELL+(CELL-art.width)//2,
                                row*CELL+77-art.height))


def original_sprite(cx, y0, y1):
    crop = source[y0:y1, cx-77:cx+77].copy()
    bg = np.median(np.vstack((crop[:8, :12].reshape(-1, 3),
                               crop[:8, -12:].reshape(-1, 3))), axis=0)
    diff = np.max(np.abs(crop.astype(np.float32)-bg), axis=2)
    mask = np.uint8(diff > 20) * 255
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, np.ones((9, 7), np.uint8))
    count, labels, stats, _ = cv2.connectedComponentsWithStats(mask, 8)
    if count < 2:
        raise ValueError(f"Nao encontrou inimigo em x={cx}, y={y0}")
    component = np.uint8(labels == 1+np.argmax(stats[1:, cv2.CC_STAT_AREA]))*255
    contours, _ = cv2.findContours(component, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    alpha = np.zeros(mask.shape, np.uint8)
    cv2.drawContours(alpha, [max(contours, key=cv2.contourArea)], -1, 255, -1)
    alpha = cv2.GaussianBlur(alpha, (3, 3), 0.5)
    return Image.fromarray(np.dstack((crop, alpha)), "RGBA")


# Linhas: espera, caminhada, ataque, dano e morte. A quinta coluna existe
# apenas nas animacoes de ataque e morte; outras linhas repetem o ultimo quadro.
rows = ((20, 185, (405, 612, 820, 1025)),
        (215, 375, (405, 612, 820, 1025)),
        (405, 555, (405, 612, 820, 1025, 1240)),
        (595, 745, (405, 612, 820, 1025)),
        (770, 945, (405, 612, 820, 1025, 1240)))
sheet = Image.new("RGBA", (5*CELL, 5*CELL))
for row, (y0, y1, centers) in enumerate(rows):
    for col in range(5):
        place(sheet, original_sprite(centers[min(col, len(centers)-1)], y0, y1), col, row)
sheet.save(ROOT / "assets/inimigo_encapuzado.png")

for name, generated in (("costas", back), ("frente", front)):
    direction_sheet = Image.new("RGBA", (4*CELL, 2*CELL))
    for row in range(2):
        for col in range(4):
            left = round(col*generated.width/4)
            right = round((col+1)*generated.width/4)
            top = round(row*generated.height/2)
            bottom = round((row+1)*generated.height/2)
            place(direction_sheet, generated.crop((left, top, right, bottom)), col, row)
    direction_sheet.save(ROOT / f"assets/inimigo_encapuzado_{name}.png")
