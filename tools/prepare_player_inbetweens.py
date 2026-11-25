"""Generate smoother player sprites from the original directional poses."""
import argparse
from pathlib import Path

import cv2
import numpy as np
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

# Clarões de quadros vizinhos que cruzavam as bordas das células do disparo.
GUN_SPILL = {
    (1, 3): (0, 29, 8, 41), (1, 4): (0, 22, 11, 44), (1, 5): (0, 26, 10, 39),
    (2, 2): (53, 37, 64, 52), (2, 3): (55, 41, 64, 52),
    (3, 3): (0, 35, 11, 54), (3, 4): (0, 30, 15, 58), (3, 5): (0, 36, 9, 51),
}

OPAQUE = 224
SHARP_ACTIONS = {"arma_fogo_disparo", "biblia_ativacao_nova"}
GUN_BODY_SHEETS = {"arma_fogo_caminhada", "arma_fogo_disparo"}


def largest_connected_area(mask):
    _, _, stats, _ = cv2.connectedComponentsWithStats(mask.astype(np.uint8))
    return int(stats[1:, cv2.CC_STAT_AREA].max(initial=0))


def keep_body_intact(frame, current, following, step):
    """Keep shared opaque pixels and reject poses that break the body's silhouette."""
    nearest = current if step * 2 < SAMPLES else following
    current_opaque = current[:, :, 3] >= OPAQUE
    following_opaque = following[:, :, 3] >= OPAQUE
    pixels = np.array(frame)
    shared = current_opaque & following_opaque
    missing = shared & (pixels[:, :, 3] < OPAQUE)
    pixels[missing] = nearest[missing]

    # A large change in pose can separate the head or limbs during interpolation.
    # In these transitions, reuse the nearest intact key pose.
    solid = pixels[:, :, 3] >= 128
    baseline = min(largest_connected_area(current[:, :, 3] >= 128),
                   largest_connected_area(following[:, :, 3] >= 128))
    reference_area = min(current_opaque.sum(), following_opaque.sum())
    poor_overlap = reference_area and shared.sum() < reference_area * .55
    if poor_overlap or (baseline and largest_connected_area(solid) < baseline * .90):
        return Image.fromarray(nearest), int(missing.sum()), True
    return Image.fromarray(pixels), int(missing.sum()), False


def repair_gun_body(frame, interpolated=False):
    """Restore dark body pixels removed from the source transparency mask."""
    pixels = np.array(frame)
    alpha = pixels[:, :, 3]
    solid = alpha >= 144
    _, labels, stats, _ = cv2.connectedComponentsWithStats((~solid).astype(np.uint8))
    height, width = solid.shape
    fill = np.zeros_like(solid)
    for index, (x, y, w, h, area) in enumerate(stats[1:], 1):
        # Closed gaps in the silhouette are missing shadow pixels. Open gaps
        # between limbs and the soft edges of the muzzle flash stay untouched.
        if x > 0 and y > 0 and x+w < width and y+h < height and area <= 96:
            fill |= labels == index

    neck = np.s_[30:45, 14:53]
    # The dark seam between the hair and the collar also has open gaps.
    # Bridge pixels only when there is solid art above and below them.
    bridged = cv2.morphologyEx(solid.astype(np.uint8), cv2.MORPH_CLOSE,
                              np.ones((7, 1), dtype=np.uint8)).astype(bool)
    fill[neck] |= bridged[neck] & ~solid[neck]
    legs = np.s_[60:75, 19:47]
    fill[legs] |= bridged[legs] & ~solid[legs]
    torso = np.s_[44:60, 19:47]
    closed_torso = cv2.morphologyEx(solid.astype(np.uint8), cv2.MORPH_CLOSE,
                                   cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 5))).astype(bool)
    fill[torso] |= closed_torso[torso] & ~solid[torso]
    missing_color = fill & (alpha < 64)
    pixels[missing_color, :3] = (24, 11, 14)
    alpha[fill] = 255
    if interpolated:
        # A pixel-art body cannot fade as it moves between opaque poses.
        alpha[:] = np.where(solid | fill, 255, 0)
    return Image.fromarray(pixels)


def repair_katana_neck(frame):
    """Recover short gaps between the hair or jaw and the red collar."""
    pixels = np.array(frame)
    solid = pixels[:, :, 3] >= 144
    # Only cover the central head/collar seam, away from the blade and cape outline.
    for x in range(68, 94):
        upper = None
        for y in range(54, 79):
            if not solid[y, x]:
                continue
            if upper is not None and 1 < y - upper <= 7:
                shoulder = pixels[y, x, :3].astype(int)
                if shoulder[0] >= shoulder[1] + 14 and shoulder[0] >= shoulder[2] + 7:
                    # Require opaque neighbors in the collar, not a passing sword pixel.
                    if solid[y:y+3, x-1:x+2].sum() >= 6:
                        missing = pixels[upper+1:y, x]
                        dark = np.minimum(pixels[upper, x, :3], shoulder)
                        missing[missing[:, 3] < 64, :3] = np.maximum(dark, (23, 10, 15))
                        missing[:, 3] = 255
            upper = y
    return Image.fromarray(pixels)


def generate(name, size, loop, ends):
    source = Image.open(ASSETS / f"{name}.png").convert("RGBA")
    width, height = size
    rows = 1 if name == "biblia_orbita" else ROWS
    assert source.width % width == 0 and source.height == rows * height, name
    columns = source.width // width
    samples = 1 if name in SHARP_ACTIONS else SAMPLES
    atlas = Image.new("RGBA", (source.width, source.height * samples))
    interpolator = MotionInterpolator(size)
    repaired = 0
    reused = 0
    for row in range(rows):
        end = ends[row] if ends else columns
        poses = []
        keys = []
        for col in range(end):
            cell = source.crop((col * width, row * height, (col + 1) * width, (row + 1) * height))
            spill = GUN_SPILL.get((row, col)) if name == "arma_fogo_disparo" else None
            if spill:
                cell.paste((0, 0, 0, 0), spill)
            if name in GUN_BODY_SHEETS:
                cell = repair_gun_body(cell)
            if name == "biblia_ativacao_nova" and 3 <= col <= 5:
                cell.paste((0, 0, 0, 0), (0, 0, 24, height))
            if samples == 1:
                atlas.paste(cell, (col * width, row * height))
                continue
            if name == "katana_golpe_2" and (row, col) == (3, 3):
                # O desenho estava 32 pixels acima das outras poses e tinha
                # um pedaço solto do efeito perto da borda inferior.
                aligned = Image.new("RGBA", size)
                aligned.paste(cell.crop((0, 0, width, 80)), (0, 32))
                cell = aligned
            keys.append(np.asarray(cell))
            poses.append(premultiplied_rgba(cell, size))
        for col, pose in enumerate(poses):
            if loop is not None and col < loop:
                following = col
            elif col + 1 < end:
                following = col + 1
            else:
                following = loop if loop is not None else col
            for step, frame in enumerate(interpolator.frames(pose, poses[following], SAMPLES)):
                if step and following != col and name != "biblia_orbita":
                    frame, restored, fallback = keep_body_intact(frame, keys[col], keys[following], step)
                    repaired += restored
                    reused += fallback
                if step and name == "arma_fogo_caminhada":
                    frame = repair_gun_body(frame, interpolated=True)
                if name.startswith("katana_"):
                    frame = repair_katana_neck(frame)
                atlas.paste(frame, (col * width, (row + step * rows) * height))
    atlas.save(OUTPUT / f"{name}.png")
    print(f"{name}: {repaired} pixels corrigidos, {reused} quadros retomam a pose original")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sheets", nargs="*", metavar="SHEET", help="Regenerate only these sheets")
    args = parser.parse_args()
    unknown = set(args.sheets) - SHEETS.keys()
    if unknown:
        parser.error(f"unknown sheets: {', '.join(sorted(unknown))}")
    cv2.setNumThreads(4)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for sheet in args.sheets or SHEETS:
        generate(sheet, *SHEETS[sheet])
