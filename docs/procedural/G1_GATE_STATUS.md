# G1 — Criteria status 2026-11-05

**G1 CLOSED WITH DEBT (user decision, 2026-11-06).** Five gate criteria have evidence, resource thresholds 500×500 and 100×100 approved and met. Open items are in [TECH_DEBT.md](TECH_DEBT.md) (D1 `SetFocus` on close, D2 unexplained activation stack, D3 untested allocation failure sites, D4 abrupt memory growth); debt is not criterion completion. Live sessions are outside G1 scope.

Disclaimers: Place 500×500 times come from EXE `15c25557…`, not current `b87d1abb…`; many proofs are single runs; PR has no CI and intermediate commits were not built separately. Evidence content below remains unchanged and describes state at time of measurement.

Gate scope derives from [generator plan](../PROJECT_RME_PROCEDURAL_GENERATOR.md) and [test plan](TEST_PLAN.md#tests-to-add-with-g1). Matrix below accounts for evidence later than open-work lists in staged reports.

| Criterion | Current evidence and scope |
| --- | --- |
| 100×100 and 500×500 on single floor | Both sizes passed functionally in native RME on EXE `15c25557…`, and 2026-11-06 full round-trip of both sizes repeated on current EXE `b87d1abb…`. OTBM status of both runs remains partial due to `SetFocus` warning. [Round-trip](G1_ROUNDTRIP.md) |
| Preview matches Place, one Undo and Redo | Observer compared all generated tiles with independently prepared result. Place created one history entry. Core also has atomicity and separate-history attempts. [Round-trip](G1_ROUNDTRIP.md), [commit](G1_COMMIT.md), [staged regression](G1_STAGED_CORE.md) |
| Complete file set after save and re-open | OTBM 5 and five XML companions for 1098-BlackTek checked in full trials of both sizes. Three 500×500 timing trials saved complete files byte-identical with retained full trial of that size. [Round-trip](G1_ROUNDTRIP.md), [repeatability](G1_PLACE_TIMING_REPEATABILITY.md) |
| No changes outside mask and protected data | Full rectangle result and fixture checked in round-trip. Native exact-mask test confirmed one protected tile inside mask preserved, with UID 32101, object identity and selection. [Round-trip](G1_ROUNDTRIP.md), [dialog matrix](G1_DIALOG_MATRIX.md) |
| Measured 500×500 resource budget | Current build has staged Preview measurements and three trials of Place, Undo, Redo and Save. Thresholds approved 2026-11-06; evaluation in “Approval of thresholds” section. [Cancel](G1_CANCEL.md), [repeatability](G1_PLACE_TIMING_REPEATABILITY.md) |
| Measured 100×100 resource budget | Current build has one verified Place, Undo, Redo and Save measurement and three cycles of staged Preview, Validate and Cancel. Thresholds approved 2026-11-06; evaluation in “Approval of thresholds” section. [100×100 measurement](G1_ROUNDTRIP.md#place-undo-redo-and-save-measurement-for-100100), [baseline measurement](G1_RESOURCES.md) |

C++ core, resolver and sandbox have evidence for seeds, bad inputs and deterministic diff. Native dialog covers Preview, Validate, Place, Cancel and old-selection refusal. Dialog-operation logs have runtime proof only for refusal-flow paths (see below); Validate, Cancel and Place not covered in it. MVP preview checked as minimap. [Core](G1_CORE.md), [resolver](G1_RESOLVER.md), [sandbox](G1_PREVIEW.md), [dialog matrix](G1_DIALOG_MATRIX.md)

Cancellation of active 500×500 Preview has current proof. Native Preview refusals including one halo allocation failure, seed edit and dialog close during work have functional proof on EXE `1660d46b…`. Manifest comparison from `15c25557…` showed changes in `terrain_preparation.cpp` and `terrain_generator_dialog.cpp`. Repeat of `--refusals` on tree from 2026-11-06 (`NEW_RUN_REFUSALS_2`, observer EXE `6de317d2…`, production `b87d1abb…`) passed functionally: six cases and `REFUSAL_PASS`, verifier `PARTIAL…FOCUS_WARNING` as before. This refreshes proof for preflight change. Result coexistence, leaks and scope of variant-limit and repair-attempt tests remain open. [Cancel](G1_CANCEL.md), [refusals](G1_REFUSALS.md), [invalidation](G1_INVALIDATION.md)

100×100 Preview measurement on staged build done 2026-11-06 (see below). `SetFocus` diagnosis in refusal flow: [G1_FOCUS_TRACE.md](G1_FOCUS_TRACE.md#window-status-at-error-in-refusal-flow-2026-11-06); on app close: see [closing](G1_FOCUS_TRACE.md#app-close-and-probe-exit-code-2026-11-06); activation stack from 2026-11-04 remains unexplained.

## Preliminary resource thresholds (2026-11-06)

Thresholds set after reviewing measurements from 2026-11-03…05, so they are **preliminary**: passing requires a fresh series of trials on one build, evaluated against values below without changing them.

| Metric | 500×500 | 100×100 |
| --- | --- | --- |
| Preview to readiness | ≤ 3 s | ≤ 250 ms (see approval below) |
| Max GUI timer pause, Preview | ≤ 250 ms | ≤ 250 ms |
| Place to history change | ≤ 1 s | ≤ 250 ms |
| Max GUI timer pause, Place | ≤ 750 ms | ≤ 250 ms |
| Working set increase, Preview | ≤ 150 MiB | ≤ 30 MiB |
| Working set / private commit increase, Place+Undo+Redo+Save | ≤ 200 MiB | ≤ 30 MiB |
| Memory after Cancel | no growth trend in 10 cycles | ditto |

## Approval of 100×100 thresholds (2026-11-06)

User approved 100×100 thresholds from table above. In table, “Preview to readiness” had value “to measure”; replaced with ≤ 250 ms (measured ~110 ms). This specific value was proposed in conversation and user did not separately confirm it, so it requires confirmation. Approved thresholds do not change earlier results: one 100×100 series on staged build fits within them, but series is single and was not repeated.

## 200-cycle run (2026-11-06)

Run `g1-leak-200-20261106` (`g1_run_leak_trend.py --cycles 200 --side 500`, self-driven: menu and button events queued, not native input; production EXE `b87d1abb…`). Runner `passed: true`, empty stderr, `LEAK_PASS`.

Criterion set before first run (2 warmup cycles, slope ≤ 1 MiB/cycle, GDI/USER/handles no higher than after cycle 2) **is met**: working set slope 0.31, private 0.31 MiB/cycle; GDI increase −12, USER −2, handles −2. GDI takes four values (479, 480, 488, 492), USER four (496–501), handles two (465, 467), no trend.

Auxiliary boundedness test added before run: difference of average private memory between cycles 101–150 and 151–200 ≤ 5 MiB. **Not met**: 619.2 → 627.9 MiB, difference 8.6 MiB (working set 770.5 → 779.0). Private memory grows in steps (559 MiB at start, 585 after cycle 20, ~615 after cycle 60, ~626 after 130, 634 after 200), at same points as 30-cycle series, but continues growing in last 100 cycles, slower than first 30. Plateau not demonstrated. Causes of steps not investigated (allocator reserve growth, container capacity or slow leak: no distinction). Series is one and not statistically evaluated.

User decision from 2026-11-06: memory growth in this series not a G1 blocker, because maps where it would matter (hundreds Preview cycles in one session) are not expected use. Cause of steps still not investigated; this is a priority decision, not a measurement result.

## Evaluation of approved thresholds (2026-11-06)

User approved 2026-11-06 thresholds 500×500 and 100×100 from table “Preliminary resource thresholds”, including Preview 100×100 ≤ 250 ms (value filled in place of “to measure”). Verifier in current form (with Google Drive extension line filter and `PARTIAL…FOCUS_WARNING` status) user deemed sufficient; statuses not reclassified. Discrepancies and warning limitations described in [debt D1 and D2](TECH_DEBT.md).

| Threshold | Measured | Source |
| --- | --- | --- |
| 500×500 Preview ≤ 3 s | 0.73–1.35 s (cycles 4–6) and 0.76–1.74 s in earlier series | `g1-res-100-20261106`, [Preview series](#cancellation-and-500×500-preview-resource-series-2026-11-06) |
| 500×500 GUI pause Preview ≤ 250 ms | ≤ 148 ms | ibid |
| 500×500 WS increase Preview ≤ 150 MiB | ≈ 80 MiB | ibid |
| 500×500 Place ≤ 1 s | 403–419 ms | three trials on EXE `15c25557…`, [repeatability](G1_PLACE_TIMING_REPEATABILITY.md) |
| 500×500 GUI pause Place ≤ 750 ms | 428–544 ms | ibid |
| 500×500 WS increase Place+Undo+Redo+Save ≤ 200 MiB | Place 110 MiB, other ops up to 2.4 MiB each | ibid |
| 100×100 Preview ≤ 250 ms | 109–111 ms | `g1-res-100-20261106` |
| 100×100 GUI pause Preview ≤ 250 ms | 38–53 ms | ibid |
| 100×100 WS increase Preview ≤ 30 MiB | ≤ 3.6 MiB | ibid |
| 100×100 Place ≤ 250 ms, GUI pause ≤ 250 ms, WS increase ≤ 30 MiB | Place 63 ms; total increase 4.4 MiB | one trial, [100×100 measurement](#100×100-measurement-on-current-build-2026-11-06) |
| Memory after Cancel no growth trend | met in 200-cycle series (slope 0.31 MiB/cycle, GDI/USER/handles no increase); plateau not demonstrated (debt D4, not blocker) | `g1-leak-200-20261106` |

Limitations of this evaluation: Place, Undo, Redo and Save measurements for 500×500 from EXE `15c25557…`, not current `b87d1abb…` (on current binary functional round-trip repeated, without timing); Place 100×100 is one trial; Preview value in 500×500 series varies widely. Document does not statistically evaluate any of these series.

## Dialog operation logs (2026-11-06)

`stdout.log` from run `NEW_RUN_REFUSALS_2` contains `Terrain generator: …` entries for: area selection readiness (info), five refusals (warning), twice “Preparing preview” (info) and final “Preview ready” with tile count, seed and memory estimate (info). Validate, Cancel and “placed N tiles” entries not executed in this run, so log criterion was confirmed partially.

Run `g1-logs-roundtrip-100-20261106` (`--roundtrip --roundtrip-timing`, side 100) added “Preview validated. 10000 tiles will change when you place it.” (info) and “placed 10000 tiles as one undo step” (info). Cancel confirmed by run `g1-cancel-logs-4-20261106` (`--cancel-preview`): in log “Preparing preview” (16:16:00.419) and 0.3 s later “Preview cancelled. Press Preview to try again.” (info), observer recorded `RESOURCE_IN_FLIGHT_CANCEL_PASS` (measurement `interrupted-preview` 277 ms, `interrupt` 36 ms). This run **did not pass as whole**: after re-Preview operator cancelled instead of proceeding to Validate and observer ended `FAILED Cancel requires observed Validate`. Credited only in-flight cancellation and its log, not retry with seed 1728 or three resource cycles. Three earlier attempts that day failed: two because 500×500 Preview ended ~1.35 s before Cancel, one because Esc closed dialog before Preview start. Log criterion thus confirmed for Preview, Validate, Place, Cancel and refusals.

## 100×100 measurement on current build (2026-11-06)

Production EXE `b87d1abb…`, observer `87ffbd44…`, verifier `PARTIAL_G1_GUI_ROUNDTRIP_TIMING_FOCUS_WARNING` (only warning `SetFocus` 0x57), CSV `4bc89b1b…`. Measurement quantized to 20 ms.

| Operation | Time | Max GUI pause | WS increase | Private increase |
|---|---|---|---|---|
| Place | 63.1 ms | 89.0 ms | 4.3 MiB | 4.9 MiB |
| Undo | 25.8 ms | 37.3 ms | 2.6 MiB | 3.0 MiB |
| Redo | 20.5 ms | 54.4 ms | 0.6 MiB | 0.7 MiB |
| Save | 85.9 ms | 89.7 ms | 0.0 MiB | 0.0 MiB |

Total increase from start of Place to end of Save: working set 4.4 MiB, private commit 5.4 MiB. Against preliminary 100×100 thresholds: Place ≤ 250 ms, GUI pause ≤ 250 ms and increase ≤ 30 MiB met in this one trial. This is single trial, so does not meet “fresh series of trials” condition. 100×100 Preview measurement and series of repeats remain open.


## Cancellation and 500×500 Preview resource series (2026-11-06)

Run `g1-cancel-logs-6-20261106` (`--cancel-preview`) passed entirely: in-flight cancellation, retry with seed 1728, and three full cycles of Preview, Validate, Cancel. Runner `passed: true`, empty stderr, verifier `PASS_G1_GUI_IN_FLIGHT_CANCEL_ONLY`, production EXE `b87d1abb…`, observer `506b714c…`. Earlier attempts that day (nr 1–5) failed for operator reasons: Cancel at wrong time or too short window; remain in `local-data` without passing.

| Cycle | Preview | Max GUI pause, Preview | WS increase, Preview | Memory after Cancel (WS / private) |
|---|---|---|---|---|
| 1 (seed 1728) | 761 ms | 139 ms | 84.7 MiB | 750.8 MB / 589.6 MB |
| 2 | 1739 ms | 152 ms | 85.1 MiB | 751.6 MB / 592.4 MB |
| 3 | 1417 ms | 154 ms | 84.1 MiB | 752.6 MB / 592.9 MB |

Against preliminary 500×500 thresholds: Preview ≤ 3 s, GUI pause ≤ 250 ms, and WS increase ≤ 150 MiB are met in all three cycles. Preview time varies 0.76–1.74 s, so threshold needs evaluation from larger series. Memory after Cancel increases ~1 MB working set per cycle, but three cycles do not resolve whether trend. Threshold “no growth trend in 10 cycles” **is not evaluated**: requires 10 cycles.

Dialog log contains full sequence: “Preparing preview”, “Preview cancelled”, “Preview ready … (seed 1728)”, “Preview validated”, and two subsequent cycles.

## Memory trend after Cancel (2026-11-06)

New probe `tests/test_procedural_dialog_leak.cpp` and runner `docs/procedural/smoke/g1_run_leak_trend.py` are **self-driven**: send queued menu and button events to dialog, so check production handlers but not operator native input. Cycle: open from menu, Preview, Cancel in-flight, Preview, Validate, Cancel, 1 s wait, measure. On app startup, modal “Warnings” dialog appears (client load warnings, `EditorFactory::EnsureVersion`); probe closes it and records in `warnings-dialog-dismissed.txt`.

Criterion set before first run and unchanging: 2 warmup cycles, then least-squares slope working set and private ≤ 1 MiB per cycle and no increase in GDI, USER, and handle objects relative to cycle 2. 500×500, production EXE `b87d1abb…`.

| Przebieg | Cykle | Nachylenie WS | Nachylenie private | GDI / USER / uchwyty | Wynik |
|---|---|---|---|---|---|
| `g1-leak-10d-20261106` | 10 | +1.55 MiB/cycle | +1.99 MiB/cycle | −12 / −1 / 0 | **fails** |
| `g1-leak-30-20261106` | 30 | +0.79 MiB/cycle | +0.95 MiB/cycle | 0 / 0 / 0 | met, on margin |

30-cycle run fits criterion with small margin (0.95 at limit 1.0). Growth not linear: private increases in steps (~+12 MiB at cycle 21), slope negative in cycles 21–30 (−0.25 MiB/cycle). Cycles 3–10 slope 1.84 MiB/cycle, cycles 11–20 0.80. This shape fits heap fragmentation or growing allocator pool stabilizing, but does not resolve leak absence. Handles and GUI objects constant. Criterion “no growth trend” from plan thus **formally met at 30 cycles**, not 10, and result depends on cycle count. Requires repeating to assess variability across runs.

### 30-cycle repeats (2026-11-06)

| Run | WS slope | Private slope | Private increase cycles 3→30 | GDI / USER / handles vs cycle 2 | Result by criterion |
|---|---|---|---|---|---|
| `g1-leak-30-20261106` | +0.79 | +0.95 | 22.0 MiB | 0 / 0 / 0 | met |
| `g1-leak-30-r2-20261106` | +0.77 | +0.90 | 22.5 MiB | +12 / +1 / 0 | **not met** (GDI, USER) |
| `g1-leak-30-r3-20261106` | +0.78 | +0.90 | 21.9 MiB | +1 / 0 / 0 | **not met** (GDI) |

Memory slopes repeatable (WS 0.77–0.79, private 0.90–0.95 MiB/cycle, all below 1.0 limit), slope decreases in consecutive decades (private in r2: 1.89 → 0.73 → 0.02; r3: 1.73 → 0.64 → −0.04). Total growth ~22 MiB over 27 cycles nearly identical across three trials and concentrated in first 20 cycles.

Two failures from GDI/USER component comparing **single** end sample to cycle 2 sample. GDI series oscillate between two levels (~481 and 493, 12-object difference; r1 477 and 489) and do not grow; USER oscillates by 1. Cycle 2 hit low level, so last cycle at high level gives formal “increase”. Criterion left unchanged per pre-first-run agreement, so official result is one pass and two failures. Supplementary post-hoc observation: GDI maximum in cycles 21–30 does not exceed maximum in cycles 3–12 in any of three runs (493 / 493 / 493 and 489 in r1). Does not replace criterion; potential change to maximum comparison requires decision and re-run on same definition.

Cause of GDI oscillation and private jumps not investigated.

## Preview 100×100 on staged build (2026-11-06)

Run `g1-res-100-20261106` (`--resources`: three 100×100 cycles and three 500×500, each Preview, Validate, Cancel, native operator input). Runner `passed: true`, empty stderr, verifier `PASS_G1_GUI_RESOURCE_BASELINE_ONLY`, production EXE `b87d1abb…`, observer `424eb788…`. Verifier previously required old production revision for this mode; changed so `--resources` treated as staged build (removed comparison branch with old build).

| Cycle | Size | Preview | Max GUI pause, Preview | WS increase, Preview | Private increase, Preview |
|---|---|---|---|---|---|
| 1 | 100×100 | 109 ms | 43 ms | 3.6 MiB | 3.6 MiB |
| 2 | 100×100 | 111 ms | 53 ms | 0.2 MiB | 0.2 MiB |
| 3 | 100×100 | 109 ms | 38 ms | 0.03 MiB | 0.03 MiB |
| 4 | 500×500 | 1351 ms | 146 ms | 80.2 MiB | 83.7 MiB |
| 5 | 500×500 | 726 ms | 136 ms | 79.2 MiB | 79.2 MiB |
| 6 | 500×500 | 861 ms | 148 ms | 80.2 MiB | 83.2 MiB |

Against preliminary 100×100 thresholds: GUI pause Preview ≤ 250 ms and WS increase Preview ≤ 30 MiB met in all three cycles. “Preview to readiness” threshold for 100×100 was “to measure”: measured ~110 ms, requires value approval. Maximum timer pause in wait phase after Cancel (not in Preview) was 201 ms in cycle 2 (100×100) and 172 ms in cycle 5 (500×500), below 250 ms. Memory after Cancel for 100×100 stabilized at 587.4–588.0 MB private. 500×500 cycles confirm earlier run: Preview 0.73–1.35 s, GUI pause ≤ 148 ms.

Single series on one build, not statistically evaluated. Memory after 10 cycles 100×100 not measured separately.
