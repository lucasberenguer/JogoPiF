"""Convert a marked gameplay screenshot into map-sized collision corrections.

Usage: python tools/extract_clearing_feedback.py <plain screenshot> <marked screenshot>
The two screenshots must have identical pixels outside the painted annotations.
"""

import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
plain = np.asarray(Image.open(sys.argv[1]).convert("RGB"))
marked = np.asarray(Image.open(sys.argv[2]).convert("RGB"))
if plain.shape != marked.shape:
    raise ValueError("Screenshots must have the same dimensions")

changed = np.max(np.abs(marked.astype(np.int16) - plain.astype(np.int16)), axis=2) > 30
black = changed & (marked.max(axis=2) < 40)
cyan = changed & (marked[:, :, 0] < 80) & (marked[:, :, 1] > 110) & (marked[:, :, 2] > 130)

# The fullscreen screenshot fits the entire 1672x941 map. This registration
# was measured from matching features in the plain screenshot and map image.
screen_from_map = np.array(
    [[1.13387466, 0.00019730, 6.05384425],
     [0.00011295, 1.13401756, -8.08800940]], dtype=np.float32
)
map_from_screen = cv2.invertAffineTransform(screen_from_map)
size = Image.open(ROOT / "assets/clareira.png").size

def to_map(mask):
    return cv2.warpAffine(mask.astype(np.uint8) * 255, map_from_screen, size,
                          flags=cv2.INTER_NEAREST) > 0

branch = cv2.dilate(to_map(black).astype(np.uint8),
                    cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7))) > 0
clear = cv2.dilate(to_map(cyan).astype(np.uint8),
                   cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (17, 17))) > 0

corrections = np.zeros((size[1], size[0], 4), dtype=np.uint8)
corrections[branch] = (0, 0, 0, 255)
corrections[clear] = (0, 255, 255, 255)
destination = ROOT / "design/clareira_ajustes.png"
Image.fromarray(corrections, "RGBA").save(destination)
print(f"Wrote {destination}: {branch.sum()} branch pixels, {clear.sum()} clearance pixels")
