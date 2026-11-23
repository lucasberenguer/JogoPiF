"""Organiza as folhas transparentes do Coveiro sem rótulos nos atlas do jogo."""
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
DENSITY = 11 / 8
# Bordas aproximadas de coluna, faixas de linha, chão e altura de referência do corpo.
SHEETS = {
    "idle": ([0, 320, 603, 883, 1195], [(0, 320), (320, 643), (643, 953), (953, 1316)],
             [285, 609, 927, 1248], 268),
    "run": ([0, 187, 350, 511, 675, 835, 1000, 1164, 1327, 1535],
            [(0, 253), (253, 493), (493, 745), (745, 1025)], [226, 471, 715, 962], 185),
    "melee": ([0, 151, 286, 417, 546, 674, 805, 945, 1110, 1246, 1386, 1535],
              [(0, 263), (263, 503), (503, 751), (751, 1025)], [242, 484, 726, 977], 151),
    "emerge": ([20, 148, 277, 407, 533, 662, 781, 900, 1025, 1149, 1270, 1394, 1520, 1640, 1774],
               [(84, 224), (297, 438), (501, 647), (716, 872)], [207, 420, 627, 840], 119),
    "fall": ([0, 230, 451, 680, 919, 1180, 1450, 1774],
             [(65, 225), (279, 441), (499, 653), (704, 887)], [207, 428, 638, 846], 142),
    "pull": (list(np.linspace(0, 1774, 10)), [(0, 207), (207, 432), (432, 641), (641, 887)],
             [181, 399, 604, 820], 151),
    "pull_effect": (list(np.linspace(0, 2172, 9)), [(0, 350), (350, 724)], [279, 617], 205),
    "charge": ([0, 270, 485, 710, 962, 1221, 1460, 1710, 1960, 2172],
               [(0, 182), (182, 363), (363, 530), (530, 724)], [151, 340, 500, 690], 140),
    "spin": ([100, 251, 430, 600, 770, 941, 1110, 1287, 1470, 1655, 1825, 2005, 2172],
             [(0, 173), (173, 350), (350, 530), (530, 724)], [153, 335, 502, 685], 130),
}
ROW_EDGES = {
    ("charge", 2): [0, 270, 498, 731, 1005, 1271, 1527, 1770, 1983, 2172],
    ("charge", 3): [0, 270, 494, 724, 987, 1264, 1513, 1740, 1980, 2172],
    ("pull_effect", 1): [0, 284, 555, 832, 1100, 1370, 1640, 1900, 2172],
}


def prepare(name, edges, rows, ground, body_height):
    with Image.open(ROOT / "design" / "coveiro_novo" / f"{name}.png") as image:
        pixels = np.asarray(image.convert("RGBA"))
    width, height = 550, 352 if name == "pull_effect" else 396
    scale = min(1.0, 205 * DENSITY / body_height) if name == "idle" else 205 * DENSITY / body_height
    if name == "pull_effect":
        scale = DENSITY
    atlas = Image.new("RGBA", ((len(edges) - 1) * width, len(rows) * height))
    for row, (top, bottom) in enumerate(rows):
        limits = ROW_EDGES.get((name, row), edges)
        coverage = (pixels[top:bottom, :, 3] > 48).sum(axis=0)
        cuts = [round(limits[0])]
        for edge in limits[1:-1]:
            center = round(edge)
            cuts.append(min(range(center-18, center+19),
                            key=lambda x: coverage[x-1]+coverage[x]+abs(x-center)*.35))
        cuts.append(round(limits[-1]))
        for col, (left, right) in enumerate(zip(cuts, cuts[1:])):
            tile = Image.fromarray(pixels[top:bottom, left:right])
            box = tile.getbbox(alpha_only=True)
            if box is None:
                raise ValueError(f"{name}: quadro vazio na linha {row+1}, coluna {col+1}")
            zoom = min(scale, width / (box[2]-box[0]), height / (box[3]-box[1]))
            art = tile.crop(box).resize(
                tuple(round(n*zoom) for n in (box[2]-box[0], box[3]-box[1])),
                Image.Resampling.NEAREST)
            x = round((width-art.width)/2)
            y = round(height-22+(top+box[1]-ground[row])*zoom)
            atlas.paste(art, (col*width+x, row*height+min(max(y,0),height-art.height)))
    atlas.save(ROOT / "assets" / f"coveiro_novo_{name}.png", optimize=True)


if __name__ == "__main__":
    for name, parameters in SHEETS.items():
        prepare(name, *parameters)
