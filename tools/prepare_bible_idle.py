"""Extract the supplied eight-frame Bible idle, preserving dark interiors."""
from pathlib import Path
import sys

import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
walking = len(sys.argv) > 1 and sys.argv[1] == "caminhada"
source_name = "biblia_caminhada_referencia.png" if walking else "biblia_idle_referencia.png"
source = np.array(Image.open(ROOT / "design" / source_name).convert("RGB"))
reference_name = "biblia_idle.png" if walking else "protagonista.png"
reference = np.array(Image.open(ROOT / "assets" / reference_name).convert("RGBA"))
bands = [(60, 240), (300, 473), (536, 708), (781, 951)]
sheet = Image.new("RGBA", (512, 320))
for direction, row in enumerate((0, 1, 3, 2)):
    top, bottom = bands[row]
    scale = 68.0 / (163, 156, 155, 153)[row]
    frames = []
    for frame in range(8):
        rgb = source[top:bottom, frame * 192:(frame + 1) * 192].copy()
        mask = np.uint8(rgb.max(axis=2) > 18) * 255
        closed = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, np.ones((3, 3), np.uint8))
        flood = closed.copy()
        cv2.floodFill(flood, None, (0, 0), 255)
        alpha = closed | cv2.bitwise_not(flood)
        yy, xx = np.indices(alpha.shape)
        head = (alpha > 128) & (yy < 65)
        center = float(np.median(xx[head]))
        feet = (alpha > 128) & (yy > 110) & (rgb[:, :, 0] < 160) & (rgb[:, :, 1] > 10)
        baseline = int(yy[feet].max())
        rgba = np.dstack((rgb, alpha))
        rgba[alpha == 0, :3] = 0
        art = Image.fromarray(rgba, "RGBA").resize(
            (round(rgb.shape[1] * scale), round(rgb.shape[0] * scale)), Image.Resampling.NEAREST)
        tile = Image.new("RGBA", (64, 80))
        tile.alpha_composite(art, (round(32 - center * scale), round(74 - baseline * scale)))
        frames.append(np.array(tile))
    ref = reference[direction * 80:(direction + 1) * 80, :64]
    target = ref[:, :, :3][ref[:, :, 3] > 128].mean(axis=0)
    current = np.concatenate([f[:, :, :3][f[:, :, 3] > 128] for f in frames]).mean(axis=0)
    gain = np.clip(target / current, 0.65, 1.2)
    for frame, pixels in enumerate(frames):
        pixels[:, :, :3] = np.clip(pixels[:, :, :3].astype(float) * gain, 0, 255).astype(np.uint8)
        sheet.alpha_composite(Image.fromarray(pixels, "RGBA"), (frame * 64, direction * 80))
output_name = "biblia_caminhada.png" if walking else "biblia_idle.png"
sheet.save(ROOT / "assets" / output_name, optimize=True)
