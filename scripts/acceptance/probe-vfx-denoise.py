"""Local SDK prerequisite probe, not a live D3D12 integration or latency test."""
import argparse
import json
from pathlib import Path
import time

import numpy as np
import cupy as cp
from nvvfx import VideoSuperRes, get_sdk_version
from nvvfx.effects.video_super_res import QualityLevel


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--quality", type=int, choices=range(8, 12), default=8)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"scope": "Official CUDA SDR prerequisite probe; CPU transfers are diagnostic only. No D3D12 interop or display latency measured.",
              "sdk": str(get_sdk_version()), "quality": args.quality,
              "device": cp.cuda.runtime.getDeviceProperties(0)["name"].decode()}
    try:
        h, w = 1080, 1920
        y, x = np.mgrid[:h, :w]
        clean = np.stack([.2+.6*x/w, .2+.6*y/h, .25+.5*((x//8+y//8) % 2)]).astype(np.float32)
        rng = np.random.default_rng(1234)
        times, mse_input, mse_output = [], [], []
        with VideoSuperRes(quality=QualityLevel(args.quality)) as effect:
            effect.output_width, effect.output_height = w, h
            start = time.perf_counter()
            effect.load()
            report["loadMs"] = 1000*(time.perf_counter()-start)
            for index in range(40):
                noisy = np.clip(clean+rng.normal(0, .02, clean.shape), 0, 1).astype(np.float32)
                device_input = cp.asarray(noisy)
                cp.cuda.runtime.deviceSynchronize()
                start = time.perf_counter()
                result = effect.run(device_input)
                # DLPack capsule owns a view of reusable SDK storage. Consume it
                # before the next run; host copies are excluded from run timing.
                output = cp.from_dlpack(result.image)
                cp.cuda.runtime.deviceSynchronize()
                elapsed = 1000*(time.perf_counter()-start)
                pixels = output.get()
                if pixels.shape != clean.shape or not np.isfinite(pixels).all():
                    raise ValueError("Invalid denoise output")
                if index >= 10:
                    times.append(elapsed)
                    mse_input.append(float(np.mean((noisy-clean)**2)))
                    mse_output.append(float(np.mean((pixels-clean)**2)))
                if index == 39:
                    for name, image in (("clean", clean), ("noisy", noisy), ("denoised", pixels)):
                        np.save(args.output/(name+".npy"), image)
        report.update(passed=True, runP50Ms=float(np.median(times)), runP95Ms=float(np.percentile(times, 95)),
                      inputMSE=float(np.mean(mse_input)), outputMSE=float(np.mean(mse_output)))
    except Exception as error:
        report.update(passed=False, error=repr(error))
    (args.output/"result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
