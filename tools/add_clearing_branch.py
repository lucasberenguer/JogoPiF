"""Add the right-hand overhanging branch marked in a cropped gameplay image."""

from pathlib import Path

import cv2
import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
marked = np.asarray(Image.open(ROOT / "design/clareira_galho_direita_marcado.png").convert("RGB"))
base = np.asarray(Image.open(ROOT / "design/clareira_galho_direita_base.png").convert("RGB"))
if marked.shape != base.shape:
    raise ValueError("Marked and base crops must have the same size")

difference = np.max(np.abs(marked.astype(np.int16) - base.astype(np.int16)), axis=2)
black = (marked.max(axis=2) < 25) & (difference > 60)
# Limit detection to the learner's black brush mark beside the tall stone.
black[:110] = False
black[280:] = False
black[:, :170] = False

# Registered from matching map features: cropped screenshot -> 1672x941 map.
map_from_crop = np.array([
    [1.11227673, -0.00003816, 1326.16257],
    [0.00003816, 1.11227673, 222.901245],
], dtype=np.float32)
size = Image.open(ROOT / "assets/clareira.png").size
branch = cv2.warpAffine(black.astype(np.uint8) * 255, map_from_crop, size,
                        flags=cv2.INTER_NEAREST)
branch = cv2.dilate(branch, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7))) > 0

destination = ROOT / "design/clareira_ajustes.png"
adjustments = np.asarray(Image.open(destination).convert("RGBA")).copy()
adjustments[branch] = (0, 0, 0, 255)
Image.fromarray(adjustments, "RGBA").save(destination)
print(f"Added {branch.sum()} foreground pixels to {destination}")
