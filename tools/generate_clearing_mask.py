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
    blocked, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7))
)
blocked = cv2.GaussianBlur(blocked, (9, 9), 0)
blocked = np.where(blocked >= 128, 255, 0).astype(np.uint8)

adjustments = np.asarray(Image.open(ROOT / "design/clareira_ajustes.png").convert("RGBA"))
if adjustments.shape[:2] != blocked.shape:
    raise ValueError("Adjustment image and collision mask must have the same size")
clearance = ((adjustments[:, :, 1] > 150) &
             (adjustments[:, :, 2] > 150) &
             (adjustments[:, :, 3] > 0))
blocked[clearance] = 0

# The cyan marks trim an oversized outline, but the large lower-right rock
# itself is solid. Fill its visible footprint so the trimmed edge cannot leave
# an opening into the stone. Points follow the rock, not the surrounding grass.
rock = np.array([
    (1380, 686), (1404, 676), (1438, 676), (1467, 691),
    (1481, 718), (1487, 755), (1478, 786), (1457, 804),
    (1380, 805), (1350, 785), (1343, 750), (1351, 714),
], dtype=np.int32)
rock_mask = np.zeros_like(blocked)
cv2.fillPoly(rock_mask, [rock], 255)
# Round the polygon's corners while keeping the stone's main footprint solid.
rock_mask = cv2.GaussianBlur(rock_mask, (11, 11), 0)
blocked[rock_mask >= 128] = 255

# The red outlines alone leave the centers of several stones walkable. Fill
# their visible bases with rounded shapes so actors slide around them without
# catching on the square guide boxes in the reference screenshot.
stone_bases = [
    (345, 202, 43, 43),   # upper-left boulder
    (235, 319, 25, 47),   # upper-left standing stone
    (101, 476, 34, 27),   # middle-left low rock
    (1040, 215, 25, 21), # small stone beside the altar
    (1527, 343, 29, 69), # upper-right standing stone
    (1519, 495, 29, 27), # right small rock
    (1557, 561, 29, 31), # right lower rock
    (320, 742, 26, 52),  # lower-left standing stone
    (418, 827, 56, 51),  # lower-left boulder
    (533, 913, 39, 32),  # lower-left edge stone
    (994, 920, 34, 26),  # bottom edge stone
]
stones = np.zeros_like(blocked)
for x, y, radius_x, radius_y in stone_bases:
    cv2.ellipse(stones, (x, y), (radius_x, radius_y), 0, 0, 360, 255, -1)
stones = cv2.GaussianBlur(stones, (9, 9), 0)
blocked[stones >= 128] = 255
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
