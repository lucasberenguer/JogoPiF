"""Cut the learner's transparent seven-frame dodge sheet into game cells."""

from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
source = np.asarray(Image.open(ROOT / "design/protagonista_dash_referencia_v2.png").convert("RGBA"))
if source.shape[:2] != (1254, 1254):
    raise ValueError("Expected the supplied 1254x1254 dodge reference")

front_edges = (0, 179, 358, 537, 716, 895, 1074, 1254)
side_edges = (0, 190, 380, 625, 850, 1075, 1254)
frames_per_direction = 7
cell_width, cell_height = 112, 80
sheet = np.zeros((4 * cell_height, frames_per_direction * cell_width, 4), dtype=np.uint8)

for row in range(4):
    top = round(row * source.shape[0] / 4)
    bottom = round((row + 1) * source.shape[0] / 4)
    edges = front_edges if row < 2 else side_edges
    # The two elongated middle frames in the vertical rows make the hero
    # appear tiny. Use the readable poses and add a compact motion frame below.
    selected = ((0, 0), (1, 1), (5, 3), (6, 4)) if row < 2 else \
               tuple((frame, frame) for frame in range(6))
    for source_frame, frame in selected:
        left = edges[source_frame]
        right = edges[source_frame + 1]
        crop = source[top:bottom, left:right]
        ys, xs = np.where(crop[:, :, 3] >= 32)
        if len(xs) == 0:
            raise ValueError(f"Dodge sprite missing at row {row}, frame {frame}")
        sprite = Image.fromarray(crop[ys.min():ys.max() + 1,
                                      xs.min():xs.max() + 1], "RGBA")
        # Match the 68-pixel body height of the regular idle sprites.
        scale = min(68 / sprite.height, 106 / sprite.width)
        width = max(1, round(sprite.width * scale))
        height = max(1, round(sprite.height * scale))
        sprite = np.asarray(sprite.resize((width, height), Image.Resampling.NEAREST))
        x = frame * cell_width + (cell_width - width) // 2
        y = row * cell_height + 77 - height
        sheet[y:y + height, x:x + width] = sprite

    if row < 2:
        base = sheet[row * cell_height:(row + 1) * cell_height,
                     cell_width:2 * cell_width].copy()
        trail = base.copy()
        trail[:, :, :3] = (trail[:, :, :3].astype(np.float32) * 0.55 +
                          np.array([110, 65, 125]) * 0.45).astype(np.uint8)
        trail[:, :, 3] = (trail[:, :, 3].astype(np.float32) * 0.4).astype(np.uint8)
        canvas = Image.new("RGBA", (cell_width, cell_height))
        canvas.alpha_composite(Image.fromarray(trail, "RGBA"),
                               (0, -7 if row == 0 else 7))
        canvas.alpha_composite(Image.fromarray(base, "RGBA"),
                               (0, 3 if row == 0 else -3))
        sheet[row * cell_height:(row + 1) * cell_height,
              2 * cell_width:3 * cell_width] = np.asarray(canvas)

# Match the normal sprite exactly at the start/end of the dodge so the face
# and eyes do not change on the input frame or the recovery frame.
normal = np.asarray(Image.open(ROOT / "assets/protagonista.png").convert("RGBA"))
for row in range(4):
    idle = normal[row * 80:(row + 1) * 80, :64]
    for frame in (0, 4 if row < 2 else 5):
        sheet[row * cell_height:(row + 1) * cell_height,
              frame * cell_width:(frame + 1) * cell_width] = 0
        x = frame * cell_width + (cell_width - 64) // 2
        sheet[row * cell_height:(row + 1) * cell_height, x:x + 64] = idle

destination = ROOT / "assets/protagonista_dash.png"
Image.fromarray(sheet, "RGBA").save(destination)
print(f"Wrote {destination} ({sheet.shape[1]}x{sheet.shape[0]})")
