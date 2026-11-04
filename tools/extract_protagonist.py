"""Extract four directional animation rows from the supplied concept sheet."""

from pathlib import Path

import cv2
import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
source = np.asarray(Image.open(ROOT / "design/protagonista_referencia.png").convert("RGB"))
centers = [
    (307, 373, 440, 506),    # front / down
    (623, 690, 756, 823),    # back / up
    (927, 995, 1063, 1131),  # left
    (1255, 1323, 1392, 1460),  # right
]
cell_width, cell_height = 64, 80
sheet = np.zeros((4 * cell_height, 5 * cell_width, 4), dtype=np.uint8)

for row, xs in enumerate(centers):
    # Column 0 is the stable idle pose; columns 1-4 are walking frames.
    poses = [(345, 261)] + [(center_x, 89) for center_x in xs] if row == 0 else \
            [(xs[0], 89)] + [(center_x, 89) for center_x in xs]
    for frame, (center_x, top) in enumerate(poses):
        crop = source[top:top + 74, center_x - 29:center_x + 29]
        # Each portrait sits on a nearly uniform dark panel. Compare each
        # pixel with the median panel color and retain the character silhouette.
        border = np.concatenate((crop[:5].reshape(-1, 3),
                                 crop[-5:].reshape(-1, 3),
                                 crop[:, :5].reshape(-1, 3),
                                 crop[:, -5:].reshape(-1, 3)))
        panel = np.median(border, axis=0)
        distance = np.max(np.abs(crop.astype(np.float32) - panel), axis=2)
        foreground = (distance > 22).astype(np.uint8)
        count, labels, stats, _ = cv2.connectedComponentsWithStats(foreground, 8)
        if count <= 1:
            raise ValueError(f"Sprite missing at row {row}, frame {frame}")
        silhouette = np.zeros(foreground.shape, dtype=np.uint8)
        minimum_area = 25 if row >= 2 else 10
        for component in range(1, count):
            if stats[component, cv2.CC_STAT_AREA] >= minimum_area:
                silhouette[labels == component] = 255
        silhouette = cv2.morphologyEx(silhouette, cv2.MORPH_CLOSE,
                                     cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9)))
        silhouette = cv2.dilate(silhouette, np.ones((3, 3), np.uint8))
        # Black outlines and interior holes belong to the character.
        contours, _ = cv2.findContours(silhouette, cv2.RETR_EXTERNAL,
                                       cv2.CHAIN_APPROX_SIMPLE)
        cv2.drawContours(silhouette, contours, -1, 255, cv2.FILLED)
        rgba = np.dstack((crop, silhouette))
        ys, x_pixels = np.where(silhouette > 0)
        left, right = x_pixels.min(), x_pixels.max() + 1
        top_pixel, bottom = ys.min(), ys.max() + 1
        sprite = rgba[top_pixel:bottom, left:right]
        x = frame * cell_width + (cell_width - sprite.shape[1]) // 2
        y = row * cell_height + 75 - sprite.shape[0]
        sheet[y:y + sprite.shape[0], x:x + sprite.shape[1]] = sprite

def place_generated(art, section_index, row, column):
    section_width = art.shape[1] // 4
    section = art[:, section_index * section_width:(section_index + 1) * section_width]
    ys, xs = np.where(section[:, :, 3] >= 128)
    if len(xs) == 0:
        raise ValueError(f"Generated pose missing at section {section_index}")
    sprite = Image.fromarray(section[ys.min():ys.max() + 1,
                                     xs.min():xs.max() + 1], "RGBA")
    height = 68
    width = round(sprite.width * height / sprite.height)
    sprite = sprite.resize((width, height), Image.Resampling.NEAREST)
    pixels = np.asarray(sprite).copy()
    pixels[:, :, 3] = np.where(pixels[:, :, 3] >= 128, 255, 0)
    x = column * cell_width + (cell_width - width) // 2
    y = row * cell_height + 75 - height
    sheet[row * cell_height:(row + 1) * cell_height,
          column * cell_width:(column + 1) * cell_width] = 0
    sheet[y:y + height, x:x + width] = pixels


# New stationary poses replace walking frames reused as idle; new back-facing
# walk frames fix the detached hair in every frame, not only while stationary.
idle_art = np.asarray(Image.open(ROOT / "design/protagonista_idle_gerado.png").convert("RGBA"))
for row in range(4):
    place_generated(idle_art, row, row, 0)

back_walk = np.asarray(Image.open(ROOT / "design/protagonista_costas_andando.png").convert("RGBA"))
for frame in range(4):
    place_generated(back_walk, frame, 1, frame + 1)

destination = ROOT / "assets/protagonista.png"
Image.fromarray(sheet, "RGBA").save(destination)
print(f"Wrote {destination} ({sheet.shape[1]}x{sheet.shape[0]})")
