"""Compare common product log counters without requiring newer trace fields."""
import argparse
from datetime import datetime
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True


def summarize(directory):
    result = json.loads((directory / "result.json").read_text(encoding="utf-8"))
    lines = (directory / "stdout.log").read_text(encoding="utf-8", errors="replace").splitlines()
    origin = datetime.fromisoformat(lines[0].split()[0])
    groups = {tag: [] for tag in ("player-timing", "frame-flow", "provider-flow", "xess-fg")}
    for line in lines:
        for tag, rows in groups.items():
            if f"[{tag}]" not in line:
                continue
            elapsed = (datetime.fromisoformat(line.split()[0]) - origin).total_seconds()
            if 10 <= elapsed <= 29:
                fields = dict(re.findall(r"(\w+)=([^ ]+)", line))
                if tag != "xess-fg" or "generatedTotal" in fields:
                    rows.append((elapsed, fields))
    rates = {}
    for tag, fields in (("player-timing", ("processed", "displaySubmits", "previewSkipped", "expiredGenerated")),
                        ("frame-flow", ("realPresented", "generatedPresented")),
                        ("provider-flow", ("sdkPresented", "sdkGenerated")),
                        ("xess-fg", ("presentId", "generatedTotal"))):
        rows = groups[tag]
        if len(rows) < 2:
            continue
        first, last = rows[0], rows[-1]
        if first[1].get("revision") != last[1].get("revision"):
            raise ValueError(f"Settings revision changed in {directory}")
        seconds = last[0] - first[0]
        rates[tag] = {"seconds": seconds}
        for field in fields:
            delta = int(last[1][field]) - int(first[1][field])
            if delta < 0:
                raise ValueError(f"Counter reset for {field} in {directory}")
            rates[tag][field + "PerSecond"] = delta / seconds
        if tag == "xess-fg":
            rates[tag]["sdkPresentedPerSecond"] = rates[tag]["presentIdPerSecond"] + rates[tag]["generatedTotalPerSecond"]
    summary = dict(directory=str(directory), case=result["case"],
                   executableSha256=result.get("executableSha256"),
                   gpu=result["telemetry"]["gpu"], power=result["telemetry"]["powerW"],
                   temperature=result["telemetry"]["temperatureC"], rates=rates,
                   gateTransitions=sum("[xess-fg-gate]" in line for line in lines),
                   smoke=result["smoke"])
    if any("[pacing-sample]" in line for line in lines):
        spec = importlib.util.spec_from_file_location("cadence", Path(__file__).with_name("analyze-fg-cadence.py"))
        cadence = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cadence)
        summary["cadence"] = cadence.analyze(directory / "stdout.log")
    return summary


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directories", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    rows = [summarize(path) for directory in args.directories for path in sorted(directory.iterdir())
            if path.is_dir() and (path / "result.json").exists()]
    args.output.write_text(json.dumps(rows, indent=2), encoding="utf-8")
    for row in rows:
        timing = row["rates"].get("player-timing", {})
        provider = row["rates"].get("xess-fg", row["rates"].get("provider-flow", {}))
        gap = row.get("cadence", {}).get("intervalMs", {})
        print(json.dumps(dict(run=Path(row["directory"]).parent.name, case=row["case"],
                              fps=timing.get("displaySubmitsPerSecond"),
                              source=timing.get("processedPerSecond"),
                              sdk=provider.get("sdkPresentedPerSecond"),
                              gpu=row["gpu"]["mean"], gate=row["gateTransitions"],
                              gapP95=gap.get("p95"), gapP99=gap.get("p99"))))
