"""Extract the eight-frame katana walk, keeping dark interior pixels opaque."""
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
source = np.array(Image.open(ROOT / "design/katana_caminhada_referencia.png").convert("RGB"))
bands = [(60, 215), (285, 437), (520, 668), (750, 896)]
sheet = Image.new("RGBA", (8 * 160, 4 * 112))
for direction, row in enumerate((0, 1, 3, 2)):
    top, bottom = bands[row]
    scale = 68.0 / (140, 136, 130, 128)[row]
    for frame in range(8):
        left = 58 + frame * 182
        rgb = source[top:bottom, left:left + 182].copy()
        visible = np.uint8(rgb.max(axis=2) > 18) * 255
        closed = cv2.morphologyEx(visible, cv2.MORPH_CLOSE, np.ones((3, 3), np.uint8))
        flood = closed.copy()
        cv2.floodFill(flood, None, (0, 0), 255)
        alpha = closed | cv2.bitwise_not(flood)
        count, labels, stats, _ = cv2.connectedComponentsWithStats(alpha, 8)
        if count > 1:
            keep = np.flatnonzero(stats[:, cv2.CC_STAT_AREA] >= 100)
            keep = keep[keep != 0]
            alpha = np.uint8(np.isin(labels, keep)) * 255
        yy, xx = np.indices(alpha.shape)
        head = (alpha > 128) & (yy < 55)
        center = float(np.median(xx[head]))
        feet = (alpha > 128) & (yy > 85) & (rgb[:, :, 0] < 160) & (rgb[:, :, 1] > 10)
        baseline = int(yy[feet].max())
        rgba = np.dstack((rgb, alpha))
        rgba[alpha == 0, :3] = 0
        art = Image.fromarray(rgba, "RGBA")
        art = art.resize((round(art.width * scale), round(art.height * scale)), Image.Resampling.NEAREST)
        tile = Image.new("RGBA", (160, 112))
        tile.alpha_composite(art, (round(80 - center * scale), round(105 - baseline * scale)))
        sheet.alpha_composite(tile, (frame * 160, direction * 112))
sheet.save(ROOT / "assets/katana_caminhada.png", optimize=True)
