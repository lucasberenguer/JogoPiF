"""Generate the Clearing Guardian's in-betweens from the original sprites."""
from pathlib import Path

import cv2
from PIL import Image

from prepare_igris_inbetweens import MotionInterpolator, premultiplied_rgba


ASSETS = Path(__file__).resolve().parents[1] / "assets"
OUTPUT = ASSETS / "guardiao" / "fluido"
SAMPLES = 4

# Each row uses its own ground line and pivots in the original sheet.
SPAWN_ROWS = (
    (0, 234, 221, (0, 224, 452, 674, 937, 1201, 1536), (116, 343, 571, 819, 1078, 1348)),
    (234, 474, 462, (0, 242, 481, 737, 984, 1236, 1536), (118, 362, 619, 868, 1122, 1374)),
    (474, 702, 692, (0, 254, 523, 788, 1055, 1288, 1536), (140, 397, 674, 938, 1188, 1433)),
    (702, 940, 924, (0, 273, 529, 784, 1049, 1278, 1536), (155, 407, 680, 930, 1181, 1408)),
)
DEFEAT_PIVOTS = (158, 334, 510, 699, 891, 1084, 1271, 1454)
DEFEAT_ROWS = (
    (0, 267, 251, (74, 248, 420, 609, 795, 988, 1176, 1364, 1536), DEFEAT_PIVOTS),
    (267, 523, 508, (73, 248, 420, 608, 795, 988, 1178, 1364, 1536), DEFEAT_PIVOTS),
    (523, 779, 763, (94, 251, 420, 607, 793, 987, 1179, 1366, 1536), DEFEAT_PIVOTS),
    (779, 1024, 1005, (78, 250, 420, 607, 797, 990, 1181, 1363, 1536), DEFEAT_PIVOTS),
)


def original(suffix):
    return Image.open(ASSETS / f"boss_clareira{suffix}.png").convert("RGBA")


def aligned_rows(source, geometry, size):
    for top, bottom, ground, edges, pivots in geometry:
        row = []
        for left, right, pivot in zip(edges, edges[1:], pivots):
            cell = Image.new("RGBA", size)
            cell.paste(source.crop((left, top, right, bottom)),
                       (size[0] // 2 - (pivot - left), 256 - (ground - top)))
            row.append(cell)
        yield row


def generate(name, rows, columns, loop=False, sequential=False):
    size = tuple(round(axis * .75) for axis in rows[0][0].size)
    width, height = size
    count = (len(rows[0]) + columns - 1) // columns if sequential else len(rows)
    atlas = Image.new("RGBA", (columns * width, count * SAMPLES * height))
    interpolator = MotionInterpolator(size)
    for row, cells in enumerate(rows):
        poses = [premultiplied_rgba(cell, size) for cell in cells]
        for index, current in enumerate(poses):
            following = poses[index + 1] if index + 1 < len(poses) else poses[0] if loop else current
            sheet_row, col = divmod(index, columns) if sequential else (row, index)
            for step, frame in enumerate(interpolator.frames(current, following, SAMPLES)):
                atlas.paste(frame, (col * width, (sheet_row + step * count) * height))
    atlas.save(OUTPUT / f"{name}.png")
    print(name, atlas.size, flush=True)


def grid(name, suffix, columns, count, loop=False, row_lengths=None):
    source = original(suffix)
    width, height = source.width // columns, source.height // count
    lengths = row_lengths or [columns] * count
    rows = [[source.crop((i * width, j * height, (i + 1) * width, (j + 1) * height))
             for i in range(length)] for j, length in enumerate(lengths)]
    generate(name, rows, columns, loop)


if __name__ == "__main__":
    cv2.setNumThreads(4)
    OUTPUT.mkdir(exist_ok=True, parents=True)
    grid("espera", "", 6, 4, loop=True)
    grid("marcha", "_andar", 8, 4, loop=True)
    grid("golpe", "_corpo_a_corpo", 8, 4)
    grid("conjuracao", "_especial_1", 9, 4)
    spawn = [cell for row in aligned_rows(original("_spawn"), SPAWN_ROWS, (384, 288)) for cell in row]
    del spawn[13]  # Esta pose não fazia parte da sequência de subida.
    generate("emergencia", [spawn], 6, sequential=True)
    generate("queda", list(aligned_rows(original("_derrota"), DEFEAT_ROWS, (224, 288))), 8)
    grid("raizes", "_raizes_area", 8, 1)
    grid("onda", "_onda", 8, 2)
    grid("maldicao", "_maldicao", 8, 2, row_lengths=(8, 7))
