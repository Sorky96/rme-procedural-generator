# G7 — gate status

**G7 open.** Criteria: [G7_METRICS.md](G7_METRICS.md).

## I57 — copy index built in the GUI (2026-11-23)

Criteria `6fe44f4`. Core `d9c2e82`, test fix `aadf14a`, GUI and probe `ab02283`.

**What it does.** The copy index (`i53-index/3`) holds 1–16 named areas instead of exactly M06 and M19. `i53-index/2` still loads. **Learn from open map…** has an option to add the analyzed area to a copy index in the local profile folder, under the profile name. The index edit is checked before the profile is written, so a refused edit saves nothing. The generator offers a **Copy check** list with None, the local indices (an index of another client or an old format is listed with the reason and cannot be chosen), and Other file…, which keeps the path field.

**Criteria results.**

1. **Parity: PASS.** The rebuilt M06/M19 index as `/2` and the same index as `/3` gave identical `candidate_rank` TSV except the similarity line, for m06 and m19, seeds 3001–3032, N=4 (8/8 blocks). The `/2` output of the new build is byte-identical to the I55 matrix produced before the change.
2. **Unit tests: PASS** (`COPY_GUARD_PASS`). They cover 1 and 16 references, a 17th refused, a declared count of 17 refused, duplicate alias and duplicate sample, path and device names, a too-small sample, `/2` and `/3` giving identical assessments, a rotated and mirrored copy of a one-reference index blocked while different terrain is clear, the sample raster projection, save without overwrite, a changed file not overwritten, and the folder listing including another client.
3. **Real-client probe: PASS** (`g1_run_resolver_probe.py --learn --visible`, client 1098-BlackTek, `LEARNING_DIALOG_PASS`, input assets unchanged). The new I57 section learned a 128×128 Natural surface area. An index name with a path saved nothing, and the area was added to a new index while the map and Undo stayed unchanged. The generator listed the index. Regenerating the same terrain was blocked at Place. Seed 4243 was evaluated below the copy thresholds (14‰) and placed with one Undo and an exact Redo. Seed 4242 was also clear (15‰) but was refused by reachability (D30), so the probe takes the first placeable seed of 4242–4249. All I56 checks passed in the same run.
4. **Regression: partly.** The canonical `rme.exe` build passed. Core tests are 23/23 after `aadf14a`, which fixes a pre-existing test that dereferenced a refused serialization (undefined behavior that passed or failed depending on the build). The Commit probe passed. G5 `--i47` with rockBlocks m06=0, m19=1 gave `G5_GATE_PASS`, 32/32 for both profiles, identical repetitions. Fingerprint 1729 is checked by the core suite. **The Surface probe fails** in its M19 cave case, also on the build without I57 changes (D30).

**Not verified.** Manual visual and keyboard review of both windows. Indices larger than one area in the GUI (covered only by unit tests). A full-size learning area (512×512) in the probe: Place of 256×256 Natural surface with caves exceeded the probe profile's Undo limit (40 MiB / 400 actions), so the probe uses 128×128.

**Found while testing.** A Place refused by the Undo limit writes its reason to the log, but the status label the probe read still showed the previous message. Not investigated.

## D30 closed (2026-11-24)

A profile without learned out-of-reach shares uses the rule from before `e4c5e80` again: every place and floor transition must be reached, single cells are not counted, and the status no longer prints a cell allowance for it. With the M19 cave case passing, the Surface probe reached two door/wall assertions made stale by `1038883` (without doors the entrance stays an open gap). The tests now expect walls to differ only at the entrances. `g5_run_suite.py --probes`: core 23/23, Commit and Surface probes PASS, `SUITE_PASS`. This completes I57 criterion 4.

## I58 — statistics and warnings (2026-11-24)

Criteria `ec4b30a`.

**What it does.** Below the candidate gallery the generator shows two lists for the Selected candidate. **Statistics:** area, Surface terrain shares (counted from the final Surface grid, now kept as `PreparedTerrain::surfaceCells`), places reached per kind, walkable floor transitions, cells out of reach on the ground floor and above with the allowance or "not limited", repairs, similarity per copy-index reference, the preparation's memory estimate against the series budget, and the preparation time of the candidate's last attempt. **Warnings:** Blocking entries, one per hardFail cause, then Warning entries, or "No warnings". The status line is one sentence with the counts. `terrainReportOf` (core) computes both lists. The hardFail decision is unchanged.

**Clarification (2026-11-24, after implementation).** The criteria named a similarity state "Near", which the copy assessment does not have. The warning is given for a reference with coverage of at least 50‰ (half the copy threshold) that is not a copy, and only when the sample was large enough to be judged.

**Criteria results.**

1. **Unit tests: PASS** (`TERRAIN_REPORT_PASS`). 4000 random combinations of every hardFail cause under the rule without learned shares and three tolerances: Blocking warnings non-empty exactly when `candidateHardFail` is true. Each cause named once. Shares add up within rounding, lakes and rivers merged into water. "Not limited" without learned shares. Over, above half, and well within the allowance. Pockets, near references and memory above 75% warn without blocking. A sample too small to judge gives no warning.
2. **Real-client probe: PASS** (`--learn --visible`, `LEARNING_DIALOG_PASS`). The copy of the learned area has a Blocking warning naming `i57-source` and a similarity row. The placeable other terrain has no Blocking warning, terrain shares and a similarity row. In a two-candidate series, selecting the other candidate refreshed the statistics. In every checked state Blocking warnings existed exactly when Place was disabled. Map and Undo unchanged. All I56 and I57 sections passed. Seed 4242 is now placeable (D30 fixed).
3. **Regression: PASS.** `g5_run_suite.py --probes`: core 24/24 (new `test_procedural_terrain_report`), Commit and Surface probes, `SUITE_PASS`. The Surface probe read "removed places" from the old status line; it now reads the Repairs row. G5 `--i47`: `G5_GATE_PASS` 32/32 for both profiles, identical repetitions.

**Not verified.** Manual visual and keyboard review, including how the lists fit at small dialog sizes. The candidate-gallery probe (`test_procedural_candidate_dialog`) was not rerun; it reads only the gallery column, which is unchanged.

## I59 — presets and seed randomization (2026-11-24)

Criteria `5f4eb7e`.

**What it does.** Above the generator's sections: a **Preset** choice, **Delete preset**, a preset name field and **Save preset**. Presets are `terrain-preset/1` files in the `presets` subfolder of the local profile folder. A preset holds every section input except the area position and the copy check. Loading applies the values with the events of a user edit, repeating up to three passes because a profile choice resets its brushes. If any value cannot be applied, every input returns to its previous value and the status names the refused value. Overwrite and delete ask first (a wx message dialog named `terrain-confirm`). **Random** next to the seed fills a new 64-bit seed from `std::random_device` and generates nothing.

**Criteria results.**

1. **Unit tests: PASS** (`TERRAIN_PRESET_PASS`). Round trip, sorted output, CRLF, empty preset and empty value. Refusals: wrong schema, empty file, missing final newline, line without a tab, duplicate, unsorted, bad names, tab and control characters, 1025-byte value (1024 accepted), 129 entries. Save refuses path and device names, does not overwrite without the flag, overwrites with it, refuses while the folder lock is held. Listing, delete once, delete refuses paths, a truncated file is refused.
2. **Real-client probe: PASS** (`--learn --visible`, `LEARNING_DIALOG_PASS`). Seed, width, path length, grass brush, vegetation and style were saved, changed and loaded back exactly while the area position stayed. The file holds no area position or copy-index entry. Presets with a missing brush, an out-of-range width and an unknown setting were refused with every checked setting unchanged. Overwrite and delete asked; No kept the file, Yes replaced or deleted it. Random seed gave a different valid 64-bit seed and started nothing. Map and Undo unchanged. The probe answers the question with `wxModalDialogHook`; a `CallAfter` or a `wxTimer` posted before the click was not run while the question was open.
3. **Regression: PASS.** `g5_run_suite.py --probes`: core 25/25 (new `test_procedural_terrain_preset`), Commit and Surface probes, `SUITE_PASS`. G5 `--i47`: `G5_GATE_PASS`.

**Not verified.** Manual visual and keyboard review of the new rows. Every input of the dialog after load (the probe checks six settings on four tabs plus the area position). A preset saved under one client and loaded under another with different brushes beyond the missing-brush refusal.

## I60 — comparison and preview navigation (2026-11-24)

Criteria `d410e19`.

**What it does.** The minimap preview zooms with the mouse wheel at the cursor (1x fit up to 32x, never above 64 pixels per cell), pans by dragging, and fits on double click. It takes keyboard focus (focus ring drawn): `+`/`-` zoom at the centre, arrows pan by a tenth of the pane, `0` fits. The label `terrain-preview-view` gives the zoom, the visible cells with map coordinates and the cell under the cursor. **Compare with** (`terrain-compare`) shows the Selected candidate on the left and another candidate of the series on the right with one shared view, only on the Surface floor. Each candidate's Surface image is kept when the gallery is built, within 64 MiB for the series; larger images are reduced and the label says so. The view stays for an area of the same size and fits otherwise. The viewport arithmetic is in core (`preview_viewport`).

**Criteria results.**

1. **Unit tests: PASS** (`PREVIEW_VIEWPORT_PASS`). Fit scale and centring; for 6 areas (including 1x1, 1x2000, 2000x2000) x 4 panes (including 1x1 pixel) x 16 anchor points x every level: the scale doubles per step, the anchor keeps its screen position within one pixel unless the view stops at the area's edge, the anchor cell stays visible, the 64 px cap holds, no level above the cap, zoom out returns to the fit. Pan clamps at all four edges, an 80 px pan at 8 px per cell moves 10 cells, the cell under a point agrees with the visible rectangle, letterbox points have no cell.
2. **Real-client probe: PASS** (`--learn --visible`, `LEARNING_DIALOG_PASS`). On a two-candidate 128x128 series: the initial view is the whole area; a wheel step at a quarter of the panel gave 2x with the reported cursor cell inside the visible cells; `+` gave 4x with a smaller view; arrows panned right; `0` fitted. Comparing with the other candidate named it in the label, kept the zoom, the statistics, the Place state and the selection. Selecting the other candidate kept the 2x view. Map and Undo unchanged.
3. **Regression: PASS.** `g5_run_suite.py --probes`: core 26/26 (new `test_procedural_preview_viewport`), Commit and Surface probes, `SUITE_PASS`. G5 `--i47`: `G5_GATE_PASS`.

**Not verified.** What is drawn: the probe reads the view label, not pixels, so the side-by-side images, captions, focus ring and stretched drawing were not checked. Mouse drag (only wheel and keys were exercised). The 64 MiB reduction (the probe's series is far below it). Manual visual and keyboard review.

## I61 — preview off the GUI thread (2026-11-24)

Criteria and baseline `21b77ea`.

**What it does.** The reachability check and repair in the preview's level loop run on the owned worker (`CoreTask`) while the preparation waits. Passes that must touch the map, brushes or the preview buffer stay on the GUI thread but now yield: the DrawOperations preview builder checks its 8 ms deadline in the item check, the memory estimate and every placement pass (structures, outline, vegetation, overlays, fires, entrances and ramps, walls, wall alignment, wall stacks, doors); the level loop's tile scan yields on the same deadline; the resolver, snapshot and blocker-clearing calls of the cave and plateau stages each start a new step. The dialog logs, per prepared candidate, the number of GUI-thread steps and the longest one, and the statistics list shows the longest step.

**Criteria results.**

1. **Responsiveness: PASS.** Same measurement as the baseline (temporary per-step instrumentation, removed again; `g1_run_resolver_probe.py --surface --visible`): 6929 steps, longest 49.4 ms (baseline 117 ms), none of 50 ms or more (baseline 376), 69 of 33 ms or more = 1.0% (baseline 9.4%). The remaining longest steps are single resolver or `clearTerrainPreviewBlockers` calls (up to 49 ms) on 512x512 multi-floor areas.
2. **Identical results: PASS.** G5 `--i47` `G5_GATE_PASS`; all 16 run logs identical to the I60 run except timing lines (repairs, bridges, removed decor, passes, routes, categories, edges). Core suite with fingerprint 1729 passed. `LEARNING_DIALOG_PASS` with the same seeds (I57 other seed 4242, I58 and I60 sections).
3. **Cancellation: PASS.** The Surface probe's cancellation and retry checks (`SURFACE_WORKER_CANCEL_RETRY_PASS`) passed.
4. **Regression: PASS.** `g5_run_suite.py --probes`: core 26/26, Commit and Surface probes, `SUITE_PASS`.

**Not verified.** The 1% limit is met with no margin (69 of 6929). Areas above 512x512 (up to 2000x2000) were not measured; single resolver calls grow with the area and will exceed 50 ms there. How the dialog feels to a user (no manual test).
