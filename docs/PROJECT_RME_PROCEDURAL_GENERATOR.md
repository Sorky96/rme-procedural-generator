# RME Procedural World Generator - Master Plan

> Project starting document for Codex / Claude Code. Status: PHASE 0 in progress; G0 not passed. Plan version: 1.1 (2026-10-26). Current evidence: `docs/PROGRESS.md` and `docs/procedural/TEST_PLAN.md`.
>
> **Main goal:** a complete map generator integrated with Remere's Map Editor that can analyze supplied reference maps and increasingly reproduce **their style, spatial logic, and playability**, without copying complete regions 1:1.

## 0. Rules for the AI agent (READ FIRST)

1. Work in the user's repository. **First audit the current code and versions**, then present concrete changes. Do not assume that API and directory names in this plan exist.
2. Do not declare functionality complete until it is implemented, tested, and demonstrated.
3. Develop functionality incrementally in small commits. Every stage must pass regression tests and have an acceptance criterion.
4. Reuse existing RME code for maps, tiles, brushes, autoborder, undo/redo, OTBM, assets, Lua, and dialogs. Avoid parallel implementations of the same mechanisms.
5. Do not modify original training maps. Work on copies and keep backups.
6. **Separation:** reference analysis -> world model/grammar -> generator -> validator -> conversion to RME elements -> map commit.
7. Priorities: data correctness > map safety > playability > style fidelity > performance > number of extras.
8. The project must support multiple client/asset versions, but each compatibility claim must be matched to data actually available in the repository.
9. Do not download, publish, or include proprietary maps or assets without the appropriate rights. Treat input maps as the user's local data. Do not use them to train external services without explicit consent.
10. After every session update `docs/PROGRESS.md` with completed work, test evidence, problems, and the next three tasks. If a test was not run, state that explicitly.
11. Follow `AGENTS.md` and `C:\Users\Sorky\.codex\RTK.md`. Before changes create a dedicated branch from `master` when Git is available. Report missing metadata. Do not initialize Git or rename directories to work around it. Editing this plan does not automatically run implementation or its example prompt from section 11.
12. Tools: use CodeGraph first for code orientation, **only when `.codegraph/` exists**. Without an index, do not index the repository yourself. Use Context7 for current dependency documentation, preferably the correct version. Use `ast-grep` for structural queries with a preview before changes, and `rg` for text and configuration. Run shell commands through RTK and other CLIs through `rtk proxy`. Choose ProjectSorky tools and skills according to the actual scope. API/protocol contracts and OTClient HTML UI are outside this RME generator.

## 1. Technical context and system boundaries

Target editor: **Remere's Map Editor Redux**. The local user code and its `AGENTS.md` are authoritative. The upstream address is only a reference:
- https://github.com/Open-Tibia-Tools/remeres-map-editor-redux
- Script documentation: `scripts/README.md` at the selected commit. Verify it instead of assuming it is complete.

The local `CMakeLists.txt` requires CMake 3.28+, C++23, and wxWidgets 3.3. `vcpkg.json` declares Lua, Sol2, and baseline `1940ee77e81573713c0d364c42f5990172198be1`. Record the actual installed library and toolchain versions during G0. Do not infer the Lua version from README alone. The canonical Windows x64 build is `rtk proxy cmd /c build_ninja.bat`, producing `build-ninja/rme.exe`. Do not modify vendored FastNoiseLite or generated protobuf.

### 1.0 Initial review result (2026-10-26)

The first review covered source code rather than runtime behavior. `.git` and `.codegraph` were missing, while `.gito` existed and was left unchanged. The table below preserves the review findings. The current audit state is in `docs/PROGRESS.md`.

After the user prepared the environment, PHASE 0 began. Git and CodeGraph work, the local base is `5a01a8fe1c34465559701d50681cc2210ef32c67`, and the branch is `docs/procedural-phase0-audit`. `docs/procedural/ARCHITECTURE.md` and `docs/procedural/TEST_PLAN.md` were created. The progress report records build and test results. This does not mean that G0 has passed.

| Confirmed point | Location and consequence for the plan |
|---|---|
| Procedural Lua APIs and a demonstrator exist | `source/lua/lua_api.cpp`: `registerNoise`, `registerAlgo`, `registerGeo`; `scripts/terrain_generator_demo.lua`. Assess reuse first. The demo has hard-coded item IDs and mutates tiles directly, so it is not a safe integration pattern. |
| Drawing depends on the GUI and creates actions | `source/editor/operations/draw_operations.cpp`: `DrawOperations::draw` overloads read `g_gui.GetCurrentBrush()` and use actions/batches. This is not a ready API for isolated preview. |
| History can group adjacent actions | `source/editor/action_queue.cpp`: `ActionQueue::addBatch`, `addAction`; `source/editor/action_queue.h`: action/batch factories. The generator needs a verified transaction boundary and must not merge with manual drawing. |
| A script transaction exists | `source/lua/lua_api_app.cpp`: `LuaTransaction`, `transaction`, and the `app.transaction` export. Verify actual rollback and tracking of every mutation. The name alone does not prove that an arbitrary generator is atomic. |
| Brush variants do not derive from the noise seed | `source/brushes/ground/ground_brush.cpp`: `GroundBrush::draw` calls `random`; `source/util/common.cpp`: `getRandomGenerator` uses `random_device` and a thread-local `mt19937`. Brush resolution also needs a controlled RNG. |
| Map identity does not replace change tracking | `source/map/map.h`: `Map::getGeneration`; `source/map/map.cpp`: construction, `initializeEmpty`, and `open`. Do not treat this value as a counter for every edit. |
| Saving includes companion files | `source/io/iomap_otbm.cpp`: `IOMapOTBM::saveMapToDisk` also writes spawns, houses, and, depending on the format, zones and waypoints. Test the complete data set and the selected format's compatibility. |

Context7: wxWidgets 3.3 documentation was checked for [threads and communication](https://docs.wxwidgets.org/3.3/overview_thread.html) and [wxThread](https://docs.wxwidgets.org/3.3/classwx_thread.html). Keep GUI work on the main thread and deliver worker results through events such as `wxQueueEvent`. The receiver must remain alive. Library documentation does not prove thread safety for RME classes.

### 1.1 Final solution scope

- Generation of complete new areas, filling selections, extending existing maps, and joining an area to old terrain.
- Surface and Z levels: mountains, entrances/exits, underground areas, caves, islands, and archipelagos.
- Biomes and transitions: grass, forests, swamps, deserts, snow/ice, rock, coastal terrain, rivers, and lakes. Configuration remains extensible.
- Structures: paths, roads, passes, bridges, villages/ruins/camps, rooms and dungeons, and points of interest (POIs).
- Meaningful decoration, edges, floor transitions, correct item ID/brush mapping, and spawn placement only on request.
- Configurable parameters, seeds, masks, protected zones, and constraints such as preserving houses, UID/AID, and spawns.
- RME UI with preview, seed history, variant generation, style similarity scoring, and explicit reject or approve actions.
- Import of OTBM reference maps and, where a safe importer exists, other formats.
- **Style learning from user maps:** statistics, patterns, spatial relationships, transitions, structures, and multi-floor topology, with optional trained models supporting variant selection or regeneration.
- Evaluation against reference maps, path testing, and quality-progress tracking.

### 1.2 Non-functional requirements

- Determinism for a given configuration, seed, and algorithm/profile version.
- No destructive changes before explicit approval. Approval is one logical Undo/Redo transaction.
- Memory control, cancellation of long operations, error reports, logs, and no GUI freeze.
- Correct round trip: generate -> save OTBM -> open -> validate.
- Unit tests without starting the full editor wherever possible.
- No external-service dependency for generation or learning from local maps.

### 1.3 First useful scope (MVP, G1)

MVP: one Z level, an exact-selection mask or empty rectangle, grass/dirt/water, a manual semantic profile for one **verified** client/assets set, seed, preview, validation, explicit commit, and one Undo/Redo. Multi-floor mountains, advanced hydrology, structures, learning, and ML belong to later phases. Types may account for Z from the start, but G1 does not promise multi-floor generation.

The minimal dialog that starts this workflow is created in PHASE 1. PHASE 7 expands the GUI. Do not create every module and empty directory in advance. Create each one with its first required functionality and tests.

## 2. Logical architecture

```text
Reference maps (*.otbm + appropriate assets)
   | -> Importer/Validator -> Canonical Tile/World Representation
   | -> Feature Extractor -> Dataset + Anonymized Style Metrics
   | -> Style Profiles / Statistical Models / Optional ML Models
   |                                        |
   +----------------------------------------+ 
                                            v
GUI RME -> GenerationConfig -> World Planner -> Terrain Generator
                              |                |
                              |                -> Biomes / Hydrology / Elevation
                              -> POI/Routes -> Structures / Caves / Floors
                                            |
                                   Style-Aware Placement
                                            |
                                Semantic Safety Validator
                                            |
                               Brush/Item Resolver + Autoborder
                                            |
                          Concrete Diff + Connectivity/Safety Validator
                                            |
                         Preview/Diff -> Transaction/Undo -> RME Map
                                            |
                              OTBM Round-Trip + Quality Report
```

**Important:** the abstract map model must not require concrete sprite IDs. The adapter determines how abstract `water_edge`, `forest_floor`, `mountain_wall`, and similar concepts become brushes/elements for the selected Tibia version.

Validation after resolution is mandatory. Actual items, doodad variants, their footprints, and borders can change walkability and the change range. Repair requires resolving and validating again. Set an attempt limit after which the candidate is rejected.

### 2.1 Modules

| Module | Responsibility | Tests |
|---|---|---|
| `core` | types, RNG, configuration, grids, work on Z levels | determinism/property |
| `reference` | import, sanity-check, anonymized map features | fixture/golden |
| `learning` | statistics, patterns, biome/style profiles, optional ML | holdout/metrics |
| `planning` | plan for large world structures, regions, POIs, and routes | connectivity |
| `terrain` | heightmap, masks, biomes, hydrology | distribution |
| `structures` | mountains, caves, cities, ruins, paths, and bridges | reachability |
| `decoration` | flora, fauna as decoration, details, item clusters | density/overlap |
| `validation` | playability, asset compatibility, and map rules | property/integration |
| `rme_adapter` | map tiles, brushes, borders, undo, OTBM | integration |
| `ui` | dialog, preview, presets, status, dry-run, and commit | manual/UI |
| `benchmark` | performance, quality, and seed/style comparison | benchmark/golden |

Example directory layout. Adjust it after the audit:

```text
source/procedural/{core,reference,learning,planning,terrain,structures,decoration,validation,rme_adapter,ui}/
scripts/procedural/presets/
tests/procedural/{unit,integration,regression,fixtures}/
tools/reference_dataset/
docs/procedural/{ARCHITECTURE,DATASET,STYLE_MODEL,TEST_PLAN}.md
docs/PROGRESS.md
profiles/                  # only neutral sample profiles, no third-party maps
local-data/references/     # gitignored
local-data/models/         # gitignored, unless publication rights exist
```

## 3. Data model

Design around the structures, without treating the following names as fixed code names:

- `WorldRegion`: bounding box, Z levels, boundaries/surroundings, area mask, protected areas.
- `GenerationConfig`: region, seed, algorithm version, `styleProfileId`, biomeWeights, water, roughness, roads, density, caveRate, targetFloor, mode, constraints.
- `WorldCell`: coordinates (x,y,z), conceptual elevation, biome, ground semantic, water classification, features, occupancy, and passability.
- `RegionGraph`: POIs, underground entrances, passes, roads, connections, and their requirements.
- `TileStyleFeatures`: ground/brush histograms, adjacency, border sequences, objects/clusters, topology, per-biome densities, and floor relationships.
- `StyleProfile`: schema version, client/assets, reference identifiers, statistics, distributions, tolerances, and an optional model.
- `GenerationResult`: generated cells, the list of RME changes, quality metrics, warnings, seed, and provenance.

**RNG:** split the seed into stable streams for regions, roads, rivers, biomes, decoration, and caves. The same seed reproduces the same result within the pinned algorithm version.

The reproducibility contract also covers the input-map and halo snapshot, mask, protected data, autoborder settings, brush/material/asset manifest, resolver version, and build/toolchain. Store their identifiers or hashes in provenance. Define stable iteration, tie-breaking, and RNG-stream derivation order. Do not rely on `std::hash` or hash-container order. Do not promise identical floating-point results across platforms without dedicated tests.

Measure two hashes: the canonical semantic result and the final RME diff, including coordinates, item-stack order, and attributes. Preview and Commit use the same prepared result without drawing brushes again. Redo replays saved changes and does not run the generator again. Select and test the adapter's controlled RNG in G0/G1 without globally reseeding the manual-editing RNG.

## 4. Learning from reference maps - the key differentiator

### 4.1 What "learning" means

The generator will not magically learn after receiving one file. Implement a **reproducible local pipeline**:

1. Import and check OTBM maps and item/asset-version compatibility.
2. Convert tile/item IDs to a semantic representation. Map unrecognized elements to `unknown` with a report. **Do not guess.**
3. Extract regions and biomes, Z levels, structures, and relationships between them.
4. Extract measurable features, calculate distributions, and retain representative patterns.
5. Save **versioned style profiles** and diagnostic data.
6. Generate candidates from the profile and score which ones fit best.
7. Allow user corrections (`good/bad`, variant selection, masks, and manual edits) to update the profile only after explicit approval.
8. If the data set is large and diverse enough, train a separate ML model and compare it with the statistical baseline on unseen data.

**Priority:** build an effective statistical/rule-based model first and ML later. A small number of supplied maps is unlikely to be enough to train a reliable generative model from scratch.

### 4.2 Features to extract

- Terrain and region proportions at different scales (16x16, 32x32, 64x64, and larger).
- Adjacency matrices for ground types and borders (N/E/S/W and diagonals).
- River length, tortuosity, width, branches, and outlets, plus lake and island size.
- Forests: tree distribution, gaps, clusters, and density by biome and distance from water.
- Mountains: surfaces, edges, wall-to-plateau ratio, elevation, floor count, and pass availability.
- Placement of rocks, plants, debris, details, items, and brush sets.
- Caves: room size, corridor length, cycles, constrictions, entrance count, and floor relationships.
- Route network: spacing, widths, branch frequency, bridges, and region/POI connections.
- Placement of buildings, settlements, ruins, and important objects relative to their surroundings.
- Microstructures: repeated arrangements of small regions and rules for modifying them.
- Walkability map: connected components, bottlenecks, inaccessible areas, and possible traversal distances.

### 4.3 How to reproduce style without copying maps

- Learn **rules, distributions, relationships, and microstructures**. Do not paste unchanged reference regions.
- When reusing a motif, transform its semantics and resolve directional elements again. Do not mechanically rotate item IDs. Walls, doors, and Z transitions need explicit counterparts. Missing mapping means rejecting the transformation. Rotation or mirroring alone does not provide novelty or publication rights.
- Add a local-fragment similarity test to detect unintended copies of complete areas.
- Keep profiles separate, for example `classic-tibia-forest`, `classic-tibia-mountain`, and `user-custom-v1`, instead of one distribution for the entire world.
- Support a **scale hierarchy**: world layout -> region -> complex such as forest/cave -> segment -> tile/decoration.
- Style depends on the **Tibia version and used assets**. Store that information in the profile.

### 4.4 Data and dataset splits

- Local references: `local-data/references/` with a manifest containing name, sha256, format, client version, license/origin, biomes, region, floors, and notes.
- Split train/validation/test **by independent maps or large regions with a spatial buffer**, not by randomly selecting neighboring tiles. Otherwise results will be artificially inflated.
- Prevent a large map from dominating small ones by weighting per map/region/biome.
- Keep visual-similarity tests separate from playability and technical-correctness tests.
- Do not store raw maps in Git when permission or a license is unavailable.

### 4.5 Model maturity stages

- **L0 (baseline):** manual presets + noise + generation rules.
- **L1:** automatic reference statistics and generator-parameter fitting.
- **L2:** conditional rules, neighborhood n-grams/2D patterns, cluster distributions, and POI/transition graph models.
- **L3:** ranking multiple generated candidates using similarity/quality scores and user feedback.
- **L4 (optional when justified by data):** a locally trained model for biome allocation, decoration placement, or fragment proposals, compared with L3 on holdout data. Do not force a neural network without an improvement.
- **L5:** iterative learning from user corrections and profile versioning with change history and rollback.

### 4.6 Quality metrics and evaluation

Measure at least:

1. `valid_item_ratio` and the number of unrecognized IDs.
2. `border_error_rate`, layer conflicts, and unintended empty tiles.
3. `reachable_poi_ratio`, `reachable_walkable_ratio`, disconnected regions, and invalid Z connections.
4. Deviation of biome/object distributions from reference maps.
5. Difference in neighborhood and multi-scale-fragment distributions, river lengths, lake sizes, and forest density.
6. Similarity score after conversion to semantic maps and, when available, a rendered minimap.
7. Human ratings for naturalness, stylistic similarity, diversity, and playability through blind A/B tests on unseen maps.
8. `novelty / memorization`: percentage of suspiciously identical large fragments from reference data.
9. Time, peak memory, and transaction size for 100x100 / 500x500 / 1000x1000 at one and multiple levels.

**Do not optimize only one similarity score**, because the model will imitate texture instead of creating meaningful transitions. There is no "ideal" objective similarity score.

For each metric define the domain, denominator, aggregation, fixture, seed set, and threshold before holdout evaluation. Missing required POIs means `N/A`, not 100% reachability. The generated diff must have 100% valid IDs, no protection violations, and no changes outside the permitted mask. Report `unknown` reference elements with analysis coverage. `border_error_rate` needs explicit rules/an oracle, and visual review is separate evidence. For G4 record the baseline comparison on a fixed seed set, result spread, and uncertainty. Set quality thresholds before testing, not after reviewing results.

## 5. Procedural engine

### 5.1 Hierarchical generation

1. **World Planner**: area boundaries, existing-terrain adjacency, target regions, and main POIs.
2. **Macro Terrain**: elevation, humidity, temperature, drainage directions, coasts, and water.
3. **Biome Assignment**: biome from climate, elevation, distance from water, and style.
4. **Topology**: roads, main passages, bridges, passes, cave entrances, and Z-level network.
5. **Structures**: villages, ruins, caves, dungeons, and buildings in suitable locations.
6. **Micro Terrain**: ground patches, smaller ledges, and terrain details.
7. **Decoration**: plant and rock clusters and biome-specific elements.
8. **Resolver + Autoborder**: semantic mapping to brushes/items and consistent borders.
9. **Validation + Repair**: check reachability, closed passages, and conflicts, then repair or reject the seed.
10. **Preview/Commit**: show the result/diff and apply it only after approval.

### 5.2 Algorithms (select after benchmarking)

- Heightmap: layered Simplex/Perlin, domain warp, ridged noise, and region masks.
- Rivers: directional drainage/flow accumulation or simpler constrained routing, with lakes, outlets, and width control.
- Biomes: ecological rules plus reference profiles, smoothing boundaries while preserving organic shapes.
- Roads: A* / Dijkstra on a cost graph using slope, water, biome, and protected terrain.
- Forests/decorations: conditional grouping plus Poisson sampling where appropriate, with style-dependent density.
- Caves: cellular automata, room-and-corridor, graph repair, and floor connections.
- Settlements: street graph, parceling, and a rule/prefab library. Buildings must respect passages.
- Style improvement: stochastic local search and variant reranking by quality scores.

### 5.3 Safety rules

- Modes: `new_area`, `replace_empty_only`, `replace_allowed_types`, `selection_only`, `blend_edges`.
- Houses, teleports, spawns, UID/AID, PZ zones, quest items, and elements marked as protected must be preserved by default.
- Conflict -> warning plus rejection/dry-run, never silent deletion.
- Generate in a worker on a data copy. Apply map changes only according to RME threading rules.
- Extend the generated area with a neighbor halo for border calculations. **Do not modify** external tiles without an explicit policy.

### 5.4 Preview -> Commit contract and resource limits

1. Pin the result to a specific editor and map identity. On the main thread, take a consistent snapshot of the mask, halo, protected data, and asset versions. The worker receives only owned data. It must not use pointers to live `Tile`, GUI, Lua, or global managers without a verified threading contract.
2. Before Commit, check that the editor exists, the map and assets are unchanged, and mask/halo input data and relevant metadata still match the snapshot. The active tab, `hasChanged`, Undo index, or `getGeneration()` alone cannot detect edits. A mismatch invalidates preview and requires regeneration. Never move the result automatically to another map.
3. Protection covers complete tiles and footprints, nested container contents, and related house, spawn, zone, waypoint, and special-position data. A selection mask is not only a bounding box. The halo is read-only by default. If a correct border cannot be produced within the mask, reject the result or show an explicit range extension for approval.
4. Separate planned modes into write policy (`empty_only` / `allowed_types`), area mask, and edge policy (`blend_edges`). A selection alone does not authorize overwriting existing data. Use `empty_only` in the MVP. Empty-tile definition must include metadata, not only missing ground.
5. Prepare the complete concrete diff and rollback state before changing the map. Integrate drawing through `DrawOperations`, extending it only as needed with an explicit brush/RNG/batch context if the audit confirms it. Do not simulate clicks or switch the global GUI brush. `Action`/`Change`/`ActionQueue` remain the only change history. Do not create a competing Undo.
6. A single `BatchAction` groups history. It does not automatically provide atomic commit and rollback after an error. Prove with tests that errors leave no partial result or empty history entry, do not merge with adjacent edits, and preserve metadata and indexes. If existing operations need changes, plan a small integration PR before the generator.
7. Cancelling preparation always leaves the map and history unchanged. In the MVP, applying the prepared batch is a short section without cancellation or re-entry into edit handling. If measurements show a long commit, larger areas need safe staging with rollback and conflicting-edit locking rather than user-visible partial commits.
8. Estimate peak memory before allocation: snapshot + halo + cells + variants + concrete tiles + undo/redo. Check size overflows and `Position::isValid()`/current map limits. Separate conceptual elevation from Z, where higher Tibia floors have lower Z. Cell, memory, candidate, and repair-attempt limits must produce a clear rejection before mutation. Chunking does not remove the cost of the complete Undo.
9. Closing the window/map, changing the client version, or starting a new generation cancels or invalidates the task. Reject delayed results by task ID and never send events to a destroyed receiver. Minimal preview may be semantic. Actual render and final diff remain distinct in the UI.
10. The MVP is offline. In a live/collaboration session, block Commit with a clear message until conflict control and integration tests are designed separately. Do not assume that existing actions are distributed-transaction atomic.

### 5.5 Reachability and save compatibility

The validator uses concrete item stacks, client flags, and an explicit movement policy for directions, diagonals, and corners. Z transitions are directed edges with known requirements. Stairs, ladders, rope spots, teleports, doors, and scripted actions may require server data. Mark unknown behavior as `unknown/conditional` and do not automatically count it as playable. A static editor test does not replace a test on the target OTS.

Round-trip includes `.otbm` and every used companion file in a separate test directory. Compare canonical content, including attributes and stack order, rather than a byte hash of compressed files. Choose the OTBM version and test consumer. Existing code supports OTBM 5 among others, so do not promise compatibility with every format in original RME. Preserve supported formats. Do not add attributes or change serialization within the generator.

## 6. UI inside RME

### Panel "Generate Area"

- Tabs: `Area`, `Terrain`, `Biomes`, `Structures`, `Decoration`, `Reference Style`, `Advanced`, `Validation`.
- Size, X/Y/Z position, selection choice, seed + randomize, primary biome, and mixed biomes.
- Sliders: mountains, water, forests, paths, rough edges, vegetation, details, floor count, and POI density.
- Style panel: choose a profile learned from maps, reference list, analysis, and profile update.
- Preview: semantic minimap and, later, an asset-backed render, with A/B/C variants and scores.
- `Preview`, `Regenerate`, `Compare`, `Validate`, `Place on map`, `Cancel`.
- Statistics: biome/water shares, reachability, unknown item IDs, warnings, and runtime/memory.
- Save/load presets using versioned schemas.

### Required UX

- Smooth preview with throttling, background updates, and cancellation.
- Separate `Preview` and `Commit` operations.
- One Undo logically reverses the entire approved generation.
- Regeneration clears only the preview, never the user's map.
- Use `Theme` for colors, fonts, and spacing, and shared `VirtualItemGrid` / `VirtualBrushGrid` for item/brush catalogs. Integrate the map preview in the correct overlay tier or a separate surface. GL/NanoVG resources must use RAII, and render loops must not allocate per tile/frame.

## 7. Implementation schedule with acceptance gates

Statuses to fill in: `[ ] TODO`, `[~] IN PROGRESS`, `[x] VERIFIED`, `[!] BLOCKED`. Do not mark an item without a test.

### PHASE 0 - Repository and audit

**Final assessment 2026-11-02: VERIFIED.** All six audit items and G0-01…09 were assessed in the [final report](procedural/G0_FINAL_20261102.md), with concrete evidence and an explicit semantic difference in the current spawn consumer. The build + integration points + RME round-trip gate passed. This does not mean full OTS-world compatibility or guarantee the future generator. G1 had not started. The dated entries for 27–28 October below remain audit history.

- [x] Use existing checkout, verify Git and branch from `master`, record SHA and local changes. Confirm license, Windows x64, `build_ninja.bat`, toolchain versions and dependencies. Evidence in audit: local snapshot `5a01a8f`, build completed with `--parallel 2`; upstream origin unconfirmed.
- [x] Identify API for Map/Tile/Item/Brush/Autoborder/Undo and Lua; link specific files and functions. Evidence: CodeGraph review and table in `docs/procedural/ARCHITECTURE.md`; this is not runtime validation.
- [x] Verify OTBM read/write and multi-floor, house/spawn/UID/AID issues. Final actual RME round-trip and complete independent fixture comparisons for OTBM 3, separate zone OTBM 5; current consumer examined separately. Radius/direction legacy not preserved by its converter, not counted as full spawn compatibility. Evidence and limits: `docs/procedural/G0_FINAL_20261102.md`.
- [x] Build RME, open test map, save copy, run minimal script modifying 20x20 with Undo. 2026-10-26/27: 400 ground, Undo/Redo and save/load PASS for synthetic fixture; details and limits in `TEST_PLAN.md`.
- [x] Assess existing `noise`/`algo`/`geo`, terrain demonstrator and Lua transactions; select C++/Lua boundary based on tests, not API names. Decision 2026-11-01: core/snapshot/resolver/commit in C++, Lua optional for presets/config/demo. Evidence: noise/algo/geo samples, Draw/Lua/Draw, callback/setter/method reproductions and regressions, RNG and snapshot/halo analysis in `ARCHITECTURE.md` and `TEST_PLAN.md`. Selection does not imply adapter readiness or full atomicity; remaining G0 tests and G1 planning still open.
- [x] Create `docs/procedural/ARCHITECTURE.md`, `docs/PROGRESS.md`, `docs/procedural/TEST_PLAN.md` and plan for small PRs. Documents created; contain actual symbols and explicit results of executed checks. Did not overwrite existing general documentation.

**G0 gate:** reproducible build + description of actual integration points + round-trip of a test map.

Status 2026-10-27: empty-map smoke test passed, but G0 remains uncertified. Reproduced callback error in `app.transaction` (401 instead of 400 fields) fixed in G0-B through validating Sol2 protected call results. Map regression confirmed error propagation, rollback of six mutations, and no history entries after three failed transactions; successful transactions and Undo/Redo work. This is not proof of full generator atomicity: metadata tests and separation from manual edits still required. Evidence in `TEST_PLAN.md`.

Later G0-06 attempt passed for `Draw` → Lua → `Draw`, also with history grouping enabled. Save/load test covered floors 6/7, ground UID/AID, flags and spawn with creature. Discovered two blockers: direct `Item.actionId` change in transaction not tracked or undone, and “Save As” of new map in same directory retained old companion-file names and overwrote spawn XML used by source map. Test data was synthetic, restored. Fixes for both errors and regression tests required before G0 completion. Houses, zones and external consumer remain unverified.

Update 2026-10-28: prepared Save As and backup fix passed limited collision and access-error tests; full atomicity and crash recovery not demonstrated. Separate house fixture retained name, town, ID, rent and size in RME after re-open. Also discovered loss of unsaved rent change on close without warning. Noise tests and selected algo/geo operations passed for described data, do not prove full generator determinism. Working boundary remains: core, snapshot, resolver and commit in C++; Lua as optional config and demo tool. Final decision and G0 still require resolving item rollback and remaining tests from `TEST_PLAN.md`.

### PHASE 1 - Abstract core and basic integration

**G1 CLOSED WITH DEBT, 2026-11-06** ([technical debt](procedural/TECH_DEBT.md), [gate status](procedural/G1_GATE_STATUS.md)). G1-A started on 2026-11-02 on `feature/procedural-g1-core`. [Core and limits](procedural/G1_CORE.md). A passing C++ 100×100/500×500 test does not replace the G1 integration gate below.

- [x] `GenerationConfig`, stable RNG streams, single-floor grid with explicit Z, masks and protected areas. C++ core and tests ready; protection and exact mask from real snapshot and selection: [snapshot](procedural/G1_SNAPSHOT.md), [mask](procedural/G1_MASK.md), and in full round-trips 100×100 and 500×500 five fixture tiles remained unchanged.
- [x] Grass/dirt/water generation without item ID dependency, per MVP from section 1.3. First semantic baseline v1, without noise and naturalness scoring; actual C++ tests 100×100/500×500 PASS.
- [x] Simple resolver to proper brushes/items per client. [Resolver](procedural/G1_RESOLVER.md); per report resolver rejects missing brush and item, missing client checked in native refusals ([refusals](procedural/G1_REFUSALS.md)); resolver tests not rerun at this update.
- [x] Dry-run (Preview and Validate), commit and Undo/Redo, logs. [Commit](procedural/G1_COMMIT.md), [round-trip](procedural/G1_ROUNDTRIP.md); dialog logs have runtime proof for Preview, Validate, Place, Cancel and refusals ([gate status](procedural/G1_GATE_STATUS.md)).
- [x] Determinism and error parameter tests. C++ core PASS; preview, mask and commit fingerprints 100×100 and 500×500 identical across builds ([staged regression](procedural/G1_STAGED_CORE.md)), and three 500×500 trials saved complete files byte-identical ([repeatability](procedural/G1_PLACE_TIMING_REPEATABILITY.md)). Scope: seed 1729 and two sizes.
- [x] Minimal dialog: seed, area, Preview, Validate, Commit, Cancel; handling missing client/assets and stale result. [Dialog](procedural/G1_DIALOG.md), [matrix](procedural/G1_DIALOG_MATRIX.md), [refusals](procedural/G1_REFUSALS.md), [invalidation](procedural/G1_INVALIDATION.md).
- [~] Test concrete diff, cancellation, application error, map/assets change after preview, budget overflow and separate Undo entry. Offline commit. Completed: [commit](procedural/G1_COMMIT.md), [cancellation](procedural/G1_CANCEL.md), [invalidation](procedural/G1_INVALIDATION.md), [budget](procedural/G1_BUDGET.md), [resources](procedural/G1_ASSETS.md). Out of scope: actual live network session (offline mode tested on stopped live host). Open: allocation errors checked at described locations, not all.

**G1:** 100x100 and 500x500 on single floor within measured resource budget, preview matches applied diff, save and open complete file set, one Undo and Redo, zero changes outside mask and protected data. Lack of budget for 500x500 is a constraint, not G1 failure.

**Status 2026-11-06:** all list items have evidence except one noted at last item. Full round-trip of both sizes repeated on current binary, 100×100 resource thresholds approved, and memory growth in 200 cycles deemed non-blocking (user decision). Gate is **closed with debt** (user decision 2026-11-06; disclaimers: Place 500×500 times from EXE `15c25557…`, not current `b87d1abb…`; many proofs are single runs; PR without CI, intermediate commits not built separately). Actual live network sessions deemed out of scope for G1. Unresolved remaining allocation failures and `SetFocus` stack from frame activation with open generator (2026-11-04); `SetFocus` on close and in refusal traces explained. Some proofs are single runs. Deferred items: [technical debt](procedural/TECH_DEBT.md). Details: [gate status](procedural/G1_GATE_STATUS.md).

### PHASE 2 - Meaningful surface and borders

- [~] Natural rivers, lakes, rocky terrain, and shores, plus biome transition zones. Multi-floor mountains remain in PHASE 5. G2 closed with debt on 2026-11-07 ([G2_GATE_STATUS.md](procedural/G2_GATE_STATUS.md)). Further G4 changes on 2026-11-08 covered river meander (D6 partial, criterion not met), large structures (`featureSize`, E64 criterion not met at side 256), a wider beach (P′ in range for m06 and m19), and ground without metric change. See [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md) and [G4_COMPARE.md](procedural/G4_COMPARE.md). Data still has no river branches (D9). Rock masses (iteration 8) are smoother than references and the criterion is not met.
- [~] Extended semantic mapping and existing-autoborder tests for multi-directional edges and neighborhood preservation, without a second border engine. G2-B evidence is in [G2_BORDERS.md](procedural/G2_BORDERS.md) (`pairs_without_border=0`, client 1098-BlackTek only). It was not rerun after G4 changes except for one GUI profile. Brushes with external borders are handled by a ring in G4.
- [x] Terrain-dependent vegetation and conditional details. G2-C covers core tests and user GUI review. G4-D covers profile-based contextual vegetation in [G4_DECOR.md](procedural/G4_DECOR.md). Vegetation quality against references is inconclusive, with 2 of 4 folds worse.
- [~] Distribution tests, no unsupported items, and visual tests on several seeds. Core distribution and seed tests PASS. Visual assessments are user GUI reviews of individual previews without blind A/B testing.

**G2:** generated terrain is legible in RME, with no known critical border errors.

### PHASE 3 - Reference-map import and analysis

**G3 CLOSED WITH DEBT, 2026-11-07 (user decision): gate criterion met for 20 maps (19 standard 10.98, 1 BlackTek), with the planned scope incomplete.** Criteria, evidence, and gaps: [G3_GATE_STATUS.md](procedural/G3_GATE_STATUS.md).

- [~] Local OTBM import and item/asset compatibility detection. Partially completed on 2026-11-07 with one reference: [G3_IMPORT.md](procedural/G3_IMPORT.md).
- [~] Normalizator map do reprezentacji semantycznej + `unknown` reports. 2026-11-07, fixture i jedna referencja: [G3_NORMALIZER.md](procedural/G3_NORMALIZER.md).
- [~] Region, biome, topology-feature, and floor recognition. On 2026-11-07, without bottlenecks or named biomes: [G3_TOPOLOGY.md](procedural/G3_TOPOLOGY.md).
- [~] Feature extraction with CSV/JSON/HTML or MD reporting. On 2026-11-07, JSON/MD/CSV without HTML and without some features from 4.2: [G3_FEATURES.md](procedural/G3_FEATURES.md).
- [~] Tests on several local fixtures and user references without publishing references. On 2026-11-07: local fixtures, `forgotten.otbm` (BlackTek), and 19 standard 10.98 maps, with two byte-identical series. Import memory limits/cancellation (D8) and feature usefulness were not assessed: [G3_REFERENCES.md](procedural/G3_REFERENCES.md).

**G3:** every reference map produces a reproducible report, statistics, and semantic map.

### PHASE 4 - L1/L2 style learning

- [x] Style profiles and a reference manifest with a versioned schema. `g4-manifest/1`, `g4-profile/6` with `checkProfile`, passing core tests, and repeatability across two runs. See [G4_PROFILE.md](procedural/G4_PROFILE.md) and [G4_SPLIT.md](procedural/G4_SPLIT.md). Profiles come from private maps and are not in the repository.
- [~] Fitting ground, adjacency, clustering, and biome-transition distributions. Distributions, adjacency, and grass/ground transition are fitted in [G4_FIT.md](procedural/G4_FIT.md). Clustering and structure scale are partial, iteration 6 criterion not met. Biomes are unnamed (D9).
- [~] Context-dependent rules for forest near water, rocks near mountains, paths, and transitions. Contextual forest and rocks exist in [G4_DECOR.md](procedural/G4_DECOR.md). Routes are optional, iteration 9 metric is in range but GUI is unverified. Optional buildings have floors and walls without doors or roofs, iteration 11 with user GUI confirmation. Transitions are partial, with grass around rock and beach, iteration 10: m06 8 of 8, m19 2 of 8. Biome transitions as such do not exist (D9, D15).
- [~] Independent profiles for surface/underground and asset versions. The GUI offers a profile per client and asset hash, but only surface with Z ≤ 7 and no underground.
- [~] Holdout evaluation comparing with a baseline on unseen maps. Completed in iterations 1–3 ([G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md)). Improvement on unseen maps was not demonstrated, with the only evidence being swamp, n = 2.

**G4:** measurable and reproducible improvement in selected style metrics over manual presets without worsening critical playability metrics.

**G4 NOT MET (2026-11-08):** the mechanisms above exist and are reproducible, including manifest, family split, profiles, distribution fitting, vegetation rules, and fold evaluation. However, unseen-map evaluation did not show improvement over manual presets without worsening land consistency. Criteria, results, and gaps: [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

### PHASE 5 - Topology and multi-floor worlds

- [~] World connections: region graph, POIs, routes, bridges, passes. Completed (2026-11-09): sand fords (Shore across river/lake, `fordLength` up to 256) connecting isolated land within one area; ford tiles checked with probe for blocking, in-game walking and appearance not verified ([G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 14b”); item bridges and passes: not done. Region graph built 2026-11-09 (`buildRegionGraph`, iteration 24 in [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), core only). Item bridges completed 2026-11-12 (iteration 37); passes: not done.
- [~] Multi-level mountain logic and Z transitions. Completed in core (2026-11-09, iteration 27): Z entrances on rock edge, cave one floor below, Z reachability validator ([G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md)); drawing on map completed 2026-11-09 (iteration 28: entrance item, cave at z + 1, ramp up, two Undo steps, “Underground” tab; GUI and in-game not verified; 2026-11-10 iteration 29: cave walls, entrance in rock wall or hole, exits outside rock; too few wall-style entrances; 2026-11-10 iteration 30: exit ramp below entrance, no hole on surface; iteration 31: cave without buildings, missing entrance above ramp in GUI not reproduced; iteration 32: plateau at z − 1 with vegetation and ramps, appearance not verified), iteration 33: up to three floors, limit by %; iteration 34: caps on own slope; appearance not verified), stairs in buildings: not done. 2026-11-12: iterations 37–39 connect cap and cave floors in validator and repair (G5 gate PASS); appearance still not verified.
- [~] Caves, dungeons, corridors, chambers, entrances, exits. Simple cave (tunnels, rooms, landings under Z entrances) in core 2026-11-09, iteration 27; drawn on map with ramp exit at entrance from iteration 28 (appearance not verified); dungeons, corridors with doors and exits at other end: not done. 2026-11-12: more entrances (median 18.5 / 33, iteration 39), cave without entrance removed (iteration 38).
- [~] Structures: settlements/ruins/camps, placement with collision control. Surface settlements: core has clusters, doors and routes to doors (2026-11-09), measurement criterion 12c not met, door adapter and GUI completed, appearance confirmed by user ([G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 12”); ruins (building without doors with wall gaps) and camps (clearing with open tent) completed 2026-11-09 ([G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 25”; appearance not verified, without doodads and without measurement in references); underground: not done.
- [~] Reachability validator and automatic repair / seed rejection. Completed (2026-11-09): surface reachability measure in core (`reachabilityOf`), not connected to GUI; buildings cut off by river in 10 of 32 seeds, criterion not met ([G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 13”); POIs and fixed start in validator (iteration 22, single-floor surface, no GUI): all entrances and exits reached in 32 of 32 seeds at `fordLength` 256; repair (`repairReach`, “Repair unreachable places” field) and seed rejection (`generateReachableSurface`, core only) completed 2026-11-09: all POIs reached in 32 of 32 seeds at `fordLength` 48 with repair; repair appearance not verified, Z transitions and underground: not done. 2026-11-12: validator across all floors (iteration 36), multi-floor repair and bridges (37), repair on final mask after autoborder (39), unreachable islands as sand (40): **G5_GATE_PASS** in m06 and m19, 32/32 seeds.

**G5:** required POIs, valid Z entrances/exits, and no critical disconnections are reachable from the fixed start.

### PHASE 6 - Advanced learning and L3/L4/L5 ranking

Criteria and evidence: [G6_METRICS.md](procedural/G6_METRICS.md), [G6_GATE_STATUS.md](procedural/G6_GATE_STATUS.md).

- [~] Multiple candidate generation + quality scoring + best selection. Lexicographic score (I51) and candidate gallery N=1–16, default 4 (I52) are implemented. The I51 and I51b quality criteria failed, so the recommendation is not proven better than random selection. Manual GUI review is open.
- [x] Feedback `approve/reject/adjust` and profile history. I54: local versioned JSON history; Adjust saves a new profile version. I56 adds learning a local profile from the open map. Manual GUI and keyboard review is open.
- [x] Optional ML experiment for selected subtask only when data sufficient and rights-compliant. Documented ML deferral does not block statistical generator release. Withdrawn with justification on 2026-11-17; reconsidered after at least 200 ratings from at least 3 map families.
- [~] Holdout comparison with L2/L3, overfitting analysis and copied-fragment detection. Copied-fragment detection, including rotations and reflections, is done (I53, D10). Blind test run 2026-11-23: FAIL, 9/20. New-family comparison not run (no data).
- [x] Model versioning and rollback mechanism. I54: return to an earlier profile version without deleting newer ones, and branching from it.

**G6 CLOSED WITH DEBT, 2026-11-23 (user decision).** I55 blind test FAIL: L3 won 9/20 pairs, threshold 13 ([G6_GATE_STATUS.md](procedural/G6_GATE_STATUS.md#i55--blind-test-result-2026-11-23)). The comparison on new families was not run because none were supplied. Debt D28 and D29 in [TECH_DEBT.md](procedural/TECH_DEBT.md).

**G6:** ranking and versioned feedback evaluated on unseen maps and in blind user test; optional ML model deployed only if it exceeds simpler baseline per pre-set criteria.

### PHASE 7 - Full GUI and workflow

- [~] `Generate Area` dialog with described tabs and parameters. I64 (2026-11-25, [G7_GATE_STATUS.md](procedural/G7_GATE_STATUS.md)): the eight tabs of section 6 and a primary biome (five biomes as partial presets). Mixed biomes and sliders in place of number fields are open.
- [ ] Data preview and map render, variants, navigation, quality comparison.
- [~] Reference import/analysis and style profile panel. Learning from the open map (I56) and the copy index built from it in the GUI (I57, 2026-11-23, [G7_GATE_STATUS.md](procedural/G7_GATE_STATUS.md)). Statistics and warnings for the Selected candidate (I58), presets and a random seed (I59), preview zoom, pan and side-by-side comparison (I60), preview steps under 50 ms on the GUI thread at 512x512 (I61), 2026-11-24. Profiles from up to 8 samples of one or several maps and ranking bands learned from them (I62), 2026-11-24.
- [ ] Background generation, cancellation, progress, preview cache, commit/undo.
- [x] Area protection and warning handling. Existing content is never changed (G1); I63 (2026-11-24, [G7_GATE_STATUS.md](procedural/G7_GATE_STATUS.md)) reports what the area keeps, warns when terrain touches houses or spawns, and adds a 0-8 tile margin around existing content.

**G7:** entire workflow possible from GUI, without terminal and without manual script invocation.

### PHASE 8 - Optimization and reliability

- [ ] Benchmarks 100x100, 500x500, 1000x1000 at 1 and multiple floors; record results and hardware.
- [ ] Chunking, caching, memory optimization and incremental preview.
- [ ] Crash recovery, cancellation, large-map, odd item ID and missing-brush tests.
- [ ] CI: build, unit, integration smoke, format/lint, release artifacts.
- [ ] Save/load compatibility with specified OTS server and client, if supplied.

**G8:** no critical errors, benchmarks documented, regressions pass automatically.

Add regression tests and basic CI with functionality from PHASE 1; PHASE 8 expands coverage and measurements. Do not defer memory limits, cancellation or bounds checking to final optimization.

### PHASE 9 - Beta, documentation, v1.0 release

- [ ] Sample profiles, presets and demo without publishing protected maps.
- [ ] Installation, launch, reference import, profile learning, generation and export instructions.
- [ ] Format/profile, configuration, troubleshooting and generator extension documentation.
- [ ] Final test: generate large surface and underground map, evaluate, approve, save, re-open, traverse paths.
- [ ] Release notes, tag `v1.0.0`, sample recording/visualizations, known-limitations list.

**G9 / Definition of Done:** user can supply maps in RME GUI, train/tune style profile, generate complete coherent multi-floor area, preview result, approve it without destroying existing map, save and open OTBM. Result passes compatibility, topology and quality tests on unseen maps.

## 8. Minimal end-to-end test scenarios

1. **Determinism:** same seed/config/style/model version → same semantic-result hash on same build.
2. **Undo:** generate 500x500, Commit, Undo → original map snapshot; Redo → identical result.
3. **Protected objects:** area contains house/spawn/UID/AID/PZ → generator does not change protected data.
4. **Neighbor blending:** generation in middle of old map → correct edges, no changes outside permitted range.
5. **OTBM:** round-trip without corruption and unexpected attribute loss.
6. **Reachability:** surface and underground POIs connected per configuration.
7. **Reference learning:** train on A/B, test on C → metrics report compared with baseline.
8. **Novelty:** detect large identical fragments including after rotations/mirrors; exclude trivial uniform areas by explicit rule. Report suspicious copies and apply fixed rejection threshold. Data-use agreement does not replace requirement to generate new areas.
9. **Cancel:** cancel preview or operation → no map changes and no leaks.
10. **Scale:** several 1000x1000 floors → stable resources, clear progress, no uncontrolled OOM.
11. **Stale preview:** change tile/halo, Undo/Redo, close map, switch tab or assets after Preview → no result applied to wrong state/map.
12. **Transaction failure:** controlled prepare or apply error → no partial mutation and no index/history corruption; next manual edit has separate Undo.
13. **Footprint/mask:** doodad crosses irregular selection or protected tile, border requires exit beyond mask → rejection or explicitly approved wider diff.
14. **Resource limits:** extreme dimensions/Z, size overflow, no Undo budget and many variants → rejection before map change; delayed cancelled-task result does not replace new preview.

## 9. Risks and design decisions

| Risk | Response |
|---|---|
| Few training maps | rules/statistics + careful validation, no advanced ML promises |
| Different item IDs | version adapter + semantic mapping + unknown report |
| Maps not uniform | biome segmentation and per-region profiles |
| Features not auto-recognizable | manual labels / annotation tools / heuristics with confidence |
| Incorrect autoborders | brush integration + neighbor tests and golden fixtures |
| Unreachable maps | topology graph + validation + repair |
| Dependency on developing fork | pin SHA, project branch, CI tests and adapter instead of core mass changes |
| Overfitting / reference copying | spatial holdout, dedup, novelty tests, limit patch reuse |
| Computational cost | profiling, chunking and multi-stage generation |
| Map/asset rights | private dataset and explicit use rights |

## 10. Codex/Claude work strategy

Break each phase into PRs with a single responsibility. For each PR the agent must present:

```text
Goal:
Verified files/API:
Scope of changes:
Risks and safeguards:
Tests added:
Tests run + result:
Demo/screenshot/log (if applicable):
What remains:
```

- The agent cannot “pass” tests by description or mock-ups when real RME integration is required.
- In each phase, update `docs/PROGRESS.md` with the exact status and blockers.
- Do not introduce an ML model until the statistical baseline and evaluation are working.
- The last step of each phase is a short manual test in RME and a screenshot/report for human verification.

## 11. First prompt for the agent (copy exactly)

> You are the lead engineer of the Tibia procedural map generator project within Remere's Map Editor. Read `docs/PROJECT_RME_PROCEDURAL_GENERATOR.md` in its entirety and treat it as the project plan and acceptance criteria contract, while following `AGENTS.md` and the scope of the current user command.
>
> Start **PHASE 0 ONLY**. Identify the repository, SHA, existing Lua/C++ API, and build. Use CodeGraph if an index exists, Context7 for dependency documentation, and RTK/ast-grep/rg according to project rules. Do not assume function names without reading the code. Build via `rtk proxy cmd /c build_ninja.bat` and run a minimal safe smoke test of import, 20x20 editing, Undo/Redo, and saving/opening OTBM along with companion files if the environment and data permit. Create `docs/procedural/ARCHITECTURE.md`, `docs/procedural/TEST_PLAN.md`, and complete `docs/PROGRESS.md`. Point to actual symbols and code locations. Check risks from section 5.4; do not yet implement the final generator or ML. Do not pass tests that were not run. At the end, provide a report according to section 10, list blockers, and at most three next tasks.

## 12. Information/data to be supplied by the user

G0 needs at least one compatible asset set and a safe test map, which may be synthetic. Do not download them automatically. Other references and evaluations may be supplied incrementally before G3/G4:

- Tibia client version and corresponding `items.otb`, `items.xml`, `dat/spr`, or assets supported by the specific build.
- Several OTBM reference maps; ideally covering surface, mountains, caves, cities, different regional styles, and Z levels.
- Information about which maps represent the desired style and which elements are particularly valuable.
- An example map to which the generator should adapt new areas.
- A set of 5-10 manual examples of "good result / bad result" when the first versions are available.
- Target OTS and required OTBM compatibility; constraints regarding spawns, PZ zones, houses, etc.

## 13. Project tracking

Keep the current state in `docs/PROGRESS.md`. The following is a starter template, not an execution report:

| Phase | Status | Commit | Evidence/tests | Blockers | Next task |
|---|---|---|---|---|---|
| 0 | VERIFIED | 2026-11-02 | [G0_FINAL_20261102.md](procedural/G0_FINAL_20261102.md) | - | G1 |
| 1 | CLOSED WITH DEBT | 2026-11-06 (PR #1, `54cec96`) | [G1_GATE_STATUS.md](procedural/G1_GATE_STATUS.md) | remaining allocation failures, `SetFocus` on activation | G2 |
| 2 | CLOSED WITH DEBT | 2026-11-07 (PR #3, `794b068`) | [G2_GATE_STATUS.md](procedural/G2_GATE_STATUS.md) | D5, D6 | G3 |
| 3 | CLOSED WITH DEBT | 2026-11-07 (PR #4, `33b5c88`) | G3_GATE_STATUS.md | D8 (remnants), D9–D11 | G4 |
| 4 | IN PROGRESS, G4 gate NOT met | 2026-11-08 (branch `feature/procedural-g4-style-learning`, no PR) | [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), [G4_COMPARE.md](procedural/G4_COMPARE.md) | no improvement on unseen maps, narrow data (caves, swamp), D9 | Features D9 (large-scale structures, mountains, trails), more maps |
| 5 | CLOSED WITH DEBT | 2026-11-17 (branch `feature/procedural-g5-settlements`, no PR) | [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md) | D23 (test in-game), D24 (mountain wall faces, in-game appearance) | G6 |
| 6 | TO START | - | - | - | ranking candidates, feedback |
| 7 | TODO | - | - | - | After G6 |
| 8 | TODO | - | - | - | After G7 |
| 9 | TODO | - | - | - | After G8 |

---

## 14. Editorial changes in version 1.1

- Aligned the plan with local CMake/build and documented the absence of Git/CodeGraph without claiming G0 completion.
- Identified existing procedural APIs and demonstrator risks; limited MVP and moved minimal GUI to G1.
- Added concrete validation after resolver, controlled RNG for brushes, and Preview/Commit/Redo identity.
- Clarified snapshot, conflict with map changes, halo/footprint protection, atomicity, and Undo memory; live outside MVP.
- Separated static validation from OTS testing, added companion files to round-trip, and measurable evaluation criteria.
- Detailed documentation goes to `docs/procedural/`; ML remains optional. Generator implementation has not started.

---

**The end goal is not “a random pretty map”.** It is a local, reproducible, and safe map generator with consistent style mapped from references, which enables conscious world design and playability verification, and whose quality can be measured and improved with each map and user evaluation.
