"""Recorta os botões enviados para o menu, removendo o fundo vinho."""
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
NAMES = ("iniciar", "continuar", "config", "creditos")
for name in NAMES:
    source = ROOT / "design" / f"menu_{name}_referencia.png"
    rgb = np.asarray(Image.open(source).convert("RGB"))
    background = rgb[0, 0].astype(np.int16)
    difference = np.max(np.abs(rgb.astype(np.int16) - background), axis=2)
    binary = np.uint8(difference > 8)
    count, labels, stats, _ = cv2.connectedComponentsWithStats(binary, 8)
    largest = 1 + np.argmax(stats[1:, cv2.CC_STAT_AREA])
    component = np.uint8(labels == largest) * 255
    # O fundo vinho também possui leve gradiente. A borda dourada define
    # a silhueta real do botão, inclusive as pontas decorativas.
    gold = ((rgb[:,:,0] > 125) & (rgb[:,:,1] > 55) &
            (rgb[:,:,0] > rgb[:,:,1] * 1.25)).astype(np.uint8)
    alpha = np.zeros(component.shape, np.uint8)
    for row in range(rgb.shape[0]):
        xs = np.flatnonzero(gold[row])
        if len(xs):
            alpha[row, max(0, xs[0]-3):min(rgb.shape[1], xs[-1]+4)] = 255
    alpha = cv2.GaussianBlur(alpha, (3, 3), 0.6)
    x, y, w, h = stats[largest, :4]
    pad = 8
    x0, y0 = max(0, x - pad), max(0, y - pad)
    x1, y1 = min(rgb.shape[1], x + w + pad), min(rgb.shape[0], y + h + pad)
    rgba = np.dstack((rgb, alpha))[y0:y1, x0:x1]
    Image.fromarray(rgba).save(ROOT / "assets" / f"menu_{name}.png")
    print(name, rgba.shape)
