#!/usr/bin/env python3
"""Procedural, seamlessly tiling ground textures for Tommyzilla (written by us, released CC0 with the game).

Writes 256x256 8-bit greyscale PNGs to data/textures/. They are luminance detail only: the game tints each one with
the surface's existing palette colour (slate ground, dark streets, park green, harbour blue, promenade sand) and divides
by the texture's mean brightness, so the palette and mood stay exactly as before. Everything is periodic (FFT-filtered
noise, integer-frequency sines, grid-aligned joints), so the tiles repeat without seams. Deterministic (fixed seeds).
Usage: python3 tools/gen_ground_textures.py [out_dir]   (needs numpy + Pillow; only re-run to change the look)
"""
import os, sys
import numpy as np
from PIL import Image

N = 256

def band_noise(seed, lo, hi, n=N):
    """Periodic noise with energy between spatial frequencies lo..hi (cycles per tile), normalised to -1..1."""
    rng = np.random.default_rng(seed)
    f = np.fft.fft2(rng.standard_normal((n, n)))
    fy = np.fft.fftfreq(n)[:, None] * n
    fx = np.fft.fftfreq(n)[None, :] * n
    r = np.sqrt(fx * fx + fy * fy)
    mask = np.clip((r - lo) / max(1e-6, lo * 0.5 + 0.5), 0, 1) * np.clip((hi - r) / max(1.0, hi * 0.25), 0, 1)
    out = np.real(np.fft.ifft2(f * mask))
    out -= out.mean()
    return out / (np.abs(out).max() + 1e-9)

def joints(period, width, n=N, offset_rows=False):
    """1 on a joint line (grid of `period` px), 0 elsewhere; optional half-period offset every other row (pavers)."""
    y, x = np.mgrid[0:n, 0:n]
    if offset_rows:
        x = (x + ((y // period) % 2) * (period // 2)) % n
    return (((x % period) < width) | ((y % period) < width)).astype(np.float32)

def cells(period, seed, n=N, offset_rows=False):
    """Per-cell random value in -1..1 (one per slab / paver)."""
    rng = np.random.default_rng(seed)
    k = n // period
    vals = rng.uniform(-1, 1, (k, k))
    y, x = np.mgrid[0:n, 0:n]
    if offset_rows:
        x = (x + ((y // period) % 2) * (period // 2)) % n
    return vals[(y // period) % k, (x // period) % k]

def save(name, img, out):
    a = np.clip(img, 0, 1)
    Image.fromarray((a * 255 + 0.5).astype(np.uint8), "L").save(os.path.join(out, name), optimize=True)
    print(f"{name}: mean {a.mean():.3f}  min {a.min():.3f}  max {a.max():.3f}")

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "data", "textures")
    os.makedirs(out, exist_ok=True)
    # concrete / pavement: 4x4 slabs (2 m at the 8 m tile), soft stains, fine grain, thin dark joints
    c = 0.62 + 0.05 * band_noise(1, 1, 6) + 0.035 * band_noise(2, 40, 128) + 0.025 * cells(64, 3)
    c -= 0.16 * joints(64, 2)
    save("ground_concrete.png", c, out)
    # asphalt: dark aggregate speckle + a few lighter worn patches (lane markings are drawn by the shader)
    a = 0.46 + 0.04 * band_noise(11, 1, 5) + 0.06 * band_noise(12, 60, 128)
    rng = np.random.default_rng(13)
    a += 0.08 * (rng.random((N, N)) > 0.985)                   # light aggregate stones
    save("ground_asphalt.png", a, out)
    # grass: clumpy low-frequency light/dark patches + streaky blades
    g = 0.58 + 0.07 * band_noise(21, 2, 10) + 0.05 * band_noise(22, 24, 80)
    blades = band_noise(23, 50, 128)
    g += 0.05 * np.sign(blades) * np.abs(blades) ** 0.6
    save("ground_grass.png", g, out)
    # water: soft interfering wave crests (integer-frequency sines -> periodic) + gentle noise; the shader scrolls it
    y, x = np.mgrid[0:N, 0:N] / N * 2 * np.pi
    w = (np.sin(3 * x + 2 * y + 1.3 * np.sin(2 * y)) + np.sin(-2 * x + 4 * y + 1.1 * np.sin(3 * x)) + 0.6 * np.sin(5 * x - 3 * y)) / 2.6
    crest = np.clip(w, 0, 1) ** 3
    wat = 0.55 + 0.05 * w + 0.12 * crest + 0.03 * band_noise(31, 8, 40)
    save("ground_water.png", wat, out)
    # paving (Dallas Road promenade): 8x8 offset pavers (0.5 m at the 4 m tile) with mortar lines
    p = 0.64 + 0.045 * cells(32, 41, offset_rows=True) + 0.03 * band_noise(42, 30, 128) + 0.03 * band_noise(43, 1, 4)
    p -= 0.18 * joints(32, 2, offset_rows=True)
    save("ground_paving.png", p, out)

if __name__ == "__main__":
    main()
