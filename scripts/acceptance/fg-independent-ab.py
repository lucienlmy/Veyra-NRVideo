#!/usr/bin/env python3
"""Short A/B matrix for the 2026-09-22 independent FG repair.

Runs the existing fg-utilization-matrix harness (unchanged) in fullscreen view
for a set of named variants that differ only in environment toggles, then
summarises source/generated rates, history resets, present blocking and the
new display-stats line from each app.log. It never edits product files.

Variants (environment differences only):
  baseline  : VEYRA_DISABLE_XESS_SOURCE_TIMING=1 VEYRA_TEST_PREVIEW_SKIP_RESET=1 (old behaviour)
  x1        : source timing on, skip resets on
  full      : source timing on, bounded skip keeps history
  (X2 XeLL pass-through was withdrawn: provider returns -15 without XeLL.)
"""
import argparse
import json
import os
import re
import runpy
import statistics
import subprocess
import sys
from pathlib import Path

VARIANTS = {
    "baseline": {"VEYRA_DISABLE_XESS_SOURCE_TIMING": "1", "VEYRA_TEST_PREVIEW_SKIP_RESET": "1", "VEYRA_TEST_SYNC_PROVIDER_PRESENT": "1", "VEYRA_TEST_FG_NO_REDUCED": "1"},
    "x1": {"VEYRA_TEST_PREVIEW_SKIP_RESET": "1", "VEYRA_TEST_SYNC_PROVIDER_PRESENT": "1", "VEYRA_TEST_FG_NO_REDUCED": "1"},
    "x1f2": {"VEYRA_TEST_SYNC_PROVIDER_PRESENT": "1", "VEYRA_TEST_FG_NO_REDUCED": "1"},
    "sync": {"VEYRA_TEST_SYNC_PROVIDER_PRESENT": "1"},
    "noreduced": {"VEYRA_TEST_FG_NO_REDUCED": "1"},
    "full": {},
}
CLEAR = ("VEYRA_DISABLE_XESS_SOURCE_TIMING", "VEYRA_TEST_XESS_LOW_LATENCY", "VEYRA_TEST_PREVIEW_SKIP_RESET", "VEYRA_TEST_SYNC_PROVIDER_PRESENT", "VEYRA_TEST_FG_NO_REDUCED")


def dist(values):
    if not values:
        return None
    v = sorted(values)
    n = len(v)
    return {"n": n, "mean": round(statistics.mean(v), 3), "p50": round(v[n // 2], 3),
            "p95": round(v[min(n - 1, int(n * .95))], 3), "p99": round(v[min(n - 1, int(n * .99))], 3), "max": round(v[-1], 3)}


def summarise(case_dir: Path, seconds: int):
    log = (case_dir / "app.log").read_text(encoding="utf-8", errors="replace")
    lines = log.splitlines()
    first = None
    counters = []
    for line in lines:
        m = re.match(r"(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d+)Z", line)
        if not m:
            continue
        stamp = m.group(1)
        if first is None:
            first = stamp
        if "[xess-fg] presentId=" in line:
            kv = dict(re.findall(r"(\w+)=(-?[\d.]+)", line))
            counters.append((stamp, int(kv["presentId"]), int(kv["generatedTotal"])))
    out = {"case": case_dir.name}
    import datetime as dt

    def secs(stamp):
        return (dt.datetime.fromisoformat(stamp) - dt.datetime.fromisoformat(first)).total_seconds()

    window = [c for c in counters if 10 <= secs(c[0]) <= seconds - 1]
    if len(window) >= 2:
        span = secs(window[-1][0]) - secs(window[0][0])
        out["xessSourcePerSec"] = round((window[-1][1] - window[0][1]) / span, 3)
        out["xessGeneratedPerSec"] = round((window[-1][2] - window[0][2]) / span, 3)
        out["counterWindowSeconds"] = round(span, 3)
    closed = next((l for l in reversed(lines) if "[frame-flow] state=closed" in l), "")
    kv = dict(re.findall(r"(\w+)=(-?[\d.]+)", closed))
    for key in ("epoch", "sourceAccepted", "realPresented", "generatedPresented", "fgEvaluated", "fgSkippedBeforeEval",
                "fgSkippedForReset", "fgWarmup", "fgReduced", "fgReadyValid", "generatedExpiredAfterEval", "cancelledBeforePresent", "slotWaitCount"):
        if key in kv:
            out[key] = float(kv[key]) if "." in kv[key] else int(kv[key])
    timing = [dict(re.findall(r"(\w+)=([^ ]+)", l)) for l in lines if "[player-timing]" in l]
    if timing:
        t = timing[-1]
        for key in ("previewSkipped", "expiredGenerated", "gpuReadyP95Ms", "presentP95Ms", "gpuSrP95Ms", "gpuNrP95Ms", "gpuFgBatchP95Ms", "playbackSpeed"):
            if key in t:
                out[key] = float(t[key])
    out["previewSkipRetainedLogLines"] = sum("[preview-skip]" in l for l in lines)
    out["xessFgGateEvents"] = sum("[xess-fg-gate]" in l for l in lines)
    out["engineStallLines"] = sum("[engine-stall]" in l for l in lines)
    out["asyncProviderPresent"] = any("helper thread" in l for l in lines)
    out["errorLines"] = sum("[ERROR]" in l for l in lines)
    adm = [l for l in lines if "[live-fg-admission]" in l and "reduced=" not in l]
    if adm:
        kv = dict(re.findall(r"(\w+)=(-?[\d.]+)", adm[-1]))
        out["admittedPairs"] = int(kv.get("admittedPairs", 0)); out["rejectedPairs"] = int(kv.get("rejectedPairs", 0)); out["predictedMs"] = float(kv.get("predictedMs", 0))
    red = [l for l in lines if "reduced=2X" in l]
    if red:
        kv = dict(re.findall(r"(\w+)=(-?[\d.]+)", red[-1])); out["reducedPairs"] = int(kv.get("reducedPairs", 0))
    pacing = [l for l in lines if "[xess-pacing] multiplier=" in l]
    if pacing:
        m = re.search(r"inBurstGaps=(\d+) mean=([\d.]+)ms min=([\d.]+)ms max=([\d.]+)ms", pacing[-1])
        if m:
            out["xessInBurstGapMs"] = {"mean": float(m.group(2)), "min": float(m.group(3)), "max": float(m.group(4))}
    xell = next((l for l in lines if "XeLL bLowLatencyMode=" in l), "")
    m = re.search(r"bLowLatencyMode=(\d)", xell)
    out["xellLowLatency"] = int(m.group(1)) if m else None
    stats = []
    for l in lines:
        if "[display-stats]" in l and "refreshHz=" in l:
            stats.append(dict(re.findall(r"(\w+)=(-?[\d.]+)", l)))
    if stats:
        out["displayRefreshHz"] = float(stats[0]["refreshHz"])
        out["displayStatsSamples"] = len(stats)
        out["displayed"] = dist([float(s["displayed"]) for s in stats])
        out["presents"] = dist([float(s["presents"]) for s in stats])
        # "discarded" replaced the old "notDisplayed" when displayed was fixed
        # to use DXGI PresentCount (2026-09-22); accept either spelling.
        key = "discarded" if "discarded" in stats[0] else "notDisplayed"
        out["discarded"] = dist([float(s[key]) for s in stats])
        if "refreshes" in stats[0]:
            out["refreshes"] = dist([float(s["refreshes"]) for s in stats])
        out["refreshesWithoutNewFrame"] = dist([float(s["refreshesWithoutNewFrame"]) for s in stats])
    else:
        unavailable = next((l for l in lines if "[display-stats] unavailable" in l), None)
        out["displayStats"] = "unavailable: " + unavailable[-60:] if unavailable else "no samples"
    trace = case_dir / "trace.txt"
    if trace.exists():
        ev = []
        for line in trace.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("event="):
                ev.append(dict(kv.split("=", 1) for kv in line.split() if "=" in kv))
        pres = [e for e in ev if e["event"] == "Present"]
        if len(pres) > 2:
            hosts = [int(e["presentEndHost"]) for e in pres]
            gaps = [(hosts[i] - hosts[i - 1]) / 1e4 for i in range(1, len(hosts))]
            out["tracePresents"] = len(pres)
            out["traceSubmitPerSec"] = round((len(pres) - 1) * 1e7 / (hosts[-1] - hosts[0]), 3)
            out["presentReturnGapMs"] = dist(gaps)
            out["gapsOver16_667ms"] = sum(g > 16.667 for g in gaps)
            out["presentBlockMs"] = dist([(int(e["presentEndHost"]) - int(e["presentBeginHost"])) / 1e4 for e in pres])
            ep = [e["epoch"] for e in pres]
            out["epochChangesBetweenPresents"] = sum(1 for i in range(1, len(ep)) if ep[i] != ep[i - 1])
            ready = [int(e["count"]) for e in ev if e["event"] == "Ready"]
            if ready:
                from collections import Counter
                out["validGeneratedPerBatch"] = dict(Counter(ready))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", type=Path, required=True)
    ap.add_argument("--native", type=Path, required=True)
    ap.add_argument("--derived", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--temp", type=Path, required=True)
    ap.add_argument("--harness", type=Path, required=True, help="scripts/acceptance/fg-utilization-matrix.py")
    ap.add_argument("--variants", nargs="+", default=list(VARIANTS))
    ap.add_argument("--cases", nargs="+", default=["sr-nr-xess4"])
    ap.add_argument("--seconds", type=int, default=30)
    ap.add_argument("--view", default="fullscreen")
    ap.add_argument("--summarise-only", action="store_true")
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    summary = {}
    for variant in args.variants:
        vout = args.output / variant
        if not args.summarise_only:
            env = dict(os.environ)
            for k in CLEAR:
                env.pop(k, None)
            env.update(VARIANTS[variant])
            # Same in-process view override as the 2026-09-22 comparison: the
            # harness hardcodes pro view; swap the argument after --smoke-view.
            real_popen = subprocess.Popen

            def popen(argv, *a, **kw):
                argv = list(argv)
                if "--smoke-view" in argv:
                    argv[argv.index("--smoke-view") + 1] = args.view
                kw["env"] = {**kw.get("env", {}), **{k: v for k, v in env.items() if k in VARIANTS[variant] or k in CLEAR}}
                for k in CLEAR:
                    if k not in VARIANTS[variant]:
                        kw["env"].pop(k, None)
                return real_popen(argv, *a, **kw)

            subprocess.Popen = popen
            sys.argv = [str(args.harness), "--exe", str(args.exe), "--native", str(args.native), "--derived", str(args.derived),
                        "--output", str(vout), "--temp", str(args.temp), "--cases", *args.cases, "--seconds", str(args.seconds)]
            try:
                runpy.run_path(str(args.harness), run_name="__main__")
            finally:
                subprocess.Popen = real_popen
        summary[variant] = {"environment": VARIANTS[variant], "view": args.view,
                            "cases": {c: summarise(vout / c, args.seconds) for c in args.cases if (vout / c / "app.log").exists()}}
        (args.output / "summary.json").write_text(json.dumps(summary, indent=1), encoding="utf-8")
    print(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
