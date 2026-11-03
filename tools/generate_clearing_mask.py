"""Extract provisional wall geometry from the red marks on the clearing image.

Requires Pillow, NumPy and OpenCV only when regenerating the mask. The game
loads the generated PNG and does not need Python or OpenCV at runtime.
"""

from pathlib import Path

import cv2
import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
marked = np.asarray(Image.open(ROOT / "design/clareira_marcada.jpg").convert("RGB"))
red = (
    (marked[:, :, 0] > 180)
    & (marked[:, :, 1] < 100)
    & (marked[:, :, 2] < 100)
).astype(np.uint8)

# JPEG compression leaves isolated red pixels. Preserve only the marked strokes.
count, labels, stats, _ = cv2.connectedComponentsWithStats(red, 8)
strokes = np.zeros(red.shape, dtype=np.uint8)
for index in range(1, count):
    if stats[index, cv2.CC_STAT_AREA] >= 500:
        strokes[labels == index] = 255

# Inflate the painted boundary by the placeholder player's 16-pixel radius.
kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (33, 33))
blocked = cv2.dilate(strokes, kernel)
overlay = np.zeros((*blocked.shape, 4), dtype=np.uint8)
overlay[blocked > 0] = (255, 30, 30, 100)

destination = ROOT / "assets/clareira_collision.png"
Image.fromarray(overlay, "RGBA").save(destination)
print(f"Wrote {destination} ({blocked.shape[1]}x{blocked.shape[0]})")
