# NR quality and performance execution

Baseline: bcdbfbf, checkpoint/pre-nr-quality-perf-20260921.
Branch: codex/playback-nr-20260920. User authorized local implementation and
verification of research items 1/2/3/4/6, then 1.4.2 versus 1.4.3 performance.
NR layering is excluded. NVOF remains default. No publishing or shutdown.

## Acceptance and bounded work

- Preserve existing effects, formats, user-selected multipliers and features.
- Do not trade image detail or fixed added latency for throughput numbers.
- Keep each accepted change in a separate Git checkpoint. Revert our own
  unsuccessful experiments and record evidence, never unrelated user changes.
- Per direction: establish baseline, one concrete implementation, targeted
  tests and one correction/retest if justified. Stop speculative tuning when
  evidence identifies a hardware limit or no benefit.
- Distinguish shader correctness, actual NVIDIA execution, software timing,
  visual acceptance and physical display latency. No claims across boundaries.

## Sequence

1. Audit current temporal NR, masks, timestamp/reset contract and upstream
   signed-residual stabilization. Implement bounded fixes with real GPU tests.
2. Investigate independent VFX denoise dependencies and GPU-only integration.
   Never substitute invented NGX quality values or CPU pixel readback. If the
   runtime/API prerequisites are unavailable, record the concrete limitation;
   do not add a nonfunctional UI or claim denoise is delivered.
3. Add regression cases for temporal output, detail, protection, HDR finite
   values, motion rejection and resets, using attributed upstream tests where
   applicable. Verify the actual product shader/pass, not only a CPU mirror.
4. Audit guidance frame identity and resource lifetime; repair demonstrated
   mismatches with focused tests, avoiding per-frame synchronous waits.
5. Evaluate source-based XeSS timing in isolation only if caller contracts
   support it. Compare against zero hint. Retain only demonstrated benefit;
   leave duplicate detection deferred unless required by a reproduced fault.
6. Compare verified 1.4.2/1.4.3 packages with identical video and parameters,
   matched runtimes where possible, source/working/output sizes, GPU timings,
   presentation intervals, skips and GPU power. Record confounders. Inspect
   source differences around any reproduced cost increase, then validate fixes.

## Evidence paths

Artifacts: E:/项目/Veyra/tests/nr-quality-perf-20260921/.
Logs: E:/项目/Veyra/logs/nr-quality-perf-20260921/.
Process temporary files: E:/项目/Veyra/tmp/nr-quality-perf-20260921/.
Build: E:/项目/Veyra/build/playback-nr-20260920/ (existing configured tree).
Video: E:/项目/Likely7 个人账号/Deepseek Grok/p001.mp4; PS5 is off.

## Progress

- Baseline clean and checkpoint created; goal active.
- Research basis: MAGPIE_NR_XESS_COMPARISON_2026-09-21.md.
- Implementation and acceptance pending.
