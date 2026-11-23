"""Generate smoother player sprites from the original directional poses."""
from pathlib import Path

import cv2
from PIL import Image

from prepare_igris_inbetweens import MotionInterpolator, premultiplied_rgba


ASSETS = Path(__file__).resolve().parents[1] / "assets"
OUTPUT = ASSETS / "protagonista" / "fluido"
SAMPLES = 3
ROWS = 4

# Cell size, loop start, and optional end of the used poses in each row.
SHEETS = {
    "protagonista": ((64, 80), 1, None),
    "protagonista_dash": ((112, 80), None, (5, 5, 6, 6)),
    "katana_idle_novo": ((160, 112), 0, None),
    "katana_caminhada": ((160, 112), 0, None),
    "katana_golpe_1": ((160, 112), None, None),
    "katana_golpe_2": ((160, 112), None, None),
    "katana_golpe_3": ((160, 112), None, None),
    "arma_fogo_idle_novo": ((64, 80), 0, None),
    "arma_fogo_caminhada": ((64, 80), 0, None),
    "arma_fogo_disparo": ((64, 80), None, None),
    "biblia_idle": ((64, 80), 0, None),
    "biblia_caminhada": ((64, 80), 0, None),
    "biblia_ativacao_nova": ((128, 112), None, None),
    "biblia_orbita": ((80, 80), 0, None),
}


def generate(name, size, loop, ends):
    source = Image.open(ASSETS / f"{name}.png").convert("RGBA")
    width, height = size
    rows = 1 if name == "biblia_orbita" else ROWS
    assert source.width % width == 0 and source.height == rows * height, name
    columns = source.width // width
    atlas = Image.new("RGBA", (source.width, source.height * SAMPLES))
    interpolator = MotionInterpolator(size)
    for row in range(rows):
        end = ends[row] if ends else columns
        poses = [
            premultiplied_rgba(source.crop((col * width, row * height,
                                           (col + 1) * width, (row + 1) * height)), size)
            for col in range(end)
        ]
        for col, pose in enumerate(poses):
            if loop is not None and col < loop:
                following = pose
            elif col + 1 < end:
                following = poses[col + 1]
            else:
                following = poses[loop] if loop is not None else pose
            if name == "arma_fogo_disparo" and col == 1:
                following = pose  # O clarão começa junto do disparo no quadro 2.
            for step, frame in enumerate(interpolator.frames(pose, following, SAMPLES)):
                atlas.paste(frame, (col * width, (row + step * rows) * height))
    atlas.save(OUTPUT / f"{name}.png")


if __name__ == "__main__":
    cv2.setNumThreads(4)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for sheet, geometry in SHEETS.items():
        generate(sheet, *geometry)
