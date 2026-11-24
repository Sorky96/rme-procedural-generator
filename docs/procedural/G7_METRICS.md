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
