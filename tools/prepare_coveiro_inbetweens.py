"""Generate three motion samples per Coveiro pose from the current game atlases."""
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1] / "assets"
SHEETS = (
    "coveiro_visual_espera", "coveiro_visual_corrida",
    "coveiro_visual_emergencia", "coveiro_visual_queda",
    "coveiro_ataque_pazada", "coveiro_ataque_raizes",
    "coveiro_ataque_arremesso", "coveiro_ataque_investida",
)
SAMPLES = 3
SCALE = .75


def generate(name):
    source = Image.open(ROOT / f"{name}.png").convert("RGBA")
    width, height = source.width // 8, source.height // 4
    size = (round(width * SCALE), round(height * SCALE))
    atlas = Image.new("RGBA", (size[0] * 8, size[1] * 4 * SAMPLES))
    yy, xx = np.mgrid[:size[1], :size[0]].astype(np.float32)
    grid = np.stack((xx, yy), axis=-1)
    flow = cv2.DISOpticalFlow_create(cv2.DISOPTICAL_FLOW_PRESET_MEDIUM)
    loop = name.endswith(("espera", "corrida"))
    for row in range(4):
        poses = []
        for col in range(8):
            tile = source.crop((col * width, row * height, (col + 1) * width, (row + 1) * height))
            rgba = np.asarray(tile.resize(size, Image.Resampling.LANCZOS)).astype(np.float32) / 255
            rgba[:, :, :3] *= rgba[:, :, 3:]
            poses.append(rgba)
        for col, current in enumerate(poses):
            following = poses[(col + 1) % 8] if loop or col < 7 else current
            def luminance(pose):
                gray = cv2.cvtColor(pose[:, :, :3], cv2.COLOR_RGB2GRAY)
                return np.clip((gray + .18 * pose[:, :, 3]) * 255, 0, 255).astype(np.uint8)
            a, b = luminance(current), luminance(following)
            forward, backward = flow.calc(a, b, None), flow.calc(b, a, None)
            for sample in range(SAMPLES):
                phase = sample / SAMPLES
                if sample == 1:
                    result = cv2.remap(current, grid - phase * forward, None, cv2.INTER_LINEAR)
                elif sample == 2:
                    result = cv2.remap(following, grid - (1 - phase) * backward, None, cv2.INTER_LINEAR)
                else:
                    result = current.copy()
                result[:, :, :3] /= np.maximum(result[:, :, 3:], .001)
                rgba = np.clip(np.rint(result * 255), 0, 255).astype(np.uint8)
                atlas.paste(Image.fromarray(rgba), (col * size[0], (row + sample * 4) * size[1]))
    target = ROOT / "coveiro" / "fluido" / f"{name}.png"
    target.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(target, optimize=True)
    print(f"{name}: {atlas.size}", flush=True)


if __name__ == "__main__":
    cv2.setNumThreads(4)
    for sheet in SHEETS:
        generate(sheet)
