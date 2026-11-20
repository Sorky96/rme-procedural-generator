# PR: terrain generator, G4 stage (style learning), terrain shape, routes and buildings

PR description draft prepared 2026-11-08. **The PR was not opened or pushed**; branch feature/procedural-g4-style-learning is local only. Compared with origin/master (branch point 33b5c88, PR #4): 74 commits, 72 files, +8 809 / −84 lines. source/ and tests/ account for 55 files (+7 440 / −65); the rest is documentation and probe scripts.

## What this is

The Natural surface generator learns a style from private reference maps and generates similar terrain. Changes in this branch:

1. **G4: style profiles** (source/procedural/core/style_*.cpp, reference_split.*, similarity.*, tools in source/procedural/tools/): map fingerprints, family split, ground-key → kind mapping, generator-parameter fitting, per-style profiles with brush and doodad palettes, and profile offers in the generator window.
2. **Terrain shape** (surface.cpp, water_shape.*, scale_stats.*): river meander, large structures (featureSize, more noise octaves), wider beaches and grass rings around rock and shore.
3. **Optional additions** (disabled by default and configured in the generator window): routes (pathLength, seventh brush), buildings with floor (buildingDensity, eighth brush) and walls (GroundPlan::walls, wall brush aligned by WallBrush::doWalls).
3a. **Settlements (PHASE 5 part 1):** clustered buildings, doors (SurfaceGrid::doors, GroundProfile::door, GroundPlan::doors, “Doors” and “Door kind” fields in the generator), routes to doors (PathRouter), settlement metrics (settlement_stats.*) and docs/procedural/smoke/g5_run_suite.py regression set. buildingDensity 0 is byte-identical to before.
4. **Metrics and documentation:** docs/procedural/G4_*.md, G4_METRICS.md (criteria recorded before measurements), G4_GATE_STATUS.md (results with qualifications), TECH_DEBT.md (D17–D21), style_render (generator preview as PNG) and reference-probe extensions.

Reference maps are private. They are not in the repository, local-data/ is git-ignored and documents contain aggregate numbers only, not tile copies.

## Existing behavior changes for reviewer attention

- **Surface output changed for every profile and default setting** (intentionally, described in G4_GATE_STATUS.md): beach-ring rule (noise threshold ≤40%, lake ≥90 cells, shoreWidth up to 8), grass ring around Rock and Shore (iteration 10c), and more height-noise octaves when featureSize >24 (unchanged for ≤24). The pinned Seed 1729 100×100 fingerprint in tests/test_procedural_surface.cpp changed from 6783381837833077004 to 17159159863505677800.
- **Rivers with meander >0** use a new path. meander 0 produces a byte-identical result to before the change.
- **GroundProfile** has up to 8 brushes (6 without routes, 7 with routes, 8 with floor) and a wall field. Profiles with fewer brushes reject route and floor grids (tests).
- **SurfaceKind** adds Path and Floor. GroundPlan adds walls (empty = previous behavior, plan fingerprint unchanged).
- **Generator window:** Style profile, Path length, Path brush, Buildings, Floor brush, Walls and Wall brush fields.
- **Bug fixed before the PR** (fadc036): SurfaceTuning field order caused profile meander to land in pathLength/buildingDensity, so profiles had ignored meander since iteration 9 (described in G4_GATE_STATUS.md). This was a process lapse: the full test set was not run after every change.

## Verified

Run on 2026-11-08 against current code:

- **14 core tests** (test_procedural_core, _surface, _reference_header, _semantic_map, _topology, _features, _semantic_image, _similarity, _style_profile, _style_fit, _decor, _style_profile_set, _water, _scale): all PASS (exit code 0).
- **G1 --commit probe** on real 10.98 client: passed: true, empty stderr.
- **G1 --surface probe:** all assertions PASS, including the new structure E2E test (SURFACE_STRUCTURES_E2E_PASS for 100×100: 150 wall cells; 250×250: Preview, Validate, Commit, Undo, Redo, refusals for profiles with incompatible brush counts and a wall brush that is a ground brush). passed: false only because stderr is non-empty (wxWidgets SetFocus warnings from D1/D2: 9 warnings, one additional per run from the added structure E2E; previously 7).
- **User GUI review** (subjective, no metric): style profiles, beach, large structures, meandering rivers, rock, grass ring, routes and walled buildings, including Validate and Undo/Redo with routes and walls.

## Not verified / known limits

- **The planned G4 gate (improvement over manual presets on unseen maps) is not met**. Description and numbers are in G4_GATE_STATUS.md (iterations 1–3). The only unseen-map evidence is swamp, n = 2.
- Shape metrics (iterations 4–11) use two references (M06, M19), in-sample (D17). Several criteria are intentionally unmet: route branching (skeleton artifact, D18), m19 boundary composition, E64 at side 256 and rock boundary.
- No roofs or building interiors, bridges, undergrounds or multi-floor mountains (D9, D19). Doors, clusters and routes to doors exist (PHASE 5 part 1, iterations 12a–12h in G4_GATE_STATUS.md): one door per building through DoorBrush, routes reach their target without stubs. User visual review was “better”, but m19 route length is outside the recorded band (174 versus ≤117), so criterion 12c as a whole is unmet. Constant sensitivity was not studied.
- Style profiles are old relative to the current generator (D20): style_styles does not fit featureSize, shoreWidth, transitionWidth or meander on the new metrics.
- Performance of new stages (routes, buildings, ring) was not measured separately. Preview worked in the GUI at 500×500.
- Windows x64 and client 10.98 only (private 1098-BlackTek profile in probes).
- Sweep scripts used for G4_GATE_STATUS.md tables are not in the repository (D21).

## Review order

Suggested order: docs/procedural/G4_GATE_STATUS.md (what is proven and how), G4_METRICS.md (criteria recorded before measurements; 9c was recorded after measurement and described), then source/procedural/core/surface.cpp (terrain rules), source/procedural/adapter/surface_resolver.cpp and source/editor/operations/procedural_draw_operations.cpp (wall layer), and source/ui/dialogs/terrain_generator_dialog.cpp (new fields).

The work could be split into several PRs, but changes in surface.cpp, resolver and dialog are interleaved (the same brushes and kinds), and core tests pin the output fingerprint after all changes. Natural smaller cuts are (1) style profiles and tools, (2) terrain shape, (3) routes and (4) buildings and walls.

## Manual GUI test

1. Copy profiles to %APPDATA%\\Remere's Map Editor\\data\\procedural\\styles (the *.profile files from local g4-profiles, not the repository).
2. Map → Generate terrain…, Natural surface, choose a profile (for example m06 or m19), area 500×500.
3. Optional: Path length 80, Buildings 30 and Walls.
4. Preview, Validate, Place on map, Undo, Redo.

## Commands to repeat

```text
cmd /c build_ninja.bat
# Core tests: build-ninja/source/procedural/test_procedural_*.exe (each exits 0)
py -I docs/procedural/smoke/g1_run_resolver_probe.py <OUT> <PROFILE_DATA> <CONFIG> --commit
py -I docs/procedural/smoke/g1_run_resolver_probe.py <OUT> <PROFILE_DATA> <CONFIG> --surface
```
