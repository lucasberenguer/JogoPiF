"""Prepare directional gun walking frames with opaque dark interiors."""
from pathlib import Path
import sys

import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
shooting = len(sys.argv) > 1 and sys.argv[1] == "disparo"
source_name = "arma_disparo_referencia.png" if shooting else "arma_caminhada_referencia.png"
source = np.array(Image.open(ROOT / "design" / source_name).convert("RGB"))
reference = np.array(Image.open(ROOT / "assets/arma_fogo_idle_novo.png").convert("RGBA"))
bands = [(135, 325), (440, 635), (750, 933), (1058, 1240)]
sheet = Image.new("RGBA", (8 * 64, 4 * 80))
for direction, row in enumerate((0, 1, 3, 2)):
    top, bottom = bands[row]
    heights = (170, 175, 160, 160) if shooting else (175, 179, 167, 166)
    scale = 68.0 / heights[row]
    ref = reference[direction * 80:(direction + 1) * 80, :64]
    target_color = ref[:, :, :3][ref[:, :, 3] > 128].mean(axis=0)
    frames = []
    for frame in range(8):
        left, right = round(frame * source.shape[1] / 8), round((frame + 1) * source.shape[1] / 8)
        rgb = source[top:bottom, left:right].copy()
        visible = np.uint8(rgb.max(axis=2) > 18) * 255
        closed = cv2.morphologyEx(visible, cv2.MORPH_CLOSE, np.ones((3, 3), np.uint8))
        flood = closed.copy()
        cv2.floodFill(flood, None, (0, 0), 255)
        alpha = closed | cv2.bitwise_not(flood)
        yy, xx = np.indices(alpha.shape)
        head = (alpha > 128) & (yy < 65)
        center = float(np.median(xx[head]))
        feet = (alpha > 128) & (yy > 120) & (rgb[:, :, 0] < 160) & (rgb[:, :, 1] > 10)
        baseline = int(yy[feet].max())
        rgba = np.dstack((rgb, alpha))
        rgba[alpha == 0, :3] = 0
        art = Image.fromarray(rgba, "RGBA").resize(
            (round(rgb.shape[1] * scale), round(rgb.shape[0] * scale)), Image.Resampling.NEAREST)
        tile = Image.new("RGBA", (64, 80))
        tile.alpha_composite(art, (round(32 - center * scale), round(74 - baseline * scale)))
        frames.append(np.array(tile))
    colors = np.concatenate([f[:, :, :3][f[:, :, 3] > 128] for f in frames])
    gain = np.clip(target_color / colors.mean(axis=0), 0.65, 1.2)
    for frame, pixels in enumerate(frames):
        pixels[:, :, :3] = np.clip(pixels[:, :, :3].astype(float) * gain, 0, 255).astype(np.uint8)
        sheet.alpha_composite(Image.fromarray(pixels, "RGBA"), (frame * 64, direction * 80))
output_name = "arma_fogo_disparo.png" if shooting else "arma_fogo_caminhada.png"
sheet.save(ROOT / "assets" / output_name, optimize=True)
