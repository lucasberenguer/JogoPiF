"""Extract Bible activation without making the halo interior opaque."""
from pathlib import Path
import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
source = np.array(Image.open(ROOT / "design/biblia_ativacao_referencia.png").convert("RGB"))
sheet = Image.new("RGBA", (1024, 448))
for direction, row in enumerate((0, 1, 3, 2)):
    top, bottom = [(15, 240), (250, 473), (485, 708), (725, 951)][row]
    scale = 68.0 / (163, 156, 155, 153)[row]
    for frame in range(8):
        rgb = source[top:bottom, frame * 192:(frame + 1) * 192].copy()
        mask = np.uint8(rgb.max(axis=2) > 18) * 255
        closed = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, np.ones((3, 3), np.uint8))
        flood = closed.copy()
        cv2.floodFill(flood, None, (0, 0), 255)
        holes = cv2.bitwise_not(flood)
        count, labels, stats, _ = cv2.connectedComponentsWithStats(holes, 8)
        alpha = closed.copy()
        for component in range(1, count):
            if stats[component, cv2.CC_STAT_AREA] < 1500:
                alpha[labels == component] = 255
        yy, xx = np.indices(alpha.shape)
        # Hair locates the body independently of the halo and sparkling pages.
        hair = (rgb[:, :, 0] > 50) & (rgb[:, :, 0] < 180) & (rgb[:, :, 1] > 20) & (rgb[:, :, 1] < 95) & (rgb[:, :, 2] > 18) & (yy < 130)
        center = float(np.median(xx[hair]))
        feet = (alpha > 128) & (yy > 140) & (rgb[:, :, 0] < 160) & (rgb[:, :, 1] > 10)
        baseline = int(yy[feet].max())
        luminous = (rgb.max(axis=2) > 180) & (rgb[:, :, 1] > 100)
        corrected = np.clip(rgb.astype(float) * np.array([.82, .85, .80]), 0, 255).astype(np.uint8)
        rgb = np.where(luminous[:, :, None], rgb, corrected)
        rgba = np.dstack((rgb, alpha)); rgba[alpha == 0, :3] = 0
        art = Image.fromarray(rgba, "RGBA").resize((round(rgb.shape[1] * scale), round(rgb.shape[0] * scale)), Image.Resampling.NEAREST)
        tile = Image.new("RGBA", (128, 112))
        tile.alpha_composite(art, (round(64 - center * scale), round(105 - baseline * scale)))
        sheet.alpha_composite(tile, (frame * 128, direction * 112))
sheet.save(ROOT / "assets/biblia_ativacao_nova.png", optimize=True)
