"""Restore dark interiors erased by color-key extraction, frame by frame."""
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SHEETS = {
    "inimigo_ranged.png": (80, 80, (18, 4, 64, 77)),
    "arma_fogo_idle_novo.png": (64, 80, (3, 3, 61, 77)),
    "arma_fogo_ataque_novo.png": (64, 80, (3, 3, 61, 77)),
    "katana_idle_novo.png": (160, 112, (54, 25, 111, 109)),
    "katana_golpe_1.png": (160, 112, (54, 25, 111, 109)),
    "katana_golpe_2.png": (160, 112, (54, 25, 111, 109)),
    "katana_golpe_3.png": (160, 112, (54, 25, 111, 109)),
}

for name, (width, height, region) in SHEETS.items():
    path = ROOT / "assets" / name
    pixels = np.array(Image.open(path).convert("RGBA"))
    repaired = 0
    for y in range(0, pixels.shape[0], height):
        for x in range(0, pixels.shape[1], width):
            frame = pixels[y:y + height, x:x + width]
            mask = np.uint8(frame[:, :, 3] > 15) * 255
            # Seal one-pixel cracks before distinguishing exterior from interior.
            closed = cv2.morphologyEx(mask, cv2.MORPH_CLOSE,
                                     np.ones((3, 3), np.uint8))
            exterior = closed.copy()
            cv2.floodFill(exterior, None, (0, 0), 255)
            solid = closed | cv2.bitwise_not(exterior)
            allowed = np.zeros(mask.shape, dtype=bool)
            left, top, right, bottom = region
            allowed[top:bottom, left:right] = True
            restore = (solid > 0) & allowed & (frame[:, :, 3] < 255)
            repaired += int(np.count_nonzero(restore))
            frame[:, :, 3][restore] = 255
    if repaired:
        Image.fromarray(pixels, "RGBA").save(path, optimize=True)
    print(f"{name}: {repaired} pixels restored")
