"""Prepare the supplied boss special and VFX without color-keying dark pixels."""
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
source = Image.open(ROOT / "design/boss_clareira_especial_1_referencia.png").convert("RGBA")
sheet = Image.new("RGBA", (9 * 384, 4 * 288))
for direction, source_row in enumerate((0, 2, 3, 1)):
    frames, anchors = [], []
    for frame in range(9):
        crop = source.crop((round(frame * source.width / 9), source_row * 181,
                            round((frame + 1) * source.width / 9), (source_row + 1) * 181))
        alpha = np.array(crop.getchannel("A"))
        yy, xx = np.indices(alpha.shape)
        feet = (alpha > 180) & (yy > 152)
        anchors.append(float(np.median(xx[feet])))
        frames.append(crop)
    # Roots obscure the feet in these two frames: interpolate their body origin.
    anchors[5] = anchors[4] + (anchors[7] - anchors[4]) / 3
    anchors[6] = anchors[4] + 2 * (anchors[7] - anchors[4]) / 3
    for frame, crop in enumerate(frames):
        art = crop.resize((round(crop.width * 1.5), round(crop.height * 1.5)),
                          Image.Resampling.NEAREST)
        tile = Image.new("RGBA", (384, 288))
        tile.alpha_composite(art, (round(192 - anchors[frame] * 1.5), 28))
        sheet.alpha_composite(tile, (frame * 384, direction * 288))
sheet.save(ROOT / "assets/boss_clareira_especial_1.png", optimize=True)

effects = Image.open(ROOT / "design/boss_clareira_efeitos_1_referencia.png").convert("RGBA")
for row, name in enumerate(("raizes_linha", "raizes_area", "folhas")):
    sheet = Image.new("RGBA", (8 * 288, 256))
    for frame in range(8):
        crop = effects.crop((round(frame * effects.width / 8), round(row * effects.height / 3),
                             round((frame + 1) * effects.width / 8),
                             round((row + 1) * effects.height / 3)))
        sheet.alpha_composite(crop, (frame * 288 + (288 - crop.width) // 2,
                                    256 - crop.height))
    sheet.save(ROOT / f"assets/boss_clareira_{name}.png", optimize=True)
