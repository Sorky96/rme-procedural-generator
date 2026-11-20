# G2-B2 — Surface algorithm in the existing pipeline, state as of 2026-11-07

**PARTIAL.** Preparation, validation, preview, commit with one Undo and Redo work for the v2 surface on the real 1098-BlackTek client (100×100 and 250×250), and the G1 regressions (resolver, preview, commit) pass. The dialog in native GUI and terrain appearance in RME were not checked, so the G2 criterion “readable in RME” remains open.

## Changes

- `GenerationConfig` has `algorithm` (`GroundRoll` by default, `Surface`) and `surface` (`SurfaceTuning`, v2 parameters), plus `operator==`, which replaced two manual configuration-comparison copies (preview, validation).
- `GroundProfile` has six name slots and `count` (3 by default, 6 for Surface), and `active()` returns the used names. Name loops in the G1 resolver, preview, preparation and commit use `active()`. The G1 resolver rejects `count != 3`. G1 initializations with three brushes compile unchanged.
- Preview source hash: the byte layout is unchanged for `GroundRoll`; the algorithm and v2 parameters are appended only for Surface.
- `prepareTerrain`: branches by algorithm, reserves generator working memory (`validateSurface`) in one shared counter, and requires a profile with 3 (GroundRoll) or 6 (Surface) brushes. New `PreparationInputError::InvalidParameters`.
- `resolveSurfacePlan` returns an ordinary `GroundPlan` (profile: Grass, Dirt, Rock, Shore, Lake, River).
- Dialog: a “Terrain style” list (Classic by default, Natural surface), rock (`rocky ground`) and shore (`sand`) brush fields. Lake and river share the water field. v2 parameters have no controls (default values).

## Results (profile `g0-reference-profile-20261030`, private runs, hidden window)

| Probe | Run | Result |
| --- | --- | --- |
| G1 resolver | `g2-b2-resolver-b-20261106` | `RESOLVER_PASS`, fingerprints 15173774556512220455 and 2039901658534265928 as in [G1_RESOLVER.md](G1_RESOLVER.md) |
| `--preview` | `g2-b2-preview-c-20261107` | `Ppassed: true`, empty stderr |
| `--commit` | `g2-b2-commit-c-20261107` | `Ppassed: true`, empty stderr; 100×100 and 500×500 rows (8881480 / 3965894 / 80195 / 6608263949644166501 and 218763816 / 98785798 / 1987447 / 173161415496114986) match [G1_COMMIT.md](G1_COMMIT.md) |
| `--surface` | `g2-b2-surface-e-20261107` | exit 0, all PASS markers (`SURFACE_RESOLVER_PASS`, `SURFACE_E2E_PASS` 100×100: 10 000 changes, approx. 14.6 MB estimate; 250×250: 62 500 changes, approx. 84.2 MB). The runner recorded `Ppassed: false` because stderr contains two `SetFocus` 0x57 warnings (see debt D1) |

The `--surface` end-to-end test checks preparation, validation, rejection of changed v2 parameters, changed algorithm and a profile with the wrong brush count (both directions), commit as one history step, placed ground matching the plan in every cell, Undo (no generated ground, tile count as before) and Redo.

Details of border results for the same resolver: [G2_BORDERS.md](G2_BORDERS.md).

## Findings

- **Undo memory limit.** The default `undo_mem_size` is 40 MiB. The first 250×250 run at the default limit returned `HistoryRefused` (value 6 in `errainCommitFailure`, determined from the code); after raising it to 512 MiB, as in the G1 probes, commit passed. I inferred the cause from the numbers (84 MB > 40 MiB), without tracing the queue code. The dialog already shows an Undo-limit message in File → Preferences → General. At the default 40 MiB, I estimate the boundary for Place at about 170×170, extrapolated from one measurement.
- **The “Warnings” window at client startup** blocked probes until it was closed manually. Test-only `tests/procedural_probe_warnings.h` was added (it closes only the modal “Warnings” window and writes `warnings-dialog-dismissed.txt`), used by the `--surface` probe and `test_procedural_preview.cpp` (only idle handling was changed). Warning text was not recorded or investigated; the application has no setting to disable it.
- **The “Save changes” window** at close hung the probe after commit. The `--surface` test now clears the change flag (`map.clearChanges()`). The G1 commit probe was not changed.
- The previous `g2-b2-surface-d-20261107` run exceeded 180 s on this window and is not counted (the log had the same PASS markers). The first run of four probes (`*-20261106`) was interrupted by a timeout traceback; no preview or commit results were collected from that run.

## Not verified

- Dialog behavior after the changes (style list, rock and shore brushes, messages) in native RME, and that Classic behaves in the GUI as in G1. The G1 probes do not drive the dialog in this run.
- Terrain and border appearance in RME, including a one-tile-wide river.
- The two `SetFocus` warnings in `--surface`: I assume their origin is the same as D1 (`MapTabbook::DeleteTab` during close), but did not trace the stack. Preview and commit had no stderr warnings.
- Behavior for clients other than 1098-BlackTek, 500×500 in the full v2 pipeline (the end-to-end probe checks 100 and 250), time and memory outside the estimates, and D5 (borders outside the area next to an existing neighbor), which remains open.
