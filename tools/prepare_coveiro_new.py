"""Organiza as folhas do Coveiro em atlas de células fixas."""
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
# Largura/altura de referência, escala do corpo, colunas, linhas e posição do chão.
SHEETS = {
    "idle": (1536, 1024, 1, 4, 4, (225, 482, 722, 979)),
    "run": (1536, 1024, 205/155, 10, 4, (215, 452, 683, 919)),
    "melee": (1536, 1024, 205/160, 10, 4, (214, 442, 683, 919)),
    "emerge": (1600, 800, 205/122, 14, 4, (167, 347, 532, 724)),
    "pull": (1774, 887, 205/175, 8, 4, (183, 414, 630, 854)),
    "pull_effect": (2172, 724, 1, 8, 2, (274, 610)),
    "charge": (2172, 724, 205/150, 9, 4, (151, 336, 502, 687)),
    "spin": (2172, 724, 205/150, 10, 4, (159, 336, 516, 695)),
    "fall": (1774, 887, 1.2, 7, 4, (207, 422, 636, 843)),
}
HORIZONTAL = {
    "idle": (250, 510, 770, 1030, 1290),
    "emerge": (25, 133, 251, 372, 489, 602, 714, 826, 937, 1046, 1155, 1260, 1372, 1484, 1600),
    "charge": (85, 270, 485, 716, 942, 1193, 1443, 1690, 1940, 2172),
    "spin": (100, 255, 460, 657, 852, 1058, 1290, 1545, 1774, 1983, 2172),
    "fall": (0, 230, 451, 680, 919, 1180, 1450, 1774),
}
ROW_HORIZONTAL = {
    ("charge", 2): (90, 268, 487, 721, 1008, 1285, 1533, 1757, 1980, 2172),
    ("charge", 3): (90, 270, 486, 715, 985, 1263, 1493, 1727, 1978, 2172),
    ("spin", 3): (100, 257, 458, 659, 882, 1069, 1319, 1574, 1777, 1985, 2172),
    ("emerge", 2): (25, 133, 262, 386, 502, 624, 744, 865, 984, 1106, 1225, 1343, 1467, 1600),
    ("melee", 1): (185, 320, 456, 586, 715, 885, 1007, 1150, 1286, 1400, 1536),
}
ROW_HORIZONTAL["emerge", 3] = ROW_HORIZONTAL["emerge", 2]
VERTICAL = {
    "idle": (0, 248, 505, 747, 1024),
    "run": (0, 253, 493, 725, 1024),
    "melee": (0, 249, 478, 720, 1024),
    "fall": (64, 224, 438, 647, 887),
}
EMERGE_ROWS = ((67, 185), (247, 365), (429, 554), (623, 760))


def opaque_alpha(rgb, background):
    seed = np.uint8(np.max(np.abs(rgb.astype(np.int16) - background), axis=2) >= 17) * 255
    closed = cv2.morphologyEx(seed, cv2.MORPH_CLOSE, np.ones((5, 5), np.uint8))
    contours, _ = cv2.findContours(closed, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    alpha = np.zeros_like(seed)
    cv2.drawContours(alpha, [c for c in contours if cv2.contourArea(c) >= 2], -1, 255, cv2.FILLED)
    return alpha


def prepare(name, width, height, scale, columns, rows, ground):
    with Image.open(ROOT / "design" / "coveiro_novo" / f"{name}.png") as original:
        has_alpha = "A" in original.getbands()
        sx, sy = original.width / width, original.height / height
        pixels = np.array(original.convert("RGBA"))
    scale /= sy
    if not has_alpha:
        background = np.median(pixels[:, -3:, :3], axis=(0, 1)).astype(np.int16)
    elif sx < 1:
        # Remove somente pontos de exportação de até três pixels, longe da arte.
        alpha = pixels[:, :, 3]
        _, labels, stats, _ = cv2.connectedComponentsWithStats(np.uint8(alpha > 0))
        area = stats[:, cv2.CC_STAT_AREA][labels]
        near = cv2.dilate(np.uint8((labels > 0) & (area > 3)), np.ones((11, 11), np.uint8))
        alpha[(area <= 3) & (near == 0)] = 0

    cell_height = 256 if name == "pull_effect" else 288
    output = Image.new("RGBA", (columns * 400, rows * cell_height))
    left = 180 if name in ("run", "melee") else 0
    default_edges = np.linspace(left, width, columns + 1)
    height_edges = VERTICAL.get(name, np.linspace(0, height, rows + 1))
    for row in range(rows):
        edges = ROW_HORIZONTAL.get((name, row), HORIZONTAL.get(name, default_edges))
        y_range = EMERGE_ROWS[row] if name == "emerge" else height_edges[row:row+2]
        y0, y1 = (round(v * sy) for v in y_range)
        if name == "pull":
            coverage = pixels[y0:y1, :, 3].sum(axis=0, dtype=np.int64)
            limits = [round(edge * sx) for edge in edges]
            pull_edges = [limits[0]]
            for limit in limits[1:-1]:
                gap = min(range(limit - 18, limit + 19),
                          key=lambda x: (coverage[x-1] + coverage[x], abs(x-limit)))
                pull_edges.append(gap)
            pull_edges.append(limits[-1])
        for col in range(columns):
            index = min(col, len(edges) - 2)
            x0, x1 = (round(v * sx) for v in edges[index:index+2])
            center = (x0 + x1) / 2
            if name == "pull":
                x0, x1 = pull_edges[col:col+2]
            if name == "melee" and row == 1 and col == 5:
                x0 = round(857 * sx)
            tile = pixels[y0:y1, x0:x1].copy()
            if not has_alpha:
                tile[:, :, 3] = opaque_alpha(tile[:, :, :3], background)
            if name == "melee" and row == 1:
                if col == 4:
                    tile[round(353 * sy) - y0:, round(857 * sx) - x0:, 3] = 0
                if col == 5:
                    tile[:round(353 * sy) - y0, :round(885 * sx) - x0, 3] = 0
            art = Image.fromarray(tile)
            box = art.getbbox(alpha_only=True)
            if not box:
                raise ValueError(f"{name}: quadro vazio na linha {row+1}, coluna {col+1}")
            art = art.crop(box)
            art = art.resize(tuple(round(n * scale) for n in art.size), Image.Resampling.NEAREST)
            if art.width > 400 or art.height > cell_height:
                raise ValueError(f"{name}: quadro excede a célula na linha {row+1}, coluna {col+1}")
            x = round(200 + (x0 + box[0] - center) * scale)
            y = round(cell_height - 16 + (y0 + box[1] - ground[row] * sy) * scale)
            output.paste(art, (400 * col + min(max(x, 0), 400-art.width),
                               cell_height * row + min(max(y, 0), cell_height-art.height)))
    output.save(ROOT / "assets" / f"coveiro_novo_{name}.png", optimize=True)


if __name__ == "__main__":
    for name, parameters in SHEETS.items():
        prepare(name, *parameters)
