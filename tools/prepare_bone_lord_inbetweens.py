"""Refine the Rasga-Ossos body sheets using their original poses and ground pivots."""
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

from prepare_igris_inbetweens import MotionInterpolator, premultiplied_rgba


ROOT = Path(__file__).resolve().parents[1] / "assets" / "rasga_ossos"
SAMPLES = 3
BOTTOM_PAD = 68

# Source grid, ground per row, and the clips consumed by the game.
SHEETS = {
    "idle": ((6, 6, 6, 6), (236, 477, 763, 983), ((0, 6),) * 4, True),
    "marcha": ((6, 6, 6, 6), (242, 486, 752, 998), ((0, 6),) * 4, True),
    "garra": ((8, 8, 8, 8), (223, 463, 719, 956), ((0, 4), (4, 8)) * 4, True),
    "disparo": ((8, 8, 8), (225, 464, 705), ((0, 13), (13, 14), (16, 24)), False),
    "chuva": ((8, 8), (349, 711), ((0, 8), (8, 16)), False),
    "dash": ((6, 5, 5), (222, 462, 692), ((0, 4), (7, 11), (11, 16)), False),
    "explosao": ((8, 8, 8), (240, 481, 685), ((0, 8), (8, 24)), False),
}

# Some authored cells are uneven. Follow the gaps between poses so a claw,
# smoke trail or ground flash cannot appear inside its neighbour's crop.
ROW_EDGES = {"explosao": (0, 217, 467, 724)}
COLUMN_EDGES = {
    "garra": {
        0: (0, 207, 402, 591, 781, 990, 1182, 1362, 1536),
        1: (0, 205, 398, 589, 778, 953, 1177, 1355, 1536),
        2: (0, 204, 401, 592, 779, 987, 1176, 1352, 1536),
        3: (0, 212, 411, 604, 791, 985, 1173, 1362, 1536),
    },
    "dash": {
        0: (0, 310, 625, 940, 1255, 1575, 1900),
        1: (0, 410, 825, 1255, 1705, 2120),
        2: (0, 434, 836, 1254, 1703, 2172),
    },
}


def align_to_ground(name, source, widths, grounds):
    boxes = []
    y_edges = ROW_EDGES.get(name, tuple(i * source.height // len(widths) for i in range(len(widths) + 1)))
    for row, columns in enumerate(widths):
        top, bottom = y_edges[row:row + 2]
        x_edges = COLUMN_EDGES.get(name, {}).get(
            row, tuple(i * source.width // columns for i in range(columns + 1)))
        for left, right in zip(x_edges, x_edges[1:]):
            boxes.append(((left, top, right, bottom), grounds[row] - top))

    # Multiples of four keep the 75% atlas and the draw scale exact.
    width = (max(box[2] - box[0] for box, _ in boxes) + 3) // 4 * 4
    ground = (max(anchor for _, anchor in boxes) + 3) // 4 * 4
    size = (width, ground + BOTTOM_PAD)
    cells = []
    for box, anchor in boxes:
        assert box[3] - box[1] - anchor <= BOTTOM_PAD
        cell = source.crop(box)
        aligned = Image.new("RGBA", size)
        aligned.paste(cell, ((width - cell.width) // 2, ground - anchor))
        cells.append(aligned)
    return cells, tuple(axis * 3 // 4 for axis in size)


def preserve_opacity(frame, current, following, step):
    pixels = np.asarray(frame).copy()
    shared = (current[:, :, 3] >= 240) & (following[:, :, 3] >= 240)
    missing = shared & (pixels[:, :, 3] < 240)
    nearest = current if step * 2 < SAMPLES else following
    pixels[missing] = nearest[missing]
    return Image.fromarray(pixels)


def generate(name, widths, grounds, clips, directional):
    source = Image.open(ROOT / f"{name}.png").convert("RGBA")
    cells, size = align_to_ground(name, source, widths, grounds)
    width, height = size
    columns, rows = max(widths), len(widths)
    atlas = Image.new("RGBA", (width * columns, height * rows * SAMPLES))
    poses = [premultiplied_rgba(cell, size) for cell in cells]
    keys = [np.asarray(cell.resize(size, Image.Resampling.LANCZOS)) for cell in cells]
    interpolator = MotionInterpolator(size)
    starts = [sum(widths[:row]) for row in range(rows)]
    for clip, (start, end) in enumerate(clips):
        if directional:
            offset = starts[clip // (2 if name == "garra" else 1)]
            start, end = start + offset, end + offset
        for index in range(start, end):
            row = max(row for row, offset in enumerate(starts) if offset <= index)
            col = index - starts[row]
            following = index + 1 if index + 1 < end else start if name in ("idle", "marcha") else index
            for step, frame in enumerate(interpolator.frames(poses[index], poses[following], SAMPLES)):
                if step and following != index and name != "dash":
                    frame = preserve_opacity(frame, keys[index], keys[following], step)
                atlas.paste(frame, (col * width, (row + step * rows) * height))
    atlas.save(ROOT / "fluido" / f"{name}.png")
    print(name, atlas.size, flush=True)


if __name__ == "__main__":
    cv2.setNumThreads(4)
    (ROOT / "fluido").mkdir(exist_ok=True)
    for sheet, geometry in SHEETS.items():
        generate(sheet, *geometry)
