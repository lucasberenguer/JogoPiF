"""Extract wall and foreground annotations from the marked clearing image.

Requires Pillow, NumPy and OpenCV only when regenerating the mask. The game
loads the generated PNG and does not need Python or OpenCV at runtime.
"""

from pathlib import Path

import cv2
import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
marked = np.asarray(Image.open(ROOT / "design/clareira_marcada.png").convert("RGB"))
red = (
    (marked[:, :, 0] > 180)
    & (marked[:, :, 1] < 100)
    & (marked[:, :, 2] < 100)
).astype(np.uint8)

# Ignore isolated pixels and preserve the marked strokes.
count, labels, stats, _ = cv2.connectedComponentsWithStats(red, 8)
strokes = np.zeros(red.shape, dtype=np.uint8)
for index in range(1, count):
    if stats[index, cv2.CC_STAT_AREA] >= 500:
        strokes[labels == index] = 255

# The red paint is much wider than a wall line. Reduce it to a centerline first,
# then use a small gameplay footprint. The drawn player is deliberately wider
# than its collision footprint so it can overlap decorative edges slightly.
skeleton = np.zeros_like(strokes)
remaining = strokes.copy()
cross = cv2.getStructuringElement(cv2.MORPH_CROSS, (3, 3))
while cv2.countNonZero(remaining):
    eroded = cv2.erode(remaining, cross)
    opened = cv2.dilate(eroded, cross)
    skeleton = cv2.bitwise_or(skeleton, cv2.subtract(remaining, opened))
    remaining = eroded

kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9))
blocked = cv2.dilate(skeleton, kernel)
blocked = cv2.morphologyEx(
    blocked, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
)
blocked = cv2.GaussianBlur(blocked, (5, 5), 0)
blocked = np.where(blocked >= 128, 255, 0).astype(np.uint8)

adjustments = np.asarray(Image.open(ROOT / "design/clareira_ajustes.png").convert("RGBA"))
if adjustments.shape[:2] != blocked.shape:
    raise ValueError("Adjustment image and collision mask must have the same size")
clearance = ((adjustments[:, :, 1] > 150) &
             (adjustments[:, :, 2] > 150) &
             (adjustments[:, :, 3] > 0))
blocked[clearance] = 0
overlay = np.zeros((*blocked.shape, 4), dtype=np.uint8)
overlay[blocked > 0] = (255, 30, 30, 100)

destination = ROOT / "assets/clareira_collision.png"
Image.fromarray(overlay, "RGBA").save(destination)
print(f"Wrote {destination} ({blocked.shape[1]}x{blocked.shape[0]})")

# Black paint identifies branches to render above actors. Compare with the
# unmarked background so naturally dark forest pixels do not enter the mask.
background = np.asarray(Image.open(ROOT / "assets/clareira.png").convert("RGB"))
if background.shape != marked.shape:
    raise ValueError("Marked image and background must have the same size")
difference = np.max(np.abs(marked.astype(np.int16) - background.astype(np.int16)), axis=2)
black = ((marked.max(axis=2) < 30) & (difference > 20)).astype(np.uint8)
count, labels, stats, _ = cv2.connectedComponentsWithStats(black, 8)
branches = np.zeros(black.shape, dtype=np.uint8)
for index in range(1, count):
    if stats[index, cv2.CC_STAT_AREA] >= 500:
        branches[labels == index] = 255
branches[(adjustments[:, :, :3].max(axis=2) < 40) &
         (adjustments[:, :, 3] > 0)] = 255

branch_overlay = np.zeros((*branches.shape, 4), dtype=np.uint8)
branch_overlay[branches > 0] = (0, 0, 0, 255)
branch_destination = ROOT / "assets/clareira_foreground_mask.png"
Image.fromarray(branch_overlay, "RGBA").save(branch_destination)
print(f"Wrote {branch_destination} ({branches.shape[1]}x{branches.shape[0]})")
