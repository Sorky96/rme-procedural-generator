# G5: results and gate status

G5 iteration results from number 24 (criteria in [G5_METRICS.md](G5_METRICS.md)). Iterations 12–23 (settlements, reachability validator, fords, paths, borders, window, POI, repair) are in the historical [G4_GATE_STATUS.md](G4_GATE_STATUS.md). **G5 (from the plan):** from the established start, the required POIs are reachable, entrances and exits Z are correct, and there are no critical cuts; today checked only on one floor of the surface.

## Iteration 24 — region graph over surface (2026-11-09): PARTIAL, criteria 1–3 met

Criteria recorded before implementation ([G5_METRICS.md](G5_METRICS.md), “Iteration 24”, commit `8a6bfe1`, moved to G5 file in `a115e93`; grep confirmed). Implemented `buildRegionGraph` (`region_graph.{h,cpp}`): nodes = 4-connected components of Land / Rock / Water classes with ≥ 16 cells (smaller ones are fragments), neighborhood edges with weight (cell pairs), POI nodes (start, entrances, exits) with region and route distance from start; pure function, generator unchanged; `graph` row in `style_render`.

**Criterion 1: met.** New `test_procedural_region_graph` with manually counted 14×6 layout: 4 nodes (Land 20, Water 18, Land 30, Rock 16), edges (0,1) 6, (0,3) 12, (1,2) 6, frame and rock center of mass, start (5,2), exits north 2, south 3, west (in rock) 5, east unreachable; 3×3 rock fragment is not a node; sum of node cells + fragments + Preserve = cell count; entrance in Land region; all cells of one Floor/Land class; errors, `stop_token` and determinism. **Criterion 3: met** (17 core tests and both G1 probes PASS; core generator unchanged; two runs byte-identical).

**Criterion 2: met.** Seeds 1001–1032, side 512, profiles v10, `buildingDensity` 30 / 40, `pathLength` 40 / 80, `fordLength` 48, `repairReach` 1 (median [min..max], m06 / m19):

| measure | m06 | m19 |
| --- | ---: | ---: |
| nodes | 24 [15..31] | 27 [15..42] |
| Land / Rock / Water | 7 / 6 / 10 | 8 / 8 / 11 |
| edges | 32 [19..42] | 36 [20..52] |
| fragments < 16 cells | 4 [0..11] | 5 [1..12] |
| seeds without unreachable POI | **32 of 32** | **32 of 32** |
| neighborhood graph connected | 32 of 32 | 32 of 32 |
| max distance to entrance (steps) | 585 [425..910] | 570 [374..1028] |
| max distance to exit (steps) | 591 [309..996] | 520 [294..980] |

Required (passage criterion): all POI nodes with distance ≥ 0 in 32 of 32 seeds (per iteration 23): is 32 of 32. One graph's time fits within generation time (one seed with graph and everything else 0.25 s in `style_render`; this is not a separate graph measurement).

**Caveats (from record):** classes are coarse (Land includes grass, dirt, beach, paths and buildings); neighborhood is cell touch, not “passage”; distance is 4-neighbor steps, not game time; graph does not know ruins, camps or points from list; no multiple floors and Z entrances; threshold 16 is my choice; graph is not saved or shown in GUI. **Not verified:** graph performance alone (not measured separately), behavior with masks and protection (Preserve is excluded from graph), graph usefulness for quality assessment (no consumer yet).

## Iteration 25 — ruins and camps (2026-11-09): PARTIAL, criteria 1–4 met, appearance awaits user

Criteria recorded before implementation ([G5_METRICS.md](G5_METRICS.md), “Iteration 25”, commit `6815b84`; grep confirmed), with explicit deviation from “measure in references first” (definitions are my idealization, criteria absolute). Implemented: `SurfaceTuning::ruinPercent` (0..100) and `campCount` (0..50 per 65,536 cells), appended at end, 0 = byte-identical as before; `SurfaceGrid::gaps` and `sites` (in snapshot only when non-empty; `valid()` checks them on Floor); **ruin** = building without doors and without route to doors, with gaps in walls (≥ 2, about quarter of perimeter without corners); **camp** = clearing (Path disk r = 3) with open tent (Floor 3×3, entire perimeter is gap), in location without water, rock and floor (≤ 12) and path (≤ 5), center separation ≥ 12; resolver skips wall at gaps; validator counts gaps like doors (passable), `PoiReport` has `sites` and `sitesReached`; ford repair mode attaches sites; “Ruins (%)” and “Camps” fields in Buildings tab (profile with floor, 8 brushes, also with camps only).

**Criterion 1: met** (options 0: Seed 1729 snapshot and 17 core tests unchanged). **Criterion 2: met** (manual tests: building without doors and without gaps closed, with one gap reachable (69 cells, 1 component), site reached through gap, gap and site outside Floor rejected by `valid()`, snapshot changes; `ruinsAndCamps` property test: options off no gaps, on: deterministic, correct grid, doors + sites cover foundation, each site reached after repair, camp has one site and 8 gaps, `campCount` 51 rejected). **Criterion 4: met:** 17 core tests and both G1 probes PASS after `rme` rebuild; new `--surface` case `SURFACE_RUINS_E2E_PASS` (side 250: 146 gaps, 23 sites, 948 walls in plan; wall count + gap count = perimeter cell count; Commit as one step, **no wall element on gap tile**, Undo, Redo); two runs byte-identical.

**Criterion 3: met.** Seeds 1001–1032, side 512, profiles v10, `ruinPercent` 30, `campCount` 6, `fordLength` 48, `repairReach` 1 (m06 / m19, `buildingDensity` 30 / 40):

| measure | m06 | m19 |
| --- | ---: | ---: |
| buildings (median) | 120 | 160 |
| ruins (median [min..max]) | 35 [26..43] | 47 [40..60] |
| ruin share among buildings (median) | 29.2 % | 29.4 % |
| camps | 24 [24..24] | 24 [24..24] |
| gaps per ruin (median) | 5 | 5 |
| seeds with all sites reached | **32 of 32** | **32 of 32** |
| seeds with all POI reached | 32 of 32 | 32 of 32 |

Required: ≥ 29 of 32 seeds with all ruins and camps reached from start: is 32 of 32; ruin share ≈ 30 % as expected.

**Not verified (and caveats from record):** appearance (ruin is building with holes in wall, no rubble; camp is clearing with path and floor without fires and doodads, because profile has no brush for them); whether wall brush gives sensible wall ends at gaps (counts `WallBrush::doWalls`); measurement in references (not done); ruins and camps as separate POI in region graph (only `sites` in validator); behavior with masks and protection; “Ruins (%)” and “Camps” window fields in GUI (built, not viewed); performance (not measured separately).

## Iteration 26 — campfire in camp (2026-11-09): PARTIAL, criteria met with one divergence, campfire with real brush not checked

Criteria recorded before implementation ([G5_METRICS.md](G5_METRICS.md), “Iteration 26”, commit `366f757`; grep confirmed). Implemented: `SurfaceGrid::fires` (one Path cell at center of clearing; in snapshot only when non-empty; `valid()` requires Path), new camp geometry (clearing: Path disk dx² + dy² ≤ 18 = 61 cells, campfire at center, tent Floor 3×3 shifted 3 cells east, open terrain Grass or Dirt at radius 7 and no Floor at radius 12), `GroundProfile::campfire` (doodad brush, no IDs in code), plan layer `GroundPlan::fires` (in plan snapshot only when non-empty; independent from “Vegetation”), item selected from brush variants per cell and seed, validation in resolver, preparation and preview, “Campfire brush” field in Buildings tab (empty when client has no “campfire” brush).

**Criterion 1: met** (without camps or without campfire brush: Seed 1729 snapshot and 17 core tests unchanged). **Criterion 2: met:** `surface` test: one campfire per camp; geometry counted by hand and checked on seed with exactly one camp (grid 37×37, `campCount` 50): campfire on Path, site = tent center 3 east of campfire, 8 gaps, 9 Floor cells and 52 Path cells in disk (61 − 9); `valid()` rejects campfire outside Path (in core test via manually built grid). **Criterion 3: met, but with brush substitute:** client 10.98 in repo **has no “campfire” brush** (items 1421–1426 without brush; brush exists in 12.90+ data), so new probe case `SURFACE_FIRES_E2E_PASS` (side 250, 7 campfires) uses “small rocks” brush as campfire brush and tests mechanism: fires layer in plan = fire cells, refusals (unknown brush, ground brush, profile without floor brush), after Commit exactly one element per fire cell (id matches plan) and nowhere else, one history step, Undo and Redo restore state; plan snapshot with campfire differs from plan without. **Criterion 5: met** (17 core tests and both G1 probes PASS after `rme` rebuild; two runs byte-identical).

**Criterion 4: met with divergence.** Seeds 1001–1032, side 512, ruins 30 %, 6 camps, `fordLength` 48, `repairReach` 1: all sites reached in 32 of 32 seeds (m06 and m19), all POI in 32 of 32; camps **in m06 24 in each seed, in m19 median 24 but min 12** (record required “still 24 per map”: in some m19 seeds new free terrain requirements (radius 7, no Floor at 12) give about half as many camps).

**Not verified (and caveats from record):** appearance; operation with real “campfire” brush (missing in repo client; user should check on their client whether “Campfire brush” field fills and campfire appears); “campfire” brush variants (ring, burnt wood, fire chosen per cell, so some campfires may be extinguished); campfire cell blocking in game; campfires as POI; performance impact.

## Iteration 27 — Z entrances and simple cave, core part (2026-11-09): PARTIAL, criteria 1–4 met, nothing drawn yet

Criteria recorded before implementation ([G5_METRICS.md](G5_METRICS.md), “Iteration 27”, commit `d0bd493`; grep confirmed), scope chosen by user: “entrances + simple cave”, split into core (this iteration) and adapter (iteration 28). Implemented: `SurfaceTuning::caveEntrances` (0..20 per 65,536 cells, appended at end, 0 = byte-identical as before), `SurfaceGrid::entrances` (Rock at rock edge, ≥ 3 from area edge, no water and floor in 5×5 window, separation ≥ 24; in snapshot only when non-empty; `valid()` requires Rock), new `underground.{h,cpp}`: `CaveGrid` (Solid / Floor, z + 1), `generateCave` (landing 21 cells, tunnel 3 wide with axis jittered by noise between consecutive entrances or toward center with one, chamber every ~24 axis cells, edge 1 cell Solid), `undergroundReachabilityOf` (entrance reached when its surface cell lies in start component; cave cells reached via component with landing of reached entrance; pockets), `under` row in `style_render`.

**Criterion 1: met** (`caveEntrances` 0: Seed 1729 snapshot and 17 core tests unchanged). **Criterion 2: met** (new `test_procedural_underground`: entrances on Rock, at open ground, 3 from edge, at separation ≥ 24, deterministic, `valid()`; cave of two entrances in 40×20: both landings 21 Floor cells, edge Solid, one component, deterministic, different seed gives different cave, no entrances gives Solid only, one entrance gives landing and tunnel in one component; validator on manually built 40×20 grid with lake: only entrance on start side reached, two disks 42 cells / 2 components / 21 reached / 1 pocket, after adding 15-cell tunnel 57 cells / 1 component / all reached; mismatched sizes rejected; comparison with independent search on 80 random grid pairs: 80 of 80). **Criterion 4: met** (18 core tests, both G1 probes PASS after `rme` rebuild; two runs byte-identical).

**Criterion 3: met.** Seeds 1001–1032, side 512, profiles v10, `caveEntrances` 4, `buildingDensity` 30 / 40, `pathLength` 40 / 80, `fordLength` 48, `repairReach` 1 (m06 / m19): entrances median 13 [8..16] / 16 [15..16] (record: up to 16, since 4 × 4); all seeds have entrances (32 of 32); cave cells per 65,536: median 1518 / 2364; in seeds with ≥ 2 entrances (32 of 32) **cave has one component and 0 pockets in 32 of 32, and all entrances reached from start in 32 of 32** (required ≥ 90 %; rock is passable, so its edges are reachable with the rest of land).

**Caveats (from record):** cave is tunnels and chambers without water, without side roads and dead ends (except one entrance); entrances only at rock edge (not in buildings, not stairs); cave exit is the same point (x, y); reachability repair from iteration 23 does not know Z entrances (32 of 32 result follows from rock passability, not repair); nothing drawn or translated to items yet (iteration 28: entrance item from doodad brush, cave ground brush, Preview, Validate, Commit and Undo for two floors, window); multi-floor mountains (several floors below) out of scope. **Not verified:** cave appearance (not on map yet), performance (not measured separately), behavior with masks and protection, cave under entrances in very small areas.

## Iteration 28 — Z entrance adapter, cave and ramp exit (2026-11-09): PARTIAL, criteria 1–4 and 6 met, 5 with divergence, GUI assessment (7) awaits user

Criteria recorded before implementation ([G5_METRICS.md](G5_METRICS.md), “Iteration 28”, commit `97a2094`; grep confirmed). User decisions: two history steps (cave, then surface), refusal of all when lower floor occupied, exit as ramp at landing, entrance “cave enter”. Commits: `597be12` (core `placeCaveExits`), `cb2f3cd`…`16e76f2` (adapter, Commit, Undo, floor preview, probe), `254a851` (review fix: full history and empty cave), `5d0a149` (window fields).

Implemented: core `placeCaveExits` (`ExitShape`, `CaveExit`, `CaveExitPlan`; first match in order (dx² + dy², y, x, variant), entrance without exit skipped), option `caveExitShapes=ramp` in `style_render` (fields `exits`, `skipped` appended to `under` row only with this option); profile fields `caveEntrance`, `caveExit`, `caveGround` (at end of struct), plan layers `entrances`, `exitTops`, `exits`, `caveFloor` (hashed only when non-empty); second preparation unit `PreparedTerrain::cave` (floor z + 1, own snapshot, plan and preview, shared budget); errors `LowerFloorOccupied` and `NoLowerFloor`; Commit in two steps via `ActionQueue::beginTerrainPair` / `finishTerrainPair` (rollback first step on second error, publish changes after both); floor selection in window preview; “Underground” tab in window.

**Criterion 1: met.** Seed 1729 100×100 = 17159159863505677800 snapshot unchanged; with `caveEntrances` 0 `style_render` result byte-identical as before (both profiles, 32 seeds); old `--surface` probe cases unchanged except time fields and memory estimates (these changed because structures `GroundProfile`, `GroundPlan` and `PreparedTerrain` grew; **divergence**, snapshots and change counts unchanged).

**Criterion 2: met.** Manual tests `placeCaveExits` in `test_procedural_underground` (order, blocked landing, close entrances, `occupied`, variant order, bad entrances, interruption) and comparison with independent search: 80 of 80.

**Criterion 3: met.** Seeds 1001–1032, side 512, profiles v10, `caveEntrances` 4, `fordLength` 48, `repairReach` 1, `pathLength` 40 / 80, `buildingDensity` 30 / 40, `caveExitShapes=ramp`:

| profile | entrances | entrances with exit | total |
|---|---|---|---|
| m06 | 13 [8..16] | 13 [8..16] | 419 / 419 = 100 % |
| m19 | 16 [15..16] | 16 [15..16] | 510 / 510 = 100 % |

Two runs byte-identical. **Divergence:** `style_render` does not generate vegetation, so measurement does not exclude vegetation fields (`occupied` empty); with “Vegetation” enabled in window percentage may be lower, not measured.

**Criterion 4: met.** Probe `--surface`: `SURFACE_CAVE_E2E_PASS	128	entrances=5 exits=5 skipped=0 caveCells=882` (Seed 1729, side 128, `caveEntrances` 20, `rockPermille` 200, `buildingDensity` 10, brushes “cave enter”, “ramp”, “cave”). Checked: preview on z and z + 1, entrance item only on entrances with exit, ramp at E and tail at E + t, top tile at E, cave ground exactly on Floor, no ground on Solid, nothing outside rectangle; Commit = two steps and map equals previews; Undo once (surface back, cave stays), Undo twice (both floors as before), Redo twice; forced step 2 error (history limit 1) leaves both floors and history as before; refusals (occupied tile on z + 1 also with mask, z = 15, unknown or non-doodad entrance brush, “small rocks” as exit, non-ground as cave ground, partial profile); shared budget (1 byte and 1 change short = refusal); changed cave plan rejected by validation; entrance and ramp items do not block per client data; plan with cave has different snapshot. Added after review: Commit with full history up to step limit trims oldest steps instead of refusing; empty cave (no rock) gives one history step. **Divergences:** (a) top tile of 10.98 ramp (“stairs” 459) is ground with floor change down, so at E replaces surface ground (task for executor required non-ground, criterion did not specify); other grounds as top tile are rejected; (b) borders: at E none, around E few RME border elements (counted in probe, appearance not assessed); (c) empty cave gives one step instead of two (empty Undo step would do nothing).

**Criterion 5: met with divergence.** Fields “Cave entrances” (0..20), “Z entrance brush”, “Cave exit brush”, “Cave ground brush” (default “cave enter”, “ramp”, “cave” when client has them) are in **separate “Underground” tab**, not Terrain tab (each window section is a separate tab); active with Surface with buildings or camps, brushes only when entrances > 0; messages for new refusals. GUI appearance and operation not checked (criterion 7); value persistence between window openings not checked.

**Criterion 6: met.** 18 of 18 core tests, both G1 probes PASS after `rme` rebuild, `SUITE_PASS` in two runs; only stderr is known SetFocus warnings.

**Not verified:** operation in game (where server places player after `floorchange=down` and after ramp), entrance appearance (variant "cave enter" chosen per cell, may be rotated wrong way), cave and border appearance in RME, window fields in GUI, percentage of exits with vegetation enabled, clients other than 10.98.

**Fix after user GUI test (2026-11-10):** "Cave exit brush" list was empty with ramps (window took only doodads with single items, ramps are composites), so exit field remained empty. Window now builds separate list from same control as adapter (`resolveCaveExitBrush`); probe checks that in 10.98 it is exactly "bamboo ramp", "dirt ramp", "pyramid ramp", "ramp", "snowy ramp" (`exitBrushes=5`). Probe skipped window, so error was not detected.

## Iteration 29 — cave walls, entrance style, exits beyond rock (2026-11-10): PARTIAL, criterion 3a not met for RockFace, rest met, GUI assessment (7) awaits user

Criteria recorded before implementation ([G5_METRICS.md](G5_METRICS.md), “Iteration 29”, commit `41924a7`; grep confirmed). Commits: `277ebf4`, `eb9bdb1` (core: `CaveEntranceStyle`, `caveEntranceAllowed`, Rock removed from exit fields), `46b83ec` (adapter: `caveWall`, hole as ground), `17cec10` (window: “Entrance style”, “Cave wall brush”), `ab7bb9c` (probe for both styles).

**Criterion 1: met.** Seed 1729 100×100 = 17159159863505677800 snapshot unchanged; with `caveEntrances` 0 results unchanged; old probe cases unchanged except time fields and memory estimates (+112 bytes, new struct fields; **divergence** as in iteration 28). `SURFACE_CAVE_E2E_PASS` row changed as expected.

**Criterion 2: met.** Manual tests of both styles (with refusals), rejection of P or P + d on Rock, comparison with independent search for both styles, determinism; 18 of 18 core tests.

**Criterion 3: (a) not met for RockFace, (b), (c), (d) met.** Seeds 1001–1032, side 512, options from iteration 28:

| profile, style | entrances | with exit | percentage | style rule / exits beyond Rock |
|---|---|---|---|---|
| m06 RockFace | 3 [0..7] | 3 [0..7] | 100 % | 100 / 100 |
| m06 FloorHole | 16 [16..16] | 16 [16..16] | 100 % | 512 / 512 |
| m19 RockFace | 6 [1..11] | 6 [1..11] | 100 % | 174 / 174 |
| m19 FloorHole | 16 [16..16] | 16 [16..16] | 100 % | 512 / 512 |

RockFace gives median 3 (m06) and 6 (m19) entrances when ≥ 8 required: the rule “north, west and east are rock, south open” is seldom met on the rock strips of these profiles. Criterion unchanged; decision on rule belongs to user. Two runs byte-identical.

**Criterion 4: met.** `SURFACE_CAVE_E2E_PASS	256	style=rockface entrances=18 exits=18 skipped=0 caveCells=7146 wallCells=3821 exitBrushes=5` and `style=floorhole entrances=20 exits=20 skipped=0 caveCells=5935 wallCells=3108 exitBrushes=5` (brushes “cave enter” / “cave hole”, “ramp”, “cave”, “mountain”). Checked: style rule, entrance item or hole ground, P and P + d beyond Rock, wall ground exactly on Solid with Floor neighbor, no ground on remaining Solid, nothing outside rectangle, Commit, Undo, Redo, forced step 2 error, new refusals, different style snapshots.

**Criterion 5: met (without GUI assessment).** “Underground” tab: “Entrance style” (Rock face / Floor hole), “Z entrance brush” list switches between doodads and grounds, “Cave wall brush” (default “mountain”).

**Criterion 6: met.** 18 of 18 core tests, both probes PASS, `SUITE_PASS` in two runs (executor worktree and main checkout after canonical `build_ninja.bat`), same probe rows. **Divergence:** executor built in worktree workaround (no memory and disk with canonical build); canonical build passed in main checkout.

**Not verified:** appearance of “mountain” walls in cave (faces only from east and south), direction of “cave enter” variants, appearance of “cave hole” on grass, behavior in game, clients other than 10.98.

## Iteration 30 — exit ramp under entrance, no hole on surface (2026-11-10): criteria 1–5 met, GUI assessment (6) awaits user

Criteria recorded before implementation ([G5_METRICS.md](G5_METRICS.md), “Iteration 30”, commit `2d4064e`; grep confirmed). Commits: `ce11d56` (core: exit only P = E), `63e2c7d` (adapter: no “top” ramp item, `exitTops` layer removed), `4ec35cb` (fix: ramp tail may lie on L).

**Deviation from definition (recorded in G5_METRICS.md as fix before measurement).** The “tail ≠ L” condition was a specification error: in all 10.98 ramp variants, the lower half of the ramp lies on L. Executor implemented the rule unchanged and got 0 exits (`SUITE_FAIL`); after fix, tail may lie on L (still Floor and unreserved).

**Criterion 1: met.** Seed 1729 100×100 = 17159159863505677800 snapshot unchanged; cases without cave unchanged except memory estimates (removed `exitTops` vector). `SURFACE_CAVE_E2E_PASS` rows unchanged in numbers (layout changed, not numbers).

**Criterion 2: met.** Manual tests of both styles with refusals (target on rock, water, decor, campfire; L not Floor; tail not Floor), tail-on-L case accepted, true 10.98 ramp geometry accepted; 160 comparisons with independent search (80 grids per style); determinism; 18 of 18 core tests.

**Criterion 3: met.** Seeds 1001–1032, side 512, options from iteration 29:

| profile, style | entrances | with exit | percentage | P = E / D open / L Floor |
|---|---|---|---|---|
| m06 RockFace | 3 [0..7] | 3 [0..7] | 100 % | 100 / 100 / 100 |
| m06 FloorHole | 16 [16..16] | 16 [16..16] | 100 % | 512 / 512 / 512 |
| m19 RockFace | 6 [1..11] | 6 [1..11] | 100 % | 174 / 174 / 174 |
| m19 FloorHole | 16 [16..16] | 16 [16..16] | 100 % | 512 / 512 / 512 |

Two runs byte-identical (output and images). RockFace entrance median still below 8 (iteration 29 criterion 3a, D24), not assessed here.

**Criterion 4: met.** `SURFACE_CAVE_E2E_PASS	256	style=rockface entrances=18 exits=18 skipped=0 caveCells=7146 wallCells=3821 exitBrushes=5` and `style=floorhole entrances=20 exits=20 skipped=0 caveCells=5935 wallCells=3108 exitBrushes=5`. Checked: ramp exactly under entrance, no “top” ramp item on surface, entrance item or hole ground at E, ground D unchanged, cave ground on L, walls, rectangle, Commit, Undo, Redo, forced step 2 error, refusals.

**Criterion 5: met.** 18 of 18 core tests, both probes PASS, `SUITE_PASS` in worktree and main checkout after canonical `build_ninja.bat`. **Process divergence:** first run in worktree used unrebuilt core tests (`build_ninja.bat` does not build `test_procedural_*` and `style_render` targets); tests and measurement repeated after rebuilding those targets.

**Not verified:** player position on L after descent and on D after ramp entry (server behavior described from memory), appearance of ramp under entrance, behavior in game, clients other than 10.98.

## Iteration 31 — entrance above each ramp in GUI, cave without buildings (2026-11-10): PARTIAL, error (a) not reproduced, (b) fixed, GUI assessment awaits

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 31”, commit `00ddf55`. Commits: `fb36897` (probe `guiCaves`), `1008b32` (window: cave without buildings).

**Criterion 1: met.** Seed 1729 100×100 snapshot unchanged, old probe rows unchanged.
**Criterion 2: not met (not reproduced).** 64 combinations (512 rectangle at (800,1000,7), 8 seeds, both styles, 0 and 40 buildings, rock “rocky ground” and “mountain”, vegetation, paths): 4660 entrance–ramp pairs, each ramp has entrance above it and vice versa. Cause of GUI symptom unknown; no production fix.
**Criterion 3: met.** With 0 buildings, cave fields active, requirement carries `caveEntrances`, cases with 0 buildings in probe pass.
**Criterion 4: met.** 18 of 18 core tests (targets rebuilt), both probes PASS, `SUITE_PASS` in worktree and main checkout. **Divergence:** executor build interrupted by full C: disk; user agreed to remove `vcpkguildtreesngle`.

**Not verified:** cause of missing entrance in user's GUI session (rectangle, seed and map state unknown), masks and protection on large area, appearance in GUI.

## Iteration 32 — plateau z − 1 with vegetation and ramps (2026-11-10): PASS in measurements and probe, GUI assessment awaits

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 32”, commit `246b922`. Commits: `693ca6c` (N step history), `52196f7` (core), `30b5ce3` (preview, commit, “Levels” tab).

**Criterion 1: met.** Seed 1729 snapshot unchanged; old numbers and probe snapshots unchanged. **Divergence:** memory estimates in old probe rows increased (+296 B, +72768 B in PREPARATION_SIZE_PASS).
**Criterion 2: met.** PLATEAU_CROSS_CHECK 60/60.
**Criterion 3: met.** m06: plateaus 4 [2..10], Top 18837.5, ramps 11 [5..22]; m19: 5.5 [2..10], Top 58767, ramps 17 [5..28]. Ramp in each plateau, each Top reachable, two runs identical. **Divergence:** in tight space fewer ramps than pattern (always ≥ 1).
**Criterion 4–5: met.** History order: plateau, cave, surface. 18/18 tests, both probes, `SUITE_PASS` in worktree and main checkout.

**Not verified:** appearance in GUI, movement in game, loading in original RME.

## Iteration 33 — mountains without walls, floors z − 2 and z − 3, percentage limit (2026-11-10): PASS in measurements and probe, GUI assessment awaits

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 33” (`7a54575`, fixes `e0d2a76`, `250fc86`, `c3c0cdd`, `ad59acc`, `507c43e`). Commits: `71d02da` (core), `2d29a41` (preview, commit, window), test and measurement in last commit.

**Criterion 1: met.** Seed 1729 and cases without plateau unchanged; plateau case changed intentionally (strip with grass ground, not “mountain”). Old profile with `plateauEdge` loads, field is ignored.
**Criterion 2: met.** Manual grids and comparison with independent search on 60 grids (3 floors, corridors, limits).
**Criterion 3: met** (`plateauMaxPercent` 10, median [min..max] floor cells):

| Profile | z − 1 | z − 2 | z − 3 |
|---|---:|---:|---:|
| m06 | 20476.5 [19713..20905] (≈ 7.8 %) | 1894 [1597..2027] | 141.5 [0..195] |
| m19 | 25621.5 [23938..26201] (≈ 9.8 %) | 2406 [2152..2580] | 184.5 [0..238] |

Ramp in each plateau, all floors reachable from surface, limits (f) and (g) met, two runs identical. Minimum offset of higher floors 4 cells (fix approved by orchestrator).
**Criteria 4–5: met.** History order: highest floor, lower, cave, surface. Refusal when floor occupied and z range over 0. 18/18 tests, both probes, `SUITE_PASS` in worktree and main checkout.

**Not verified:** appearance in GUI, movement in game, loading in original RME.

## Iteration 34 — mountain caps on own slope (2026-11-11): PASS in measurements and probe, GUI assessment awaits

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 34”, `8b7a14f`. Commits: `2aedc0b`, `74f7614`, `2eed650`, `7d65381`.

**Criterion 1: met.** Seed 1729 and all cases without mountains unchanged (comparison with iteration 33 no differences except time and memory). Mountain cases changed intentionally (whole cap, ramp openings, no corridors).
**Criterion 2: met.** PLATEAU_CROSS_CHECK 60/60, PLATEAU_LEVELS_LIMIT_CROSS_CHECK 60/60.
**Criterion 3: met** (median [min..max]; vegetation: share on cap grass / on surface grass):

| Profile | Floor | Caps | Cells | Ramps | Vegetation cap / surface |
|---|---|---:|---:|---:|---:|
| m06 | z − 1 | 4 [2..12] | 20884 | 12 | 11.5 % / 11.7 % |
| m06 | z − 2 | 1.5 [1..4] | 1907 | 4.5 | 10.8 % / 11.7 % |
| m06 | z − 3 | 1 [1..2] | 155 | 1 | 10.4 % / 11.7 % |
| m19 | z − 1 | 5 [1..10] | 18080 | 12.5 | 38.3 % / 37.6 % |
| m19 | z − 2 | 1.5 [0..5] | 1687 | 4 | 38.1 % / 37.6 % |
| m19 | z − 3 | 1 [0..2] | 113.5 | 1 | 35.9 % / 37.6 % |

Slope under each cap, everything reachable, limits, vegetation ≥ 88 % surface, two runs identical.
**Criteria 4–5: met.** Probe: “mountain” ground under each cap after autoborder, ramp openings only at mountains. 18/18 tests, both probes, `SUITE_PASS` in worktree and main checkout.

**Not verified:** appearance in GUI (slope corners, drawing offset), movement in game.

## Iteration 36 — reachability validator across all floors, G5 gate measurement (2026-11-12): validator PASS, **G5 gate FAIL**

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 36” (`4b56caf`, mask completion `2154da9`). Commits: `1ba60ef` (core `levelReachabilityOf`), `fe0cc50` (measurement, `docs/procedural/smoke/g5_measure_levels.py`), `84a5b66` (adapter: mask from resolved items, window: status line, warning on Place, seed retries).

**Model.** Surface rock outside caps passable (10 of 10 “rocky ground” variants without blocking flag in 10.98); “mountain” slope under cap impassable. “small rocks” has mixed variants, so adapter counts mask per cell. **Divergence:** mask includes blocking items of outlines, walls and ground, not just decor (otherwise model would be map-incompatible). `style_render` uses cautious rule (class blocks when any variant blocks).

**Criteria 1, 2, 3, 5: met.** Seed 1729 snapshot and old probe rows unchanged (except memory estimates). 100 comparisons with independent search of five floors. Probe: model matches map on five floors, Z edges with floorchange items, seed retries in window. 18/18 tests, both probes, `SUITE_PASS` in worktree and main checkout.

**Criterion 4 (G5 gate): (a) NO, (b) NO, (c) NO, (d) yes.** Full reachability: m06 0 of 32, m19 0 of 32 (no retries and with 8 attempts). Z edges passable both ways: m06 692 / 774, m19 708 / 748.

| Profile | Doors | Camps and ruins | Cave entrances | Cave exits | Cap cells | Cave cells |
|---|---:|---:|---:|---:|---:|---:|
| m06 | 2835 / 3086 | 1197 / 1266 | 1 / 83 | 75 / 83 | 541265 / 593789 | 54944 / 57163 |
| m19 | 3248 / 4102 | 1254 / 1523 | 57 / 101 | 89 / 101 | 109139 / 322370 | 92943 / 106611 |

**Causes (diagnostics on seed 1001, generator unchanged):** (1) reachability repair from iteration 23 uses old model (rock passable, decor ignored), so blocking vegetation divides land — in m19 main cause; (2) slopes under caps (from iteration 34) cut paths that previously went over rock — in m06 main cause; (3) cave entrances lie on rock edge, which from iteration 34 is a slope. Disabling decor and slope blocking in model restores full reachability in both profiles.

**Not verified:** movement in game, diagonal steps, appearance of status line and warning in GUI, 2000×2000 area with validator.

## Iteration 37 — multi-floor repair and bridges (2026-11-12): implementation and probes PASS, **G5 gate FAIL; D22 remains open**

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 37”. Implementation commits: `567269f` (core), `fc1f91b` (adapter, window, probe and measurement). User approved replacing water with bridge ground from wall brush “bridge”; criteria unchanged in this branch.

**Implementation.** `repairLevelReach` assesses complete plan after each repair. Dijkstra: passable 1, blocking decor 3, water 6; state contains length of next water segment, bounded by `fordLength`. Ties by cell index (y, x). Slopes, walls, Preserve and cave Solid are forbidden. No path: remove cap and higher caps supported by it or surface POI; limit 64, then `unrepaired`. Caps created before entrance filter, cave after. Adapter uses resolved decor and ground variants, measurement cautious rule.

**Bridge.** Direction recorded by core from path axis, tie-break horizontal preferred. Adapter selects first variant with positive chance from appropriate horizontal/vertical node of loaded brush. Without brush, sand remains. **Geometric divergence:** crossing widens water by one cell on each side of axis (water only, clipped to rectangle). Single-cell bridge had passable ground but diagonal sea borders blocked both abutments. Widening preserves normal autoborder without removing border items. Bridge cell count below means bridge ground cells including this widening.

**Tests and probes.** 18/18 core targets and both probes PASS, `SUITE_PASS`, after canonical Windows x64 build and separate rebuild of tests/style_render (two tasks). Seed 1729 100×100 = 17159159863505677800 snapshot. Independent Dijkstra: 120 random grids; independent five-floor crossing: 100 plans. Manual grids: decor, bridge, ford, wall/slope ban, remove cap with higher and return rock, remove POI, entrance filter under cap, limit 64. Probe checks both bridge and ford directions, removed decor, Commit/Undo/Redo and mask–map agreement in plan with cave and cap.

**G5 gate.** Seeds 1001–1032, side 512, iteration 36 settings plus `repairReach=1`. Each variant run twice; stdout and stderr byte-identical. Median [min..max]:

| Profile | Attempts | Full seeds | Passable Z edges | Repairs | Bridge cells | Removed caps | Removed POI | Removed POI share |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| m06 | 1 | 32/32 | 610/610 | 19 [9..26] | 4 [0..107] | 0 [0..0] | 0 [0..1] | 0.04595 % |
| m06 | ≤8 | 32/32 | 610/610 | 19 [9..26] | 4 [0..107] | 0 [0..0] | 0 [0..1] | 0.04595 % |
| m19 | 1 | 1/32 | 668/668 | 64 [13..64] | 0 [0..18] | 0 [0..0] | 0 [0..2] | 0.07035 % |
| m19 | ≤8 | 7/32 | 589/589 | 64 [13..64] | 0 [0..18] | 0 [0..0] | 0 [0..1] | 0.03511 % |

Criterion 4(a) and 4(b) **not met for m19**; 4(c), 4(d), 4(e) met for both profiles. Repair does not pass gate by deletion: removed POI total respectively 2, 2, 4 and 2.

**Causes and examples.** m19, seed 1001: after 64 repairs (76 decor deleted) reachable caps 4351/9677; on z−1, z−2 and z−3 remain 395, 52 and 15 pockets. First reported cap target: (196,523,4). Cautious vegetation rule gives many small components and exhausts limit. m19, seed 1003: 19 repairs, cave 0/334, no placed entrances/exits; no Floor path to start component. Repair reports `unrepaired`, does not cut through forbidden Solid or delete whole cave. Without retries `unrepaired` in 31/32 m19 plans, with retries in 25/32.

**Other divergences and limits.** For old result compatibility entrance filter enabled together with new repair; `repairReach=0` preserves historical entrances under cap. In old probe matrix `SURFACE_CAVE_GUI_PASS` repairReach option was 1, so its results change intentionally; with blocking “mountain” ground repair may delete all entrances and report `unrepaired`. Old cases without repair preserve snapshots and numbers except memory estimates (new struct fields). Adapter repairs before autoborder, then checks full mask of resolved tiles and reports remaining blocks; does not retry search on final border mask. In complex probe case final state is `unrepaired=true`, with model map-compatible.

**Not verified:** appearance of bridges, fords and mountains in GUI, movement in game, original RME, other clients, 2000×2000 repair. Full report, raw probe rows and measurements in uncommitted `local-data/I37_IMPLEMENTATION_REPORT.md`.

## Iteration 38 (2026-11-12)

Commits `504236b`, `bd6b921`. Cap vegetation is permitted only if it does not cut a connected component (exact articulation point test, within 64-repair limit). Cave entrances selected before caps, with 3×3 indentation in C_1. Cave components with no placed entrance are replaced with Solid. Build, 18/18 tests, both probes PASS, `SUITE_PASS` in main checkout. Seed 1729 100×100 = 17159159863505677800 unchanged.

| Profile | Full seeds (1 attempt) | Z edges | Repairs | Deleted POI | Median entrances | Cap flora z−1/z−2/z−3 | Deleted caves |
|---|---:|---:|---:|---:|---:|---:|---:|
| m06 | 32/32 | 774/774 | 3 [0..7] | 0.04510 % | 2 | 97.4 / 93.0 / 89.0 % | 0 |
| m19 | 32/32 | 750/750 | 15 [6..23] | 0.06986 % | 3 | 94.7 / 92.0 / 80.1 % | 2 |

Criteria (a), (b), (c), (d), (e), (g) met; two runs byte-identical. **(f) not met:** median entrances 2 and 3 (required ≥8). The cause is upstream of caps: RockFace selector from iteration 29 (Rock from three sides, open south, budget 6800 attempts, spacing 24) finds few candidates, and exit fitting rejects entrances with vegetation at the target location. Seed m06 1013 has no entrances already at `repairReach=0`. G5 gate: **FAIL on (f) only**.

**Not verified:** appearance in GUI, gameplay. Report: uncommitted `local-data/I38_IMPLEMENTATION_REPORT.md` in worktree.

## Iteration 39 (2026-11-12)

Commits `bee437a`, `4b4c75a`. After autoborder, the adapter builds a blockage mask from actual items and repairs again (combined limit 64, via DrawOperations, in existing Undo steps). Cave entrance selector finds more candidates. Build, 18/18 tests, both probes PASS, `SUITE_PASS` in main checkout; `FINAL_MASK_REPAIR_E2E_PASS` (seed 1212, Undo/Redo).

| Profile | Full seeds (1 attempt) | Z edges | Repairs | Deleted POI | Median entrances | Cap flora z−1/z−2/z−3 |
|---|---:|---:|---:|---:|---:|---:|
| m06 | 32/32 | 1808/1808 | 2 [0..5] | 0.04040 % | 18.5 | 97.5 / 93.2 / 86.2 % |
| m19 | 32/32 | 2737/2737 | 14.5 [6..22] | 0.04465 % | 33 | 94.7 / 91.5 / 79.2 % |

Criteria (a)–(g) met, two runs byte-identical: **G5_GATE_PASS** for reachability and levels. Diagnosed blocker for a fallback case: “shallow water” edge (sea border, blocking item) on grass, invisible to pre-autoborder repair; now removed.

**Not verified:** exact case from screenshot (91 places, (1149,1026,7)) not reproduced because remaining settings were unknown; appearance in GUI after water edge removal; gameplay.

## Iteration 40 (2026-11-12)

Cause: semantic Dirt remaining inside an island outside the reach of rim rings; not decoration or path. Before the fix, in corpus m06 there were 15 islands, 21 Dirt cells and 122 Grass; m19: 8 islands, 107 Grass. The core after POI and fords, before save mask, decoration, caps and repair, replaces unreachable islands without POI as Shore. The main component (largest, ties broken by row), land bridges, POI, and components touching the rectangle border or Preserve are preserved. Seed 1729 footprint unchanged: 17159159863505677800 (no such islands).

Measurement as in iteration 39, seeds 1001–1032, side 512, cautious decoration rule. Results identical without retry and with limit 8; both stdout/stderr byte-identical.

| Profile | Full seeds | Z edges | Repairs, median [min..max] | Deleted POI | Median entrances | Cap flora z−1/z−2/z−3 | Islands | Islands not Shore-only / cells other than Shore |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| m06 | 32/32 | 1808/1808 | 2 [0..5] | 0.04040 % | 18.5 | 97.5 / 93.2 / 86.2 % | 15 | 0 / 0 |
| m19 | 32/32 | 2737/2737 | 14.5 [6..22] | 0.04465 % | 33 | 94.7 / 91.5 / 79.2 % | 8 | 0 / 0 |

**G5_GATE_PASS**, criteria (a)–(g) without regression. Canonical Windows x64 build, rebuilt core targets, 18/18 tests, both probes PASS, **SUITE_PASS**. Independent DFS: 120/120 plans; manual cases: dirt, ford/bridge/path, POI, Preserve, border, largest component, other floor and bridge metadata. Old beach-width assertions and post-ford differences account for island normalization; ford probe still checks walkability, connectivity and Undo/Redo (443 ford cells, 7 connected components, zero blockages). Working-memory estimate accounts for island classification. Full test and probe results and limitations: uncommitted `local-data/I40_IMPLEMENTATION_REPORT.md`.

**Not verified:** exact user case (no settings), appearance in GUI, gameplay, clients other than 10.98, loading in original RME, profiling large areas.

## Iteration 41 — distant cave exits and corridors with doors (2026-11-12): PASS in measurements and probes

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 41”, unchanged. Core `561cf88`, tight corridor fitting `071b4ad`, distance preservation after component joining `c7cb069`, adapter/GUI `0540d28`. `CaveExit::distant` denotes a ramp without entrance/hole on the surface. Multi-source BFS from all entrances, distance ≥24, ties broken by (y,x); short cave gets tunnel extension width 3 for 24..48 Manhattan steps. Extension joining components checks all their entrances and distance preservation for previously placed distant ramps. Corridors length 3, width 1, door in center and Solid orthogonally; pinching does not cut remaining Floor. Impossible layout with ≥2 rooms is rejected instead of returning a plan without the required corridor. `GroundProfile::caveDoor` = `stone wall`, ordinary locked door without key from client 10.98, both directions, no ID in code. Door option disabled by default; missing appropriate brush disables it with a message. Distant exits enabled by default for caves. Adapter mask treats designated cave doors as passable; remaining blockers and existing building doors retain previous behavior.

Measurement `g5_measure_levels.py --i41`: seeds 1001–1032, side 512, settings from iteration 39, both new mechanisms enabled. Separately without retry and with limit 8; results identical. Each variant twice: stdout/stderr byte-identical, stdout contains footprint of entire LevelPlan (floors, walkability, decoration, Z edges and POI).

| Profile | Full seeds | Components with distant exit | Median share | BFS mean / min | Seeds with doors | Doors with invalid neighborhood | Walkable Z edges |
|---|---:|---:|---:|---:|---:|---:|---:|
| m06 | 32/32 | 32/32 | 100 % | 174.9375 / 35 | 32/32 | 0/32 | 1840/1840 |
| m19 | 32/32 | 32/32 | 100 % | 179.125 / 91 | 32/32 | 0/32 | 2769/2769 |

Without unreachable POI and `unrepaired`; cave/cap pockets = 0. Repairs: m06 2 [0..5], m19 14.5 [6..22]; deleted caps 0, deleted POI respectively 0.04014 % and 0.04444 %. Median entrances 18.5 / 33. Cap flora relative to surface z−1/z−2/z−3: m06 97.5/93.2/86.2 %, m19 94.7/91.5/79.2 %. Islands: 15 / 8; islands not Shore-only and cells other than Shore = 0. **G5_GATE_PASS**.

Criteria status:

1. **PASS:** manual tests of separated components and short cave, rejection under caps, reservations, exit without new hole; 120 independent BFS and determinism.
2. **PASS:** median share 100 % in both profiles, every distance ≥24.
3. **PASS:** option disabled by default, brush stored in `GroundProfile::caveDoor`; probe checks both states of control availability and locked ordinary door without key. Core test checks rejection of impossible layout.
4. **PASS:** 32/32 seeds in each profile, every door with two opposite neighbors. `CAVE_FAR_EXIT_DOOR_GUI_PASS farExits=1 doors=1 maskDoors=1 undoRedo=4 brush=stone wall`: item on map, neighborhood and walkability in actual adapter mask.
5. **PASS:** 32/32 in both profiles without retry and with ≤8 attempts, all targets reachable, 100 % Z edges. Old five-floor probe: `LEVEL_REACHABILITY_E2E_PASS`, 0 divergence between model and map, floorchange 112/112.
6. **PASS:** actual Preview/Validate/Place dialog, four Undo to empty map, four Redo with identical stacks in range z=4..8; new probe marker above.
7. **PASS:** Seed 1729 100×100 = 17159159863505677800 unchanged, canonical Windows x64 build, separately rebuilt test and style_render targets (D25), 18/18 tests, both probes PASS, `SUITE_PASS`. Two measurement runs identical. Final probes: `suite-commit-20261112-220835` and `suite-surface-20261112-220835`; client data unchanged, only 99 known SetFocus warnings in surface probe, no other stderr entries.

**Decisions and deviations.** No change to criteria or thresholds. Corridor has fixed allowed length 3; target tunnel extension length measured in Manhattan metric, then mandatory re-BFS from entrances. Both surface cells (head and arrival) additionally required as open ground. Historical ramp=entrance-relation probes explicitly disable new options; memory estimates account for new fields and layers. Regression from overly broad door walkability exception detected by old probe and fixed before final run.

**Not verified:** user visual assessment, movement and door opening on game server, clients other than 10.98, original RME, 2000×2000 area with new options, all allocation failures (D3). Full report and raw results in uncommitted `local-data/I41_IMPLEMENTATION_REPORT.md` and `local-data/i41-measurement/`.

## Iteration 42 — mountain passes (2026-11-12): PASS in measurements and probes

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 42”, unchanged. Core: `4c8fc93`, adapter/GUI: `c18c7e7`. New `passes` 0..8 (default 0), “Mountain passes” control in Levels. Massif: 4-connected Rock, ≥400 cells. Opposite ends of cross-sections every 3 cells, then two BFS: bypass and traverse through entire allowed massif; bypass ≥2× traverse. Dijkstra: step `1+4*number_of_caps`, turn +6, at least 3 straight steps between turns, ties (y,x). Pass width 3, no decoration; cutting all caps with additional spacing cell. After each cut, re-select entrances and rebuild caps/ramps with existing selectors and previous field minimum. Without Path brush adapter selects Dirt. After autoborder removes blocking borders/decoration via DrawOperations and checks entire pass.

Measurement `py -I docs/procedural/smoke/g5_measure_levels.py --i42`: seeds 1001–1032, side 512, settings from iteration 41, both profiles and passes=0/4. Each variant twice without retry and with limit 8; stdout/stderr byte-identical, footprint of entire LevelPlan in stdout. Results without retry and with ≤8 attempts identical:

| Profile | passes | Full seeds | Passes median / sum | Max traverse / bypass | Pass blockages / under cap / small caps | Walkable Z edges |
|---|---:|---:|---:|---:|---:|---:|
| m06 | 0 | 32/32 | 0 / 0 | — | — | 1840/1840 |
| m19 | 0 | 32/32 | 0 / 0 | — | — | 2769/2769 |
| m06 | 4 | 32/32 | 4 / 116 | 0.5 | 0 / 0 / 0 | 1878/1878 |
| m19 | 4 | 32/32 | 4 / 92 | 0.5 | 0 / 0 / 0 | 2809/2809 |

**G5_GATE_PASS** for both passes settings, without unreachable targets or `unrepaired`. Preserved criteria from earlier iterations: median entrances ≥8, cap flora ≥70% of surface, deleted POI ≤2%, unreachable islands Shore-only, distant exits and cave doors. Category, pocket and repair details in `local-data/i42-measurement/summary.json`.

Criteria status:

1. **PASS:** manual massif, cap cutting, field minimum, ramp on pass deleted and re-selected; probe confirms walkable ground, Dirt without Path brush and empty space above pass.
2. **PASS:** BFS of actual routes, Dijkstra through massif, count limit, default 0; count=9 rejected, cancellation and determinism in tests.
3. **PASS:** median 4 in both profiles; each final route ≤0.5 bypass before given cut (required ≤0.6).
4. **PASS:** zero blockages, zero cap coverage and zero cap components below minimum; probe checks 261 cells in final adapter mask and on saved map.
5. **PASS:** all 32 seeds, all POI categories and Z edges reachable, passes=0 and passes=4, without retry and with ≤8 attempts.
6. **PASS:** `MOUNTAIN_PASS_GUI_PASS cells=261 farExits=1 doors=1 maskDoors=1 undoRedo=4`; actual Preview/Validate/Place dialog, four Undo to empty map and four Redo with identical stacks z=4..8. No items above pass at z−1, z−2 and z−3. Additionally Dirt without Path brush with repair disabled.
7. **PASS:** canonical Windows x64 build, separate rebuild of style_render and 18 targets (D25), 18/18 tests, both probes PASS, **SUITE_PASS**. Seed 1729 100×100 at passes=0: **17159159863505677800**, unchanged. Final probes: `suite-commit-20261112-225322` and `suite-surface-20261112-225322`; client data unchanged, 100 known SetFocus warnings in Surface probe, zero other stderr entries.

Additional passes=0 compliance check: independently compiled tool and core sources from `af333e6` in uncommitted local-data; entire stdout and stderr for both profiles, 32 seeds and retry=0/8 identical to new tool (`PASSES_ZERO_BASELINE_BYTE_IDENTICAL`, `local-data/i42-baseline/comparison.json`).

**Decisions and deviations.** Criteria, thresholds and measurement parameters unchanged. Bypass measured before specific cut on surface floor, without Z shortcuts; final route on levelReachabilityOf model after decoration and repair. Selectors re-select entire list of entrances/ramps, not just element on pass. At passes=0 new field bytes added to memory estimates; old footprints and semantic results remain unchanged.

**Not verified:** user visual assessment, movement on game server, clients other than 10.98, original RME, 2000×2000 area with passes, full profiling and all allocation failures (D3). Full report: uncommitted `local-data/I42_IMPLEMENTATION_REPORT.md`.

## Iteration 43 — floor and stairs in buildings, debt D27 part 1 (2026-11-13): PASS in measurements and probes

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 43”, unchanged. Core `c4edf9a`, adapter/GUI `0030c4d`, probe and measurement `8a12eef`. New fields `SurfaceTuning::buildingStoreys` (disabled by default) and `upperFloorPercent` (0..100, default 30); in Buildings tab: “Upper floors”, “Upper floor share (%)”, “Stairs brush”. Qualifies: building with doors, rectangular Floor outline and interior ≥ 16 cells (ruins and camps do not). One draw per building (separate stream), rows by first cell. Upper floor z − 1: same outline, floor from building floor brush, walls from wall brush, no doors. Collision (cap, landing, ramp opening, pass or other floor within distance ≤ 1) leaves building single-floor. Stairs: ramp shape (cell S with floorchange item, tail, entry cell past tail), S at wall parallel to direction, arrival S + direction inside; ground floor interior without S and tail connected to doors; random choice among valid positions. **Stairs brush: doodad “ramp”** from client 10.98 (`GroundProfile::buildingStairs`, shapes like cave exits: ramps 1388–1395 with tail, at z − 1 opening “stairs” with floorchange down), no IDs in code. Client 10.98 has no doodad for single house stairs with part on upper floor (“wooden stairs”/”stairs” are RAW items only). Without brush option disabled with message. In `levelPlanOf` floor interiors belong to cap's floor or separate z − 1 floor, new POI category `Storey`, Z edge S ↔ arrival. Repair removes unreachable floor or floor of deleted building (`removedStoreys`).

Measurement `py -I docs/procedural/smoke/g5_measure_levels.py --i43`: seeds 1001–1032, side 512, settings from iteration 42 (passes = 4), option disabled and enabled with default 30 %. Each variant twice without retry and with limit 8; stdout/stderr byte-identical, results with and without retry identical.

| Profile | Floors | Full seeds | Qualifying / with floor | Share (median) | With stairs | Collisions at selection / deleted by repair | Cells under cap/pass | Storey reachable | Walkable Z edges |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| m06 | dis. | 32/32 | — | — | — | — | — | 0/0 | 1878/1878 |
| m19 | dis. | 32/32 | — | — | — | — | — | 0/0 | 2809/2809 |
| m06 | en. | 32/32 | 2942 / 895 | 30.4 % | 895/895 | 1 / 1 | 0 | 30357/30357 | 2773/2773 |
| m19 | en. | 32/32 | 3902 / 1206 | 31.3 % | 1206/1206 | 0 / 1 | 0 | 41155/41155 | 4015/4015 |

Share per seed: m06 18.3–44.6 %, m19 21.2–41.7 %. Other categories, repairs and metrics from earlier iterations identical with option disabled and enabled (doors 3116 / 4132, deleted POI 0.0401 % / 0.0592 %, median entrances 18 / 34.5, cap flora ≥ 81.5 % of surface, passes median 4, zero blockages and coverage, distant exits 100 %, islands Shore-only). **G5_GATE_PASS.** Details in `local-data/i43-measurement/summary.json`.

Criteria status:

1. **PASS:** option disabled by default. Byte comparison of `style_render` with core and tool from `4476731` (separate compilation in uncommitted `local-data/i43-baseline/`): m06 and m19, passes 0 and 4, retry 0 and 8 — stdout and stderr identical except new line `storeys 0/0`, also with explicit `buildingStoreys=0` (`STOREYS_OFF_BASELINE_BYTE_IDENTICAL`). Floor, walls without doors, collisions: core tests and probe.
2. **PASS:** “ramp” brush in profile (`buildingStairs`), ground floor: ramp and tail on floor, free entry cell; z − 1: opening and arrival on floor. Missing brush disables option with message (probe). Stairs do not cut ground floor (checked at selection and independently in tests).
3. **PASS (core model):** every floor cell reachable from start via doors and stairs (30357 and 41155 cells); 32/32 seeds with and without option, without retry and with ≤ 8 attempts.
4. **PASS:** median share 30.4 % and 31.3 % (range 20..40 %), 100 % floors with stairs, 0 floor cells under caps and passes.
5. **PASS:** `BUILDING_STAIRS_GUI_PASS adapterStairs=4 maskCells=116 doorCells=10 committedPairs=4 storeyWalls=106 undoRedo=2 dialogFloors=2 brush=ramp`: actual adapter mask (floor and stair cells open), stair item at z and opening at z − 1 in preview and on map, floor walls, Commit, Undo to empty map, Redo with identical stacks z = 4..8; dialog: switch, message and Preview/Validate.
6. **PASS:** determinism (two runs identical), Seed 1729 100×100 = 17159159863505677800 with option disabled (core test), new core tests (`BUILDING_STOREY_CORE_PASS seeds=60`: manual grids, collision with cap, shared floor with cap, repair, rejections, cancellation, 60 generated settlements with independent 3D check), canonical Windows x64 build, separate rebuild of style_render and 18 targets (D25), 18/18 tests, both probes PASS, **SUITE_PASS** (`suite-commit-20261113-073733`, `suite-surface-20261113-073733`; client data unchanged, 101 known SetFocus warnings, zero other stderr entries).

**Decisions and deviations.** Criteria and thresholds unchanged. (a) Adapter mask, since iteration 41, treats locked building doors as blocking; therefore probe checks passage start → doors → stairs → floor on actual mask with open door cells, and validator in window for buildings with walls reports doors and floors as unreachable and Place asks to continue. For this reason probe runs in window Preview/Validate, and Commit/Undo/Redo on same result type from `prepareTerrain`. Change in building door semantics out of scope (recorded in D27). (b) Spacing 1 cell from caps, landings, openings and passes at z − 1 (criterion says collision). (c) Floor without caps gets own z − 1 unit and empty z − 1 check like for caps. (d) Option requires walls (wall brush); with option enabled, multi-floor world built also without repair, as with passes.

**Not verified:** user visual assessment (appearance of “ramp” in building, floor walls), stair movement on game server, Place in window without question about unreachable doors, clients other than 10.98, original RME, 2000×2000 area with floors, all allocation failures (D3). Full report: uncommitted `local-data/I43_IMPLEMENTATION_REPORT.md`.
### Fix after iteration 43 (2026-11-13): doors in level mask

The adapter's blockage mask treated locked building doors as blocking (exception was only for cave corridor doors), so the validator in window reported doors and building floors as unreachable. Now every door from plan (`doors`) made with door brush and unblocked is walkable on every floor; probe compares map to mask by same rule. Build, 18/18 tests, both probes PASS, `SUITE_PASS`. Gate measurements not repeated (core unchanged; cautious rule does not apply to doors). Not verified: Place in window without question about unreachable places.

### Fix after iteration 43 (2026-11-13): repair halted on ramp in sealed pocket

Report from GUI (seed 1729, Buildings 100, walls, doors, floors 30 %, “ramp”, island with rivers): “Repairs: 0 … Unrepaired. Unreachable: 5913 places (first: 1508, 1053, 7)”. Exact rectangle and Area/Terrain/Levels settings unknown, so exact case not reproduced. In searched rectangles around (1508, 1053) (512×512, 768×768, with and without vegetation) same symptom occurred at (1300, 850) 512×512: 0 repairs, “Unrepaired”, 195 unreachable cap cells. Symptom identical with floors disabled, so cause not in floors. Cap has single ramp whose bottom end (cell with ramp item at z) lies in surface strip closed by cap itself and untouched area border (`surfaceKeepClear`). No item blocks there, so nothing to clean. `repairLevelReach` (world) takes targets in order (floor, cell), so ramp bottom end at z was target before cap cells. For such target (not Floor, not cave entrance) it interrupted entire repair as “unrepaired”, leaving all further targets unhandled: doors and buildings cut off by rivers (normally deleted as “removed places”), caps, floors. Thus we explain 0 repairs and 0 deleted places with thousands of unreachable places in report (this is inference, not reproduction).

Fix (`18113e6`): when such target is bottom end of cap's first ramp, repair removes that ramp. Cap without access deleted by next pass of existing cap deletion (`removedCaps`). Criteria of iterations 36–40 and 43 unchanged, validation not disabled. Other targets without legal repair still end “unrepaired”. Core test “ramp into sealed pocket” in `test_procedural_reachability` (without fix FAIL). Probe `STOREY_SEALED_RAMP_REPAIR_PASS` (`0bb027f`, required in `g5_run_suite.py`): seed 1729, (1300, 850) 512×512, floors enabled, all reachable, 2 repairs, 1 deleted cap, 3442/3442 floor cells. Build, 18/18 tests, both probes PASS, SUITE_PASS, Seed 1729 100×100 = 17159159863505677800 unchanged. Same rectangle after fix: with vegetation 21867/21867, without floors 20050/20050. 768×768 at (1000, 600) with vegetation: 49873/49873.

**Not verified:** exact GUI case (needed: X, Y, width, height, style profile, path and ford lengths, passes, mountain levels, vegetation, attempt count); Place in window; gate measurements (not repeated); analogous case of cave exit landing in sealed pocket (still “unrepaired”).

## Iteration 44 — impassable rock in profile m19, GUI case (2026-11-13): PASS

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 44”, unchanged. Core `80e4de6`, adapter `8839e19` and `51263b7`, measurement tools `9cfd432`, probe `69b3468`.

**Note on earlier m19 results (iterations 39–43).** All gate measurements for m19 from iterations 39–43 (`style_render`, offline model) counted with passable rock: m19's rock brush “mountain” has impassable ground (919) in client 10.98, which the offline model did not see; cautious rule covered only decoration. Those m19 numbers (e.g., 32/32, 4132 doors in iteration 43) thus describe terrain where rock is passable, not actual map in GUI. m06 (“mountain top ground”) is passable and not affected.

Changes: `SurfaceTuning::rockBlocks` (disabled by default; outside profile). Adapter sets it in preparation and validation from client flags of rock brush ground variants (`withRockPassability`; no caller provides it manually). Core with `rockBlocks`: buildings, camps, ruins and cave entrances only on land connected to start (`startLandOf`, BFS with doors/breaks, rock closed; building must touch that land, entrance — have field before it), cap ramps only on that land (also with passes and no level world), wall entrance walkable in `levelPlanOf`. Repair: target on surface without path removes in one repair all POI of its component (buildings, camps, entrances, cap ramps), `removedPois` counted as before.

Criteria status:

1. **PASS:** wall entrance with impassable rock stands on ground of profile's ground brush (`profile.brushes[1]`, base variant, no IDs in code); entrance item remains. Rock edges at entrance and field before it adapter cleans like on passes. In GUI case all Z edges walkable in adapter mask (probe, `allReached` and `walkableEdges == edges`); cave probe with `rock=mountain` (`SURFACE_CAVE_GUI_PASS`) passes.
2. **PASS:** core test `IMPASSABLE_ROCK_PASS seeds=12 doors=389 entrances=53 removedPois=15` (every door and field before entrance on start land, determinism), manual grids: POI beyond rock strip deleted, entrance with field on start land remains.
3. **PASS:** core test “one repair removes every POI of a cut-off area” (3 POI, 1 repair); existing repair tests unchanged.
4. **PASS:** `M19_ROCK_GUI_CASE_PASS floors=5 rockBlocks m06=0 m19=1 status=Reachable: 24032/24032 places on 5 floors. Seed 1729, 1 attempts. Repairs: 20, …, removed places: 20.` — exact case via actual window (fixture m19, x 1000, y 1000, 512×512, 10 entrances, 3 mountain levels, buildings 100 with walls, Normal door, floors 30 % “ramp”). Before change: 28523/33736, 64 repairs, “Unrepaired”, 5213 unreachable. Marker required in `g5_run_suite.py`.
5. **PASS:** `py -I docs/procedural/smoke/g5_measure_levels.py --i44 --rock-blocks m06=0 m19=1` (passability from probe; settings from iteration 43, passes = 4, floors 30 %), `G5_GATE_PASS`. Each variant twice, without retry and with limit 8; results identical.

| Profile | Rock | Full seeds | Walkable Z edges | Doors | Median entrances | Storey reachable | Deleted POI by repair (share) | Median repairs |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| m06 | passable | 32/32 | 2773/2773 | 3116 | 18 | 30357/30357 | 2 (0.04 %) | 2 |
| m19 | impassable | 32/32 | 2508/2508 | 2929 | 17 | 28880/28880 | 0 (0 %) | 11 |

POI cut off by rock in m19 do not form during generation (2929 doors vs. 4132 in iteration 43 measurement without rock passability) and are not counted as deleted by repair. Floor share m06 30.4 %, m19 30.5 %; passes median 4, zero blockages; islands Shore-only; cap flora ≥ 0.75 of surface. Details: uncommitted `local-data/i44-measurement/summary.json`.
6. **PASS:** determinism (measurement and core test), Seed 1729 100×100 = 17159159863505677800 unchanged (`rockBlocks` disabled by default), Windows x64 build, 18/18 core tests, **SUITE_PASS** (`suite-commit-20261113-164940`, `suite-surface-20261113-164940`; 103 known SetFocus warnings, zero other stderr entries).

**Decisions and deviations.** (a) Ground under entrance: profile ground brush instead of cave ground — preview accepts only profile brush grounds on surface (cave ground gave `InvalidItem` in cave probe). (b) Fixture from iteration 39 (`FINAL_MASK_RAW_REGRESSION`) retains m19 tuning but with passable rock brush (“rocky ground”) and first seed from 1212, whose autoborder still needs repair (1215): with impassable “mountain” none of 32 seeds needed that repair anymore because POI do not form beyond rock. Impassable rock case covered by `M19_ROCK_GUI_CASE_PASS`. (c) Area repair works also with passable rock (criterion does not limit it); m06 unchanged in gate results. (d) Start land counted before passes: land joined only by pass loses POI (cautious). (e) Cave exits can join cut-off land, but filter does not account for them (cautious).

**Not verified:** visual assessment of entrance on ground in “mountain” wall and cleaned edges, movement on game server, Place/Undo/Redo in window for GUI case (probe does Preview/Validate), clients other than 10.98, original RME, wall entrance without level world (repair and passes disabled, floors disabled) — then rock edges at entrance not cleaned.

## Iteration 45 — dungeons under buildings, debt D27 part 2 (2026-11-13): PASS in measurements and probes

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 45”, unchanged. Core `690a75a`, adapter/GUI `94f4c4d`, probe and measurement `2b21fb3`. New fields `SurfaceTuning::buildingDungeons` (disabled by default) and `dungeonPercent` (0..100, default 20); in Levels tab (cave section, “Underground” from criteria): “Dungeons”, “Dungeon share (%)”, “Dungeon floor/wall/stairs brush”. Qualification as for floors (building with doors, rectangular outline, interior ≥ 16; function `qualifyingBuildings` shared by both), one draw per building from separate stream. Dungeon lies on cave floor z + 1: 2..6 rooms (interiors 4×4..9×9), each next attached to previous by simple corridor length 2..6 and width 1..2 with doors in wall of both rooms (one corridor per room side). Walls: ring (8-neighborhood) of walkable cells. Spacing: between dungeon wall ring and cave wall ring (Floor + wall strip) and other dungeons at least 2 empty cells; unconnected parts of one dungeon ≥ 3 cells; walkable cells ≥ 2 from area border and not under surface Preserve cell. No space: building without dungeon (`noSpace`). Descent: cell D in interior with D − direction at building wall and arrival D + direction inside; interior without D (and without stairs and floor tail from iteration 43) connected to doors; D nor arrival lying on stairs, tail or floor entry field. At z + 1 upstairs stand at D, tail, landing and field past tail in room 0. **Brushes (client 10.98, no IDs in code):** floor “dark tiled stone floor” (`GroundProfile::dungeonFloor`), walls “dark stone wall” with its locked normal doors without key (`dungeonWall`), descent/exit doodad “ramp” (`dungeonStairs`: at z + 1 ramps 1388–1395 with tail, on surface opening “stairs” with floorchange down as ground). In `levelPlanOf` rooms and corridors are new POI category `Dungeon` on cave floor, their doors are POI `Door`, Z edges: D (z) ↔ D (z + 1) and D (z + 1) ↔ arrival (z). Repair removes unreachable dungeon and dungeon of deleted building (`removedDungeons`).

Measurement `py -I docs/procedural/smoke/g5_measure_levels.py --i45 --rock-blocks m06=0 m19=1`: seeds 1001–1032, side 512, settings from iteration 44 (passes = 4, floors 30%, actual rock passability), option disabled and enabled with default 20%. Each variant twice without retry and with limit 8 attempts; stdout/stderr byte-identical, results with and without retry identical.

| Profile | Dungeons | Full seeds | Passable Z edges | Qualifying / with dungeon | Share (median) | Rooms (median, range) | With stairs / coherent | Collisions with cave / between dungeons / bad doors | Dungeon reached | Doors reached |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| m06 | off | 32/32 | 2773/2773 | — | — | — | — | — | 0/0 | 3116/3116 |
| m19 | off | 32/32 | 2508/2508 | — | — | — | — | — | 0/0 | 2929/2929 |
| m06 | on | 32/32 | 3795/3795 | 2942 / 511 | 17.4% | 4 (2..6) | 511/511 | 0 / 0 / 0 | 92216/92216 | 6104/6104 |
| m19 | on | 32/32 | 3420/3420 | 2759 / 456 | 16.2% | 4 (2..6) | 456/456 | 0 / 0 / 0 | 81272/81272 | 5545/5545 |

Share per seed: m06 12.0–25.3%, m19 10.2–25.0% (m19: 3 seeds without qualifying buildings — 1016, 1023, 1030 — omitted from median, like for floors). Selected no space: m06 57 of 568, m19 81 of 537; no stair position 0; removed by repair 0. With option disabled all metrics identical to iteration 44 (doors 3116 / 2929, floors 30357 / 28880, median entrances 18 / 17, removed POI 2 / 0). With enabled: remaining categories unchanged, removed POI 2 / 0 (share counted with dungeon doors in denominator), median entrances 18 / 17, passes median 4, zero blockages, islands Shore-only, vegetation cap ≥ 0.75 surface. **G5_GATE_PASS.** Details: uncommitted `local-data/i45-measurement/summary.json`.

Criteria status:

1. **PASS:** option disabled by default. Byte comparison `style_render` with core and tool from `a229aa1` (separate build in uncommitted `local-data/i45-baseline/`): iteration 44 settings, m06 and m19, retry 0 and 8 — stdout and stderr identical except for new `dungeons 0/0` category line, also with explicit `buildingDungeons=0` (`DUNGEONS_OFF_BASELINE_BYTE_IDENTICAL`). Rooms 2..6, interiors 4..9, corridors 2..6 × 1..2, doors with two opposite passable neighbors, spacing from cave and other dungeons: core tests, measurement and probe.
2. **PASS:** descent in building at wall, exit in dungeon directly below it with landing and field past tail in room 0; ground floor without opening coherent with doors, no collisions with floor stairs (core test with floors and dungeons in same buildings, 48 generated worlds, half with floors). Missing brush disables option with message (probe: wall without doors “bridge”).
3. **PASS (core model and adapter mask):** each dungeon cell reachable from start via building doors, descent and corridors (92216 and 81272 cells); gate 32/32 with and without option, no retry and ≤ 8 attempts. In probe all dungeon cells and doors open in real adapter mask, `allReached`, Z edges passable.
4. **PASS:** median share 17.4% and 16.2% (range 10..30%), median 4 rooms per dungeon, 100% of dungeons with descent and exit, 0 collisions with cave.
5. **PASS:** `DUNGEON_GUI_PASS adapterStairs=12 maskCells=2217 walls=1634 doors=70 committedStairs=9 committedWalls=1254 committedDoors=54 undoRedo=3 dialogFloors=3 brushes=dark tiled stone floor/dark stone wall/ramp`: walls from brush, locked doors without key (both directions), ramp at z + 1 and opening with floorchange down at z in preview and on map, adapter mask; through real window Preview/Validate/Place, Undo to empty map and Redo with identical stacks z = 4..8.
6. **PASS:** determinism (two measurement runs identical, core tests), Seed 1729 100×100 = 17159159863505677800 fingerprint (option disabled), new core tests (`BUILDING_DUNGEON_CORE_PASS seeds=48 qualifying=529 placed=180 rooms=683`: manual grids, cave under building, floors in same buildings, repair removes unreachable dungeon and dungeon of deleted building, refusals, cancellation, 48 worlds with independent rule and 3D reachability checks), canonical Windows x64 build, separate rebuild of style_render and 18 targets (D25), 18/18 tests, both probes PASS, **SUITE_PASS** (`suite-commit-20261113-175919`, `suite-surface-20261113-175919`; client data unchanged, 104 known SetFocus warnings, zero other stderr entries). D27 updated in TECH_DEBT.md (part 2 closed, rest described).

**Decisions and deviations.** Criteria and thresholds unchanged. (a) Dungeon requires cave enabled (cave entrances > 0): floor z + 1 is drawn by cave unit and its empty z + 1 check; without cave option is inactive with message, core places nothing. (b) “Spacing ≥ 2 cells” counted between wall rings (dungeon and cave Floor with wall strip) — two empty cells, i.e., Chebyshev distance of dungeon passable cells from cave Floor ≥ 5; same between dungeons. (c) Doors at both ends of corridor (each room entry). (d) Dungeons require buildings with walls and doors (qualification as in iteration 43). (e) Share of removed POI counted by existing script with dungeon doors in denominator; with option disabled identical to iteration 44. (f) First probe version checked walls by ground brush (`brushOf`) and gave false FAIL; fixed before commit to wall brush element set.

**Not verified:** user visual assessment (look of “dark stone wall” without tjunction/diagonal elements, “ramp” as dungeon stairs), movement on stairs and opening doors on game server, clients other than 10.98, original RME, 2000×2000 area with dungeons, dungeons with selection mask (cells outside mask excluded by Preserve, but probe does not check this), all allocation failures (D3). Full report: uncommitted `local-data/I45_IMPLEMENTATION_REPORT.md`.

### Iteration 45b — dungeon fixes after GUI test (2026-11-13): PASS

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 45b”, unchanged. Fix `98bfa08`, probe `7078bdb`.

1. **Dungeon corners — diagnosis:** wall ring is 8-neighborhood of passable cells, and adapter placed dungeon floor under each ring cell, including corners touching dungeon only diagonally. RME places pillar element (25202) or corner (25204) of “dark stone wall” brush there, which leaves most of tile exposed, so floor was visible outside the wall. Floor brush has no borders (`friend all`), so it's not a border. **Fix:** ring cell without passable side neighbor gets only wall element, no ground (`caveFloor` 5); preview validation checks this. In probe 0 tiles with dungeon floor outside outline in preview and on map (elements on corners: 25204×50, 25202×46, rest are straight sections at corridors).
2. **Descent — diagnosis:** on surface stood upper part of “ramp” doodad, ground “stairs” (459), drawn as hole, i.e., black hole. **Fix:** new profile field `GroundProfile::dungeonDescent` (ground or doodad brush, no ID in code); its ground with floorchange down replaces building ground. By default in window “gray stone stairs” (doodad from 10.98 data, full down-stairs ground; in 10.98 data down-stairs are ground-only). Ramp up in dungeon unchanged; Z edge passable in adapter mask.
3. `DUNGEON_GUI_PASS … outsideFloor=0/0 surfaceOpenings=0/0` (preview/map, zero “stairs” ground at z 7), Place/Undo/Redo as in iteration 45. Measurement `--i45` (core unchanged): `summary.json` identical to iteration 45 measurement, `G5_GATE_PASS`, both runs identical; Seed 1729 fingerprint unchanged; 18/18 tests (targets rebuilt separately), **SUITE_PASS** (`suite-commit-20261113-202336`, `suite-surface-20261113-202336`; 104 SetFocus warnings, zero other stderr entries).

**Not verified:** GUI look after fix (user assessment), “gray stone stairs” graphics direction relative to dungeon ramp, stair movement on game server, which element lands on which corner (probe counts elements, not their position).
### Iteration 45c — dungeons: stair direction, passages, building walls (2026-11-13): PASS

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 45c”, unchanged. Fixes `9bbc421` (core), `2b5224c` (adapter/window), probe `2ecad33`.

1. **Direction.** Core used any of four “ramp” doodad shapes, and “gray stone stairs” has one variant. Now dungeon uses only shapes facing north (ramp with floorchange north, tail and landing south of it), and descent by default is “stairs down” ground brush (ground with floorchange down; in 10.98 data it's pair for up-stairs facing north). Profile field `dungeonDescent` now describes stairs facing north.
2. **Passages — diagnosis:** doors stood, but in door slots of “dark stone wall” brush are items 25283–25296, which client data describes as plain “stone wall” (no door type, impassable). RME door logic placed them as “doors”, so at each corridor end full wall was visible (and adapter mask treated them as openable). **Fix:** `resolveCaveDoorBrush` accepts only items that are doors in client; “dark stone wall” is rejected (option disabled with message), default dungeon wall is “brick wall” (true normal doors).
3. **Building walls — diagnosis:** with impassable rock (m19) adapter cleans blockers at each entrance cell and field south of it (rock wall entrances, iteration 44). Dungeon down stairs are recorded as entrance cell, and field south of them is building wall — was removed as “cleanable” wall element. **Fix:** only wall entrances (entrance item on ground) are cleaned, not dungeon descents.
4. `DUNGEON_GUI_PASS … committedDoors=64 outsideFloor=0/0 surfaceOpenings=0/0 missingWalls=0 brushes=dark tiled stone floor/brick wall/ramp/stairs down` (adapter with “mountain” rock): ramps facing north in preview and on map, building wall south of each descent, all planned building walls in preview, dungeon doors are client door items, “dark stone wall” rejected in window. Measurement `--i45 --rock-blocks m06=0 m19=1`: `G5_GATE_PASS`, disabled option identical to iteration 44; enabled: share 16.3% / 17.3%, median 4 rooms, stairs and coherence 486/486 and 458/458, zero collisions and bad doors, Dungeon 90977/90977 and 83501/83501. Core tests (`BUILDING_DUNGEON_CORE_PASS` checks north direction), Seed 1729 fingerprint unchanged, 18/18, **SUITE_PASS** (`suite-commit-20261113-211146`, `suite-surface-20261113-211146`; 104 SetFocus warnings, zero other stderr entries).

**Not verified:** look of “stairs down” and “brick wall” in GUI (user assessment; graphics could not be viewed with tool — sprite mapping from available Tibia.dat was inconsistent), passage in game, probe not run on code without wall fix (error 3 diagnosed from code; probe checks wall at each descent with impassable rock).

## Iteration 46 — cleanup of debts D22–D27 (2026-11-14): PASS, D25 and D26 closed, D24 and D23 narrowed

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 46”, unchanged. Commits: `46bd778` (adapter: “cave enter” variant), `9568922` (cave preview validation by brush name), `61e5ad4` (probe: entrance variant, cave wall faces, D26 case), `860611f` (`g5_run_suite.py`: target rebuild, `local-data` from main checkout). Core unchanged.

1. **D25 — PASS.** `g5_run_suite.py` lists targets `style_render` and `test_procedural_*` in `build-ninja` (`ninja -t targets all`), rebuilds them via `vcvarsall.bat` (found by `vswhere`) and `cmake --build`, prints target list and build step count (`rebuilt targets (19): style_render test_procedural_core …`, `rebuild exit=0 build steps=38`), and on error ends with `SUITE_FAIL: core target rebuild failed` (verified: first version with wrong `vcvarsall.bat` path quoting ended with this message, without running tests). Relative `PROFILE_DATA` and `CONFIG` missing relative to current directory are taken from `--local-data`, by default from main checkout `local-data` (first entry of `git worktree list --porcelain`); series in worktree run as `--probes procedural-smoke/g0-reference-profile-20261030/data procedural-smoke/g0-reference-profile-20261030/config.toml`. Probe output still in `local-data/procedural-smoke` of given checkout (requirement of `g1_run_resolver_probe.py`).
2. **D24 — (a) and (b) fixed, (c) measured, no fix.**
   (a) RockFace rule (`caveEntranceAllowed`) requires open field south of entrance, so each wall entrance faces south. Adapter chose doodad variant by cell and seed (`(e + seed) % 2`). Variant direction determined from reference maps (OTBM scanner in executor notebook, 19 maps M01–M19 and forgotten): with stairs at wall with ground 459, item 8203 (first “cave enter” variant) stands 4 times with rock 919 from north, west and east with open south, 8212 (second) 5 times with rock from north, south and west with open east; 8202 (“cave enter3”, first) once with rock from north, west and east (no tile south). `CaveBrushes::entrance` now carries only first variant (all still checked as plain cave items), preview validation accepts only it. Probe: each RockFace entrance has open south, carries first variant, no second anywhere (`caves`, `guiCaves`). Tool `tibia_data.py` could not display graphics: for this `Tibia.dat` gives inconsistent sprites (e.g., 3031 as container), so direction based on reference maps, not image.
   (b) Cave preview validation resolves `caveGround` and `caveWall` from profile fields by name and checks Floor ground, walls and their border elements against these brushes; profile index 1 no longer used for cave (indices remain only for plateau, out of range).
   (c) `CAVE_WALL_FACES` in probe (256 × 256, “mountain”): RockFace — 3821 wall cells, elements 873 × 611, 874 × 2385, 877 × 385; FloorHole — 3108 cells, 873 × 522, 874 × 1801, 877 × 374. This is border family 3 of “mountain” brush (east, south faces and SE corner). 1870 of 3821 (1522 of 3108) wall cells see floor only from north or west and have no face. In 10.98 data “mountain” brush has faces only in families 3 and 51 (`e`, `s`, corners `dne`, `dsw`, `dse`); family 10 (`n`, `w`, …) is edge to other grounds, not face, and family 29 is optional. No north and west face variants, so by criterion no fix.
3. **D23 — state at HEAD.** Ramp upper tile as second hole: false since iteration 30 (no “top” item, probe `checkItem(…, topIds)`). “cave enter” variant: fixed (point 2a). Exit measurement without `occupied`: false for gate measurement (`style_render` in levels mode builds world with decoration and passes `occupied` to `placeCaveExits`); old `--cave-exits` mode (iteration 29–30 tables) still counts without vegetation, documented in code. Repair unaware of Z entrances: false since iteration 36–37 (`levelReachabilityOf`, `repairLevelReach`). Empty entire z + 1 rectangle at mask: true and intentional — cave generated in full rectangle without mask knowledge, cells outside mask not saved, so allowing occupied z + 1 would give discontinuous caves; requires cave generator change, stays in D23. One Undo leaves cave without entrance: true (N-step history, order plateau, cave, surface); user decision “two steps”, no change. Game movement: unchecked without server.
4. **D22, D26, D27.** D22 closed (description updated). D26: `D26_RAMP_ENTRANCE_PASS runs=40 pairedRamps=404 farExitRamps=10` — real window with defaults plus 10 entrances, m06 and m19 fixture (tuning and g4 profile palettes), 512×512 rectangle around (1018, 1237), surface at z 6 (ramps at z 7) and z 7, seeds 1729 and 1001–1004, far exits enabled (default) and disabled; each ramp except far exits has entrance directly above, each entrance has ramp below, field above far exit has ground and doesn't block, window reports all reachable places. Symptom does not occur; D26 closed as unreproducible after changes in iteration 37–45. **Discovery:** with m19 at default settings (no passes, rock impassable) in all 20 runs in this rectangle repair removes all entrances (11–28 removed places, reachable 75–24276 places), so m19 gives no ramp here; D26 ramp coverage from m06. D27 updated per 45c.
5. **PASS, no regression.** Measurement `py -I docs/procedural/smoke/g5_measure_levels.py --i45 --rock-blocks m06=0 m19=1` on rebuilt `style_render`: **G5_GATE_PASS**, `summary.json` byte-identical to iteration 45c measurement (worktree `agent-a49bab52faf7e9ad8`); core unchanged, so entrance variant doesn't change plan or fingerprint. Determinism: both runs of each variant identical (`identical: true`). Seed 1729 100×100 = 17159159863505677800 fingerprint (test `test_procedural_surface`). Rows `SURFACE_CAVE_E2E_PASS` and `SURFACE_CAVE_LAYOUT` with same numbers as iteration 30. Canonical `build_ninja.bat` in worktree, 18/18 core tests after series rebuild, both probes PASS, **SUITE_PASS** (`suite-commit-20261114-100028`, `suite-surface-20261114-100028`; series rebuilt 19 targets — 0 steps, previous series run rebuilt them in 38 steps; client data unchanged, 144 known SetFocus warnings, zero other stderr entries).

**Not verified:** graphics of both “cave enter” variants (direction from reference maps, not image), cave walls without north and west faces in GUI, cause of original D26 symptom (user session data unknown; only described combinations checked), D26 ramps for m19 at default settings (don't form), game movement, clients other than 10.98, original RME.

### Patch after iteration 46 — m19 without caves in GUI (2026-11-14): PASS

User report (HEAD `59ee144`): m19, x 1000, y 1000, 512 × 512, seed 1729, 10 wall entrances, rest default — no caves. Criteria unchanged. Fix `dfa7a11` (adapter), probe `36b1b7e`.

1. **Reproduction (real window):** `M19_CAVES_GUI_CASE candidates=771 coreEntrances=40 startLandEntrances=20 mapEntrances=0 withRamp=0 caveCells=0 status=Reachable: 21616/21616 places on 3 floors. Seed 1729, 1 attempts. Repairs: 20, bridges: 0, removed caps: 0, removed places: 20.` — 771 cells satisfy relaxed RockFace rule, core places 40 entrances, start land filter (iteration 44) leaves 20, adapter repair removes all 20 with caves.
2. **Cause:** adapter blocking mask (`terrain_preparation.cpp`) took ground resolved for Rock type, i.e., impassable “mountain” ground (919), also at wall entrance cell, though adapter later places passable dirt ground there (iteration 44). Each such entrance was unreachable, and repair can't path through blocking ground (cost 0) and removes entrance. Offline (`style_render`, no client mask) same seed has 34 entrances, hence median 17 in gate measurement. **Not a regression of iteration 46:** probe row of iteration 44 (`380b522`) already had `Repairs: 20, … removed places: 20` (described then as removed places); `46bd778` only changes entrance item ID, mask doesn't read it. Error from iteration 44 when m19 rock became impassable.
3. **Fix:** with impassable rock, RockFace style and available passable dirt ground, mask doesn't block wall entrance cells (same conditions as dirt ground placement). After fix: `mapEntrances=20 withRamp=20 caveCells=12284`, `Reachable: 33941/33941 … Repairs: 0, … removed places: 0`, marker `M19_CAVES_GUI_PASS` required in `g5_run_suite.py`. Case from iteration 44: `Reachable: 36357/36357 places on 5 floors … Repairs: 0, removed places: 0` (previously 24032 and 20 removed). `D26_RAMP_ENTRANCE_PASS runs=40 pairedRamps=740 m19PairedRamps=336 farExitRamps=20` (previously m19 without ramps; now required > 0).
4. No regression: `g5_measure_levels.py --i45 --rock-blocks m06=0 m19=1` — **G5_GATE_PASS**, `summary.json` byte-identical to iteration 45c/46 (core unchanged, offline measurement doesn't use adapter mask); Seed 1729 fingerprint unchanged (core test), 18/18, **SUITE_PASS** (`suite-commit-20261114-105147`, `suite-surface-20261114-105147`; 145 SetFocus warnings, zero other stderr entries).

**Not verified:** look and movement in game, remaining places where client mask may differ from offline model (gate measurement still doesn't use it), FloorHole style with impassable rock (not applicable: hole on grass or dirt).

## Iteration 47 — cave appearance (2026-11-15): PASS in measurements and probes

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 47”, unchanged. Branch `fix/procedural-g5-cave-shape` from `cc8c06d`. Commits: `cd5ceb0` (core: new `generateCave`), `007a0fc` (shape tests), `58674c1` (`style_render`: row `level cave i47` and image `<seed>-cave.png`; `g5_measure_levels.py --i47`), `d1b55bb` (loops away from other tunnels), `2488bf4` (test: loop in each cave with ≥ 4 entrances), `5be96e1` (measurement of parallel pairs along chords).

1. **Connection graph — PASS.** Entrances (sorted and deduplicated, so result doesn't depend on order) linked by minimum spanning tree (Prim by squared distance, tie to lower cell index). With ≥ 4 entrances 1 loop, with ≥ 8 — 2: off-tree pairs no longer than 1.5 × longest tree edge, selected by tree path to straight distance ratio. Loop must diverge from both ends ≥ 35° from tunnels in that entrance, and its 1/4, 1/2 and 3/4 points lie ≥ 1/5 its length from other planned tunnels. Without this condition measurement found loop running ~30 cells alongside tree edge (m06, 1 pair) — hence `d1b55bb`. Layout without such pair gets fewer loops (not occurred in measurement: 64 loops in 32 m06 seeds, 57 in m19; test: loop in 24/24 caves with ≥ 4 entrances).
   Core test: 4×4 entrance grid (connecting rows would give four parallel tunnels through area) — identical cave, `rooms` and connections after flip and permute entrances; tree edge length equals MST from independent Kruskal algorithm; no row-end to next-row-start connection; 2 loops.
   Parallel pair measurement (`style_render`): axes whose chord ≥ 40% of side; pair is “beside each other” when chords differ < 20°, at least half shorter axis lies along longer chord and those cells average < 32 cells from it. **0 pairs** in m06 and m19. Longest straight axis segment (longest chord of consecutive axis cells each lying ≤ 1 cell from chord): **m06 ≤ 20, m19 ≤ 16** (criterion ≤ 20); core test 256×256, 24 seeds: 15.
2. **Tunnels — PASS.** Axis from points every 7–10 cells around arc with deflection up to min(L/5, 28) cells (half to full, random side, reversed at area edge), plus side sway up to min(max(L/10, 2), 6) cells per point; each kink at least 4 cells from neighbor chords (point shift to sides, then two passes improving), segments longer than 10 cells split into chunks ≤ 8 with 2.5-cell kinks alternating. Points keep 3 cells from edge (deflection shortened, not clipped — previously clipped points made straight lines along edge up to 110 cells). Width 2–5 from noise value (step 7, stretched so 2 and 5 also occur), square without corners at 4 and 5. 0–2 blind side branches per tunnel from 24-cell axis: 6–14 cells, one kink, width 2–3, small widening at end. Measurement: 631 tunnels and 654 branches (m06), 611 and 609 (m19).
3. **Chambers — PASS.** ~Every 36 axis cells (at least one from 16 cells) a chamber: ellipse with radii 4–12, center offset half radius from axis cell, edge from noise (step 3), 4 cellular automaton rounds (Floor if ≥ 5 Floor neighbors from 8 or stays Floor if ≥ 4), from result only part connected to axis cell (3×3 around it forced) — so each chamber belongs to network. Center goes to `CaveGrid::rooms` (corridors with doors, far exits and their tests work as before). Chamber cell share (Floor with ≥ 5 Floor neighbors from 8 and width > 5: shortest Floor path through cell along both axes and both diagonals, diagonal ×1.4): **median 0.779 in m06 (0.735–0.811) and 0.779 in m19 (0.517–0.822)**, criterion ≥ 0.25; 1641 chambers in m06, 1515 in m19. Core test: 0.798.
4. **Coherence and GUI — PASS.** All entrances in one component: `apart 0` in all 64 caves of measurement; core test — all entrances, all chambers and entire floor in one component (grid and 24 seeds). Landings (21 cells) below entrances unchanged. User case through real window (probe `--surface`, m19, x 1000, y 1000, 512×512, seed 1729, 10 wall entrances): `M19_CAVES_GUI_CASE … mapEntrances=20 withRamp=20 caveCells=13240 status=Reachable: 34897/34897 places on 3 floors. … Repairs: 0, … removed places: 0`; iteration 44 case: `M19_ROCK_GUI_CASE_PASS … Reachable: 37313/37313 places on 5 floors … removed places: 0`; `D26_RAMP_ENTRANCE_PASS runs=40 pairedRamps=740 m19PairedRamps=336 farExitRamps=20`; `DUNGEON_GUI_PASS`.
   Gate: `py -I docs/procedural/smoke/g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1` (settings and conditions `--i45` plus shape criteria) — **G5_GATE_PASS**, m06 and m19 **32/32** in each variant (dungeons 0/1, retry 0/8), Z edges all passable (m06 2773/2773 and 3777/3777, m19 2509/2509 and 3387/3387), 0 `Unrepaired`, removed POI share ≤ 0.0004, median entrances 18 (m06) and 17 (m19); i41: far exit in each component (median 1.0), min distance 41 and 34, doors in 32/32 seeds, 0 bad; dungeons: median share 0.164 and 0.160, median rooms 4, 0 cave collisions. Measurement in worktree, `local-data/i47-measurement/summary.json`.
5. **No regression — PASS.** Determinism: both runs of each variant byte-identical (`identical: true`), core tests check repetition and entrance order independence; no `std::random_device` (`StableRandom` streams with seed and absolute tunnel-end cells, plus `surfaceValueNoise`). `stop_token` checked before each tunnel (cancellation test). **Seed 1729 100×100 = 17159159863505677800 fingerprint unchanged** — this is surface fingerprint (`test_procedural_surface`), cave doesn't change it. Canonical `build_ninja.bat` in worktree, 18/18 core tests, both probes PASS, **SUITE_PASS** (`suite-commit-20261115-162102`, `suite-surface-20261115-162102`; 145 known SetFocus warnings, zero other stderr entries). Cave fingerprints in probe (`caveFingerprint`) and cave cell counts changed as intended.

**Image for assessment:** `style_render` mode now writes `<seed>-cave.png` (rock, floor, entrance); examples m19 512×512 with 16 entrances: seeds 1729 and 1001 in executor working directory (outside repo).

**Not verified:** GUI look on map (walls, borders and “mountain” faces around irregular chambers viewed only as PNG mask), game movement, impact on D23/D24 (north and west faces with more jagged walls), clients other than 10.98, original RME. Chamber share (~0.78) is well above threshold 0.25 — cave is network of chambers linked by short tunnels; if user prefers more tunnels, parameters are chamber spacing (36) and radii (4–12).

## Iteration 48 — D24: regular entrances in reserve, cave wall brush (2026-11-15): part 1 PASS, part 2 unchanged (no brush qualifies)

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 48”, unchanged. Branch `fix/procedural-g5-d24` from `b211473`. Commits: `b9ca195` (core: regular entrance reserve, `caveEntranceFits`), `43dddd0` (adapter: regular entrance on own ground), `58dabd9` (`style_render`: row `level cave i48`), `17c85bf` (re-classify islands after POI removal), `8b7f950` and its revert `01320ca` (default “snowy mountain” in window — reverted), `2c601ff` and `3cca6f5` (probe: wall faces, RockFace without rock).

1. **Entrance reserve — PASS.** `placeEntrances`: first wall entrances exactly as before (same stream and attempt count), then — only with RockFace and when short of target count — regular FloorHole-shaped entrances (Grass/Dirt, 4 open neighbors, no water and Floor in 5×5) from separate stream, spacing 24 to all. Regular entrance gets doodad from entrance brush on own ground (swap to “dirt” ground only for entrance on impassable rock). After mask entrances filtered by `caveEntranceFits` (style rule or, with RockFace, FloorHole). Measurement (`level cave i48`, seeds 1001–1032, side 512): wall entrances in each seed equal HEAD (`26fd415`, row `i47 entrances`) — **m06 599, m19 586**, 0 seeds with drop; regular 1449 (m06) and 685 (m19); **median entrances 64 (m06) and 44.5 (m19)** vs 18 and 17 at HEAD (criterion ≥ 8); **each entrance with ramp below** (2048/2048 and 1271/1271), `apart 0` (all entrances in one cave component) in 64 caves.
   First measurement gave `G5_GATE_FAIL`: m19 had 67 non-shore island cells — island avoided normalization only via regular entrance, which `keepStartLandPois` then removed. Fix `17c85bf`: after POI removal (and after `reselectCaveEntrances`) islands re-classified; without removed POI nothing changes.
2. **Cave wall brush — unchanged.** Faces from all sides (N, W, E, S, corners, diagonals) exist in 10.98 data only on “snowy mountain” and “icy mountain” (inner border 53, “ice” items). Probe `CAVE_WALL_FACES` (new measure: wall cells with no border piece; 256×256, seed 1729): “mountain” 574/3331 = **17.2%** (RockFace) and 604/3488 = 17.3% (FloorHole), of which 1593 cells (48%) see floor only from north or west; “snowy mountain” and “icy mountain” **0%**. But their outer border 52 (6719–6768) is **impassable** and lands on cave floor at wall: m19 GUI case with “snowy mountain” — `Reachable: 28603/34897 … Repairs: 64 … Unrepaired` (probe `--surface`, SUITE_FAIL). No-regression criterion takes priority, so default window brush reverted to “mountain” (`01320ca`). Also checked and rejected: “mountain ground”, “mountain top ground”, “rocky ground”, “dirt mountain ground (thick border)” (no wall faces; inner borders only to empty), wall-type brushes “rock wall”, “rocky wall” (`caveWall` must be ground brush). Note: g4 profiles have no cave wall field — brush chosen by window (one default for all profiles).
3. **No regression — PASS.** User case through real window (m19, x 1000, y 1000, 512×512, seed 1729, 10 RockFace entrances): `M19_CAVES_GUI_CASE candidates=771 coreEntrances=40 startLandEntrances=20 mapEntrances=20 withRamp=20 caveCells=13240 … removed places: 0` (40 wall places enough, reserve not needed). Gate `py -I docs/procedural/smoke/g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1` — **G5_GATE_PASS**, m06 and m19 **32/32** in each variant (dungeons 0/1, retry 0/8), Z edges all passable (m06 5671 and 6481, m19 3879 and 4467), 0 `Unrepaired`, removed POI share ≤ 0.0002, 0 non-shore island cells; cave shape: longest straight segment 20 (m06) and 16 (m19), median chamber share 0.774 and 0.777, 0 parallel pairs; i41: far exit in 32/32 and 30/32 components (median share 1.0), min distance 58 and 34, doors in 32/32 seeds. Both runs of each variant byte-identical. **Seed 1729 100×100 = 17159159863505677800 fingerprint unchanged** (`test_procedural_surface`). Core tests 18/18. Canonical `build_ninja.bat` in worktree, both probes PASS, **SUITE_PASS** (`suite-commit-20261115-204045`, `suite-surface-20261115-204045`; 145 known SetFocus warnings, zero other stderr entries); `D26_RAMP_ENTRANCE_PASS runs=40 pairedRamps=1180 m19PairedRamps=380 farExitRamps=20`, `M19_ROCK_GUI_CASE_PASS … 37313/37313 … removed places: 0`.

**Not verified:** look of regular entrance (doodad “cave enter” on grass or dirt) and game movement; generation time: `style_render` run in measurement ~1.5–2× longer than HEAD (from log file timings, with parallel tasks — not profiling); wall entrances compared with iteration 47 measurement logs, not rebuilt HEAD.

## Iteration 48b — appearance of regular reserve entrances (2026-11-16): PASS

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 48b”, unchanged. Branch `fix/procedural-g5-fallback-entrance-look` from `b3b4bb8`. Commits: `1389dcd` (adapter and preview validation), `5fefe43` (probe, marker in `g5_run_suite.py`).

1. **Look — PASS.** Existing entrance styles: RockFace (doodad “cave enter”, first variant, at rock wall) and FloorHole (ground brush “cave hole”: item 383 “hole”, `floorchange down`, border 7 on adjacent cells). Regular RockFace reserve entrance (cell outside rock) now gets FloorHole style look: ground from “cave hole” brush, resolved by name in `resolveCaveBrushes` with same conditions as FloorHole style (correct ground brush, ground item with floor change down; new field `CaveBrushes::hole`). Without this brush in data regular entrance stays as doodad. Wall entrance unchanged (doodad, dirt ground on impassable rock). Preview validation accepts this hole as entrance-ground (like FloorHole style); blocking mask and rock border cleaning don't apply to hole (entrance equals ground).
2. **Probe — PASS.** `checkEntranceLook`: on placed map “cave enter” item only on cell with rock brush ground or adjacent to it (N, W, E, S), each entrance (doodad or hole) has item with `floorchange down` and exit brush ramp below. Used in D26 cases and new case through real window (m19, x 1000, y 1000, 512×512, seed 1729, RockFace, 10 and 15 entrances on 65536 cells, i.e., 40 and 60 ordered): `FALLBACK_ENTRANCE_LOOK_CASE entrances=10 rockFace=20 holes=0 … Reachable: 34897/34897 … removed places: 0`, `entrances=15 rockFace=23 holes=13 … Reachable: 38767/38767 … removed places: 0`; marker `FALLBACK_ENTRANCE_LOOK_PASS rockFace=43 holes=13` required in `g5_run_suite.py`. `D26_RAMP_ENTRANCE_PASS runs=40 pairedRamps=1180 m19PairedRamps=380 farExitRamps=20 rockFaceEntrances=740 holeEntrances=440` (holes in 28 of 40 runs). Probes `caves`, `guiCaves` and cap indents expect hole for entrances outside rock.
3. **No regression — PASS.** Wall entrances: 740 in D26 (same as all ramps with entrance in iteration 47 before reserve), total with reserve entrances 1180 as in iteration 48; m19 user case with 10 entrances: 20 wall entrances, 0 removed places (`M19_CAVES_GUI_CASE … mapEntrances=20 withRamp=20 caveCells=13240`, numbers as iteration 48); `M19_ROCK_GUI_CASE_PASS … 37313/37313 … removed places: 0`. Gate `py -I docs/procedural/smoke/g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1` — **G5_GATE_PASS**, m06 and m19 **32/32** in each of 8 variants, 0 `Unrepaired`, median entrances m19 44.5 (as iteration 48; core unchanged); both runs of each variant identical (`identical: true`). Seed 1729 100×100 = 17159159863505677800 fingerprint unchanged (`test_procedural_surface`). Canonical `build_ninja.bat` in worktree, 18/18 core tests, both probes PASS, **SUITE_PASS** (`suite-commit-20261116-173243`, `suite-surface-20261116-173243`; 147 known SetFocus warnings — two new `NewMap` — zero other stderr entries).

**Not verified:** hole look in GUI by eye (checked only map items; “cave hole” border on grass and dirt profile not viewed), game movement, clients other than 10.98, original RME. Gate measurement doesn't see look (offline, no adapter), so proves only lack of core regression.

## Iteration 48c — landing under hole (2026-11-16): PASS

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 48c”, unchanged. Branch `fix/procedural-g5-hole-landing` from `036cfde`. Commits: `ff20fb2` (adapter and preview validation), `b9e545a` (probes).

1. **Ground under hole — PASS.** Below each hole entrance (FloorHole style and regular RockFace reserve entrances, “cave hole” brush, item 383) at z + 1 no ramp or tail; lies ground from “cave ropespot” brush, resolved by name in `resolveCaveBrushes` (new field `CaveBrushes::landing`): correct ground brush, passable ground item, no floor change flag. In 10.98 this is item 384 “dirt floor” (probe checks item name); no ID in code. Without this brush in data hole keeps ramp. RockFace wall entrances and far exits keep ramps unchanged. Entrance and exit selection (core, `placeCaveExits`) unchanged; field below ramp still reserved for caps, so caps don't change.
2. **Z edge — PASS.** Adapter level plan removes ramp edge (field below hole → target field on surface) for such holes since no up-ramp; stays hole → field below it. Landing field passable in adapter mask (ground 384 doesn't block), preview validator accepts landing ground on cave floor cell without exit item. Probe `caves` (both styles): below each hole ground 384, non-blocking, both fields free in `blockingMask` and passable in report, edge {z, hole} → {z + 1, hole} exists, landing reached when hole reached; `SURFACE_CAVE_E2E_PASS` for rockface and floorhole (20/20 exits). `checkEntranceLook` (D26 and reserve case): each hole has “dirt floor” below without ramp, each RockFace entrance has ramp. `FALLBACK_ENTRANCE_LOOK_CASE entrances=10 rockFace=20 holes=0 … Reachable: 34897/34897 … removed places: 0`, `entrances=15 rockFace=23 holes=13 … Reachable: 38767/38767 … removed places: 0`, `FALLBACK_ENTRANCE_LOOK_PASS rockFace=43 holes=13`. `D26_RAMP_ENTRANCE_PASS runs=40 pairedRamps=740 m19PairedRamps=336 farExitRamps=20 rockFaceEntrances=740 holeEntrances=440` (ramps with entrance: 740 = all RockFace entrances; 440 holes in 28 of 40 runs have ground instead of ramp, hence 1180 → 740). Matrix `guiCaves` expects ground below hole (e.g. floorhole seed 1729: 80 entrances, 80 landings).
3. **No regression — PASS.** User m19 cases (x 1000, y 1000, 512×512, seed 1729, RockFace, 10 and 15 entrances): 0 removed places, place counts as iteration 48b; `M19_CAVES_GUI_CASE … mapEntrances=20 withRamp=20 caveCells=13240 … removed places: 0`; `M19_ROCK_GUI_CASE_PASS … 37313/37313 … removed places: 0`. Gate `py docs/procedural/smoke/g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1` — **G5_GATE_PASS**, m06 and m19 **32/32** in each of 8 variants, Z edges as iteration 48 (m06 5671 and 6481, m19 3879 and 4467), 0 `Unrepaired`; both runs of each variant identical (`identical: True`). Seed 1729 100×100 = 17159159863505677800 fingerprint unchanged (`test_procedural_surface`). Canonical `build_ninja.bat` in worktree, 18/18 core tests, both probes PASS, **SUITE_PASS** (`suite-commit-20261116-185535`, `suite-surface-20261116-185535`; 147 known SetFocus warnings, zero other stderr entries).

**Not verified:** ground 384 and its border look in cave by eye (checked only map items), entrance and exit in game (rope on cell 384, return through hole), clients other than 10.98, original RME. Gate measurement is offline (core, no adapter), so proves only lack of core regression; core level validator (`levelPlanOf`) still has ramp edge for holes — only adapter removes it.

## Iteration 49 — D23: cave with occupied z + 1, one Undo step (2026-11-16): PASS

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 49”, unchanged. Branch `fix/procedural-g5-d23` from `54b3a40`. Commits: `33286b0` (one history step), `7fe1eb2` (cave with occupied z + 1), `b23d852` (mountain probes: one step), `a48a852` (probe `OCCUPIED_Z1_CAVE_PASS`).

1. **Occupied z + 1 — PASS.** Preparation no longer refuses non-empty z + 1 (`LowerFloorOccupied` removed; `NoLowerFloor` for z = 15 stays). Tiles z + 1 in rectangle that are non-empty or protected (ground, item, flags, spawn, waypoint, town, house exit — same test as lower dump protection), plus 2-cell border around, go to `generateCave` as `blocked` mask: these cells stay solid (`CaveGrid::blocked`, honored by far exit tunnels too), so cave floor keeps 3 cells from occupied tile, wall ring 2, its borders 1 — no brush or border touches occupied tile. Dungeons keep another cell spacing. Entrance whose landing would fall in mask doesn't form (existing exit selection skips it); cave parts cut off from all exits removed (`removeUnenteredCaveComponents`, in no-repair path only with non-empty mask). With free z + 1 mask is empty and generator works unchanged. Probe `OCCUPIED_Z1_CAVE_CASE` (m19, x 1000, y 1000, 512×512, seed 1729, 10 entrances, 128×128 ground block at z + 1 center): without block 20 entrances and 13240 cave cells, 2595 in block area; with block `entrances=18 caveCells=11455 droppedNearBlock=2 added=0 removedPois=0 removedCaveComponents=1`; both lost entrances lie ≤ 4 cells from block; block byte-identical after Place, Undo and Redo; cave preview doesn't change any block tile; each entrance has cave ground below outside block and is reached; level report `allReached`, all Z edges passable. Core test: zero mask = no mask, blocked strip splits cave into two parts with entrance, wrong-size mask rejected. Probe `caves`: occupied z + 1 tile with mask doesn't block cave.
2. **One Undo step — PASS.** Place combines all floors (surface, cave, mountains) into one `ACTION_GENERATE_TERRAIN` action (`commitValidatedTerrain` counts budget once for all); pair mechanism in `ActionQueue` (`beginTerrainPair`/`finishTerrainPair`) has no calls and removed; other editor operations unchanged. Probes: one Undo restores both floors before Place, one Redo both after Place (`caves`, `OCCUPIED_Z1_CAVE_CASE … undoSteps=1`), mountains 1 and 3 levels, entrance indents, `FINAL_MASK_REPAIR_E2E_PASS … undoRedo=1`, I41/I43/I45 `undoRedo=1`; refusal (change budget 1 too small) leaves nothing in map or history.
3. **No regression — PASS.** User m19 cases (x 1000, y 1000, 512×512, seed 1729, RockFace, empty map): `M19_CAVES_GUI_CASE … mapEntrances=20 withRamp=20 caveCells=13240 … Reachable: 34897/34897 … removed places: 0`, `FALLBACK_ENTRANCE_LOOK_CASE` 10 entrances `rockFace=20 holes=0 … 34897/34897`, 15 entrances `rockFace=23 holes=13 … 38767/38767`, `M19_ROCK_GUI_CASE_PASS … 37313/37313 … removed places: 0`, `D26_RAMP_ENTRANCE_PASS runs=40 pairedRamps=740 … holeEntrances=440` — numbers as iteration 48c. Gate `py docs/procedural/smoke/g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1` — **G5_GATE_PASS**, m06 and m19 **32/32** in each of 8 variants, Z edges m06 5671 and 6481, m19 3879 and 4467, 0 `Unrepaired`, both runs of each variant identical (`identical: True`). Seed 1729 100×100 = 17159159863505677800 fingerprint unchanged (`test_procedural_surface`). Canonical `build_ninja.bat` in worktree, 18/18 core tests, both probes PASS, **SUITE_PASS** (`suite-commit-20261116-210610`, `suite-surface-20261116-210610`; 149 known SetFocus warnings, zero other stderr entries).

**Not verified:** game (landing, movement with occupied z + 1), cave with other occupied tiles than ground block (houses, spawns, waypoints) and with occupied z + 1 and dungeons (only core test via mask, no probe), capacity probe `--capacity` (changed to one step, not run; one step requires Undo memory limit for all floors at once), manual Undo/Redo in GUI, clients other than 10.98, original RME.

## Iteration 50 — D24: cave walls with rock wall brush (2026-11-17): PASS

Criteria: [G5_METRICS.md](G5_METRICS.md), “Iteration 50”, unchanged. Branch `fix/procedural-g5-cave-wall-brush` from `06b23f2`. Commits: `c6e8cf8` (adapter and preview validation), `b186a65` (window), `3a701f3` (probes).

1. **Wall brush: PASS.** The profile field `caveWall` now accepts a wall brush. Ground brushes retain their existing behavior. With a wall brush, each wall-band cell, a non-floor cell touching the floor in its eight-cell neighborhood, receives cave ground and a piece from the default position of the wall brush. The existing preview wall pass (`WallBrush::doWalls`, also used for building and corridor walls with doors) chooses horizontal, vertical, corner, or post pieces based on adjacent pieces. The resolver (`resolveCaveBrushes`, the new `CaveBrushes::wallPiece`, and `caveWallBrushOf`) accepts a wall brush only if each of its pieces is a valid, impassable client item other than ground. The **Cave wall brush** control lists wall brushes first and ground brushes second. The default is **rocky wall**, with **mountain** as the fallback when the data lacks that brush. The implementation uses brush names without hardcoded item IDs.
   **Brush selection.** Both **rocky wall** and **rock wall** provide faces on all sides, with zero cells missing a piece in the measurement below. **rocky wall** uses items 13644–13654, called `rocky wall` in `items.xml`. These depict natural rock and the brush has no `friend` or `redirect` dependencies. **rock wall** uses items 903–913, called `stone wall`, which depict cut stone. It has `friend name="plaster wall" redirect="true"`, so it connects to plaster walls and can use their pieces for orientations it lacks.
2. **Faces — PASS.** Probe `caves` (256×256, seed 1729), `CAVE_WALL_FACES` (wall-strip cell without wall element of this brush): **"rocky wall" 0/3331 (RockFace) and 0/3488 (FloorHole)**, elements 13644 (vertical) 1185, 13645 (horizontal) 1078, 13648 (corner) 577, 13646 (pillar) 491; of these 1593 and 1658 cells seeing floor only from north or west (48%), which had no face with "mountain". "rock wall" also 0/3331 and 0/3488 (only 903, 904, 905, 907, no "plaster wall" elements). "mountain" unchanged: 574/3331 = 17.2% and 604/3488 = 17.3% without border. Probe requires ≤ 5% for each wall brush and only chosen brush elements on strip. On map after Place (measurement from map: floor tile is "cave" or "dirt floor" ground without wall, each other field in its 8-neighborhood must have wall element): m19 case 10 entrances **4237 strip cells, 0 without element**, 15 entrances 5355 and 0, occupied z + 1 3620 and 0; 0 elements of other brush.
3. **Passability — PASS.** Wall sits only on wall strip: preview validation rejects plan with strip cell without element or brush element on floor (except corridor doors). Probe `caves` (both styles, three brushes): no floor field has blocking item, each strip field with wall blocks, adapter blocking mask (`levels.blockingMask`, computed from client items after all passes) equals blockage from items on each cave field. On m19 map and with occupied z + 1: 0 blocking floor fields, 0 passable wall fields. User m19 cases through real window (x 1000, y 1000, 512×512, seed 1729, RockFace, default "rocky wall"): `M19_CAVES_GUI_CASE … mapEntrances=20 withRamp=20 caveCells=13240 … Reachable: 34897/34897 … removed places: 0` (13240 floor cells as 48c; strip now has "cave" ground, so probe counts floor without wall), `FALLBACK_ENTRANCE_LOOK_CASE` 10 entrances `rockFace=20 holes=0 … 34897/34897`, 15 entrances `rockFace=23 holes=13 … 38767/38767`, all 0 removed places; each RockFace entrance with ramp, each hole with "dirt floor" (`checkEntranceLook`). Occupied z + 1 (`OCCUPIED_Z1_CAVE_CASE`, "rocky wall"): `entrances=18 droppedNearBlock=2 removedCaveComponents=1`, as iteration 49; block untouched after Place, Undo and Redo.
4. **No regression — PASS.** Corridors with doors (`CAVE_FAR_EXIT_DOOR_GUI_PASS … brush=stone wall`, corridor wall replaces cave wall on its fields; corridors don't clean wall layer anymore) and dungeons (`DUNGEON_GUI_PASS … missingWalls=0`) through window with new default wall; `D26_RAMP_ENTRANCE_PASS runs=40 pairedRamps=740 m19PairedRamps=336 farExitRamps=20 rockFaceEntrances=740 holeEntrances=440`, `M19_ROCK_GUI_CASE_PASS … 37313/37313 … removed places: 0` — numbers as 48c/49. One Undo and one Redo restore both floors (`caves`, `OCCUPIED_Z1_CAVE_CASE … undoSteps=1`). Gate `py docs/procedural/smoke/g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1` — **G5_GATE_PASS**, m06 and m19 **32/32** in each of 8 variants, Z edges m06 5671 and 6481, m19 3879 and 4467, 0 `Unrepaired`, both runs of each variant identical (`identical: True`; core unchanged). Seed 1729 100×100 = 17159159863505677800 fingerprint unchanged (`test_procedural_surface`). Canonical `build_ninja.bat` in worktree, 18/18 core tests, both probes PASS, **SUITE_PASS** (`suite-commit-20261117-152638`, `suite-surface-20261117-152638`; 157 known SetFocus warnings — more `caves` probe runs — zero other stderr entries).

**Outside verification scope:** Manual GUI appearance, including the cave ground border between the wall band and the void under the wall. The checks covered the placed items and their orientation. In-game movement, the appearance of **rock wall** next to **plaster wall**, clients other than 10.98, and loading in the original RME also remain unverified. The resolver checks the brush pieces directly and leaves the `redirect` chain unexamined. The map uses ordinary wall items and the existing format.

### Iteration 50 — GUI correction (2026-11-17)

User: “Rocky wall looks terrible, old look was better”. Default cave wall in window reverts to “mountain” (commit 02bf1bc); “rocky wall”/”rock wall” remain for selection in “Cave wall brush” field. Probes select “rocky wall” explicitly to still check wall brush path. Lack of N/W faces with “mountain” stays in D24 as accepted look. SUITE_PASS (with probes).

## G5 Closure (2026-11-17)

User decision: “Yes, we're closing it”. G5 CLOSED WITH DEBT (D23, D24). Final state: build OK, SUITE_PASS with probes, gate `--i47` 32/32 m06/m19, Seed 1729 100×100 = 17159159863505677800 fingerprint.
