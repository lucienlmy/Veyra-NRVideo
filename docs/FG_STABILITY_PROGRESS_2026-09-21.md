# FG stability execution evidence

## Scope and recoverable state

Worktree: E:/项目/Veyra/worktrees/playback-nr-20260920.
Branch: codex/fg-stability-20260921. Pre-work checkpoint d2ae8ee /
checkpoint/pre-fg-stability-20260921. Diagnostics checkpoint 0143baf.
Goal remains active. No release, package replacement or claim of completion.

## P0/P1: XeSS heavy-load timing candidate

Source-linked XeLL/Present diagnostics distinguish preparation cycle IDs from
actual source identities. Multiple queued sources can share a preparation
cycle; mismatch alone is not a failure. Provider output return records have
no source identity: they must not be attributed to a source by guessing.

Baseline source Present intervals alternate short/long around repeated epoch
resets. Within-burst generated spacing approaches 8.4ms despite consecutive
60fps source PTS; a 4X continuous pair corresponds to 4.167ms spacing.
The SDK scheduler waits are implicated, but exact internal causes remain open.

Candidate VEYRA_TEST_XESS_SOURCE_TIMING=1 supplies the actual consecutive source
PTS delta as frameRenderTime only when generation history is valid, identity
advances by exactly one, PTS increases, and delta is 0.125..500ms. Resets,
skips, repeated/unknown sources retain zero. It never feeds wall Present time
back, fills a fixed nominal period, adds waiting, disables effects or changes
the selected multiplier. Existing source correspondence checks still apply.
Default remains unchanged; this is a diagnostic candidate, not release approval.

Reference examined: Magpie 3841698348bfb246623d4acf791984c8b68a577b,
XeSSFGTiming.h, XeSSFGPresenter.cpp, XeSSFGPacing.h (clean checkout).
Magpie also has timestamp-deadline hooks absent from Veyra; its full timing
implementation has NOT been ported. This candidate uses existing source PTS
directly without copying the upstream median/fallback or timestamp hooks.

### Matched evidence

RTX5070, p001-derived1080.mp4 -> VideoSR quality3 4K + NR + XeSS4.
Same candidate executable, trace enabled, sequential 30s off/on runs:

| Metric | Off | On |
| --- | ---: | ---: |
| Retained source Present/s | 39.94 | 52.30 |
| NVML device GPU mean | 70.09% | 95.08% |
| Retained source interval P99 ms | 46.27 | 21.10 |
| Retained SDK return interval mean ms | 7.693 | 5.367 |
| Retained SDK return interval P99 ms | 23.25 | 18.58 |
| Retained SDK return maximum ms | 23.43 | 19.03 |
| Process age-to-Present-return P95 ms | 56.35 | 38.84 |
| SDK intervals under1ms / samples | 359/1566 | 399/1676 |
| SDK intervals over10ms / samples | 361/1566 | 401/1676 |

Both exit0, failed=false. The candidate improves throughput and long-gap
duration, but burst fractions do not improve. Short/long outputs remain;
this does NOT meet uniform4X acceptance. Source coverage remains below60.
No physical scanout, synchronized output-image quality comparison or glass-to-
glass latency measured. Ring tails differ in duration; raw counts are not
comparable rates. Original rings overwrite10697/16908 records respectively.

Earlier20s runs reproduce source39.98 ->52.35, native4K + NR XeSS4 remains
~60 source/s with candidate on. Neither pair establishes long-run stability.
Candidate-on20s2X smokes also maintain ~60 source/s for native4K+NR and
true1080p-to4K SR+NR, both exit0/failed=false. Evidence:
tests/fg-stability-20260921/source-timing-2x/{native-nr-xess2,sr-nr-xess2}.
These are source cadence/lifecycle checks, not2X visual or scanout acceptance.
The extra native SDK call instrumentation is opt-in and bounded, with no
per-frame file writes. It records return times, not physical display events.

### Commands and artifacts

Root for all artifacts: E:/项目/Veyra/.
Build: scripts/build-isolated.ps1 -Root . -BuildDirectory
E:/项目/Veyra/build/playback-nr-20260920 -DependencyCache
E:/项目/Veyra/build/frame-pacing-20260918/CMakeCache.txt -TempDirectory
E:/项目/Veyra/tmp/fg-stability-20260921 -Targets veyra -DisplayVersion 1.4.4beta.
Build logs: logs/fg-stability-{source-timing,output-trace}-20260921.log; exit0.
Initial staging Copy-Item guessed a Release subdirectory and failed without
copying; corrected to the actual build-root veyra.exe.

Test harness: scripts/acceptance/fg-utilization-matrix.py with --exe
tests/fg-stability-20260921/app/veyra.exe, native user p001.mp4 and derived
tests/nr-fg-followup-20260921/p001-derived1080.mp4, --cases sr-nr-xess4
--seconds30, process TEMP/TMP tmp/fg-stability-20260921. Each process has
seconds+45 watchdog. Evidence roots:
tests/fg-stability-20260921/{timeline-a,source-timing-b,output-off,output-on}.
Per-case result.json records executable hash, environment, exact argv and
telemetry; trace.txt/app.log/stdout.log preserve raw evidence.
analyze-xess-timeline.py generated adjacent xess-timeline.json files.

Three synthetic analyzer tests pass (epoch boundaries, rebuild/revision,
missing/ambiguous joins); git diff --check passes. Initial analyzer omitted
epoch-boundary gaps: corrected before drawing conclusions or saving results.

## Whole-run and lifecycle follow-up

Added opt-in cumulative SDK-return histograms to XessPacing.cpp, bounded to
1001 quarter-millisecond bins per phase. First5s from first hooked return and
the remainder are separate. Percentiles are bin upper bounds (overflow uses
observed maximum). Totals include boundary gaps and are emitted on hook
release after provider context destruction, not per frame. They measure hook
lifetime, not application launch or scanout; 2X has no pacing hook coverage.

Same executable SHA256
6eafd66777b4ce307ccfc14abe70324a694b67f98e66f77327988d0f0419ad74:
heavy SR+NR+XeSS4, sequential120s on,30s off,30s on. Steady statistics:

| Metric | Off30s | On30s | On120s |
| --- | ---: | ---: | ---: |
| SDK interval samples | 2928 | 4198 | 20841 |
| Mean interval ms | 7.701 | 5.383 | 5.402 |
| P50 upper ms | 8.500 | 4.250 | 4.250 |
| P95 upper ms | 23.000 | 12.500 | 12.500 |
| P99 upper ms | 23.250 | 18.750 | 18.750 |
| Maximum ms | 41.124 | 43.733 | 45.954 |
| Intervals under1ms | 668 | 997 | 4940 |
| Intervals over10ms | 676 | 1008 | 4994 |
| Retained source presents/s | 40.03 | 52.23 | 52.09 |
| Process age-to-return P95 ms | 56.312 | 38.752 | 38.990 |

All exit0/failed=false. Sustained throughput improvement repeats, but burst
fraction and maximum gaps do not improve. Unequal run lengths make maxima
and raw event counts unsuitable as comparative rates. The matched30s pair
also has isolated >40ms gaps. This candidate is NOT uniform-cadence accepted.
Earlier retained-tail maxima understated whole-run spikes. Default stays off.

Evidence roots: tests/fg-stability-20260921/whole-{on120,off30,on30}.
Build log: logs/fg-stability-whole-output-20260921.log; build exit0.
Harness and fixed media/configuration are the same as above, with respective
--seconds120/30/30 and source timing enabled only for on runs.

ui-fg-backends.py EXE p001.mp4 OUTPUT --portable with timing candidate enabled
passes12 selector transactions: XeSS/DLSS round trips, XeSS2X/4X rebuilds,
DLSS6X to XeSS supported4X, and off. Three layout sizes checked. Evidence:
tests/fg-stability-20260921/source-timing-switch/result.json and app.log.
This script disables NR/SR and is a lifecycle check, not heavy-load visual,
seek or continuous-resize acceptance. Analyzer3 synthetic tests still pass.

## Next acceptance and remaining phases

Full-run interval aggregation, sustained run and selector switches now have
evidence above. Dynamic seek/continuous-resize and visual correspondence
remain required before enabling by
default. Do not call it an accepted complete repair. Investigate persistent
short/long burst boundaries independently; no extra queue or fixed wait.
P2 DLSS long gaps and P3-P6 remain as listed in the execution plan; this work
does not change or complete them. Independent denoising prerequisites remain
unresolved. No software performance claim is extended to other GPUs.
