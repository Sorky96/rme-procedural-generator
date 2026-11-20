# G5: criteria recorded before implementation and measurement

Document G5 (PHASE 5: topology and multi-floor worlds). Iterations 12–23 of this phase (settlements, reachability validator, fords, paths through rock and river, borders, window, POI, repair) are recorded in historical [G4_METRICS.md](G4_METRICS.md) and [G4_GATE_STATUS.md](G4_GATE_STATUS.md), because they were developed on the G4 branch; from iteration 24 onwards, criteria and results for G5 are in this document and in [G5_GATE_STATUS.md](G5_GATE_STATUS.md). Same rule: I record criteria, bands, and seeds before implementation and measurement, verify with grep that the record is in the file.

### Iteration 24 — region graph over surface (PHASE 5, plan point 3; recorded 2026-11-09 before implementation and before measurement; user decision: “Region graph”)

**Scope.** Pure core function `buildRegionGraph(SurfaceGrid, walls)` (`region_graph.{h,cpp}`) describing the surface as a graph: **without generator change, without GUI, without map save**; the only consumer for now is `style_render` and tests (the graph is the basis for later quality assessments and multiple floors).

**Definitions.**
- **Cell class:** Land = Grass, Dirt, Shore, Path, Floor; Rock = Rock; Water = Lake, River; Preserve has no class (does not enter the graph).
- **Region (node):** 4-connected component of one class with at least **16** cells (`regionMinCells`); smaller fragments are not nodes (counted separately in `smallFragments` and `smallFragmentCells`). Node numbering: order of first cell (rows, then columns). Node contains: class, cell count, bounding box (minX, minY, maxX, maxY), and centroid (average of coordinates, integer division).
- **Neighborhood edge:** two different nodes with touching (4-neighborhood) cells; weight = number of such cell pairs (each pair once, horizontally and vertically); edges sorted by (a, b), a < b.
- **POI nodes:** start, entries (doors) and exits from iteration 22 (same reachability model, walls included); each has the index of the region-node it lies in (−1 for fragment smaller than 16) and **route distance** from start in 4-neighborhood steps over reachable cells (−1: unreachable; start 0).

**Criteria.**
1. Manually counted test (14×6: Water x 6..8 = 18, Rock x 0..3, y 1..4 = 16, Land left = 20, Land right x 9..13 = 30): 4 nodes in order (Land 20, Water 18, Land 30, Rock 16), edges (0,1) weight 6, (0,3) weight 12, (1,2) weight 6; start (5,2) in node 0, exits north (5,0) distance 2, south (5,5) distance 3, west (0,2) (rock, node 3) distance 5, east (13,2) in node 2 unreachable (−1); fragment smaller than 16 (Rock 3×3 in separate grid) is not a node and counts in `smallFragments`; sum of node cells + fragments + Preserve = total cell count; determinism; empty grid invalid; `stop_token`.
2. Measurement (seeds 1001–1032, side 512, profiles v10 m06 and m19, `buildingDensity` 30 / 40, `pathLength` 40 / 80, `fordLength` 48, `repairReach` 1): median and range of node count (Land, Rock, Water separately), edges, fragments; fraction of seeds where all POI nodes have distance ≥ 0 (**expectation: 32 of 32, per iteration 23; this is a reachability criterion**); median and maximum distance to entries and exits; neighborhood graph connected (each node reachable from each via edges) in how many seeds (no band).
3. 16 core tests + new test, both G1 probes PASS (generator core unchanged: Seed 1729 fingerprint unchanged); two runs byte-identical.

**Caveats.** Classes are coarse (Land includes grass, dirt, beach, paths, and buildings; region interiors are not distinguished); neighborhood is cell touching, not “passage”; route distance is 4-neighborhood steps, not game time; graph does not know ruins, camps, or points from the list; no multiple floors or Z-entries yet; 16-cell threshold is my choice; graph is not saved or shown in GUI.

### Iteration 25 — ruins and camps (PHASE 5, “Structures” point; recorded 2026-11-09 before implementation and before measurement; user decision: “Ruins and camps”)

**Deviation from “measure references first” rule.** I do not measure in references what ruins and camps look like (recognizing them in reference tiles requires new probe code and a brush list); criteria are **absolute**, N/A for reference bands. Definitions are **my idealization** of existing mechanisms (floor, walls, paths), not replication of references.

**Definitions (new `SurfaceTuning` fields, appended at end: `ruinPercent` 0..100 and `campCount` 0..50 per 65,536 cells; 0 = as before, byte-identical; not part of style profile).**
- **Ruin:** building (Floor foundation from iterations 11–12) selected deterministically with probability `ruinPercent` %; **has no doors or path to them**; instead, **gaps in walls**: at least 2 and about one-quarter of the outline cells (excluding corners) without wall (`SurfaceGrid::gaps`). Gap cell is passable.
- **Camp:** clearing (Path disk of radius 3, Grass or Dirt cells) on terrain without water, rock, floor, or path within 5 cells of center, with one “tent”: Floor foundation 3×3 in center, whose **entire outline is a gap** (no walls); minimum distance between camp centers and from buildings 12.
- **Sites (`SurfaceGrid::sites`):** one cell per ruin and per camp (interior cell nearest the foundation center); ruin or camp is “reached” when its site lies in the start component (reachability model from iterations 13/22; gaps like doors without being an entry).

**Derivative changes.** `SurfaceGrid::valid()` checks gaps (on Floor cells) and sites (on Floor); grid fingerprint appends gaps and sites only when non-empty; resolver ignores wall on gap cells; reachability validator counts gaps like passable outline cells, and a building without doors but with gaps is reachable; `PoiReport` gets `sites` and `sitesReached`; generator window: fields “Ruins (%)” and “Camps” in Buildings tab (profile with floor, i.e., 8 brushes, is required also for camps alone).

**Criteria.**
1. `ruinPercent` 0 and `campCount` 0: Seed 1729 fingerprint, 17 core tests, and `style_render` result unchanged.
2. Manual tests: ruin without doors, with gaps: gap cells passable, building reachable; building with same grid but no gaps and no doors closed; camp: disk and tent 3×3 with 8 outline gaps, site in center; validation (`valid()`) rejects gap outside Floor; fingerprint differs from grid without gaps; determinism and mask independence.
3. Measurement (seeds 1001–1032, side 512, profiles v10 m06 and m19, `buildingDensity` 30 / 40, `ruinPercent` 30, `campCount` 6, `fordLength` 48, `repairReach` 1): count of ruins and camps (median [min..max]), ruins as percentage of buildings (expectation near 30 %), gaps per ruin (median), **fraction of ruins and camps reached from start: 100% reachability criterion in ≥ 29 of 32 seeds** (all POI status from repair: 32 of 32), buildings with doors still all reached.
4. 17 core tests and both G1 probes PASS after `rme` rebuild; `--surface` probe gets case with ruins and camps (Preview, Validate, Commit, Undo, Redo, wall element count equals plan without gaps); two runs byte-identical.
5. User appearance assessment takes precedence.

**Caveats.** Ruin is a building with holes in walls, not a collapsed structure (no rubble, no uneven height); camp is a clearing with path and floor but without tent item (no campfires, chests, doodads, because the profile has no brush for them); gaps are random, not “realistically damaged”; I do not verify that the wall brush has all orientations at gaps (walls are placed by `WallBrush::doWalls`, so wall ends at gaps get their own brush orientations); not measured in references; ruins and camps are not yet in the region graph as separate POI (only `sites` in the validator).

### Iteration 26 — campfire in camp (PHASE 5; recorded 2026-11-09 before implementation and before measurement; user decision: “Add campfire there. Let's keep going”)

**Scope.** Camp from iteration 25 (Path clearing with open tent 3×3) gets a **campfire**: one cell `SurfaceGrid::fires` in the clearing center. Item comes from doodad brush selected in profile (`GroundProfile::campfire`, default “campfire” in window when client has the brush), no item IDs in code. New plan layer `GroundPlan::fires` (item per cell, 0 = none), separate from vegetation, so campfire works without “Vegetation” checked.

**Camp geometry changes (at cost of changing iteration 25 results for `campCount` > 0; without `campCount` nothing changes).** Clearing is a Path disk of radius about 4 (cells with dx² + dy² ≤ 18) around center; **campfire in center (Path)**; tent Floor 3×3 offset 3 cells east of center (its center is the `sites` place, outline is 8 gaps), entirely within the clearing; free terrain condition: Grass or Dirt within 7 cells of center, no Floor within 12 cells, camp center spacing ≥ 12.

**Criteria.**
1. `campCount` 0 or empty `campfire`: Seed 1729 fingerprint, 17 core tests, and plan fingerprint without fires unchanged; plan without fires has same fingerprint as before.
2. Tests: each camp has exactly one campfire on Path cell at clearing center, tent does not cover it (campfire cell ≠ Floor cells), `valid()` rejects fire outside Path, grid fingerprint changes with fires, manually counted geometry of one camp (clearing cell count, 8 gaps, 1 fire, 1 site), determinism, new `campCount` without buildings works.
3. `--surface` probe (client 10.98 **has no “campfire” brush**, so case uses any doodad brush with single items, e.g., “small rocks”, as the fire brush and checks only mechanism): after Commit exactly one element of that brush on each fire cell, none on other cells, Undo and Redo restore state; refusals: fire brush unknown, brush not a doodad; plan with fire has different fingerprint than without.
4. Measurement as in iteration 25 (seeds 1001–1032, side 512, m06 / m19, ruins 30 %, 6 camps, `fordLength` 48, `repairReach` 1): camps still 24 per map, fires 24, all sites reached in ≥ 29 of 32 seeds (previously 32 of 32).
5. 17 core tests and both G1 probes PASS after `rme` rebuild; two runs byte-identical.
6. User appearance assessment takes precedence.

**Caveats.** Campfire is an item from selected brush (brush “campfire” in clients 12.90+ has variants: ring, burnt wood, fire; variant selected deterministically per cell, so some fires may be extinguished); in client 10.98 from repo there is no “campfire” brush (there are items 1421–1426 without a brush), so **operation with real campfire will not be tested in the probe, only mechanism**; “Campfire brush” field in window is empty when client has no brush by that name (then no fires, and user can select any doodad); blocking item (e.g., fire field) may block the fire cell in game; campfire is not yet a POI.

### Iteration 27 — Z-entries and simple cave below surface, core part (PHASE 5, “Z-passages” and “caves” points; recorded 2026-11-09 before implementation and before measurement; user decision: “Z-entries and exits”, scope “entries + simple cave”)

**Split into two iterations.** 27 = **core** (entries on surface, cave as separate grid floor z+1, reachability validator through Z, `style_render`). 28 = **adapter** (entry item from doodad brush, cave ground brush, Preview, Validate, Commit and Undo for two floors, window). Iteration 27 draws nothing on the map.

**Deviation from “measure references first” rule.** I do not measure in references entries or caves (requires new probe code); criteria absolute, definitions are my idealization.

**Definitions (new field `SurfaceTuning::caveEntrances`, 0..20 per 65,536 cells, appended at end, 0 = byte-identical as before; new field `SurfaceGrid::entrances`).**
- **Z-entry:** Rock cell on rock edge (at least one 4-neighbor Grass, Dirt, Shore, or Path), at least 3 cells from area edge, no water and floor in 5×5 window, spacing from other entries ≥ 24 (Chebyshev). Cell remains Rock type (passable); list `SurfaceGrid::entrances` (in fingerprint only when non-empty; `valid()` requires Rock).
- **Cave (`generateCave`, new file `underground.{h,cpp}`):** `CaveGrid` with same area, z = z + 1, cells Solid or Floor. Entry has same position (x, y) on lower floor (passage “through hole”, like in Tibia). Around each entry landing (disk dx² + dy² ≤ 5, 21 cells); successive entries (in index order) connected by tunnel with axis offset by noise and width 3; with one entry, tunnel to point toward area center, up to 40 cells; about every 24 cells of axis a chamber (disk radius 3–5); cells within ≤ 1 of area edge become Solid. Result depends only on seed, surface, and options.
- **Z-reachability validator (`undergroundReachabilityOf(SurfaceGrid, CaveGrid, walls)`):** entry is **reached** when its surface cell lies in start component (iteration 22); cave cells are **reached** when they lie in component (4-neighborhood) containing landing of a reached entry. Report: entries, reached entries, Floor cells, reached Floor cells, cave components, **pockets** (components with no reached entry).

**Criteria.**
1. `caveEntrances` 0: Seed 1729 fingerprint, 17 core tests, and `style_render` result unchanged.
2. Manual tests (hand-count): entries lie on rock edge and meet spacing and margin requirements; cave: landing 21 cells, tunnel connecting two entries makes cave have one component, all landings are Floor, area edge Solid; validator on manually built grids (surface with lake blocking one entry: that entry unreached, its cave reached only if connected by tunnel; cave with no reached entry is a pocket); determinism; compare validator against independent search on ≥ 50 random grid pairs.
3. Measurement (seeds 1001–1032, side 512, profiles v10 m06 and m19, `caveEntrances` 4, `buildingDensity` 30 / 40, `pathLength` 40 / 80, `fordLength` 48, `repairReach` 1): entry count (median [min..max]; expectation near 4 × 4 = 16, but only where rock exists), seeds with at least one entry, cave cells per 65,536 cells, cave components; **reachability criterion: in seeds with ≥ 2 entries cave has one component and 0 pockets, and all entries are reached from start in ≥ 90 % of such seeds** (thanks to repair from iteration 23, which should reach POI; repair does not yet know Z-entries, so result may be lower; I report it unadjusted).
4. 17 core tests + new tests, both G1 probes PASS; two runs byte-identical.

**Caveats.** Cave is tunnels and chambers without water, without side passages and without dead ends except when one entry; entries only on rock edge (not in buildings, not stairs); cave exit is same point (x, y) (no exit at other end); reachability repair from iteration 23 does not include Z-entries; no cave under entries forces `caveEntrances` > 0 on rock; no drawing, items, or Undo for two floors yet (iteration 28); multi-floor mountains (several floors below) out of scope.

### Iteration 28 — adapter for Z-entries and cave: items, two floors, Undo, window (PHASE 5; recorded 2026-11-09 before implementation and before measurement; user decisions: “entries + simple cave”, “Two steps”, “Refuse whole”, “Ramp at landing”, “cave enter”, order “cave, then surface”)

**Scope.** Drawing on the map what core of iteration 27 only counts: Z-entry on surface (z), cave on floor z + 1, **cave exit by ramp** at landing of each entry, Preview, Validate, Commit, Undo and Redo for two floors, fields in “Generate Terrain” window and case in G1 probe `--surface`. No item IDs in code: all items from brushes selected in profile.

**Client 10.98 data checked before recording (from `data/1098`, public client data in repo).**
- Doodad “cave enter”: two variants (8203, 8212); in `items.xml` both have `floorchange=down` (range 8202–8203 and 8210–8213). I did not check whether items block movement in client data (probe will, see criterion 5).
- Ground “cave”: outer border id 7 (drawn by RME on empty adjacent tiles, i.e., on Solid cells; this gives cave walls).
- **No single-tile upward exit doodad** (stairs 1386 and 3678 are items without brush and without `floorchange`, work via server scripts). Only doodads with `floorchange` north/south/east/west are ramps (“ramp”, “dirt ramp”, “snowy ramp”, “bamboo ramp”, “pyramid ramp”). Each is a composite with four variants: tile (0, 0) with item `floorchange` in one direction, tile (0, 0, z − 1) with item “stairs” (459, `floorchange=down`) and tail tile opposite the direction (e.g., direction north, tail (0, +1)).

**Definitions.**
- **Profile (new `GroundProfile` fields, appended at end of structure):** `caveEntrance` (entry doodad brush, default in window “cave enter” when client has it), `caveExit` (exit doodad brush, default “ramp”), `caveGround` (cave ground brush, default “cave”). All empty = no cave, byte-identical as before. Require eight-brush profile (like `campfire`) and `caveEntrances` > 0. Set partially (e.g., entry without cave ground) = `InvalidProfile` refusal.
- **Exit shape (`ExitShape`, core, no items):** direction d ∈ {N, S, E, W} and tail offset t. Adapter translates each `caveExit` brush variant to shape from OTB `floorchange` flags. Brush accepted only if **each** variant has exactly: one tile z = 0 at (0, 0) with item `floorchange` N/S/E/W, one tile z = −1 at (0, 0), and at most one additional tile z = 0 adjacent to (0, 0) (tail). Otherwise `InvalidProfile` refusal.
- **Exit placement (pure core function `placeCaveExits(surface, cave, shapes)`, deterministic):** for each entry E (in order of `entrances` list) I search for position P and shape s. Candidates for P are landing cells of E (dx² + dy² ≤ 5) different from E, in order (dx² + dy², y, x); shapes in order of brush variants. Conditions:
  - on z + 1: P and P + t (if tail exists) are cave Floor, neither is E nor cell of another exit;
  - on z: cell P (where “stairs” from z − 1 tile lands) is passable, not Floor nor building outline, not entry, campfire, door, nor vegetation cell;
  - on z: target P + d is passable (Grass, Dirt, Rock, Shore, Path), not entry, campfire, door, outline, or vegetation, and lies in area.
  First match wins. **Entry without matching exit is skipped:** gets no entry item (no one will fall into cave without return), and its landing stays in cave. Report: entries, entries with exit, skipped entries.
- **Drawing:**
  - **floor z:** as before, plus `caveEntrance` brush item on each entry with exit (variant selected deterministically per cell, like campfire) and ramp z = −1 tile item in each cell P;
  - **floor z + 1:** `caveGround` ground on Floor cells, ramp item in P and tail item in P + t (variant s). **Solid cells get no ground**; only border elements of cave ground from adjacent Floor cells can appear on them (RME border logic). Solid cells without Floor neighbor (8-neighborhood) stay without tile. Core leaves Solid in 2-cell band from area edge, so border does not extend beyond area (checked, not assumed).
- **Floor z + 1 protection (“refuse whole”):** Preview refuses (new error, e.g., `LowerFloorOccupied`) if any non-empty tile exists in area rectangle at z + 1. Also refuses when z + 1 > 15. With mask or selection, entire enclosing rectangle applies. Entries (and cells P) outside surface mask are not drawn.
- **Two history steps, order:** Validate checks both floors before any save. Commit = **step 1: cave (z + 1)**, then **step 2: surface (z)**, each as separate “Generate Terrain” action in `ActionQueue`. One Undo reverts surface (entries vanish, cave stays without entry), second Undo reverts cave. When step 2 fails, step 1 is automatically reverted and window shows error: map returns to pre-Commit state, and no generator steps remain in history. No GUI or Lua events fire between steps. Surface staleness check after step 1 permits only step 1 changes (floor z + 1).
- **Budget:** one shared budget for prep and Commit for both floors (no refunds), refusal if exceeded as before.

**Criteria.**
1. Empty `caveEntrance`, `caveExit` and `caveGround` fields or `caveEntrances` 0: Seed 1729 100×100 fingerprint = 17159159863505677800, all core tests, plan fingerprints and probe result unchanged (old cases unchanged).
2. Core tests for `placeCaveExits` (hand-count on manual grids): exit in landing, P ≠ E, target P + d passable and not entry; entry surrounded by water is skipped; two entries do not share exit cells; determinism; compare against independent condition search on ≥ 50 random grids.
3. Measurement (seeds 1001–1032, side 512, profiles v10 m06 and m19, options as in iteration 27: `caveEntrances` 4, `buildingDensity` 30 / 40, `pathLength` 40 / 80, `fordLength` 48, `repairReach` 1, exit shapes = geometry of four 10.98 ramp variants provided to tool as direction and tail, no items): entries, entries with exit (median [min..max]), percentage of entries with exit across seeds. **Criterion: ≥ 90% of entries get exit** in each of two profiles. Two runs byte-identical.
4. G1 probe `--surface`, new case `SURFACE_CAVE_E2E_PASS` (client 10.98: “cave enter”, “ramp”, “cave”):
   - Preview: changes on z and z + 1, entry item exactly on entries with exit, ramp in P and tail in P + t, “stairs” item in P on z, cave ground exactly on Floor cells, no ground on Solid cells, no tile outside rectangle on z + 1;
   - Commit: two new history steps, map equals preview on both floors;
   - Undo once: surface as before Commit, cave stayed; Undo second time: both floors as before Commit (tile comparison); Redo twice: state after Commit;
   - forced step 2 error (e.g., history limit): both floors as before Commit, history without generator steps;
   - refusals: non-empty tile on z + 1 in rectangle; z = 15; entry brush unknown or not doodad; exit brush without `floorchange` (e.g., “small rocks”); cave ground brush not ground; only part of cave fields set;
   - entry and ramp items do not block movement per client data (`isBlocking`); if they do, case reports it, and I record it as deviation without changing criterion;
   - plan with cave has different fingerprint than without.
5. Window: Terrain tab, “Underground” section: “Cave entrances” (0..20), “Z entrance brush” (doodad brush list), “Cave exit brush” (doodad brush list), “Cave ground brush” (ground brush list). Default names only when client has them, otherwise empty. Fields active only with Surface algorithm and buildings (eight-brush profile). Controls on `host`, colors and spacing from `Theme`.
6. All core tests (18 + new) and both G1 probes PASS after `rme` rebuild; SetFocus warnings are only tolerated stderr.
7. GUI appearance and operation assessment belongs to user and takes precedence over metrics.

**Caveats.** Operation in game will not be tested. It is unknown where server places player after `floorchange=down` (I assume same x, y) and after ramp (I assume P + d on z). Ramp direction comes from OTB flags, not drawing. “cave enter” variant is selected per cell, so entry may face wrong direction relative to rock. Cell P on surface is second hole downward (“stairs” 459, usually invisible), so each entry practically has two cave entrances. Reachability repair from iteration 23 still does not know Z-entries. One Undo leaves cave without entry (per “Two steps” decision). With mask entire z + 1 rectangle must be empty, which may reject acceptable cases. Multi-floor mountains, stairs in buildings, and exits at cave other end are out of scope.

### Iteration 29 — cave walls, entry in south rock face or floor hole, exits not to rock (PHASE 5; recorded 2026-11-10 before implementation and before measurement; user decisions after iteration 28 GUI test: “Dungeons don't have walls (just floor), and exits/entries go into rock. Better to make them on rock's south face or floor hole”, in questions: “Both, choice in window”, “Rock ground band”)

**Scope.** Three fixes to what iteration 28 draws: (a) cave walls on z + 1, (b) Z-entry position in one of two styles selected in window, (c) ramp exit never to rock or from rock. No item IDs in code; brushes from profile.

**Client 10.98 data checked before recording (`data/1098`).**
- Ground “cave” has only outer border id 7 (floor edge), not walls. Ground “mountain” has inner borders id 3 (east and south wall) and outer id 10 and 51 (to empty).
- Doodads “cave enter”, “cave enter2”, “cave enter3” (`items.xml`: “cave entrance”, `floorchange=down`), two variants each; **I do not know which variant faces south** (sprite export with tool `tools/tibia_data.py` gave corrupt images, so I don't judge them from data; user judges in GUI).
- Ground-holes with `floorchange=down`: “cave hole” (383), “cave small hole” (392); “grass pitfall” (293) is invisible trap.

**Definitions.**
- **Entry style (new field `SurfaceTuning::caveEntranceStyle`, `enum class` at end of structure, hashed only when `caveEntrances` > 0):**
  - **RockFace** (default): cell E is Rock; south neighbor (x, y + 1) is Grass, Dirt, Shore, or Path (not Floor, not water); north, west, east neighbors are Rock. Entry brush item (doodad, as in iteration 28) on E. Player enters from cell (x, y + 1).
  - **FloorHole:** cell E is Grass or Dirt; its 4 neighbors are Grass, Dirt, Shore, or Path; in 5 × 5 window no water and no Floor. On E, entry brush ground (ground brush with `floorchange=down`, default “cave hole”) instead of surface ground; no doodad.
  - Common to both: at least 3 cells from area edge, spacing ≥ 24 (Chebyshev), same attempt count and same random stream as iteration 27. Entry not on path, door, campfire, outline, or vegetation cell.
- **Ramp exit (change `placeCaveExits`):** on surface P and target P + d must be Grass, Dirt, Shore, or Path. **Rock removed from list** (today allowed, so exits lead into rock). Rest of conditions from iteration 28 unchanged.
- **Walls (new profile field `caveWall`, ground brush, end of `GroundProfile`, default in window “mountain”):** Solid cell on z + 1 with at least one Floor neighbor in 8-neighborhood gets `caveWall` ground. Remaining Solid cells stay without tile. RME borders draw wall faces. 2-cell Solid band at area edge (core) means wall cells lie in area; outer wall borders may enter area cells without ground, but not beyond rectangle (checked, not assumed).
- **Profile:** cave now requires four fields (`caveEntrance`, `caveExit`, `caveGround`, `caveWall`). Missing any while others set = `InvalidProfile`. RockFace requires entry doodad with single items, FloorHole ground with item `floorchange=down`; otherwise `InvalidProfile`.
- **Window, “Underground” tab:** new field “Entrance style” (Rock face / Floor hole); “Z entrance brush” list changes from doodads to grounds by style (default “cave enter” / “cave hole” when client has them); new field “Cave wall brush” (ground list, default “mountain”). Controls on `host`, colors and spacing from `Theme`.

**Criteria.**
1. `caveEntrances` 0 or empty cave fields: Seed 1729 100×100 fingerprint = 17159159863505677800, all core tests, plan fingerprints and old probe cases unchanged. Row `SURFACE_CAVE_E2E_PASS` from iteration 28 **will change** (new entry and exit rules, walls); this is announced change, not deviation.
2. Core tests on manual grids: RockFace (E Rock, N/W/E Rock, S open; rejected: S rock, S water, S Floor, corner); FloorHole (E Grass/Dirt, 4 neighbors open; rejected: Rock, at water, on path); `placeCaveExits` rejects P or P + d on Rock; compare against independent search on ≥ 50 random grids for both styles; determinism.
3. Measurement (seeds 1001–1032, side 512, profiles v10 m06 and m19, options from iteration 28: `caveEntrances` 4, `buildingDensity` 30 / 40, `pathLength` 40 / 80, `fordLength` 48, `repairReach` 1, `caveExitShapes=ramp`), separately for RockFace and FloorHole: entries, entries with exit (median [min..max]), percentage of entries with exit. **Criteria:** (a) median entries ≥ 8 (half of 16 ordered) in each profile and style; (b) ≥ 90% entries with exit in each profile and style; (c) 100% entries meet style rule and 100% exits have P and P + d outside Rock (checked by tool); (d) two runs byte-identical.
4. G1 probe `--surface`, case `SURFACE_CAVE_E2E_PASS` for both styles (client 10.98: RockFace “cave enter”, FloorHole “cave hole”, exit “ramp”, ground “cave”, walls “mountain”):
   - Preview: style rule met for each entry; RockFace: entry item on E; FloorHole: E ground is hole brush item, no entry doodad; P and P + d outside Rock; wall ground exactly on Solid cells with Floor neighbor (8-neighborhood); no ground on remaining Solid; no tile outside rectangle on z + 1;
   - Commit, Undo once/twice, Redo twice and forced step 2 error as in iteration 28, for both styles;
   - refusals: no `caveWall` with other fields; `caveWall` not ground; FloorHole with doodad as entry; FloorHole with ground without `floorchange=down` (e.g., “grass”); RockFace with ground as entry;
   - plans of both styles have different fingerprints.
5. Window as in definitions; user judges appearance and operation.
6. All core tests and both G1 probes PASS after `rme` rebuild, `SUITE_PASS` in two runs; SetFocus warnings are only tolerated stderr.
7. GUI appearance assessment belongs to user and takes precedence over metrics: “cave enter” variant direction on wall, “mountain” wall appearance in cave, hole appearance on grass.

**Caveats.** “cave enter” variant is still selected per cell, not by direction; if one variant faces east, half of entries may look wrong (decision after GUI assessment, e.g., variant selection in window). “mountain” border walls draw faces only from east and south (how that brush works); appearance from north and west is not evaluated. Player after hole descent lands per server (I assume same x, y). Operation in game will not be tested. Fewer entries than iteration 27 is possible, because rules are stricter (criterion 3a).

### Iteration 30 — ramp exit below entry, no hole on surface (PHASE 5; recorded 2026-11-10 before implementation and before measurement; user note after iteration 29 GUI test: “Look how that entry looks - there's a hole before it”, agreement: “yes, do it”)

**Reason.** `placeCaveExits` searches nearest field P (distance² ≤ 5) outside rock; in RockFace style always wins field (x, y + 1) before entry, i.e., the approach field. Ramp template puts “top” item on P one floor up (in 10.98 ground “stairs”, black), which replaces surface ground. Effect: black hole right before entry, on the field player enters from.

**Definitions.**
- **Exit below entry (change `placeCaveExits`):** for entry E only candidate is P = E (same x, y on z + 1). Shapes in list order; shape matches when: target D = E + d (shape direction) meets current surface field condition (Grass, Dirt, Shore or Path, unoccupied by decor, door, campfire, gap, outline or other exit); landing L = E − d (where server moves player after descent on ramp) is cave Floor and not reserved; shape tail (if any) is Floor, not reserved, and different from L. No matching shape = entry skipped (`skipped`), **without further search** (no exit beside entry). Reservations: below P, L and tail; above D. In RockFace only south direction matches (N, W, E are rock); in FloorHole first shape with open target.
- **Surface above ramp:** field E stays as style sets (rock with entry doodad or hole ground). Ramp template “top” item is not placed anywhere; if plan layer `exitTops` no longer applies, it is removed (no dead code), and plan fingerprint with cave changes (announced).
- Skipped entry gets no entry item (as in iteration 28).

**Criteria.**
1. `caveEntrances` 0 or empty cave fields: Seed 1729 100×100 fingerprint = 17159159863505677800, all core tests, plan fingerprints and old probe cases unchanged. Rows `SURFACE_CAVE_E2E_PASS` **will change** (announced).
2. Core tests on manual grids: each exit has P = E; RockFace gets south direction; FloorHole first shape with open target; rejections: target on rock, water, Floor surface, decor; L not Floor; tail not Floor or equals L; no shape = `skipped` without exit elsewhere. Compare against independent search on ≥ 50 random grids for both styles; determinism.
3. Measurement (seeds 1001–1032, side 512, profiles v10 m06 and m19, options as in iteration 29), separately for RockFace and FloorHole: entries, entries with exit (median [min..max]). **Criteria:** (a) ≥ 90% entries with exit in each profile and style; (b) 100% exits have P = E, D outside rock on open field and L on Floor (checked by tool); (c) two runs byte-identical. RockFace entry count reported for record; criterion 3a from iteration 29 (median ≥ 8) still not met and not re-evaluated here (TECH_DEBT D24).
4. G1 probe `--surface`, `SURFACE_CAVE_E2E_PASS` for both styles: ramp on z + 1 exactly below each entry with exit; on surface no ramp template “top” item; E has entry item (RockFace) or hole ground (FloorHole); D on open field without ground change; L has cave ground; rest of checks from iteration 29 (walls, nothing outside rectangle, Commit, Undo once/twice, Redo twice, forced step 2 error, refusals) unchanged.
5. All core tests and both G1 probes PASS after `rme` rebuild, `SUITE_PASS` in two runs; SetFocus warnings are only tolerated stderr.
6. GUI assessment belongs to user: no black hole before entry, ramp in cave below entry.

**Correction after implementation, before measurement (2026-11-10): specification deviation.** Condition “tail different from L” was specification error: in all 10.98 ramp variants tail (ramp bottom, item without `floorchange`) lies exactly on L, so rule rejected every exit (implementer: 0 exits, `SUITE_FAIL`). Applies: tail may lie on L; still must be Floor and not reserved. In criterion 2 case “tail equals L” is accepted, not rejected. Remaining criteria unchanged.

**Caveats.** Player displacement after descent on ramp (to L) and after ramp entry (to D) I describe from memory of TFS-like server behavior; I don't verify server code, game operation cannot be verified in the editor.

### Iteration 31 — entry above each ramp in GUI, cave without buildings (PHASE 5; recorded 2026-11-10 before implementation and before measurement; user notes after iteration 30 GUI test: “no entry despite stairs”, “underground should probably be available also without buildings”, agreement: “Yes, do it”)

**Symptoms.** (a) In GUI after Commit on z + 1 there is a ramp, and on surface above it is just rock without entry item (user screenshots, area x 1018, y 1237, z 7). Probe `SURFACE_CAVE_E2E_PASS` (area 256, one seed) does not detect this. Cause unknown at time of recording. (b) Window enables “Underground” tab and passes `caveEntrances` only when `buildingDensity` > 0 or `campCount` > 0 (`terrain_generator_dialog.cpp`, `UpdateControlStates` and request build); core does not require this.

**Definitions.**
- **Diagnosis (a):** reproduce error in probe in settings like from GUI (rectangle not at map start, side ≥ 512, vegetation, paths, both styles, buildings 0 and > 0, ≥ 8 seeds), find cause and describe in report with proof. Fix at cause location, without workaround in probe.
- **Window (b):** “Underground” available for any natural surface; when `caveEntrances` > 0 profile has floor field (`surfaceWithFloorCount`), so “Floor brush” field is then active and required; buildings and camps work as before.

**Criteria.**
1. `caveEntrances` 0: Seed 1729 100×100 fingerprint = 17159159863505677800, all core tests, plan fingerprints and old probe cases unchanged.
2. New probe case `SURFACE_CAVE_GUI_PASS`: for each combination from definitions after Commit each ramp on z + 1 has entry item above (RockFace) or hole ground (FloorHole), each entry has ramp below; entry and exit counts in row. Before fix case must fail (proof of reproduction in report); if cannot reproduce, report says so and lists what checked.
3. Window: when `buildingDensity` 0 and `campCount` 0, “Cave entrances” field and cave fields are active, request carries `caveEntrances`, and probe with buildings 0 passes criterion 2.
4. All core tests (rebuilt `test_procedural_*` and `style_render` targets, D25) and both G1 probes PASS after `rme` rebuild, `SUITE_PASS`.
5. GUI assessment belongs to user: entry above each ramp, cave without buildings.

### Iteration 32 — plateau on mountains (z − 1) with vegetation, ramp entry (PHASE 5; recorded 2026-11-10 before implementation and before measurement; user decisions: “1 floor with vegetation”, “Ramp from list”)

**Scope.** On large rock fields of surface (z) floor z − 1 is created: plateau with grass and vegetation in center, rock at edge and ramp from surface up. No item IDs in code; brushes from profile.

**Definitions.**
- **New field `SurfaceTuning::plateauMinTop`** (at end of structure, 0 = disabled, 16..4096, hashed only when > 0, outside style profile): minimum cell count of plateau Top.
- **Core, `generatePlateau(SurfaceGrid, seed)` → `PlateauGrid`** (same rectangle, z − 1; cells None, Edge, Top) and ramp list:
  - rock component (4-neighborhood) of surface; **Top** = rock cells at distance (Chebyshev) ≥ 2 from non-rock cell and ≥ 3 from area edge; Top component (4-neighborhood) smaller than `plateauMinTop` dropped entirely;
  - **Edge** = rock cells of that component at distance 1 from Top (rock band around Top on z − 1);
  - **Ramps**: 1 per plateau plus 1 per full 400 Top cells, at most 4, spacing ≥ 16 (Chebyshev) between ramps; ramp is field P on surface (Grass, Dirt or Path, without decor, door, campfire, gap, outline, Z-entry and cave exit) with shape from shape list; target D = P + direction on z − 1 should be Top cell or Edge cell converted to **landing** (then its neighbor in ramp direction must be Top); shape tail (if any) lies on surface, on open field like P; order: fixed random stream, deterministically;
  - plateau without ramp is removed (None) — no unreachable plateaus;
  - **vegetation on Top**: same algorithm as `generateDecoration`, separate random stream, only on Top cells (landings and cells beside ramp without decor).
- **Profile:** new fields `plateauGround` (Top ground, default “grass”), `plateauEdge` (Edge ground, default “mountain”), `plateauRamp` (brush from ramp list like “Cave exit brush”, default “ramp”); all three or none, otherwise `InvalidProfile`. On landing, Top ground. The “top” template item is not placed (as in iteration 30). Vegetation items from the same brushes as surface.
- **Adapter and history:** floor z − 1 is the next preparation unit; floor z − 1 must be empty in the entire rectangle (like z + 1 for caves), otherwise refusal with message in window. Each floor is a separate history step (plateau, cave, surface — order to be determined by implementer and described), Commit, Undo and Redo all steps; error of any step reverts previous (like iteration 28). If `beginTerrainPair` / `finishTerrainPair` handles only two steps, generalization to N steps in separate commit.
- **Window:** in “Underground” tab (or new “Levels”, implementer's decision with justification) fields “Mountain tops (min cells)”, “Plateau ground brush”, “Plateau edge brush”, “Plateau ramp brush”; preview of floor z − 1 in “Preview floor” choice. Colors and spacing from `Theme`. Available without buildings.

**Criteria.**
1. `plateauMinTop` 0: Seed 1729 100×100 fingerprint = 17159159863505677800, all core tests, plan fingerprints and old probe cases unchanged.
2. Core tests on manual grids: Top / Edge / None per definition, rejection of small component, ramp (P open, D on Top or landing, tail open), plateau without ramp removed, ramp spacing, vegetation only on Top; comparison with independent search on ≥ 50 random grids; determinism.
3. Measurement (seeds 1001–1032, side 512, profiles v10 m06 and m19, options as iteration 29 plus `plateauMinTop` 64; in `style_render` tool): plateaus, Top cells, ramps (median [min..max]). **Criteria:** (a) 100 % plateaus have ≥ 1 ramp; (b) 100 % Top cells reachable (4-neighborhood through Top and landings) from target of some ramp; (c) 100 % ramps meet P / D / tail rule; (d) two runs byte-identical. Plateau count reported without threshold (depends on profile).
4. G1 probe `--surface`, new case `SURFACE_PLATEAU_E2E_PASS` (client 10.98: “grass”, “mountain”, “ramp”): Top / Edge / landing ground exactly per plan, nothing outside rectangle on z − 1, ramp on surface at P and tail, no “top” item, vegetation only on Top, Commit, Undo all steps in order, Redo, forced step error reverts previous, refusal when z − 1 occupied, profile refusals (missing one of three fields, Edge not ground, ramp not from list); case with plateau and cave together.
5. All core tests (rebuilt targets, D25) and both G1 probes PASS after `rme` rebuild, `SUITE_PASS`.
6. GUI assessment belongs to user: plateau appearance (edge, grass, trees), ramps upward, z − 1 rendering offset.

**Caveats.** Edge from “mountain” ground on z − 1 draws faces only from east and south (like cave walls). Entry by ramp to z − 1 I describe from server behavior memory; operation in game is not verified. When z has Z-entry to cave in rock wall, Edge plateau may lie above it (permitted).

### Iteration 33 — plateau without walls, floors z − 2 and z − 3, ramps between floors (PHASE 5; recorded 2026-11-10 before implementation and before measurement; user decisions: “floor +1 should not have walls”, “Grass to edge”, “ramps everywhere selected”, z − 3 only when space for at least 30 cells)

**Scope.** Change from iteration 32: no rock band at plateau edge and new floors z − 2 and z − 3. On all upward transitions the same selected ramp (`plateauRamp`) stands.

**Definitions.**
- **No walls on z − 1:** Edge cells from iteration 32 get Top ground (`plateauGround`), not `plateauEdge`. Still without vegetation (band at edge without trees). Profile field `plateauEdge` removed from profile, window and validation. If removal changes profile format, implementer describes backward compatibility: old file with `plateauEdge` loads, field ignored or produces clear error, decision with justification. Fingerprint at `plateauMinTop` 0 unchanged.
- **Floor k (k = 2, 3), new field `SurfaceTuning::plateauLevels`** (1..3, default 1, hashed only when > 1, at end of structure):
  - **Base** = cells covered on floor k − 1 (Top and old Edge, without landings and without ramp, tail and target cells).
  - **Top_k** = base cells at distance (Chebyshev) ≥ d_k from cell outside base, band_k = base cells at distance d_k − 1 (width 1). **d_k is not constant (user decision 2026-11-10: “each successive floor may occupy same % as previous, not hard offsets”):** d_k grows from 4 (minimum so tail and P fit on floor k − 1 at straight edge; implementer correction) upward, until cell count of floor k (Top_k, band_k and corridors) ≤ `plateauMaxPercent` % cell count of floor k − 1. Without limit (`plateauMaxPercent` 0) d_k = 4. When ramp target D of floor k is not in Top_k or band_k, landing corridor goes like on z − 1 (width 1, through base, to first Top_k cell, length ≤ d_k, counted in limit). Criterion 3 (g): floor k ≤ 10 % floor k − 1 in each seed.
  - Top_k component (4-neighborhood) smaller than **30 cells** dropped entirely.
  - **Band_k** = base cells at distance 1 from Top_k. Lie on floor k, have Top ground and no vegetation.
  - Floor k − 1 under floor k stays unchanged (no ceilings in Tibia, floor k drawn above with offset).
- **Ramps on floors k ≥ 2:** field P lies on floor k − 1 (Top without vegetation, outside projection of floor k, outside targets and tails of other ramps), shape from list like iteration 32. Target D = P + direction lies on floor k (Top_k or landing from band_k, then neighbor in that direction is Top_k). Tail, if any, lies on floor k − 1, on open field. Ramp count as iteration 32 (1 + 1 per 400 cells, at most 4, spacing ≥ 16). Floor without ramp removed with floors above it. Vegetation removed from P and tail on floor k − 1, neighbors of P on floor k − 1 stay open.
- **Vegetation** on Top_k like z − 1 (separate random stream per floor), no vegetation on band_k and landings.
- **Adapter and history:** each floor z − 1, z − 2, z − 3 must be empty in entire rectangle, otherwise refusal with message. Each non-empty floor is separate history step. Order: highest floor first, then lower, cave, surface. Error of any step reverts all previous. When area lies at z = 0..2, floors going below z = 0 rejected in validation with message (not silently truncated).
- **Window, “Levels” tab:** removed field “Plateau edge brush”, added “Mountain levels” (1..3). “Preview floor” shows each floor from result.

- **Plateau size limit (supplement 2026-11-10, user decisions “add limit now”, “better expressed as %”), new field `SurfaceTuning::plateauMaxPercent`** (0 = no limit, else 1..100, hashed only when > 0, at end of structure): share of floor z − 1 cells (Top and band) in rectangle cell count. Single shared distance d from rock edge grows from 2 (2, 3, 4, …), until sum of z − 1 cells ≤ `plateauMaxPercent` % rectangle. At each d, Top components (4-neighborhood) < `plateauMinTop` dropped. Band always width 1: rock cells at distance 1 (Chebyshev) from Top. Rock cells outside Top and band stay without floor. **Landing corridor (correction after implementer conflict report):** ramp rule from iteration 32 remains (P outside rock, D = P + direction on rock cell). When at d ≥ 3 cell D is not in Top or band, from D in ramp direction goes straight corridor width 1 through rock cells, until first Top cell. Corridor belongs to floor z − 1 (Top ground, no vegetation) and counts in percentage limit. Ramp permitted only if corridor entirely on rock and length ≤ d. Ramps with and without corridor follow same random order and spacing. When adding corridors exceeds limit, d grows further (condition checked after adding corridors). Floors z − 2 and z − 3 count from z − 1 result without additional limit (minimum 30 cells). Window: field “Max plateau area (%)”, window default 10. Measurement in criterion 3 with `plateauMaxPercent=10`, additional criterion (f): z − 1 cells ≤ 10 % rectangle in each seed. Criterion 2 includes limit (manual grid and comparison with independent search).

**Criteria.**
1. `plateauMinTop` 0: Seed 1729 100×100 fingerprint = 17159159863505677800, core tests, plan fingerprints and old probe cases (without plateaus) unchanged. Plateau cases from iteration 32 change knowingly (no Edge), change is described.
2. Core tests on manual grids: z − 1 without Edge ground, Top_k and band_k per definition, rejection of component < 30, ramp of floor k (P, D, tail, P outside projection of floor k), removal of floor without ramp with higher floors, vegetation only on Top_k. Comparison with independent search on ≥ 50 random grids for `plateauLevels` 3. Determinism.
3. Measurement (seeds 1001–1032, side 512, m06 and m19, options as iteration 32 plus `plateauLevels=3`, in `style_render`), per floor: plateaus, Top cells, ramps (median [min..max]). **Criteria:** (a) each plateau of each floor has ≥ 1 ramp from floor below; (b) 100 % cells of each floor reachable from surface via ramps (search through floors); (c) 100 % ramps meet P / D / tail rule; (d) each component z − 3 has ≥ 30 cells; (e) two runs byte-identical.
4. G1 probe `--surface`: case `SURFACE_PLATEAU_E2E_PASS` updated (no edge ground), new case `SURFACE_PLATEAU_LEVELS_E2E_PASS` (3 floors). Checked: ground and ramps of each floor exactly per plan, nothing outside rectangle, no “top” item, Commit, Undo all steps in order, Redo, forced step error reverts previous, refusal when z − 2 occupied, refusal when z too low, case with cave together.
5. All core tests (rebuilt targets) and both G1 probes PASS after `rme` rebuild, `SUITE_PASS`.
6. GUI assessment belongs to user: appearance of floors without walls, ramps between floors and plateau size.

**Caveats.** Without walls, floor edge visible only as ground edge (grass autoborder). In game from lower floor level no obstacle at edge. Operation in game not verified. Plateaus z − 1 with m19 occupy about 22 % area (iteration 32); in this iteration I don't change that.

### Iteration 34 — mountain as cap on its own slope (PHASE 5; recorded 2026-11-10 before implementation and before measurement)

**Reason.** User assessment of iteration 33 in GUI (screenshot, z 6): higher floors “hang in air”, grass doesn't reach edge, plateau almost without vegetation, landing corridor sticks into void, ramp stands alone in grass. Analysis of 20 reference maps (local, `local-data/MOUNTAIN_REFERENCE_ANALYSIS.md`, not published): under cap of floor u lies slope from `mountain` brush on floor u + 1 **at same (x, y)** (34817 of N = 59362; +1/+1 offset less common); cap grass reaches last edge cell with grass autoborder to empty fields; each successive floor has own slope; ramp stands on cell just at slope, arrival lies on cap; vegetation on peaks with grass has share similar to surface or higher (M06 14.3 % vs 11.9 %, M19 44.4 % vs 36.9 %, forgotten 31.6 % vs 23.2 %).

**Changes from iteration 33 (replace its geometry).**
- **Cap z − 1 = entire surface rock component** (4-neighborhood), without offset, without band and without corridors. Rock surface under cap becomes `mountain` ground (slope, same (x, y)). Component < `plateauMinTop` doesn't get cap.
- **Limit `plateauMaxPercent`** for z − 1: components in fixed random order, component gets cap if sum of cap cells with it ≤ limit; otherwise skipped (stays as rock). No component clipping.
- **Floor k ≥ 2:** cap C_k = cells of C_{k − 1} at distance (Chebyshev) ≥ d_k from cell outside C_{k − 1}, d_k from 3 upward, until |C_k| ≤ `plateauMaxPercent` % |C_{k − 1}| (no limit d_k = 3); C_k components < 30 dropped. **Under C_k on floor z − k + 1 (same (x, y)) `mountain` ground** (slope), not grass; rest of C_{k − 1} is grass (terrace).
- **Slope brush:** new profile field `plateauCliff` (default “mountain”, must be ground brush with autoborder; in 10.98 not “mountain ground”). Drawn by autoborder (`DrawOperations`), no manual wall items. On surface slope is existing rock ground.
- **Cap edge:** grass (`plateauGround`) to last cell, framing from grass autoborder. No band without grass.
- **Ramps:** head H on lower floor on passable terrace cell (for z − 1: Grass, Dirt or Path of surface) 4-adjacent to slope cell; H + direction on lower floor is slope, on higher floor cap cell (arrival). Tail H − direction on same terrace, open. **On higher floor at (x, y) of head placed opening item from ramp template (in 10.98 “stairs”)** — change from iteration 30, justified by references (M19: 370 of 438 ramps, forgotten: 78 of 92); applies only to mountain ramps, not cave ramps. Arrival and its 8 neighbors on cap without vegetation. Ramp count and spacing as iteration 32. Cap without ramp removed with higher floors.
- **Vegetation on caps:** same algorithm and same density as surface (no reduction), separate random stream per floor, only on cap grass and terrace (not on slope, arrivals and openings).
- **Omitted from iteration 33:** landing corridors, band, minimum offset 4. `plateauLevels`, percentage limit, history (highest floor first), refusals (occupied floor, z range) remain.

**Criteria.**
1. `plateauMinTop` 0: Seed 1729 100×100 fingerprint = 17159159863505677800, core tests, plan fingerprints and probe cases without mountains unchanged.
2. Core tests on manual grids and comparison with independent search on ≥ 50 grids: C_1 equal to selected components, limit, C_k and slope under C_k at same (x, y), ramps (H, tail, arrival, opening), removal without ramp, vegetation only on grass; determinism.
3. Measurement (seeds 1001–1032, side 512, m06 and m19, options as iteration 33 unchanged numerically): per floor caps, cells, ramps, share of cells with vegetation on cap grass and on surface grass. **Criteria:** (a) each cell of C_k (k ≥ 1) has below it (same x, y) slope; (b) each cell z − k + 1 under C_k is slope, not grass; (c) 100 % cap cells reachable from surface; (d) percentage limits; (e) vegetation share on cap grass ≥ 70 % of share on surface grass in same seed (median); (f) two runs identical.
4. G1 probe `--surface`: mountain cases updated: `mountain` ground under each cap (after autoborder same ground brush), grass to edge, opening item on higher floor at (x, y) of head, no opening item at cave ramps, Commit, Undo successive steps, Redo, error of last step, refusals, old profile loaded.
5. All core tests and both probes PASS after rebuild, `SUITE_PASS`.
6. GUI assessment belongs to user (with lower floors visible): mountain stands on slope, grass at edge, ramps at slope, trees on peaks.

**Caveats.** Slope from “mountain” autoborder has faces from east and south; corners and rendering offset appearance not verified. References also contain rare “shells” without ground inside; here slope is full. Black field outside cap in screenshot may depend on lower floor view in editor.

### Iteration 36 — reachability validator across all floors and G5 gate (PHASE 5; recorded 2026-11-11 before implementation and before measurement; user decision: “make validator — biggest blocker”)

**Scope.** Single validator for entire generation result: surface z, cave z + 1, mountain caps z − 1..z − 3, with Z-passages. Result visible in window, with seed rejection. G5 gate measurement with all features at once.

**Definitions.**
- **Walking model (core, new `levelReachabilityOf`):** node = cell (x, y, floor) from plan, edges 4-neighborhood on same floor between passable cells and Z-edges. Passable: surface open ground (Grass, Dirt, Path, Shore, Sand, ford), building floor through doors, cave floor, plateau grass, landings, arrivals and ramp openings. **Impassable:** water, wall without doors, cave wall, **slope under cap (surface rock or `mountain` under C_k)**, cell with blocking vegetation. Blocking vegetation: vegetation classes whose items in client have blocking flag (trees, large stones, etc.). Core receives this classification as input, without IDs. **Supplement after implementer report (2026-11-11):** “small rocks” brush in 10.98 has blocking and non-blocking variants, so vegetation class insufficient. Core accepts **per-cell blocking mask** (optional). Adapter computes it from items actually selected resolving plan (client flags, no IDs in code); variant selection and generator result unchanged. Without mask (e.g., `style_render` without client) cautious rule applies: vegetation class any variant blocks → blocks; G5 gate measurement in criterion 4 uses this rule, report states it with results. Map agreement in criterion 3 checked with adapter mask. Surface rock outside caps: behavior from iteration 13 (passable) **changed to impassable** if rock ground items in client block; implementer bases decision on 10.98 data and describes. Then old `reachabilityOf` results unchanged (new function, old kept for old tests).
- **Z-edges:** cave entry (surface cell) ↔ z + 1 landing; cave exit ramp: head on z + 1 ↔ arrival on z; mountain ramp: head H on lower floor ↔ arrival H + direction on higher floor. All bidirectional (per Tibia floorchange behavior; not verified in game).
- **Start:** as iteration 22 (passable surface cell nearest center). **POI:** building doors, camps, cave entries and exits, each cap (each its passable cell), each cave cell.
- **Report:** per POI category reached / total, count of unreachable pockets per floor, up to 16 first unreachable POI with (x, y, z), deterministic order.
- **Seed rejection in window:** new field “Retry seeds until all places are reachable” (0..16 attempts, default 0 = disabled). Next seeds as in `generateReachableSurface` (same formula), but evaluated by `levelReachabilityOf` of entire plan. Window shows used seed and attempt count.
- **Window:** after Preview status line with validator result (e.g., “Reachable: 412/412 places on 5 floors” or “Unreachable: 3 places (first: x, y, z)”), colors and spacing from `Theme`. Place with unreachable POI shows warning with option to continue (not blocking). No changes to history and Commit.

**Criteria.**
1. Seed 1729 100×100 fingerprint = 17159159863505677800, old core tests, plan fingerprints and old probe rows unchanged (validator only reads plan).
2. Core tests: manual grids for each Z-edge (cave entry, exit ramp, mountain ramp k = 1..3), slope impassable, blocking vegetation, wall with and without doors, unreachable pocket on z − 2 detected, report with order. Comparison with independent 3D search on ≥ 100 random multi-floor plans. Determinism.
3. G1 probe `--surface` (client 10.98): for plan with buildings, cave and 3 mountain floors after Commit **model-map agreement**: each passable cell in model has ground on map and no item with blocking flag; each impassable cell due to vegetation or slope has blocking item or no passable ground; each Z-edge has item with floorchange on map at proper location. Row `LEVEL_REACHABILITY_E2E_PASS` with numbers. Seed rejection: case where first seed has unreachable POI, next one doesn't (chosen by implementer), gives saved seed and attempt count.
4. **G5 gate measurement** (seeds 1001–1032, side 512, m06 and m19, `buildingDensity` 30 / 40, `pathLength` 40 / 80, `ruinPercent` 20, `campCount` 4, `fordLength` 48, `repairReach` 1, `caveEntrances` 4, `caveExitShapes` ramp, `plateauMaxPercent` 10, `plateauLevels` 3; in `style_render`): per POI category reached / total (sum and median), pockets per floor, seeds with full reachability. **G5 criteria:** (a) without seed rejection: full reachability all POI in ≥ 30 of 32 seeds each profile; (b) with rejection (≤ 8 attempts): 32 of 32; (c) 100 % Z-edges passable from both sides; (d) two runs identical. When (a) doesn't succeed, implementer states reasons (categories and examples) and **does not fix generator this iteration**; fixes in next, with separate criteria.
5. All core tests (rebuilt targets) and both probes PASS after `rme` rebuild, `SUITE_PASS`.
6. GUI assessment belongs to user: status line after Preview, warning at Place, seed rejection.

**Caveats.** Model doesn't check game server movement (floorchange, monster collisions, moving objects). Diagonal steps omitted (4-neighborhood), so model more cautious than game. Generator doesn't create peaks without ramps from iteration 32, so “caps” category should come 100 % if ramps correct.

### Iteration 37 — multi-floor reachability repair, bridges, D22 closure (PHASE 5; recorded 2026-11-12 before implementation and before measurement; user decision: “D22 — take it now”)

**Reason.** Iteration 36: G5 gate FAIL (0 of 32 seeds in m06 and m19). Causes: repair from iteration 23 uses old model (vegetation ignored, rock passable), mountain slopes cut land, cave entries on slope. From D22 remain: bridges with separate item and real repair (validator in window, seed rejection and client passability check done in iteration 36).

**Definitions.**
- **Cave entries:** not on slope cell or under cap: entry only on surface rock above which (same x, y) no cap exists, with landing as before. Generation order: caps before entries or filter after caps (implementer decision, described).
- **Repair (new, `repairLevelReach`, core):** after generating entire plan (with caps and cave), while unreachable POI exists: cheapest path (Dijkstra, 4-neighborhood, deterministic ties by (y, x)) on POI floor from any cell of POI component to any cell of start component, cost: passable 1, blocking vegetation 3 (removed), water 6 (becomes bridge when path crosses water on ≤ `fordLength` successive cells), rock without cap 1, **slope and wall forbidden**. After repair re-evaluate `levelReachabilityOf`. On cap floors path through cap floor cells (vegetation cost 3, higher floor slope forbidden). When no path for cap: **cap and higher above removed**, its slope returns to surface rock (passable). When POI on surface has no path: POI removed (building, camp, cave entry), count in report. Limit 64 repairs per plan; exceeding it reports “unrepaired”.
- **Bridges (correction after implementer report 2026-11-12: both “bridge” variants in 10.98 are passable ground, so bridge replaces water ground, water underneath doesn't stay):** water cells on repair path get new type `Bridge` with bridge ground instead of water; further description below in brush and variant selection scope. Original text: water ground stays, bridge item from profile `bridgeBrush` placed on it (in 10.98 brush “bridge”, type wall; implementer checks whether draws correctly by DrawOperations and whether passable — client flags; if not, reports and stays as sand ford like iteration 14b, without changing criteria). Bridge passable in model. Without bridge brush: ford (old behavior).
- **Field `repairReach`** (existing) switches new repair instead of old; old behavior at `repairReach` 0 unchanged. Window: existing “Repair unreachable places” + new “Bridge brush” (brush list, default “bridge”, empty = fords).

**Criteria.**
1. `repairReach` 0: Seed 1729 100×100 fingerprint = 17159159863505677800, old tests, fingerprints and probe rows unchanged (except knowingly changed cases with repair, described).
2. Core tests: manual grids — vegetation passage, bridge across river, slope forbidden, cap removal without path and rock return, POI removal without path, cave entry not under cap, repair limit; comparison with independent Dijkstra on ≥ 100 random plans; determinism.
3. G1 probe `--surface` (10.98): plan with bridge — bridge item on water after Commit, bridge cell without blocking flag, model agreement with map (like iteration 36), Undo/Redo; plan with removed vegetation — no blocking item on path; new row `LEVEL_REPAIR_E2E_PASS`.
4. **G5 gate** (settings from iteration 36 plus `repairReach` 1, cautious vegetation rule in `style_render`): (a) without retries full reachability in ≥ 30 of 32 seeds each profile; (b) with ≤ 8 attempts 32 of 32; (c) 100 % Z-edges passable; (d) two runs identical. Additionally reported: repair count, bridges, removed caps, removed POI (median [min..max]). **(e)** removed POI ≤ 2 % all POI each profile (repair cannot “pass” gate by deletion).
5. All core tests and both probes PASS after rebuild, `SUITE_PASS`.
6. GUI assessment belongs to user: bridges, paths through forest, mountains without peaks after repair.

**Caveats.** Bridge wall type in 10.98 may have directions (horizontal, vertical); implementer describes variant selection. Removed vegetation may look like cut-through. Game movement not verified.

### Iteration 38 — cap vegetation without splitting, cave entries before caps, cave only with entry (PHASE 5; recorded 2026-11-12 before implementation and before measurement; user agreement)

**Reason.** Iteration 37: G5 gate m06 32 / 32, m19 1 / 32 (7 / 32 with retries). Causes m19: dense blocking vegetation splits caps into hundreds of pockets (seed 1001: 395), repair hits limit 64; caves without entry (all entries filtered under caps) unreachable. In m06 filter left 1 entry per 32 seeds.

**Definitions.**
- **Cap vegetation (only `repairReach` 1):** blocking vegetation on cap cell (each floor) placed only if doesn't increase count of passable components of that floor within cap (exact check, deterministic row order). Non-blocking vegetation unchanged. Arrivals, openings and their neighbors without vegetation as before. Surface unchanged (repair handles it in m06 and m19).
- **Cave entries before caps (only `repairReach` 1):** entries selected like iteration 29 (rock at open ground, spacing), **before** caps are created. Entry cell and its 8 neighbors excluded from C_1 (notch in cap; rock under notch stays surface rock, not slope). Higher caps counted from C_1 after notch. Filter of entries under caps from iteration 37 no longer needed (removed if dead).
- **Cave only with entry:** cave component without any entry not generated (Solid), so its cells not POI. Report gives count of such removed components.

**Criteria.**
1. `repairReach` 0: Seed 1729 100×100 fingerprint = 17159159863505677800, old tests and probe rows unchanged (except memory estimates).
2. Core tests: manual grids — vegetation doesn't split caps, notch around entry and rock under it, cave without entry removed; comparison with independent search on ≥ 100 random plans; determinism.
3. G1 probe `--surface`: case with entry in cap notch — entry item, no slope under notch, Z-edge passable, Commit, Undo, Redo; new row `LEVEL_ENTRANCE_NOTCH_E2E_PASS`.
4. **G5 gate** (settings from iteration 37): (a) without retries ≥ 30 / 32 each profile; (b) with ≤ 8 attempts 32 / 32; (c) 100 % Z-edges passable; (d) two runs identical; (e) removed POI ≤ 2 %. Additionally: **(f)** cave entries median ≥ 8 per seed each profile (m06 in iteration 27: 13); **(g)** vegetation share on cap grass ≥ 70 % of share on surface grass (median, like iteration 34).
5. All core tests and both probes PASS after rebuild, `SUITE_PASS`.
6. GUI assessment belongs to user: notches at entries, trees on peaks.

**Caveats.** Notch in cap is visible rock cove at entry; if looks bad, next iteration entry in slope. Vegetation rule changes tree distribution on peaks (less dense clusters).

## Iteration 39: repair on final mask and cave entry count (2026-11-12)

Binding before implementation.

1. **Repair after autoborder.** Adapter (window and probe) after brush resolution and autoborder builds blocking mask from actual client items and, if `levelReachabilityOf` finds unreachable targets, re-runs repair on this mask (vegetation/cut-through removal by DrawOperations, in same Undo action). Loop to stability, total limit 64 repairs per plan. First diagnose and record in report what blocks (1149,1026,7) for seed 1212 in window (m19-like settings from screenshot: 10 entries, mountains 10 %, 3 levels) — concrete item/brush name and why earlier repair didn't see it.
2. **E2E GUI test:** seed 1212 with these settings ends without `Unrepaired`; new probe marker `FINAL_MASK_REPAIR_E2E_PASS`; Undo restores empty map, Redo identical result.
3. **Cave entries (criterion f iteration 38):** median placed entries ≥ 8 at `caveEntrances=16` (measurement settings as iteration 38) for m06 and m19. Allowed: larger RockFace selector retry budget, loosening Rock requirement from three sides to two while keeping exit open, removing vegetation at exit target instead of rejecting entry. 24-cell spacing remains.
4. Without regression: criteria (a)–(e), (g) iteration 38, Seed 1729 100×100 fingerprint = 17159159863505677800 (if selector change changes it, only at `repairReach=1`; old paths unchanged), two runs byte-identical, 18/18 tests, `SUITE_PASS`.

## Iteration 40: unreachable islands only sand (2026-11-12)

Binding before implementation. GUI report: on sea are sand islands with dirt on them.

1. First diagnose where dirt on islands comes from (semantic type, brush, vegetation or path) and record in report.
2. Island is surface land component (z=7) completely surrounded by water, without connection to main land component by ford, bridge or path, and without POI. Each cell of such island gets Shore type (sand): no dirt, grass, paths, vegetation, caps or cave entries. Resolved in core before repair and brush resolution, deterministically.
3. Islands connected to land (ford, bridge) or containing POI stay unchanged.
4. Core test with manual case (island with dirt → only sand; island with ford → unchanged) and 120 random plans: no unreachable island has cell other than Shore. G5 measurement: island count and non-Shore cells on unreachable islands = 0 for m06 and m19.
5. Without regression: criteria (a)–(g) iterations 38–39, determinism, 18/18 tests, `SUITE_PASS`. Seed 1729 100×100 fingerprint may change only if that seed has such islands; justify change in report.

## Iteration 41: far cave exits and corridors with doors (2026-11-12)

Binding before implementation. User decision: work order “far cave exits + corridors with doors”.

1. **Far exit.** For each cave component (z + 1) with at least one entry, core tries to place **far exit**: ramp up to surface in Floor cell of that component distant (BFS along Floor) from each entry of component by ≥ 24 cells; when component lacks such cell, tunnel (width 3, as iteration 27) extended from farthest cell toward open surface ground 24..48 cells. Target on surface: open ground (Grass, Dirt, Path, Shore), not rock, not under cap, outside POI, spacing ≥ 12 from other entries/exits. Ramp shape and reservation as `placeCaveExits` (without entry above ramp: far exit is exit only, no hole on surface). Deterministically, ties by (y, x).
2. **Measurement (a).** Seeds 1001–1032, side 512, settings iteration 39: share of cave components with entry that got far exit, ≥ 80 % (median across seeds) in m06 and m19; mean BFS distance far exit → nearest entry ≥ 24.
3. **Corridors with doors.** Option in “Underground” tab (default disabled, old results unchanged when disabled): part of chamber connections (at least one per component if has ≥ 2 chambers) becomes corridor width 1, length 3..8 with one **door** cell in middle; on both sides of door (perpendicular) Solid or wall band. Doors from door/wall brush available in 10.98 data (implementer chooses brush name from `data/`, records new profile field `caveDoor`; no IDs in code; without brush option disabled with message in window). Doors closed, not key-locked; in validator door cell passable (like building doors).
4. **Measurement (b).** When option enabled: ≥ 1 corridor with doors in ≥ 80 % seeds in both profiles; each door cell has exactly two passable neighbors on opposite sides; probe in GUI confirms door item on map and passability in adapter mask.
5. **Reachability.** Far exits and doors enter `levelReachabilityOf` and repair; G5 gate iterations 39–40 without regression (32/32 seeds, zero unreachable targets) with both features enabled.
6. **Undo / probe.** Commit, Undo to empty map, Redo identical; new probe marker `CAVE_FAR_EXIT_DOOR_GUI_PASS`.
7. Without regression: determinism (two runs byte-identical), Seed 1729 100×100 fingerprint = 17159159863505677800 with new options disabled (far exit: default enabled only when cave enabled; if changes fingerprint, report justifies), core tests (new tests added), `SUITE_PASS`.

## Iteration 42: mountain passes (2026-11-12)

Binding before implementation. User decision: work order, point 3 “passes in mountains”.

1. **Definition.** Pass is valley through mountain massif on surface floor: band width 3 (tolerably 2–4 at turns) of passable ground (Path, or Dirt if no brush) crossing rock and slopes, with caps z − 1..z − 3 cut over band and one cell beside (valley walls = slope from “mountain” of higher floor). Cap cut into two parts stays if each part meets previous area minimum; otherwise smaller part removed (surface rock). Ramps and cave entries cut by band removed and re-placed by existing selector.
2. **Where.** Core determines massif (rock/slope component on surface area ≥ 400 cells) and two passable land cells on its opposite sides for which route around massif ≥ 2× longer than route through (BFS). Pass course: cheapest path (Dijkstra, ties (y, x)) cost growing with floor count above cell, smooth turn (no 1×1 steps). Max `passes` passes per plan (new field, default 0 = disabled; in “Levels” tab: number 0..8). At 0 result byte-identical as before.
3. **Measurement (a).** Seeds 1001–1032, side 512, settings iteration 41 plus `passes=4`: median placed passes ≥ 2 in m06 and m19; for each pass route through ≤ 0.6 route around (BFS on `levelReachabilityOf` model).
4. **Measurement (b).** Zero pass cells under cap, zero blocking cells on band (cautious rule mask and adapter mask in probe), zero caps below area minimum after cut.
5. **Reachability.** G5 gate without regression (32/32 seeds, zero unreachable targets) with `passes=4` and with `passes=0`.
6. **Undo / probe.** Commit, Undo to empty map, Redo identical; probe marker `MOUNTAIN_PASS_GUI_PASS`; probe checks nothing above pass cell on z − 1.
7. Without regression: determinism (two runs byte-identical), Seed 1729 100×100 fingerprint = 17159159863505677800 at `passes=0`, new core tests (manual massif with pass, cut cap, ramp on band), `SUITE_PASS`.

## Iteration 43: stairs and upper floor in buildings — debt D27, part 1 (2026-11-13)

Binding before implementation. User decision: “Start work on debt”.

1. **Building upper floor.** Option in buildings tab (default disabled; when disabled result byte-identical as before): part of buildings (`upperFloorPercent`, 0..100, default 30) with interior ≥ 16 cells gets floor z − 1 with same outline: floor from building floor brush, walls at outline from building wall brush, no exterior doors. Floor can't collide with mountain cap, pass or other building on z − 1 (then building stays ground floor).
2. **Stairs.** In each such building one pair of Z-passages: on ground floor upward stair item in interior, at wall, with free entry cell before it; on z − 1 arrival place (passable floor cell) and downward exit. Items from brush/doodad available in 10.98 data (implementer chooses names from `data/`, records in profile, e.g., `buildingStairs`; no IDs in code). Without brush option disabled with message in window. Stairs don't block path from doors to rest of ground floor interior.
3. **Reachability.** Building floor new floor in `levelReachabilityOf` and repair; each floor cell reachable from start via doors and stairs. G5 gate without regression (32/32 seeds, zero unreachable targets) with option enabled and disabled.
4. **Measurement.** Seeds 1001–1032, side 512, m06 and m19, settings iteration 42 plus option with default percent: share of qualifying buildings with floor in range 20..40 % (median), 100 % floors with stairs, zero collisions with caps/passes.
5. **Undo / probe.** Commit, Undo to empty map, Redo identical; probe marker `BUILDING_STAIRS_GUI_PASS`, checking stair items on both floors and adapter mask.
6. Without regression: determinism (two runs byte-identical), Seed 1729 100×100 fingerprint = 17159159863505677800 with option disabled, new core tests, `SUITE_PASS`.

## Iteration 44: impassable rock in m19 profile — GUI case (2026-11-13)

Binding before implementation. GUI report: x 1000, y 1000, 512×512, m19, 10 entries, mountains 3 levels, seed 1729, buildings 100 with floors 30 % → about 5200 unreachable (diagnosis: rock brush “mountain” has impassable ground; gate measurement iterations 39–40 didn't see this because cautious rule applied only to vegetation). User decisions: “Passable ground”, “POI only in connected”, “Remove area at once”.

1. **Entry in rock wall:** entry item cell and field before it get passable ground (brush from profile, e.g., earth or cave ground; no IDs in code); entry appearance in wall stays. Each entry has passable Z-edge in adapter mask.
2. **POI only in connected land:** core gets info whether surface rock passable (from adapter, from rock brush ground flags in client; without client — cautious rule: impassable if any variant blocks). With impassable rock buildings, camps, ruins, cave entries and caps arise only on land connected to start (BFS in this model, with fords/bridges/passes); cut-off land without POI.
3. **Area repair:** when target on surface has no path, one repair removes all POI of its component (one event in 64 limit); removed place count in report as before.
4. **Regression test:** exact case from report via real window with m19 fixture → full reachability (0 unreachable, no `Unrepaired`); new probe marker `M19_ROCK_GUI_CASE_PASS`.
5. **Gate measurement with real rock passability:** seeds 1001–1032, side 512, m06 and m19, settings iteration 43 (with floors): 32/32 full seeds in both profiles; median cave entries ≥ 8; share of removed POI reported. Earlier results iterations 39–43 for m19 described in G5_GATE_STATUS.md as measured without rock ground passability.
6. Without regression: determinism (two runs byte-identical), Seed 1729 100×100 fingerprint = 17159159863505677800 (if changes, justify: m06/no client), core tests, `SUITE_PASS`.

## Iteration 45: dungeons under buildings — debt D27, part 2 (2026-11-13)

Binding before implementation. User decision: “Accept — take next debt point”.

1. **Dungeon.** Option in “Underground” tab (default disabled; when disabled result byte-identical as before): part of buildings (`dungeonPercent`, 0..100, default 20) with interior ≥ 16 cells gets dungeon on z + 1: 2..6 rectangular rooms (interior 4×4..9×9) with floor and walls from profile brushes (new fields, e.g., `dungeonFloor`, `dungeonWall`; no IDs in code), connected by corridors width 1–2 with doors (closed, not key-locked) at room entrances. Dungeon doesn't overlap cave z + 1 (spacing ≥ 2 cells from cave floor and wall band) or other dungeons; without space building stays without dungeon.
2. **Descent.** In building with dungeon one downward passage in interior at wall (item/brush from 10.98 data with downward floor change, e.g., stairs or ladder; implementer choice, name in profile) and exit up in dungeon exactly below with passable field beside. Doesn't block path from doors to rest of interior; doesn't collide with iteration 43 upper floor stairs.
3. **Reachability.** Dungeon part of z + 1 floor in `levelReachabilityOf` and repair; each room reachable from start via building doors, descent and corridors. G5 gate (measurement iteration 44, real rock passability) without regression with option enabled and disabled.
4. **Measurement.** Seeds 1001–1032, side 512, m06 and m19, settings iteration 44 plus option: share of qualifying buildings with dungeon in range 10..30 % (median), median rooms per dungeon ≥ 3, 100 % dungeons with descent and exit, zero cave collisions.
5. **Undo / probe.** Commit, Undo to empty map, Redo identical; probe marker `DUNGEON_GUI_PASS` checking walls, doors, descent/exit items and adapter mask.
6. Without regression: determinism (two runs byte-identical), Seed 1729 100×100 fingerprint = 17159159863505677800 with option disabled, core tests (new added), `SUITE_PASS`. After D27 completion in TECH_DEBT.md closed or with described remainder.

## Iteration 45b: dungeon fixes after GUI test (2026-11-13)

Binding before implementation. User report from GUI (m19, x 1000, y 1000, 512×512, seed 1729): “Corners stick out in dungeons”; “where entry ramp should be is floor hole instead of descent”.

1. **Dungeon corners.** On z + 1 no cell outside dungeon wall outline (also diagonally from corners) has dungeon ground or item; floor only inside walls and under walls. First diagnose source (floor under corner cell, floor brush border, corner wall element) and record in report. Test/probe: zero cells with dungeon ground outside outline (corridors permitted).
2. **Visible descent.** On surface in building with dungeon no “stairs” ground (black hole). Descent should look like downward stairs: downward floor-change stair item (e.g., “stairs” or “wooden stairs” from 10.98 data, also as RAW item named in profile, no ID in code) on building floor, in dungeon below passable arrival field with upward ramp/stairs. Works like Z-passages: passable Z-edge in adapter mask.
3. Probe `DUNGEON_GUI_PASS` checks both points (no “stairs” ground in buildings, no dungeon ground outside outline). G5 gate and iteration 45 measurement without regression, determinism, Seed 1729 fingerprint with option disabled, `SUITE_PASS`.

## Iteration 45c: dungeons — stair direction, passages, building walls (2026-11-13)

Binding before implementation. User notes after GUI test 45b (m19, x 1000, y 1000, 512×512, seed 1729; e.g., building c. (1094,1402,7), dungeon c. (1328,1336,8)).

1. **Direction.** Downward stairs on surface and upward ramp in dungeon same direction: north (per ramp). Stair item/ground variant chosen by direction (brush direction node or separate names in profile; no ID in code). Arrival in dungeon on side matching direction.
2. **Room passages.** Each corridor–room connection has opening in wall: doors (closed, not key-locked) or passage without wall. First diagnose why GUI shows full wall at corridor ends (doors not placed? replaced by wall autoborder?). Probe: each corridor–room contact on map has cell without blocking wall or with door item; zero contacts closed by wall.
3. **Building walls untouched.** Descent stands in building interior, never on wall cell or so autoborder/ground change removes wall; building wall outline with dungeon on map identical as without (except doors). Probe checks this.
4. Probe `DUNGEON_GUI_PASS` extended with points 1–3; G5 gate, iteration 45 measurement without regression, determinism, Seed 1729 fingerprint with option disabled, `SUITE_PASS`.

## Iteration 46: debt D22–D27 cleanup (2026-11-13)

Binding before implementation. User decision: “OK, next fixes”.

1. **D25.** `g5_run_suite.py` self-rebuilds `test_procedural_*` and `style_render` targets (ninja in `build-ninja`) before running tests, errors if rebuild fails; path to `local-data` in worktree resolved without manual absolute path (argument or main checkout detection via `git worktree list`). Report records which targets rebuilt.
2. **D24 (remainder).** (a) “cave enter” variant chosen by rock side (entry direction), not cell; (b) cave preview validation selects wall brush from profile field by name (`caveWall`), not index 1; (c) assessment: whether “mountain” brush still has faces only east and south in generated cave walls — measure and record; fix only if brush has other face variants.
3. **D23 (code part).** Check and record which D23 statements still true on HEAD (second hole from ramp top cell, “cave enter” variant, exit measurement without `occupied`, repair doesn't know Z-entries, empty z + 1 requirement, Undo leaving cave without entry). True and fits code — fix with test; game movement stays in D23 as unverifiable without server.
4. **D22, D26, D27.** Update descriptions: D22 closed (validator, repair, rejection, client passability, iterations 36–44); D26 — try repro on HEAD at (1018, 1237, 7) with default settings and m19/m06 (several seeds, ~512×512 around); if doesn't occur, closed as non-reproducible after changes iterations 37–45; D27 — per 45c (wall “brick wall”, descent “stairs down”, direction north).
5. Without regression: G5 gate (measurement iteration 45), determinism, Seed 1729 100×100 fingerprint = 17159159863505677800 (“cave enter” variant change doesn't change plan fingerprint; if changes — justify), 18/18 tests (plus new), `SUITE_PASS`.

## Iteration 47: cave appearance (2026-11-15)

Binding before implementation. User report from GUI (m19, x 1000, y 1000, 512×512, seed 1729, 10 entries in rock wall): cave is zigzag of parallel tunnels. Cause: `generateCave` connects entries sorted by rows sequentially with width-3 tunnel and round chambers every 24 cells.

1. **Connection graph.** Entries connected by shortest-span tree (MST by distance) plus 1–2 extra edges creating loops (when entries ≥ 4); no dependence on cell index order. Measurement: zero pairs of parallel tunnels running beside each other across full area width; longest straight tunnel axis segment ≤ 20 cells.
2. **Tunnels.** Variable width 2–5 cells along tunnel, noise meanders more pronounced (sway depends on length, not ≤ 3), 0–2 short dead ends per tunnel.
3. **Chambers.** Irregular rooms from noise smoothed by cellular automaton (not perfect circles); share of chamber cells (cells with ≥ 5 floor neighbors radius 1 when width > 5) ≥ 25 % cave floor (median seeds 1001–1032, side 512, m06 and m19). Each chamber connected to network.
4. **Coherence and GUI.** All entries in one component; ramps under entries as before; user case m19 via adapter: 0 removed places, all reachable. G5 gate (measurement iteration 45, m06 and m19 32/32) without regression.
5. Without regression: determinism, 18/18 tests (plus new), `SUITE_PASS`. Seed 1729 100×100 fingerprint may change only if case has cave — then new value recorded with justification.

## Iteration 48: D24 — RockFace entries fallback, cave wall brush (2026-11-15)

Binding before implementation. User decisions: “Regular entry as fallback”, “Different wall brush”.

1. **Entry fallback.** With RockFace style, when rock walls lack space for requested entry count, missing placed as regular entries (previous style outside rock). Criterion unchanged: median entries ≥ 8 (seeds 1001–1032, side 512, m06 and m19, measurement settings iteration 29 or later), with rock-wall entries priority — their count doesn't drop vs HEAD. Each entry with ramp below and in cave component.
2. **Cave wall brush.** Find in 10.98 data (brushes/, materials) cave wall/mountain brush with faces all sides (N, W, E, S and corners) and set by name as `caveWall` in m06/m19 profiles (no ID in code). Measurement `CAVE_WALL_FACES`: share of wall cells facing floor without visible face ≤ 5 % (was c. 49 %). If no brush qualifies — report with checked list, no change.
3. Without regression: user case m19 (x 1000, y 1000, 512×512, seed 1729, 10 RockFace entries) via adapter — 0 removed places; G5 gate `--i47` 32/32; determinism; Seed 1729 100×100 fingerprint = 17159159863505677800; core tests; `SUITE_PASS`. D24 in TECH_DEBT.md closed or with described remainder.

## Iteration 48b: fallback entry appearance (2026-11-16)

Binding before implementation. User report from GUI (e.g., x 1073, y 1243, z 7): regular fallback entry from RockFace is doodad “cave enter” (rock face) placed on grass — looks like it's hanging in air.

1. Fallback entry doesn't use rock face item. Looks like non-RockFace style entry (e.g., ground hole / downward stairs with ground or border matching terrain, chosen by name from 10.98 data, no ID in code). First check what entry styles exist and use existing look.
2. Probe (e.g., extension `D26_RAMP_ENTRANCE_PASS` or new marker): no entry outside cell adjacent to rock has “cave enter” item; each has downward passage (floorchange) and ramp below.
3. Without regression: RockFace entries unchanged, user m19 case 0 removed places, gate `--i47` 32/32, determinism, Seed 1729 100×100 fingerprint, `SUITE_PASS`.

## Iteration 48c: hole landing (2026-11-16)

Binding before implementation. User note: “Under hole instead of stair should be dirt floor - 384”.

1. Under hole-type entry (“cave hole”, including RockFace fallback entries and hole style) on z + 1 no ramp/stairs; lies passable ground “dirt floor” (in 10.98 item 384), chosen by name from data (brush or RAW item named in profile/constant), no ID in code. RockFace entries and far exits with ramp unchanged.
2. Z-edge hole → field below passable in adapter mask and validator (landing field reachable in cave). Probe: each hole entry has “dirt floor” below without ramp; each RockFace entry still has ramp.
3. Without regression: user m19 cases (10 and 15 entries) 0 removed places, gate `--i47` 32/32, determinism, Seed 1729 100×100 fingerprint, `SUITE_PASS`.

## Iteration 49: D23 — cave on occupied z + 1, one Undo step (2026-11-16)

Binding before implementation. User decisions: “Skip occupied”, “One step”.

1. **Occupied z + 1.** Cave doesn't require empty rectangle on z + 1: cells already occupied on map (any ground or item) are solid for cave generator (not dug, not overwritten). Entry/hole/ramp arises only above empty cave cell; entries without free landing don't arise (or moved by existing selector mechanism). Coherence: all placed entries in cave component, reachable. Test: map with occupied z + 1 fragment (e.g., band or rectangle in middle) → cave arises, occupied cells byte-untouched, 0 removed places or only entries above occupied cells.
2. **One Undo step.** Place with cave one action in history: one Undo restores state before Place (surface and z + 1), one Redo restores whole. Probe checks: after 1 Undo map identical as before, after 1 Redo identical as after Place. Without change to other editor operations.
3. Without regression: user m19 cases (10 and 15 entries, empty map) results unchanged, gate `--i47` 32/32, determinism, Seed 1729 100×100 fingerprint, `SUITE_PASS`. D23 in TECH_DEBT.md updated (only game test and tab field memory remain if not verified).

## Iteration 50: D24 — cave walls with rock wall brush (2026-11-17)

Binding before implementation. User decision: variant 1 (rock wall brush instead of “mountain”).

1. Cave walls placed with wall-type brush (wall brush) from 10.98 data chosen by name (candidates: “rock wall”, “rocky wall”; choice justified in report), on cave ground, no ID in code. “Cave wall” field in window accepts wall brushes (and still ground for compatibility) — default rock wall selected.
2. **Faces.** `CAVE_WALL_FACES`: share of wall cells touching cave floor without visible face/segment ≤ 5 % (was c. 48 %); wall visible from each side (N, W, E, S, corners).
3. **Passability.** Wall doesn't enter cave floor fields (no floor field gets blocking item); adapter mask from client agrees with map; user m19 cases (10 and 15 entries, empty and occupied z + 1) — 0 removed places, all entries with ramp/dirt floor as in 48c.
4. Without regression: gate `--i47` 32/32, determinism, Seed 1729 100×100 fingerprint, one Undo, `SUITE_PASS`. D24 in TECH_DEBT.md closed or with described remainder.
