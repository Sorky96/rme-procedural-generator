# G7: criteria recorded before implementation and measurement

Document G7 (PHASE 7: full GUI and workflow). Same rules as [G6_METRICS.md](G6_METRICS.md): criteria for each iteration are committed before implementation. A change after measurement must be explicit and dated.

**G7 gate:** the whole workflow runs from the GUI, without a terminal and without manual script invocation.

## Audit 2026-11-23 (before I57)

Already in the GUI: generator dialog with Area/Terrain/Validation tabs, seed, area and Z, terrain weights, brushes, paths, buildings, levels, reachability repair, minimap preview, candidate gallery N=1–16 (I52), feedback/versions/rollback (I54), learning a profile from the open map (I56), progress, cancellation, stale-result checks, one Undo per Place.

Terminal-only today: the I53 copy index (`g6_build_copy_index.py` + `copy_index.exe`, M06/M19 only), multi-map profiles and ranking bands (G4 CLI tools), G3 reference reports. Missing in the GUI: statistics and warnings, presets, seed randomization, side-by-side comparison, zoom/pan, background-thread preview, sprite preview.

Iteration order (user decision 2026-11-23): I57 copy index from the GUI, I58 statistics and warnings, I59 presets and seed randomization, I60 comparison and preview navigation, I61 background-thread preview. Sprite preview and tab layout per section 6 come last or become debt.

## I57 — copy-protection index built in the GUI (2026-11-23, before implementation)

**Goal.** A user who learns a style from their own map can protect that map against copying without a terminal. The generator chooses an index from a list instead of a typed path.

**Scope.**
1. **Format `i53-index/3`:** 1–16 references with local aliases (1–64 ASCII letters, digits, `-`, `_`), client and OTB/DAT hashes, and for each reference only the fingerprint and source-sample hashes. No map names, paths or coordinates. `generic` is the intersection of all references when there are at least two, and empty for one. `i53-index/2` (M06, M19) still loads and gives identical assessments. `i53-index/1` is still refused with the rebuild hint.
2. **Assessment per reference:** the hardFail rule is unchanged: at least 16 shared unique windows **and** coverage of at least 100‰ for any one reference. Thresholds, projection (`g4-natural-v1`) and D10 canonicalization are unchanged.
3. **Learning dialog (I56):** after **Read rectangle**, a new option **Add this sample to a copy index** with an alias and a target index (new or existing in the local profile folder). The raster comes from the same sample and the same class mapping as the analysis (`Grass`…`Water`; other classes absent). Write after explicit confirmation, atomically, without overwriting a concurrently changed file. An index from a different client is refused. A duplicate alias is refused. Above 16 references, it is refused.
4. **Generator dialog:** a choice list "Copy check" with "None" plus compatible indices from the local profile folder. The free path field stays as "Other file…". Incompatible or unreadable indices are listed with the reason and cannot be selected. Changing the choice invalidates the series as today.
5. Learning, index writing and choice change neither the map nor the Undo queue. Building the fingerprint runs off the GUI thread with cancellation, like I56 fitting.

**Criteria.**
1. **Parity:** an index built in core from the M06/M19 rasters of `g6_build_copy_index.py`, saved as `i53-index/3`, gives assessments byte-identical to `i53-index/2` for m06 and m19 seeds 3001–3032 N=4 (`candidate_rank` TSV identical except the similarity header line).
2. **Unit tests:** 1 and 16 references, 17 refused, duplicate alias, bad alias, wrong client, corrupted file, truncated file, `/1` refused, `/2` read, generic for 1 vs 2+ references, cancellation, write conflict, and a copy of a reference sample (rotated 90° and mirrored) giving hardFail while a different sample is Clear.
3. **GUI probe with a real client:** synthetic source map → Learn → add to a new index → generator lists the index → a generated area that copies the sample is blocked at Place; a different area passes Place → one Undo → Redo. Map and Undo unchanged by learning and index writing. Stale map invalidates the sample.
4. **Regression:** canonical build, `g5_run_suite.py --probes` SUITE_PASS, G5 `--i47` 32/32 for both profiles, fingerprint 1729 = `17159159863505677800`.

**Out of scope.** Multi-map profiles and ranking bands from the GUI (later iteration), item and underground similarity, G3 reports in the GUI. Manual visual and keyboard review is reported separately.

## I58 — statistics and warnings (2026-11-24, before implementation)

**Goal.** After Preview, the user sees what the Selected candidate contains and why Place is blocked or risky, in two lists instead of one long status line.

**Scope.**
1. **Statistics list** (`terrain-statistics`, two columns) for the Selected candidate: area size and cells; Surface terrain shares in per mille of the area (grass, dirt, rock, shore, water, path, building floor, bridge; counted from the final Surface grid, which the preparation does not keep today); places reached per kind; unwalkable floor transitions; walkable cells out of reach on the ground floor and above, with the allowance or "not limited" for a profile without learned shares; repairs, bridges, removed caps and places; similarity per copy-index reference; preparation memory estimate against its budget; preparation time of the candidate.
2. **Warnings list** (`terrain-warnings`, severity and text). **Blocking**: one entry per hardFail cause (copy similarity per reference over the thresholds, places out of reach per kind, unwalkable transitions, cells out of reach over the allowance, removed caps/places/storeys/dungeons, repair gave up under the rule without learned shares). **Warning**: similarity state Near, cells out of reach above half the allowance, closed pockets, memory estimate above 75% of the budget. No warning: "No warnings".
3. One core function computes the rows and warnings from plain inputs (reach report, repair report, level plan, copy assessment, tolerance, memory, terrain counts). The hardFail decision itself is unchanged.
4. The status line keeps one sentence (selected candidate, validated or blocked, see the lists).
5. Unknown item IDs are not listed: preparation already refuses a plan with an unknown item (`InvalidItem`), so the count would always be 0.

**Criteria.**
1. **Unit tests:** for synthetic reports covering each hardFail cause under both reach rules, the Blocking list is non-empty exactly when `candidateHardFail` is true, and names each cause once. Terrain shares add up to the area (±rounding of 1‰ per row). "Not limited" for a profile without learned shares. Near and pocket warnings never block.
2. **Real-client GUI probe:** after Preview both lists are filled; Blocking warnings exist exactly when Place is refused; a candidate blocked by the copy index shows a Blocking similarity warning naming the reference; selecting another candidate refreshes both lists; the lists change neither the map nor the Undo queue. I57 and I56 probe sections still pass.
3. **Regression:** canonical build, `g5_run_suite.py --probes` SUITE_PASS, G5 `--i47` 32/32 for both profiles, plan fingerprints unchanged (fingerprint 1729 in the core suite).

**Out of scope.** Charts, export of statistics, statistics for the gallery thumbnails, runtime/memory measured by the OS (the memory row is the preparation's own estimate). Manual visual and keyboard review is reported separately.

## I59 — presets and seed randomization (2026-11-24, before implementation)

**Goal.** A user can keep a set of generator settings under a name, load it later or on another map, and draw a new seed with one button.

**Scope.**
1. **Format `terrain-preset/1`** (text, UTF-8): a schema line, then `name<TAB>value` lines sorted by name. Names are the generator controls' names (`terrain-…`); values have no tab, newline or control characters, at most 1024 bytes; at most 128 entries. Parsing is strict: unknown schema, duplicate or unsorted names, bad characters or an oversize file are refused.
2. **What a preset holds:** every input of the generator's sections except the area position (`terrain-scope`, `-x`, `-y`, `-z`) and the copy check (a local file). Size, seed, retries, candidate count, style, profile, brushes and all generation options are included.
3. **Dialog:** a **Preset** choice (None plus the presets in the local profile folder's `presets` subfolder), **Save preset…** (name; the same name rules as learned profiles; overwriting asks first; temp file and rename) and **Delete preset** (asks first). Loading applies the values in tab order with the same change events as the user's edits. If any value cannot be applied (unknown control, choice entry or brush not available, number out of range), the dialog returns to its previous settings and the status names the first value refused. Loading or saving invalidates the series like any edit and never changes the map or Undo.
4. **Seed randomization:** a **Random seed** button next to the seed field fills a new seed from `std::random_device` (64-bit, decimal). It never generates by itself.

**Criteria.**
1. **Unit tests (core format):** round trip; sorted output independent of input order; refusals for wrong schema, duplicate name, unsorted names, tab/newline/control characters in a value, a value over 1024 bytes, more than 128 entries, a truncated line; a file with CRLF line ends reads like LF.
2. **Real-client GUI probe:** change several settings on different tabs → save preset → change them again → load → every control equals the saved value; the area position stays as it was. A preset with a missing brush or an out-of-range number is refused and every control keeps its previous value. Overwrite and delete ask first (probe answers no, then yes). Random seed changes the seed field to a valid 64-bit number and starts nothing. Map and Undo unchanged throughout.
3. **Regression:** canonical build, `g5_run_suite.py --probes` SUITE_PASS, G5 `--i47` 32/32, `LEARNING_DIALOG_PASS`.

**Out of scope.** Sharing presets between clients beyond copying the file, built-in presets shipped with the editor, preset versions or history.

## I60 — comparison and preview navigation (2026-11-24, before implementation)

**Goal.** The user can look closely at a candidate (zoom and pan the minimap preview) and see two candidates side by side with the same view.

**Scope.**
1. **Viewport (core, pure):** zoom levels 1× (whole area fits), 2×, 4×, 8×, 16×, 32× relative to the fit, capped where one cell would exceed 64 screen pixels. Zooming keeps the cell under the cursor (or the pane centre for keys) at the same screen position. Pan moves by screen pixels. The centre is clamped so the view never leaves the area. The visible cell rectangle and the cell under a point are computed from the same state.
2. **Preview panel:** mouse wheel zooms at the cursor, left drag pans, double click fits. The panel takes keyboard focus: `+`/`-` zoom at the centre, arrows pan by a tenth of the pane, `0` fits. A label `terrain-preview-view` gives zoom, visible cells and the cell under the cursor in map coordinates. Changing the preview floor, candidate or series keeps the view if the area size is the same and fits otherwise.
3. **Comparison:** a **Compare with** choice (`terrain-compare`: None, then the other candidates of the series). With a candidate chosen and the Surface floor shown, the panel shows the Selected candidate on the left and the compared one on the right, each labelled, with one shared viewport. Each candidate's Surface minimap image is kept when the gallery is built; above 64 MiB for the whole series the kept images are reduced to fit, and the label says so. The comparison never changes the selection, the statistics or Place.
4. Navigation and comparison never change the map, Undo or the series.

**Criteria.**
1. **Unit tests:** fit scale; zoom keeps the anchor cell within one screen pixel for every level and anchors at corners, centre and edges; zoom levels capped at 64 px per cell; zoom out to 1× returns to the fit; pan clamps at all four edges; visible rectangle and cell under a point agree; areas of 1×1, 1×2000 and 2000×2000 cells; panes of 1×1 pixel.
2. **Real-client GUI probe:** after a two-candidate preview: wheel at a point zooms and the view label reports the zoomed rectangle containing that cell; keys `+`, arrows and `0` change the label as expected and `0` returns to the full area; choosing **Compare with** the other candidate keeps the Selected candidate, statistics and Place state and the label names both candidates; switching floor or candidate keeps a same-size view; map and Undo unchanged.
3. **Regression:** canonical build, `g5_run_suite.py --probes` SUITE_PASS, G5 `--i47` 32/32, `LEARNING_DIALOG_PASS`.

**Out of scope.** Sprite (asset) rendering of the preview, comparison of the lower floors, synchronized statistics of the compared candidate, more than two candidates at once. Manual visual and keyboard review is reported separately; the probe cannot check what is drawn.

## I61 — preview off the GUI thread (2026-11-24, before implementation)

**Goal.** The generator dialog stays responsive while a preview is prepared: no single step of the preparation holds the GUI thread long enough to be noticed.

**Baseline (measured 2026-11-24 before the change, temporary instrumentation, `g1_run_resolver_probe.py --surface --visible`, mostly 512x512 dialog runs, 4384 GUI-thread steps).** Longest step 117 ms; 376 steps of 50 ms or more, 35 of 33-50 ms. The long steps are: one step of the DrawOperations preview builder (up to 118 ms; the passes for structures, mountain outline, vegetation, overlays, walls, wall alignment and doors run without a yield), one iteration of the reachability check and repair loop (up to 120 ms: tile scan, `levelReachabilityOf`, `repairLevelReach`), and the cave and plateau stages (60-100 ms).

**Scope.**
1. Pure computations of these stages run on the existing owned worker (`CoreTask`), with the GUI coroutine suspended until they finish: reachability and repair in the loop, and any other pure core call found in a step over the limit.
2. Passes that must touch the map, brushes or the preview buffer (GUI thread only) yield on the same 8 ms deadline as the existing ones.
3. The dialog logs one line per prepared candidate: number of GUI-thread steps and the longest one in ms. The statistics list shows the longest step.
4. Results do not change: same plan, preview and reachability for the same inputs.

**Criteria.**
1. **Responsiveness:** in the same Surface probe run, no GUI-thread step of 50 ms or more, and at most 1% of steps of 33 ms or more. Reported with the same measurement as the baseline (the new log line).
2. **Identical results:** G5 `--i47` 32/32 with fingerprints, passes, categories and edges identical to the run before the change (`scratchpad/i60-g5`); core fingerprint 1729 unchanged; the I57 copy probe and the I58 report probe pass with the same seeds.
3. **Cancellation:** Cancel during the worker stages still stops the series without a late result (existing Surface probe cancellation checks pass).
4. **Regression:** canonical build, `g5_run_suite.py --probes` SUITE_PASS, `LEARNING_DIALOG_PASS`.

**Out of scope.** Moving map, brush or preview-buffer access off the GUI thread (not thread-safe in RME), Place/commit time, the learning dialog's steps. Areas larger than 512x512 are measured if the probe covers them but not required.

## I62 — profiles from several samples and learned ranking bands (2026-11-24, before implementation)

**Goal.** The last terminal-only parts of the workflow move to the GUI: a profile learned from several rectangles (of one or several maps), and ranking bands for a learned profile so the generator recommends a candidate as it does for M06/M19. User decision 2026-11-24: both parts; paths and buildings stay without bands.

**Scope.**
1. **Samples:** the learning dialog keeps a list of 1–8 read samples (**Add sample** after Read rectangle, **Remove sample**). Samples may come from different maps opened in turn; each must have the same client (OTB/DAT hashes) as the first. Map identity changes no longer discard samples already added, only the one not yet added. Analyze fits one profile from the merged samples.
2. **Merge rule (core, `mergeLearningSamples`)**, weight = present tiles of a sample:
   - counts (stats, vegetation contexts, present, recognized, sea tiles) are summed;
   - rates (permille, ppm, edges, building density/ruin/floor/compound shares, shore distance percentiles, unreached shares, clump, shore trees) are weight-rounded means;
   - named entries (grass/tree/bush mixes, overlays, interiors, wall stacks by sides and items, structures) are combined by name with weighted rates and the existing count limits; ties break by name, so the result does not depend on sample order;
   - single names (palette per kind, decor palette, building floor and wall, carpet, shore tree) come from the sample with the largest weight among those that have one, ties by name;
   - width and height are those of the largest sample.
   One sample merges to itself.
3. **Profile:** `sources` lists `sample-1`…`sample-N` with per-mille weights summing to 1000. Optional `band` lines (metric name, reference, lower and upper bound in hundredths) store the learned bands. Profiles without them read and rank as before; M06/M19 keep the frozen g6-bands-v1 bands.
4. **Bands:** each sample is measured with `candidateMetricsOf` on its natural classes (Grass, Dirt, Rock, Shore; Water as lake; other classes preserved). The reference is the weight-rounded mean over the samples where the metric exists. Rule of 396e057: B = [0.67r; 1.5r]; trans.R*/D* use A50 = [max(0, r-50); r+50] when r < 50; patch.RS uses U20 = [0; r+20] when r < 20. Signed metrics take the band ordered low to high. Path and building metrics (paths.L, paths.W, build.per64k, build.long, build.short, outline.median) get no band: N/A as today.
5. **Generator:** a learned profile with bands ranks with order A and shows Recommended; the policy text names the learned bands (`learned-bands/A/I54-v1`). Feedback records the same policy.

**Criteria.**
1. **Unit tests:** merge of one sample is identity; any permutation of 2–8 samples gives a byte-identical profile; two identical samples give the same target and tuning as one; 9 samples refused; a different client refused; weights sum to 1000; band rule cases B, A50, U20, signed and N/A; `band` lines round-trip and bad ones are refused; a profile without bands ranks as before.
2. **Parity:** the I56 single-sample profile serialized by the new code equals the old one apart from the added `band` lines. G5 `--i47` 32/32 and fingerprint 1729 unchanged (M06/M19 untouched).
3. **GUI probe with a real client:** learn two rectangles of a synthetic map → profile with two sources → generator shows Recommended for it; one sample only → also bands; removing a sample before Analyze changes the result; map and Undo unchanged by learning; Place → one Undo → Redo.
4. **Report only, no gate:** learned references on M06/M19 rectangles against the frozen bands (how many of the 20 metrics fall in the frozen band). No claim that learned bands improve ranking quality (I51 and I55 failed).
5. **Regression:** canonical build, `g5_run_suite.py --probes` SUITE_PASS, `LEARNING_DIALOG_PASS`, I57 copy probe and I58 report probe pass.

**Out of scope.** Bands for paths and buildings, holdout validation of learned bands, learning underground or similarity references from several samples at once, editing bands in the GUI.

## I63 — protected content report and a protection margin (2026-11-24, before implementation)

**Goal.** The generator already changes empty tiles only, so houses, spawns, zones, waypoints, towns and items with unique or action ids are never touched. The user now sees what was kept and can keep generated terrain at a distance from it. User decision 2026-11-24: report plus margin.

**Scope.**
1. **Margin:** setting `terrain-protection-margin` 0–8 (Area tab, default 0). Surface cells within this Chebyshev distance of an existing (protected) tile of the generated floor are left unchanged too. Only tiles inside the area are seen (the existing halo of two cells is not extended). 0 gives exactly the current result. The value is part of `GenerationConfig` and of presets.
2. **Statistics** for the Selected candidate: existing tiles kept on the generated floor; of them house tiles, house exits, spawns, waypoints, towns, tiles with zones, items with a unique or action id; cells kept clear by the margin.
3. **Warnings (non-blocking):** "Generated terrain touches N house or spawn tile(s)" when a written cell is 4-adjacent to a house or spawn tile; "The margin leaves only N% of the area to generate" when the margin removes more than half of the writable cells. Blocking warnings are unchanged (they stay equal to hardFail).
4. Map and Undo are unchanged by Preview; Place still writes only cells the preview writes.

**Criteria.**
1. **Unit tests:** the margin dilation for 0, 1 and 8 on a hand-counted grid, at area edges and with a mask; report rows and both warnings for hand-made inputs; no warning at zero contacts.
2. **Identity:** margin 0 leaves G5 `--i47` 32/32 logs identical to the I62 run and core fingerprint 1729 unchanged.
3. **GUI probe with a real client:** a target map with a house tile, a spawn, a waypoint, a zone tile and an item with an action id inside the area. Preview with margin 0 reports each count and keeps those tiles byte-identical after Place; margin 3 leaves every cell within 3 of them unchanged after Place and reports the kept-clear count; one Undo restores the map, Redo reproduces it. A preset saves and restores the margin.
4. **Regression:** canonical build, `g5_run_suite.py --probes` SUITE_PASS, `LEARNING_DIALOG_PASS`.

**Out of scope.** Margins on cave or upper floors (they keep their iteration 49 clearance), user-drawn protection zones, protecting content outside the area.

## I64 — tabs per section 6 and a primary biome (2026-11-25, before implementation)

**Goal.** The generator dialog uses the tabs listed in section 6 of the plan. User decision 2026-11-25: rearrange the tabs and add a simple Biomes tab.

**Scope.**
1. **Tabs, in this order:** Area (area, X/Y/Z, size, seed), Terrain (weights, terrain style, ground brushes, smooth beaches), Biomes, Structures (paths, buildings, camps, caves, dungeons, mountains; scrolls), Decoration (vegetation and its brushes), Reference Style (style profile, learning, copy check), Advanced (candidate count, copy index file), Validation (reachability repair, bridge brush, seed retries, protection margin). Each existing input keeps its name and behaviour and appears exactly once.
2. **Primary biome:** `terrain-biome` choice with "Keep current settings" and five biomes (temperate, snow, desert, jungle, swamp). Choosing one sets the terrain weights, the five ground brushes, vegetation on and the three vegetation brushes. A brush the loaded client does not offer is left unchanged and named in the status. The fields stay editable. The choice is an action, not a stored setting: presets do not save it, and loading a preset sets it back to "Keep current settings". The table of biomes lives in core and is unit-tested.
3. Generation, presets and their keys are otherwise unchanged.

**Criteria.**
1. **Unit test:** every biome sets only known preset keys, weights in range and at least one non-zero weight; names are unique.
2. **GUI probe with a real client:** the notebook has exactly the 8 titles in order; a saved preset has the same key set as one saved before I64 (the 60 keys the I63 source gives, listed in the probe); choosing the snow biome sets the weights and the brushes the client offers; a preset load resets the biome choice; Preview with a biome leaves map and Undo unchanged, Place gives one Undo.
3. **Identity:** generation code is untouched, so G5 `--i47` logs are identical to the I63 run apart from timings.
4. **Regression:** canonical build, `g5_run_suite.py --probes` SUITE_PASS, `LEARNING_DIALOG_PASS`.

**Out of scope.** Mixed biomes (several biomes in one area); biome-aware generation; learned biomes from profiles. They become debt at the G7 close.

## I65 — preview cache: Preview reuses an unchanged series (2026-11-25, before implementation)

**Goal.** The remaining part of the item "Background generation, cancellation, progress, preview cache, commit/undo". Generation already runs in GUI-thread steps with Cancel, a candidate progress gauge and Place as one Undo (I52, I61). Pressing Preview again with nothing changed currently discards a finished gallery and generates the same candidates again. Candidates are deterministic for a request, so the gallery can be kept.

**Scope.**
1. Preview keeps the finished series, without generating, when all of these equal the series: the request (`GenerationConfig` and `GroundProfile`), the candidate count, the ranking bands, the copy index path and the file's size and write time, the map change generation and the client asset revision. The status says the series was reused.
2. Anything else (a changed field or seed, a map edit, Place, Undo, a client reload, an unfinished or cancelled series) generates as before. Selection mode always generates (the selection is not compared).
3. No candidate results are kept beyond the gallery that already exists, so the memory budget is unchanged.

**Criteria.**
1. **GUI probe with a real client:** Preview, then Preview again: the gallery rows, seeds and selected candidate are unchanged, the status reports reuse, and it finishes without generating a candidate; Place is enabled and gives one Undo. After that Undo (map generation changed) Preview generates. A changed seed generates. Map and Undo are unchanged by a reused Preview.
2. **Identity:** generation code is untouched, so G5 `--i47` logs are identical to the I64 run apart from timings.
3. **Regression:** canonical build, `g5_run_suite.py --probes` SUITE_PASS (existing dialog probes that press Preview twice keep passing), `LEARNING_DIALOG_PASS`.

**Out of scope.** Caching prepared candidates for switching between gallery rows (it would exceed the single-candidate working budget); caching across dialog sessions or on disk.

## I66 — sprite preview when zoomed in (2026-11-25, before implementation)

**Goal.** Part of the item "Data preview and map render, variants, navigation, quality comparison". Variants (I52), navigation (I60) and comparison (I60) exist; the preview shows minimap colours only. Zoomed in, it now shows the client's sprites, so the user sees the actual grounds, borders and vegetation before Place.

**Scope.**
1. `terrain-preview-sprites` check box ("Sprites when zoomed in", on by default, not a preset key). When on and the Selected candidate's preview is shown at 16 or more screen pixels per cell, every visible cell draws its ground and items, bottom to top, from the loaded client's sprites: first frame, the pattern of the cell's position, the part of the sprite on its own tile (no draw offset, no overflow onto neighbours, no creatures, no lights). Below 16 pixels per cell, while a candidate loads, and on the compared pane the minimap colours stay.
2. The cell shows the preview buffer where it changes and the captured original elsewhere, like the minimap image; the floor choice applies.
3. Scaled sprite images are cached per client id and pattern for the current cell size, dropped when the size changes, the preview is released or the client assets change. Painting never changes the map, Undo or the candidate.

**Criteria.**
1. **GUI probe with a real client:** after Preview, zooming to the highest level gives a painted pane whose pixels differ from the same view with sprites off (captured from the panel); at fit level the two captures are identical. Map and Undo are unchanged; Place still gives one Undo.
2. **Timing (report only):** the longest sprite paint of the probe at the highest level, from a log line.
3. **Identity:** generation code is untouched, so G5 `--i47` logs are identical to the I65 run apart from timings.
4. **Regression:** canonical build, `g5_run_suite.py --probes` SUITE_PASS, `LEARNING_DIALOG_PASS`.

**Out of scope.** Exact map-drawer rendering (offsets, multi-tile overflow, animation, light, creatures); sprites on the compared pane; an OpenGL preview canvas.

## I67 — sliders for the share fields (2026-11-25, before implementation)

**Goal.** The open part of the item "`Generate Area` dialog with described tabs and parameters": sliders in place of number fields. The number fields stay (exact values, keyboard, presets and probes use them); each 0–100 share field gets a slider beside it.

**Scope.**
1. A slider (0–100) next to each of `terrain-building-density`, `terrain-ruin-percent`, `terrain-upper-floor-percent`, `terrain-dungeon-percent` and `terrain-plateau-percent`, named `<field>-slider`. Moving it sets the field and acts like editing the field (the preview is released, dependent controls update). The slider follows the field whatever changed it (typing, a preset, a profile) and is enabled exactly when the field is.
2. Sliders are not preset keys: presets keep the 60 keys of I63/I64.
3. The terrain weights (0–1000, ratios) and counts keep number fields only.

**Criteria.**
1. **GUI probe with a real client:** each of the five sliders exists; moving it sets its field to the same value and releases an existing preview; setting the field (as a preset load does) moves the slider; slider and field have the same enabled state; a saved preset has the I64 key set.
2. **Identity:** generation code is untouched, so G5 `--i47` logs are identical to the I66 run apart from timings.
3. **Regression:** canonical build, `g5_run_suite.py --probes` SUITE_PASS, `LEARNING_DIALOG_PASS`.

**Out of scope.** Mixed biomes (several biomes in one area): they need biome-aware generation and become debt at the G7 close.

## I68 — a secondary biome in regions of the area (2026-11-25, before implementation)

**Goal.** The last open part of the item "`Generate Area` dialog": mixed biomes. User decision 2026-11-25: do mixed biomes in G7. The core already places a second ground brush with its own vegetation in noise patches over the Grass cells (`grassMix`, `grassMixDecor`, `assignGrassPatches`, learned in G4). A secondary biome uses that path, so generation code is not changed.

**Scope.**
1. Biomes tab: `terrain-secondary-biome` ("None" and the five biomes) and `terrain-secondary-share` (0–100 %, default 30, with a slider as in I67). Both are settings and preset keys (presets grow from 60 to 62 keys; older presets load with None).
2. With a secondary biome, the request gets one more grass mix entry: the biome's grass brush on the given share of the Grass cells, and with vegetation on its tree, bush and stone brushes on those cells (keep 1000). Regions use patch step 48 unless the chosen style profile sets its own step. Learned mixes of the profile come first; the share is refused if it and the learned shares exceed 100 % or no mix slot is free.
3. A secondary brush the client cannot use is refused with its name in the status (no silent fallback). Choosing the primary biome as secondary is allowed (it only changes nothing visible).

**Criteria.**
1. **GUI probe with a real client:** for every ordered pair of different biomes (20), primary applied and secondary at 40 %: Preview either places (Place enabled) or is refused with a named reason, and the probe lists which pairs place. At least the pairs with Temperate as primary place. For one placed pair: the preview contains the secondary grass brush's ground items on 30–50 % of the grass-kind cells, map and Undo unchanged by Preview, Place gives one Undo. Share 0 gives the same candidate fingerprint as None.
2. **Presets:** a saved preset has the 60 I64 keys plus the two new ones; a preset without them loads with None and 30.
3. **Identity:** with None, G5 `--i47` logs are identical to the I67 run apart from timings.
4. **Regression:** canonical build, `g5_run_suite.py --probes` SUITE_PASS, `LEARNING_DIALOG_PASS`.
5. **Review:** an independent review of the I68 diff (Codex, medium effort); findings fixed or answered.

**Out of scope.** Biome regions that also change dirt, water, rock and shore; more than two biomes; biome-aware terrain weights; learning biomes from references.
