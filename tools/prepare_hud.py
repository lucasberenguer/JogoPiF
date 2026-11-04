"""Separate the supplied HUD art into a frame and dynamic bar fills."""

from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
source = np.asarray(Image.open(ROOT / "design/hud_personagem_referencia.png").convert("RGB"))
if source.shape[:2] != (724, 2172):
    raise ValueError("Expected the supplied 2172x724 HUD reference")

left, top, right, bottom = 90, 0, 2172, 640
art = source[top:bottom, left:right]
y, x = np.indices(art.shape[:2])
red = art[:, :, 0].astype(np.int16)
green = art[:, :, 1].astype(np.int16)
blue = art[:, :, 2].astype(np.int16)

health = ((x >= 285) & (x < 1545) & (y >= 202) & (y < 302) &
          (red > 110) & (red > green * 1.55) & (red > blue * 1.4))
dodge = ((x >= 340) & (x < 1450) & (y >= 360) & (y < 435) &
         (blue > 120) & (green > 100) & (blue > red + 15))

alpha = np.where(art.max(axis=2) > 10, 255, 0).astype(np.uint8)
base = np.dstack((art, alpha))
health_fill = np.zeros_like(base)
dodge_fill = np.zeros_like(base)
health_fill[health] = base[health]
dodge_fill[dodge] = base[dodge]

# Dim the original colored pixels in the base. Drawing a variable-width slice
# of each fill on top restores the supplied art as the bars refill.
base[health, :3] = (65, 13, 27)
base[dodge, :3] = (10, 40, 58)

for name, pixels in (
    ("hud_base.png", base),
    ("hud_vida.png", health_fill),
    ("hud_esquiva.png", dodge_fill),
):
    destination = ROOT / "assets" / name
    Image.fromarray(pixels, "RGBA").save(destination)
    print(f"Wrote {destination}")

# The other portraits and frames are supplied as separate full HUD images.
# Their baked-in bars are dimmed too, so the same live fills can be drawn on
# top while their expressions and colors change with health.
states = (
    ("hud_media.png", "hud_vida_media_referencia.png", 560),
    ("hud_critica.png", "hud_vida_critica_referencia.png", 480),
    ("hud_derrota.png", "hud_derrota_referencia.png", None),
)
for output_name, source_name, red_end in states:
    variant_source = np.asarray(Image.open(ROOT / "design" / source_name).convert("RGB"))
    if variant_source.shape != source.shape:
        raise ValueError(f"HUD variant has a different size: {source_name}")
    variant = variant_source[top:bottom, left:right]
    variant_alpha = np.where(variant.max(axis=2) > 10, 255, 0).astype(np.uint8)
    variant_base = np.dstack((variant, variant_alpha))
    if red_end is not None:
        vr = variant[:, :, 0].astype(np.int16)
        vg = variant[:, :, 1].astype(np.int16)
        vb = variant[:, :, 2].astype(np.int16)
        baked_health = ((x >= 285) & (x < red_end) &
                        (y >= 202) & (y < 302) &
                        (vr > 110) & (vr > vg * 1.55) & (vr > vb * 1.4))
        baked_dodge = ((x >= 340) & (x < 1450) &
                       (y >= 360) & (y < 435) &
                       (vb > 120) & (vg > 100) & (vb > vr + 15))
        variant_base[baked_health, :3] = (65, 13, 27)
        variant_base[baked_dodge, :3] = (10, 40, 58)
    destination = ROOT / "assets" / output_name
    Image.fromarray(variant_base, "RGBA").save(destination)
    print(f"Wrote {destination}")
