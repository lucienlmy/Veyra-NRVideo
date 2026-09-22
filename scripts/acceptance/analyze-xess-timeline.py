"""Join opt-in XeSS cycle events to source Present events, without guessing IDs."""
import argparse
import json
from pathlib import Path
import re


def distribution(values):
    values = sorted(values)
    if not values:
        return {"samples": 0}
    return dict(samples=len(values), mean=sum(values) / len(values),
                p50=values[int((len(values) - 1) * .50)],
                p95=values[int((len(values) - 1) * .95)],
                p99=values[int((len(values) - 1) * .99)], max=values[-1])


def analyze(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    events = [dict(re.findall(r"(\w+)=([^\s]+)", line))
              for line in text.splitlines() if line.startswith("event=")]
    cycles = {}
    presents = [row for row in events if row["event"] == "Present"]
    outputs = sorted((row for row in events if row["event"] == "ProviderOutput"),
                     key=lambda row: int(row["presentEndHost"]))
    output_gaps = [(int(current["presentEndHost"]) - int(previous["presentEndHost"])) / 10000
                   for previous, current in zip(outputs, outputs[1:])]
    transitions = {}
    deadlines = [row for row in events if row["event"] == "ProviderDeadline"]
    fence_groups = {name: [] for name in ("readyAtEntry", "pendingThenReady", "stillPending", "deviceRemoved")}
    first_schedule_parts = {name: [] for name in ("beforeDeadlineMs", "afterDeadlineMs", "deadlineLeadMs")}
    for output in outputs:
        begin = int(output.get("providerScheduleBeginHost", 0))
        end = int(output["presentBeginHost"])
        matches = [row for row in deadlines if begin <= int(row["host"]) <= end and int(row["detail"]) == 1]
        if begin and len(matches) == 1:
            row = matches[0]
            first_schedule_parts["beforeDeadlineMs"].append((int(row["host"]) - begin) / 10000)
            first_schedule_parts["afterDeadlineMs"].append((end - int(row["host"])) / 10000)
            first_schedule_parts["deadlineLeadMs"].append(float(row["ms"]))
            if int(row.get("providerFenceSampled", 0)):
                before, after, target = (int(row[key]) for key in
                                        ("providerFenceBefore", "providerFenceAtDeadline", "providerFenceTarget"))
                category = ("deviceRemoved" if max(before, after) == (1 << 64) - 1 else
                            "readyAtEntry" if before >= target else
                            "pendingThenReady" if after >= target else "stillPending")
                fence_groups[category].append((int(row["host"]) - begin) / 10000)
    for previous, current in zip(outputs, outputs[1:]):
        if not int(previous.get("providerCallerRva", 0)) or not int(current.get("providerCallerRva", 0)):
            continue
        key = f'{int(previous["providerCallerRva"]):x}->{int(current["providerCallerRva"]):x}'
        group = transitions.setdefault(key, {name: [] for name in ("returnGapMs", "beforeHookMs", "hookPacingMs", "nativeCallMs")})
        group["returnGapMs"].append((int(current["presentEndHost"]) - int(previous["presentEndHost"])) / 10000)
        if int(current.get("providerScheduleBeginHost", 0)):
            group["beforeHookMs"].append((int(current["providerScheduleBeginHost"]) - int(previous["presentEndHost"])) / 10000)
            group["hookPacingMs"].append((int(current["presentBeginHost"]) - int(current["providerScheduleBeginHost"])) / 10000)
        group["nativeCallMs"].append((int(current["presentEndHost"]) - int(current["presentBeginHost"])) / 10000)
    for event in events:
        if not event["event"].startswith("Xess"):
            continue
        key = (event["providerInstance"], event["providerCycle"])
        cycle = cycles.setdefault(key, {})
        if event["event"] in cycle:
            raise ValueError(f"Duplicate event for provider cycle {key}")
        cycle[event["event"]] = event
    if not cycles:
        raise ValueError("No XeSS trace events; enable VEYRA_TEST_TRACE_XESS")
    rows = []
    for key, cycle in cycles.items():
        bind, present, sleep = (cycle.get(name) for name in ("XessBind", "XessPresent", "XessSleep"))
        row = dict(instance=int(key[0]), cycle=int(key[1]),
                   sleepMs=float(sleep["ms"]) if sleep else None,
                   presentMs=float(present["ms"]) if present else None,
                   preparationCycle=int(bind["preparationCycle"]) if bind else None,
                   source=int(bind["source"]) if bind else None)
        matches = []
        if bind and present:
            matches = [event for event in presents
                       if all(event[field] == bind[field] for field in ("source", "epoch", "revision"))
                       and int(event["presentBeginHost"]) <= int(present["presentBeginHost"])
                       and int(event["presentEndHost"]) >= int(present["presentEndHost"])]
        row["matchingAppPresents"] = len(matches)
        if len(matches) == 1:
            event = matches[0]
            row.update(pts100ns=int(event["pts"]), batch=int(event["batch"]),
                       epoch=int(event["epoch"]), revision=int(event["revision"]),
                       beginHost=int(event["presentBeginHost"]), endHost=int(event["presentEndHost"]))
        rows.append(row)
    linked = sorted((row for row in rows if row["matchingAppPresents"] == 1), key=lambda row: row["beginHost"])
    gaps, same_epoch_gaps, boundary_gaps, pts_steps, long_gaps = [], [], [], [], []
    for previous, current in zip(linked, linked[1:]):
        if any(current[key] != previous[key] for key in ("instance", "revision")):
            continue
        gap = (current["beginHost"] - previous["beginHost"]) / 10000
        gaps.append(gap)
        epoch_changed = current["epoch"] != previous["epoch"]
        (boundary_gaps if epoch_changed else same_epoch_gaps).append(gap)
        pts_steps.append((current["pts100ns"] - previous["pts100ns"]) / 10000)
        if gap > 25:
            long_gaps.append(dict(cycle=current["cycle"], gapMs=gap,
                                  epochChanged=epoch_changed,
                                  previousPresentMs=previous["presentMs"], sleepMs=current["sleepMs"]))
    header = re.search(r"Frame trace: records=(\d+) capacity=(\d+) overwritten=(\d+)", text)
    return dict(scope="Retained bounded CPU trace only; Present wall time includes descheduling, not GPU execution or scanout.",
                overwritten=int(header[3]) if header else None, cycles=len(rows), linkedCycles=len(linked),
                sdkReturnIntervalMs=distribution(output_gaps),
                firstScheduleParts={name: distribution(values) for name, values in first_schedule_parts.items()},
                fenceEntryToDeadlineMs={name: distribution(values) for name, values in fence_groups.items()},
                sdkCallerTransitions={key: {name: distribution(values) for name, values in group.items()}
                                      for key, group in transitions.items()},
                sdkReturnGapsOver10ms=sum(gap>10 for gap in output_gaps),
                sdkReturnGapsUnder1ms=sum(gap<1 for gap in output_gaps),
                missingOrAmbiguousLinks=len(rows)-len(linked),
                preparationCycleMismatch=sum(row["preparationCycle"] not in (None, 0, row["cycle"]) for row in rows),
                sleepMs=distribution([row["sleepMs"] for row in rows if row["sleepMs"] is not None]),
                providerPresentMs=distribution([row["presentMs"] for row in rows if row["presentMs"] is not None]),
                sourcePresentIntervalMs=distribution(gaps), sourcePtsStepMs=distribution(pts_steps),
                sameEpochIntervalMs=distribution(same_epoch_gaps),
                epochBoundaryIntervalMs=distribution(boundary_gaps),
                longSourceGaps=long_gaps, rows=rows)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.trace)
    args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({key: value for key, value in result.items() if key not in ("rows", "longSourceGaps")}, indent=2))
