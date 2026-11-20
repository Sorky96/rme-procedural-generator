# PR: terrain generator, PHASE 5 part 1: settlements, reachability, fords, paths, shores and window layout

PR description draft prepared 2026-11-09. Branch `feature/procedural-g5-settlements` is cut from PR #5 (merged into `master` as `ec8664e`). Compared with `origin/master`: 62 commits, 33 files, +3,211/−174 lines. `source/` and `tests/` account for 23 files (+2,313/−167), `source/procedural/core` for 9 files (+1,128/−142), and the rest is documentation and scripts.

## What it is

Continuation of the surface generator (Natural surface). All new options are **disabled by default in the core** (byte-identical output remains the default). Only “Smooth beaches” is enabled by default in the generator window, as described below.

1. **Osady (iteracje 12a–12h):** budynki w skupiskach zamiast losowych, jedne drzwi na budynek przez `DoorBrush` (`SurfaceGrid::doors`, `GroundProfile::door`, `GroundPlan::doors`), trasy do drzwi (`PathRouter`), metryki osad (`settlement_stats.*`), pola „Doors” i „Door kind” w oknie.
2. **Reachability validator (13):** `reachabilityOf` measures connected components, pockets and closed buildings (`reachability.*`), with a `reach` row in `style_render`.
3. **Fords (14, 14b–14d):** `SurfaceTuning::fordLength` connects isolated land with a sandy crossing over a river or lake. It uses end inlets, a tilted axis and variable width for longer crossings. The window exposes “Ford length”.
4. **Paths through rock and rivers (15, 16, 17):** `rockPasses` allows paths through rock bands to targets on the other side. When `fordLength` > 0, paths cross rivers using sandy fords. The window exposes “Rock passes”.
5. **Shores (18, 19, 20):** `shoreSmoothing` applies a beach majority filter and removes narrow sand tongues between water, with `shoreRadius`. The window exposes “Smooth beaches”.
6. **Window (21):** fields are organized in Area, Terrain, Paths, Buildings and Vegetation tabs because one column of 35 fields did not fit at 1080p.
7. **Tools and documentation:** `g5_run_suite.py` runs all core tests and both G1 probes, `g5_shape_table.py` writes metric tables with byte-identical repeated runs, and `g5_reference_settlements.py` measures references. `G4_METRICS.md` records criteria before measurement, `G4_GATE_STATUS.md` records results and caveats, and `TECH_DEBT.md` records D19–D22.

Reference maps are private. They are not in the repository, `local-data/` is in `.gitignore`, and the documents contain aggregate numbers only.

## Behavioral changes in existing code (for reviewer attention)

- **Generator window:** tabbed layout with new “Doors”, “Door kind”, “Ford length”, “Rock passes” and “Smooth beaches” fields. **“Smooth beaches” is checked by default**, so the default preview differs from before this PR (smoothed shores and narrow sand tongues changed to water). In the core, `shoreSmoothing` = 0 preserves the old result.
- **12h bug fixed:** `reachEnds` path endpoints were joined in reverse (the path ran from target to start). This was invisible on short paths to doors, but produced straight diagonal lines on long paths. **Results change when `buildingDensity` > 0** because door paths now have correct endpoints. `buildingDensity` 0 is unchanged.
- **New `SurfaceTuning` fields** (`rockPasses`, `fordLength`, `shoreSmoothing`, `shoreRadius`) are appended to the structure and are not part of the style profile. The plan configuration fingerprint includes them only when set. The `Seed 1729 100×100` fingerprint (`17159159863505677800`) is unchanged.
- **`GroundProfile` and `GroundPlan`:** `door` in the profile and `doors` in the plan, included in the fingerprint only when non-empty. Preview fingerprint validation accepts a `Door` class item without a house id because RME represents doors as `Door` objects.
- **Working-memory limit** (`validateSurface`) grows by 28 bytes per cell only when `fordLength` > 0.

## Verified

Run on the current code on 2026-11-09:

- **16 core tests** including the new `test_procedural_settlement` and `test_procedural_reachability` with hand-counted layouts and an independent union-find comparison (160/160): all PASS.
- **G1 `--commit` probe:** PASS. **G1 `--surface` probe:** PASS (`SURFACE_DOORS_E2E_PASS` with 6 and 38 doors, `SURFACE_FORDS_E2E_PASS` with Preview, Validate, Commit, Undo and Redo; after Commit there were zero blocking ford cells and zero blocking neighbors, and pockets were connected on the committed map; blocking by kind was Rock 19/4983 and Shore 0/7705). The runner's `passed: false` is caused only by 12 known `SetFocus` warnings.
- **Repeatability:** two metric-table runs were byte-identical.
- **GUI user review** (subjective): doors and paths (12f–12h), fords (“looks good” after 14d), paths through rock and rivers (“looks OK”), shores (“better” after 19), and window layout (“looks OK” after 21).

## Not verified / known limitations

- **Several recorded criteria are intentionally unmet** (full numbers are in `G4_GATE_STATUS.md`): 12c (settlement features), 14 (closed buildings for reach {8, 16, 32}: 5/32 seeds at 32, while reach ≥ 48 meets the criterion only after the fact), 15/16/17 (the path-through-rock and “tips” metric proved unreliable), 18 (filter rule changed after measurement), and 20 (boundary steps are tile-grid steps; radius 5 removes 10% of corners for +2 s and is disabled in the window).
- **Stepped edges and rectangular grass cuts into sand** remain unresolved.
- **In-game walkability was not checked**, only blocking flags from client 10.98 data. There are no bridges with a separate item. Rock is walkable, so “passes” are not required for reachability. Blocking rock with passes was not implemented.
- **No POIs, fixed start, floors, z transitions, region graph, seed rejection or repair** (D22).
- **No reference measurement** exists for paths through rock, fords or shore smoothing because these features are absent from the references. The criteria are absolute.
- Only Windows x64 and client 10.98 were tested with the private probe profile. The user visually checked the window layout, without a separate Preview / Validate / Place / Undo report after the layout change. `test_universal_rotator` does not link during the full `ninja` build and was not touched or tested.
- Performance of the new stages (fords and river paths) was not measured separately. `shoreRadius` 5 adds about 2 s to 512×512 generation in one measurement.

## Review order

Suggested order: `docs/procedural/G4_GATE_STATUS.md` (iterations 12–21), `G4_METRICS.md` (criteria recorded before measurement), then the code: `source/procedural/core/surface.cpp` (path router, fords and smoothing), `reachability.*`, `settlement_stats.*`, `source/editor/operations/procedural_draw_operations.cpp` (doors), and `source/ui/dialogs/terrain_generator_dialog.cpp` (tabs and new fields). Natural PR boundaries would be (1) settlements and doors, (2) validator and fords, (3) paths through rock and rivers, (4) shores, and (5) window layout. Changes in `surface.cpp` are interleaved, so splitting them is not recommended.

## Manual GUI test

1. Map → Generate terrain…, Natural surface mode, choose a profile such as m19, and set a 500×500 area.
2. **Paths** tab: “Path length” 80, “Ford length” 48, check “Rock passes”. **Buildings** tab: “Buildings” 30, Walls and Doors. **Terrain** tab: check “Smooth beaches”.
3. Choose Preview, Validate, Place on map, Undo and Redo.

## Commands required to reproduce

```bash
cmd /c build_ninja.bat
py -I docs/procedural/smoke/g5_run_suite.py --probes <PROFILE_DATA> <CONFIG>
py -I docs/procedural/smoke/g5_shape_table.py <PROFILE_DIR> <OUT_FILE> --extra fordLength=48
```
