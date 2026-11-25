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

## I62 — profiles from several samples and learned ranking bands (2026-11-24)

Criteria: [G7_METRICS.md](G7_METRICS.md#i62--profiles-from-several-samples-and-learned-ranking-bands-2026-11-24-before-implementation).

1. **Unit tests: PASS.** `test_procedural_candidate_score`: the 396e057 rule reproduces the frozen m06/m19 bounds (B, A50 below 50, U20 below 20), no band for paths, buildings or non-ranking metrics, signed and zero references. `test_procedural_style_profile`: `band` lines round-trip; a bound off the rule, a path metric, an unknown metric and a duplicate are refused. `test_procedural_style_learning`: one sample merges to itself; three samples in two orders give byte-identical profiles; counts are summed; sources `sample-1..3` sum to 1000; two identical samples fit the same target, tuning and bands as one; 0 and 9 samples refused; conflicting classes for one ground refused.
2. **Parity: PASS.** One read rectangle takes the I56 path unchanged (merge of one is identity, sources stay `user-local-surface`); the profile only gains `band` lines. M06/M19 keep g6-bands-v1. G5 `--i47` G5_GATE_PASS with all 32 logs identical to the I60 run apart from timings.
3. **GUI probe: PASS** (`--learn --visible`, client 1098-BlackTek): three rectangles added, one removed, Analyze reports "from 2 samples"; a copy index with several samples is refused and saves nothing; the profile has two weighted sources and only learnable bands; the generator recommends a candidate for it and names the learned policy; map and Undo unchanged by learning; Place → one Undo → exact Redo. I56–I60 sections, including the I57 copy and I58 report checks, still pass (`LEARNING_DIALOG_PASS`).
4. **Report only: not measured.** Learned references on M06/M19 rectangles against the frozen bands need those maps opened in the GUI; not run. No claim that learned bands improve ranking.
5. **Regression: PASS.** `g5_run_suite.py --probes`: SUITE_PASS (core tests, commit and surface probes).

Deviations from the scope text: a metric without a learned band is not scored at all (no N/A penalty), as for any metric absent from a band list. Sources are numbered heaviest first so that the profile does not depend on sample order. Not verified: manual GUI review of the sample list, and samples from two different open maps in one session (the probe reads all three from one map).

## I63 — protected content report and protection margin (2026-11-24)

Criteria: [G7_METRICS.md](G7_METRICS.md#i63--protected-content-report-and-a-protection-margin-2026-11-24-before-implementation).

1. **Unit tests: PASS.** `test_procedural_terrain_report`: margin 0 is empty, margin 1 matches a hand-counted 5 x 4 grid at the area edges, margin 8 covers everything but the protected cells, values above 8 are clamped, a size mismatch gives nothing; preserved rows; no warning without contacts; contact and "leaves only 37%" warnings are non-blocking.
2. **Identity: PASS.** Margin 0 leaves every stage unchanged: G5 `--i47` G5_GATE_PASS with all 16 logs identical to the I62 run apart from timings.
3. **GUI probe: PASS** (`--learn --visible`, client 1098-BlackTek): a 2 x 2 house with its exit, a spawn of radius 2, a waypoint, a zone tile and a ground with an action id. Margin 0 reports 32 kept cells (4 house + 3 single tiles + 25 cells of the spawn area), 4 house tiles, 1 exit, 1 spawn, 25 spawn-area cells, 1 waypoint, 1 zone tile, 1 id item, and Place and Undo leave the fixture tiles unchanged. Margin 3 reports the kept-clear cells; after Place every cell within 3 of the fixture is still empty; one Undo restores, Redo reproduces exactly. A preset saves `terrain-protection-margin 3` and loading it restores the setting.
4. **Regression: PASS.** `g5_run_suite.py --probes` SUITE_PASS; `LEARNING_DIALOG_PASS`.

Found while probing: the margin first lived only in the generator's own protection copy, and the preview stage, which recomputes the edge clearance, refused the plan as invalid. `surfaceKeepClear` now includes the margin, so every stage sees the same writable cells. The capture's protection still matches the map exactly, as the preview requires. Spawn areas are protected cell by cell (existing rule), so "Existing tiles kept" counts them; spawn centres and spawn-area cells are separate rows.

Not verified: manual GUI review; margins near content just outside the area (only the area is seen, as recorded in the criteria).

## I64 — tabs per section 6 and a primary biome (2026-11-25)

Criteria: [G7_METRICS.md](G7_METRICS.md#i64--tabs-per-section-6-and-a-primary-biome-2026-11-25-before-implementation).

1. **Unit test: PASS.** `test_procedural_terrain_preset`: five unique biomes, each sets exactly the 12 terrain and vegetation keys, weights 0-1000 with a non-zero sum, and serializes as a valid preset.
2. **GUI probe: PASS** (`--learn --visible`, client 1098-BlackTek). The notebook has Area, Terrain, Biomes, Structures, Decoration, Reference Style, Advanced, Validation in that order. A preset saved with the new layout has exactly the 60 keys of the I63 layout. Snow sets weights 3/2/1, vegetation on and its brushes. Each of the five biomes previews without changing map or Undo and can be placed; the first is placed with one Undo step and undone. Loading a preset resets the biome to "Keep current settings". I56-I63 sections still pass (`LEARNING_DIALOG_PASS`).
3. **Identity: PASS.** G5 `--i47` G5_GATE_PASS, all 16 logs identical to the I63 run apart from timings.
4. **Regression: PASS.** `g5_run_suite.py --probes`: core 26/26, commit and surface probes, SUITE_PASS.

Found while probing: Snow first used `ice` for the shore. Each brush passed the resolver's checks alone, but the border between snow and ice uses items client 1098 lacks, so every Snow series was refused. Snow now uses sand for the shore. The biome skips a brush the resolver would refuse (`checkGroundBrush`, `checkDecorBrush`, now shared with the resolver, which is unchanged in behaviour); it cannot see a pair of brushes whose border is unusable, so a biome can still be refused in another client, with the usual status text.

Deviations: the Structures tab holds paths, buildings and levels with headings and scrolls. Not verified: manual review of the layout and of the scrolling Structures tab; biomes in clients other than 1098; whether the biomes look like their names (no visual review). `test_universal_rotator` fails to link on this branch already at `fac7ad9` (unrelated, not investigated).

## I65 — preview cache (2026-11-25)

Criteria: [G7_METRICS.md](G7_METRICS.md#i65--preview-cache-preview-reuses-an-unchanged-series-2026-11-25-before-implementation). Commits 757ce65 (criteria), e19682e, d0db993.

1. **GUI probe: PASS** (client 1098, `LEARNING_DIALOG_PASS`). `I65_REUSE_PASS 3 ms`: a second Preview of a finished 2-candidate series kept the rows and seeds, enabled Place and left map and Undo unchanged. `I65_MAP_CHANGE_GENERATES_PASS`: Place gave one Undo; after the Undo, Preview generated again (same deterministic rows). `I65_SEED_CHANGE_GENERATES_PASS`: a new seed generated different rows. Earlier probe sections (I56–I64) pass.
2. **Identity: PASS.** G5 `--i47` G5_GATE_PASS, 16/16 logs identical to I64 apart from timings.
3. **Regression: PASS.** `g5_run_suite.py --probes` SUITE_PASS.

Not verified: selection mode (it always generates by design); a copy index rebuilt within the file system's write-time resolution with the same size would be treated as unchanged.

With I65 the item "Background generation, cancellation, progress, preview cache, commit/undo" is complete: GUI-thread steps with Cancel and a progress gauge (I52, I61), Place as one Undo (G1), and series reuse (I65). Caching prepared candidates for switching gallery rows stays out of scope (memory budget).

## I66 — sprite preview when zoomed in (2026-11-25)

Criteria: [G7_METRICS.md](G7_METRICS.md#i66--sprite-preview-when-zoomed-in-2026-11-25-before-implementation). Commits ef22f39 (refactor), 84edfec, d022992.

1. **GUI probe: PASS** (client 1098, `LEARNING_DIALOG_PASS`). At fit level the pane is pixel-identical with sprites on and off; at zoom 32x the pixels differ (sprites drawn). Map and Undo unchanged by painting, Place enabled and gives one Undo (`I66_SPRITES_PASS`, `I66_PLACE_PASS`).
2. **Timing (report only):** one sprite paint at zoom 32x (12 x 4 visible cells, cold cache) took 15 ms on the GUI thread.
3. **Identity: PASS.** G5 `--i47` G5_GATE_PASS, 16/16 logs identical to I65 apart from timings.
4. **Regression: PASS.** `g5_run_suite.py --probes` SUITE_PASS.

Not verified: whether the sprites look right to a person (the probe only checks that they differ from minimap colours); paint time with many visible cells at 16 px per cell; other clients. Known simplifications: no draw offsets, multi-tile overflow, animation or creatures.

## I67 — sliders for the share fields (2026-11-25)

Criteria: [G7_METRICS.md](G7_METRICS.md#i67--sliders-for-the-share-fields-2026-11-25-before-implementation). Commits 12619ae (criteria), 4646e72, bce25fa.

1. **GUI probe: PASS** (client 1098, `LEARNING_DIALOG_PASS`). For all five share fields the slider set the field, the first move released the preview, the slider followed a field set directly, and both had the same enabled state (`I67_SLIDERS_PASS`). A saved preset has 60 keys and no slider key (`I67_PRESET_KEYS_PASS`).
2. **Identity: PASS.** G5 `--i47` G5_GATE_PASS, 16/16 logs identical to I66 apart from timings.
3. **Regression: PASS.** `g5_run_suite.py --probes` SUITE_PASS.

Not verified: the look of the rows with sliders at other DPI settings. Mixed biomes remain open.
