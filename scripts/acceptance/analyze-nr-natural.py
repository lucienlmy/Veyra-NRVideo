"""Offline matched NR crops. Metrics are diagnostic, not a perceptual score."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


def srgb(x):
    x = np.clip(x, 0, 1)
    return np.where(x <= .0031308, 12.92*x, 1.055*x**(1/2.4)-.055)


def warp(image, motion):
    h, w = motion.shape[:2]
    y, x = np.mgrid[:h, :w]
    px, py = x+motion[..., 0], y+motion[..., 1]
    valid = np.isfinite(px) & np.isfinite(py) & (px >= 1) & (px < w-2) & (py >= 1) & (py < h-2)
    px = np.clip(np.nan_to_num(px), 0, w-2)
    py = np.clip(np.nan_to_num(py), 0, h-2)
    ix, iy = np.floor(px).astype(int), np.floor(py).astype(int)
    fx, fy = (px-ix)[..., None], (py-iy)[..., None]
    result = (image[iy, ix]*(1-fx)+image[iy, ix+1]*fx)*(1-fy)+(image[iy+1, ix]*(1-fx)+image[iy+1, ix+1]*fx)*fy
    return result, valid


def analyze(directory):
    width, height = 640, 360
    arrays = {}
    for name, channels in (("base", 4), ("raw", 4), ("filtered", 4), ("motion", 2)):
        path = directory/(name+(".rg16f" if channels == 2 else ".rgba16f"))
        if path.stat().st_size % (width*height*channels*2):
            raise ValueError(f"Incomplete frames: {path}")
        arrays[name] = np.memmap(path, dtype="<f2", mode="r").reshape(-1, height, width, channels)
    count = len(arrays["base"])
    if count < 2 or any(len(a) != count for a in arrays.values()):
        raise ValueError("Unmatched frame counts")
    rows, previous = [], None
    selected = {0, count//4, count//2, 3*count//4, count-1}
    sheet = Image.new("RGB", (width*3, (height+24)*len(selected)), "black")
    draw = ImageDraw.Draw(sheet)
    sheet_row = 0
    for i in range(count):
        base, raw, filtered = (np.asarray(arrays[k][i, ..., :3], dtype=np.float32) for k in ("base", "raw", "filtered"))
        motion = np.asarray(arrays["motion"][i], dtype=np.float32)
        if not all(np.isfinite(a).all() for a in (base, raw, filtered, motion)):
            raise ValueError(f"Nonfinite pixels: frame {i}")
        if i in selected:
            for col, (name, pixels) in enumerate((("Input", base), ("NR raw", raw), ("NR anti-flicker", filtered))):
                x, y = col*width, sheet_row*(height+24)
                draw.text((x+6, y+4), f"Frame {i}: {name}", fill="white")
                sheet.paste(Image.fromarray(np.uint8(np.rint(srgb(pixels)*255))), (x, y+24))
            sheet_row += 1
        row = {"frame": i, "changeMAE": float(np.abs(filtered-raw).mean()),
               "edgeRaw": float(np.abs(np.diff(raw, axis=1)).mean()),
               "edgeFiltered": float(np.abs(np.diff(filtered, axis=1)).mean()),
               "motionP95": float(np.percentile(np.linalg.norm(motion, axis=2), 95))}
        if previous is not None:
            old_base, valid = warp(previous[0], motion)
            # Restrict to photometrically matching source pixels, excluding disocclusion/cuts.
            stable = valid & (np.max(np.abs(base/(1+np.abs(base))-old_base/(1+np.abs(old_base))), axis=2) < .008)
            row["stablePixels"] = int(stable.sum())
            if stable.any():
                for k, current, old in (("raw", raw, previous[1]), ("filtered", filtered, previous[2])):
                    old_residual, _ = warp(old-previous[0], motion)
                    row[k+"ResidualDelta"] = float(np.abs((current-base)-old_residual)[stable].mean())
        rows.append(row)
        previous = (base, raw, filtered)
    averages = {k: float(np.mean([r[k] for r in rows if k in r])) for k in rows[-1] if k != "frame"}
    result = {"scope": "Same-frame SDR linear crops with real NR/NVOF; motion-compensated stable-source residuals. Edge energy is not a perceptual quality guarantee; cropped/offline results do not measure latency.",
              "frames": count, "averages": averages, "perFrame": rows}
    (directory/"quality.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    sheet.save(directory/"contact.png")
    print(json.dumps({"directory": str(directory), "frames": count, "averages": averages}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    analyze(parser.parse_args().directory)
