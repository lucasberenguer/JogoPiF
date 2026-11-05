"""Uniformiza luminosidade e remove resíduos de alfa dos quadros do menu."""

from pathlib import Path

import numpy as np
from PIL import Image


ASSETS = Path(__file__).resolve().parents[1] / "assets"
FRAMES = (0, 10, 6, 8, 11, 2, 7, 12, 4, 9, 13, 5)
TARGET_SIZE = (656, 600)


def load_frame(number):
    image = Image.open(ASSETS / f"menu_heroi_{number}.png").convert("RGBA")
    if image.size != TARGET_SIZE:
        image = image.resize(TARGET_SIZE, Image.Resampling.LANCZOS)
    return np.asarray(image, dtype=np.float32).copy()


frames = [load_frame(number) for number in FRAMES]
levels = []
for frame in frames:
    # Parte fixa do personagem, excluindo quase toda a capa móvel.
    area = frame[230:530, 365:530]
    visible = area[:, :, 3] > 180
    rgb = area[:, :, :3][visible]
    levels.append(float(np.mean(rgb @ np.array([0.2126, 0.7152, 0.0722]))))

reference = float(np.median(levels))
for index, (frame, level) in enumerate(zip(frames, levels)):
    gain = reference / level
    frame[:, :, :3] = np.clip(frame[:, :, :3] * gain, 0, 255)
    alpha = frame[:, :, 3]
    alpha[alpha < 14] = 0
    # Evita resíduos RGB invisíveis virarem manchas na interpolação da GPU.
    frame[:, :, :3][alpha == 0] = 0
    output = ASSETS / f"menu_heroi_tratado_{index:02d}.png"
    Image.fromarray(frame.astype(np.uint8), "RGBA").save(output)
    print(output.name, f"brilho {level:.1f} -> {level*gain:.1f}")
