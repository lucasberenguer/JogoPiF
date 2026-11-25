"""Generate Igris animation atlases. Requires OpenCV, NumPy and Pillow."""
import argparse
from pathlib import Path

import cv2
import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1] / "assets" / "igris"
# Columns, rows, samples per pose, loop, sequential rows.
SHEETS = {
    "idle": (8, 4, 3, True, False),
    "walk": (7, 4, 3, True, False),
    "melee": (8, 4, 3, False, False),
    "eruption": (6, 4, 3, False, False),
    "spears": (8, 4, 3, False, False),
    "vortex": (6, 4, 3, False, False),
    "final_idle": (4, 4, 3, True, False),
    "final_walk": (4, 4, 3, True, False),
    "final_melee": (4, 4, 4, False, False),
    "abyss_grab": (6, 4, 3, False, False),
    "echoes": (6, 4, 3, False, False),
    **{f"z_{side}": (4, 3, 3, False, True) for side in ("down", "up", "left", "right")},
}
EFFECTS = {
    "eclipse_fx": (8, 4, 3, False, False),
    "eruption_fx": (6, 4, 3, False, True),
    "spears_fx": (6, 4, 3, False, False),
    "vortex_fx": (6, 4, 3, False, False),
    "abyss_grab_fx": (6, 4, 3, False, False),
    "echoes_fx": (6, 4, 3, False, False),
}


def eruption_cell(source, row, col):
    edges = (
        (0, 224, 448, 686, 925, 1168, 1451),
        (0, 231, 459, 695, 936, 1193, 1536),
        (0, 242, 477, 716, 956, 1216, 1536),
        (0, 283, 525, 776, 1020, 1280, 1536),
    )
    top = (0, 224, 501, 740)[row]
    ground = (214, 490, 729, 970)[row]
    cell = source.crop((edges[row][col], top, edges[row][col + 1], ground + 9))
    # Keep the ground line and horizontal center fixed across irregular crops.
    aligned = Image.new("RGBA", (288, 284))
    aligned.paste(cell, ((288 - cell.width) // 2, 272 - (ground - top)))
    return aligned


def premultiplied_rgba(cell, size):
    image = np.asarray(cell.resize(size, Image.Resampling.LANCZOS)).astype(np.float32) / 255
    image[:, :, :3] *= image[:, :, 3:]
    return image


class MotionInterpolator:
    def __init__(self, size):
        width, height = size
        x, y = np.meshgrid(np.arange(width, dtype=np.float32), np.arange(height, dtype=np.float32))
        self.grid = np.stack((x, y), axis=-1)
        self.flow = cv2.DISOpticalFlow_create(cv2.DISOPTICAL_FLOW_PRESET_MEDIUM)

    @staticmethod
    def luminance(pose):
        gray = cv2.cvtColor(pose[:, :, :3], cv2.COLOR_RGB2GRAY)
        return np.clip((gray + .18 * pose[:, :, 3]) * 255, 0, 255).astype(np.uint8)

    def frames(self, current, following, samples):
        a, b = self.luminance(current), self.luminance(following)
        forward, backward = self.flow.calc(a, b, None), self.flow.calc(b, a, None)
        for step in range(samples):
            phase = step / samples
            if step == 0:
                image = current.copy()
            else:
                start = cv2.remap(current, self.grid - phase * forward, None, cv2.INTER_LINEAR)
                end = cv2.remap(following, self.grid - (1 - phase) * backward, None, cv2.INTER_LINEAR)
                image = start * (1 - phase) + end * phase
            image[:, :, :3] /= np.maximum(image[:, :, 3:], .001)
            rgba = np.clip(np.rint(image * 255), 0, 255).astype(np.uint8)
            yield Image.fromarray(rgba)


def generate(name, columns, rows, samples, loop, sequential_rows):
    source = Image.open(ROOT / f"{name}.png").convert("RGBA")
    original_width, original_height = source.width // columns, source.height // rows
    if name == "eruption_fx":
        original_width, original_height = 288, 284
    width, height = round(original_width * .75), round(original_height * .75)
    poses = []
    for row in range(rows):
        for col in range(columns):
            x, y = col * original_width, row * original_height
            if name == "eruption_fx":
                cell = eruption_cell(source, row, col)
            elif name == "eclipse_fx":
                left = round(col * source.width / columns)
                right = round((col + 1) * source.width / columns)
                cell = source.crop((left, y, right, y + original_height))
            else:
                cell = source.crop((x, y, x + original_width, y + original_height))
            poses.append(premultiplied_rgba(cell, (width, height)))
    atlas = Image.new("RGBA", (width * columns, height * rows * samples))
    interpolator = MotionInterpolator((width, height))
    for index, current in enumerate(poses):
        row, col = divmod(index, columns)
        at_end = col == columns - 1 and not sequential_rows or index == len(poses) - 1
        following = poses[row * columns if loop else index] if at_end else poses[index + 1]
        if name == "spears_fx" and col == 4:
            following = poses[row * columns + 2]
        for step, image in enumerate(interpolator.frames(current, following, samples)):
            atlas.paste(image, (col * width, (row + step * rows) * height))
    atlas.save(ROOT / "fluido" / f"{name}.png")
    print(name, atlas.size, flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--effects", action="store_true", help="Generate only the separate effect atlases")
    args = parser.parse_args()
    cv2.setNumThreads(4)
    (ROOT / "fluido").mkdir(exist_ok=True)
    for sheet, geometry in (EFFECTS if args.effects else {**SHEETS, **EFFECTS}).items():
        generate(sheet, *geometry)
