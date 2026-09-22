"""Synthetic coverage for retained trace joins and discontinuity intervals."""
import importlib.util
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("timeline", Path(__file__).with_name("analyze-xess-timeline.py"))
timeline = importlib.util.module_from_spec(spec)
spec.loader.exec_module(timeline)


class Trace:
    def __init__(self, text):
        self.text = text

    def read_text(self, **kwargs):
        return self.text


def cycle(number, epoch, begin, instance=1, revision=1, app=True):
    identity = f"source={number} epoch={epoch} revision={revision}"
    provider = f"providerInstance={instance} providerCycle={number}"
    lines = [f"event=XessBind {identity} {provider} preparationCycle={number}",
             f"event=XessPresent {identity} {provider} ms=1 presentBeginHost={begin + 1} presentEndHost={begin + 10000}"]
    if app:
        lines.append(f"event=Present {identity} pts={begin} batch={number} presentBeginHost={begin} presentEndHost={begin + 10001}")
    return "\n".join(lines) + "\n"


class TimelineTests(unittest.TestCase):
    def test_fence_readiness_and_device_removal(self):
        for before, after, expected in [(9, 10, "pendingThenReady"), (10, 10, "readyAtEntry"),
                                         (8, 9, "stillPending"), ((1 << 64) - 1, 10, "deviceRemoved")]:
            text = cycle(1, 1, 0)
            text += "event=ProviderOutput presentBeginHost=50000 presentEndHost=60000 providerScheduleBeginHost=30000\n"
            text += (f"event=ProviderDeadline host=40000 detail=1 ms=0.9 providerFenceSampled=1 "
                     f"providerFenceBefore={before} providerFenceAtDeadline={after} providerFenceTarget=10\n")
            groups = timeline.analyze(Trace(text))["fenceEntryToDeadlineMs"]
            self.assertEqual(groups[expected]["samples"], 1)
            self.assertEqual(sum(group["samples"] for group in groups.values()), 1)

    def test_output_gap_components_and_legacy_unknown(self):
        text = cycle(1, 1, 0)
        text += "event=ProviderOutput presentBeginHost=9000 presentEndHost=10000 providerCallerRva=1 providerScheduleBeginHost=8000\n"
        text += "event=ProviderOutput presentBeginHost=50000 presentEndHost=60000 providerCallerRva=2 providerScheduleBeginHost=30000\n"
        text += "event=ProviderDeadline host=40000 detail=1 count=4 ms=0.9\n"
        text += "event=ProviderOutput presentBeginHost=69000 presentEndHost=70000\n"
        result = timeline.analyze(Trace(text))
        group = result["sdkCallerTransitions"]["1->2"]
        self.assertEqual(group["returnGapMs"]["mean"], 5)
        self.assertEqual(group["beforeHookMs"]["mean"], 2)
        self.assertEqual(group["hookPacingMs"]["mean"], 2)
        self.assertEqual(group["nativeCallMs"]["mean"], 1)
        self.assertEqual(len(result["sdkCallerTransitions"]), 1)
        self.assertEqual(result["sdkReturnIntervalMs"]["samples"], 2)
        self.assertEqual(result["firstScheduleParts"]["beforeDeadlineMs"]["mean"], 1)
        self.assertEqual(result["firstScheduleParts"]["afterDeadlineMs"]["mean"], 1)
        self.assertEqual(result["firstScheduleParts"]["deadlineLeadMs"]["mean"], 0.9)

    def test_epoch_boundary_included(self):
        result = timeline.analyze(Trace(cycle(1, 1, 0) + cycle(2, 1, 160000) + cycle(3, 2, 620000)))
        self.assertEqual(result["sourcePresentIntervalMs"]["mean"], 31)
        self.assertEqual(result["sameEpochIntervalMs"]["mean"], 16)
        self.assertEqual(result["epochBoundaryIntervalMs"]["mean"], 46)
        self.assertTrue(result["longSourceGaps"][0]["epochChanged"])

    def test_rebuild_or_revision_not_joined(self):
        result = timeline.analyze(Trace(cycle(1, 1, 0) + cycle(1, 1, 160000, instance=2)
                                        + cycle(2, 1, 320000, instance=2, revision=2)))
        self.assertEqual(result["linkedCycles"], 3)
        self.assertEqual(result["sourcePresentIntervalMs"]["samples"], 0)

    def test_overwritten_or_ambiguous_app_record(self):
        text = cycle(1, 1, 0, app=False) + cycle(2, 1, 160000)
        text += next(line for line in text.splitlines() if line.startswith("event=Present")) + "\n"
        result = timeline.analyze(Trace(text))
        self.assertEqual(result["linkedCycles"], 0)
        self.assertEqual(result["missingOrAmbiguousLinks"], 2)


if __name__ == "__main__":
    unittest.main()
