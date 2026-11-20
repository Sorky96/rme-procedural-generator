# Procedural World Generator — Progress

## 2026-11-17 — G5: D24, cave walls with stone wall brush (iteration 50) — PASS

Cave wall can be a stone wall brush: on each tile of the band around the floor lies cave ground with a wall element, and RME wall logic selects the arrangement, so the wall is visible from every side (also north and west, which was missing with “mountain”). Window has in “Cave wall brush” field wall and ground brushes; default “rocky wall” (natural stone, brush without links to other walls). `CAVE_WALL_FACES` 0% tiles in band without element (was 17% without face, 48% without north or west face with “mountain”); floor without blocking items, adapter mask matches map. User m19 cases (10 and 15 entrances, empty and occupied z + 1) — 0 removed places, ramps and “dirt floor” as in 48c. G5 32/32, Seed 1729 fingerprint unchanged, one Undo, SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 50”.

## 2026-11-16 — G5: D23, cave with occupied z + 1 and one Undo step (iteration 49) — PASS

Cave no longer requires empty z + 1: tiles already on the map (and a band of 2 tiles around them) are solid for the cave generator and remain untouched; entrance without free landing does not form, cut-off cave parts are removed. Place is one history step for all levels (surface, cave, mountains): one Undo restores the map before Place, one Redo the whole thing; pair of steps in `ActionQueue` removed. Probe with 128 × 128 block on z + 1 (m19, seed 1729): 18 of 20 entrances (2 lost at block), block byte-identical, everything reachable. User m19 cases unchanged, G5 32/32, Seed 1729 fingerprint unchanged, SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 49”.

## 2026-11-16 — G5: landing below hole (iteration 48c) — PASS

Below the entrance-hole (“cave hole”, FloorHole style and fallback RockFace entrances), cave on z + 1 no longer has a ramp, only walkable ground “dirt floor” (brush “cave ropespot”, in 10.98 item 384, selected by name). RockFace entrances and far exits retain ramps. Hole edge → landing walkable in adapter mask; non-existent ramp edge removed from adapter level plan. Probes: each hole has “dirt floor” without ramp, each RockFace entrance has ramp; m19 cases (10 and 15 entrances) 0 removed places. G5 32/32, Seed 1729 fingerprint unchanged, SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 48c”.

## 2026-11-16 — G5: appearance of regular fallback entrances (iteration 48b) — PASS

Regular fallback RockFace entrance (outside rock) is no longer a “cave enter” doodad on grass, but a hole from the “cave hole” brush — the same appearance as FloorHole style. Entrances in the rock wall unchanged. New probe: “cave enter” only at rock, each entrance with floor change down and ramp underneath; m19 case (seed 1729, 512 × 512) with 40 and 60 ordered entrances: 20 + 0 and 23 + 13 holes, 0 removed places. G5 32/32, Seed 1729 fingerprint unchanged, SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 48b”.

## 2026-11-15 — G5: D24, regular fallback entrances (iteration 48) — part 1 PASS, wall brush unchanged

RockFace style picks regular entrances outside rock when space in walls is lacking; entrances in wall unchanged from HEAD (m06 599, m19 586), entrance median 64 (m06) and 44.5 (m19), each with ramp and in one cave component. Wall brush with faces from all sides in 10.98 is only “snowy/icy mountain” — their impassable external border blocked the cave in m19 GUI case, so “mountain” remains. G5 32/32 in m06 and m19, m19 GUI case without removed places, Seed 1729 fingerprint unchanged. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 48”.

## 2026-11-15 — G5: cave appearance (iteration 47) — PASS, GUI pending

Instead of zigzag parallel tunnels (entrances linked by rows): tree of shortest connections plus 1–2 loops away from other tunnels, meandering tunnels 2–5 width with dead ends, irregular grottos from noise and cellular automaton. Longest straight axis section 20 (m06) and 16 (m19), grotto share median 0.78, 0 parallel tunnel pairs, all entrances in one component. G5 32/32 in m06 and m19, m19 GUI case without removed places, SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 47”.

## 2026-11-12 — G5: mountain passes (iteration 42) — PASS, GUI pending

Valleys width 3 through massifs, caps cut above the band, “passes” field (0–8) in “Levels” tab. Median 4 passes in m06 and m19, G5 32/32 at `passes` 0 and 4. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 42”.

## 2026-11-12 — G5: cave exits at far end, corridors with doors (iteration 41) — PASS, GUI pending

Distant exit in 100% of caves with entrance (≥ 24 tiles from entrance); optional corridors with doors (`stone wall`). G5 without regression, SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 41”.

## 2026-11-12 — G5: PHASE 5 status after iterations 37–40

| PHASE 5 Criterion | Status | Evidence |
|---|---|---|
| G5 gate: reachable POI from fixed start, entrances/exits Z, no critical cuts | **PASS** (m06 and m19, 32/32 seeds, iterations 39–40) | [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md) |
| Validator, repair, seed rejection, bridges | done (iterations 36–39), D22 closed in measurement scope | iterations 36–39 |
| Region graph, fords, bridges | done | iterations 14b, 24, 37 |
| Mountain passes | done 2026-11-12, GUI unverified | iteration 42 |
| Multi-level mountains (z − 1..z − 3), ramps | done, GUI appearance unverified | iterations 32–34, 38 |
| Cave z + 1 with entrances and exit ramp below entrance | done (entrance median 18.5 / 33) | iterations 27–31, 38–39 |
| Cave exits at far end, corridors with doors | done 2026-11-12, GUI unverified | iteration 41 |
| Dungeons / underground structures, stairs in buildings | debt D27 | [TECH_DEBT.md](procedural/TECH_DEBT.md) |
| Settlements, ruins, camps with collision control | done; ruins and camps without GUI assessment | iterations 12, 25–26 |
| Inaccessible islands with sand only | done | iteration 40 |
| Game movement, original RME, clients other than 10.98 | unverified | D23 |

**PHASE 5 closed with debt 2026-11-17** (user decision, iterations 37–50, HEAD of `feature/procedural-g5-settlements` branch): G5 gate PASS (m06, m19 32/32), GUI accepted by user. Debt: D23 (game movement), D24 (no N/W faces with default “mountain”, “rocky wall” optional; game appearance). Next: PHASE 6.

## 2026-11-12 — G5: inaccessible islands with sand only (iteration 40) — PASS

Islands without connection to land and without POI get only Shore; 0 tiles other than sand in m06 (15 islands) and m19 (8 islands). Without regression, SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 40”.

## 2026-11-12 — G5: repair on final mask, more cave entrances (iteration 39) — G5_GATE_PASS

Repair repeated after autoborder on actual items (blocker: border “shallow water”). Entrance median 18.5 (m06) and 33 (m19). 32/32 seeds in both profiles, SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 39”.

## 2026-11-12 — G5: cap vegetation without splitting, entrances before caps (iteration 38) — FAIL on entrance count only

32/32 seeds reachable in both profiles; entrance median 2 and 3 (required ≥ 8). SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 38”.

## 2026-11-12 — G5: multi-level repair and bridges (iteration 37) — gate FAIL for m19

`repairLevelReach` and bridges from “bridge” brush. m06 32/32, m19 1/32 (7/32 with retry). SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 37”.

## 2026-11-12 — G5: validator across all levels (iteration 36) — validator PASS, G5 gate FAIL

Reachability validator for five levels in core, adapter, and window (status line, warning, seed retry). G5 gate: 0 of 32 seeds with full reachability in both profiles; causes: blocking vegetation, mountain slopes, cave entrances on slope. SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 36”.

## 2026-11-11 — G5: mountains on own slope (iteration 34) — PASS, GUI pending

Cap z − 1 on entire rock field, “mountain” slope under each higher level, grass to edge, ramps at slope with opening at top, vegetation as on surface. Based on analysis of 20 reference maps. SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 34”.

## 2026-11-10 — G5: mountains up to three levels without walls, % limit (iteration 33) — PASS, GUI pending

Levels z − 1..z − 3 with grass to edge and vegetation, ramps between levels, landing corridors, percentage limit for area on each level. SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 33”.

## 2026-11-10 — G5: plateau on mountains (iteration 32) — PASS, GUI pending

Level z − 1 above large rock fields: grass with vegetation, edge from mountain ground, ramps from surface. “Levels” tab. SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 32”.

## 2026-11-10 — G5: cave without buildings; missing entrance above ramp not reproduced (iteration 31) — PARTIAL

“Underground” tab works without buildings and camps. Missing entrance above ramp from GUI test did not repeat in 64 probe combinations (4660 pairs); cause unknown. SUITE_PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 31”.

## 2026-11-10 — G5: exit ramp below entrance, no black hole before entrance (iteration 30)

Cave exit now stands exactly below the entrance on the bottom and leads to the tile in front; “stairs” ground (black hole) does not form on surface. All entrances get exit (100% in both profiles and styles). 18 core tests and both probes PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 30”.

## 2026-11-10 — G5: cave walls, entrance in rock wall or hole, exits outside rock (iteration 29) — PARTIAL

Cave on z + 1 gets wall ground band (“mountain”) around floor. Entrance in two selectable styles in window: in south rock wall (“cave enter”) or hole in grass or earth (“cave hole”). Exit ramp no longer stands on rock. Hole style: 16 entrances of 16 ordered; wall style: median 3 (m06) and 6 (m19), below criterion 8. 18 core tests and both probes PASS. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 29”.

## 2026-11-09 — G5: Z entrances, cave and exit ramp on map (iteration 28) — PARTIAL

Generator draws cave on level z + 1, entrance item (“cave enter”) on surface and ramp up at landing of each entrance; entrance without ramp space is skipped. Commit is two history steps (cave, then surface), second error undoes first; occupied level z + 1 = refusal. Exit has 100% of entrances in both profiles (32 seeds, without vegetation). New window tab “Underground”. 18 core tests and both G1 probes PASS. To be verified by user: appearance and fields in GUI; game movement not verified. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 28”.

## 2026-11-09 — G5: Z entrances and simple cave, core (iteration 27) — PARTIAL

`caveEntrances` (entrances on rock edge), `generateCave` (cave on z + 1: landings, tunnels, halls), `undergroundReachabilityOf` (reachability through Z, pockets). In 32 of 32 seeds cave has one component and 0 pockets, all entrances reached from start. 18 core tests and both G1 probes PASS. Nothing is yet drawn: adapter (entrance item, cave ground brush, Preview and Undo for two levels, window) is iteration 28. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 27”.

## 2026-11-09 — G5: campfire in settlement (iteration 26) — PARTIAL

Settlement has campfire in center of clearing (doodad brush from “Campfire brush” field, plan layer `fires`, independent of vegetation) and larger clearing (disk 61 tiles, tent 3×3 beside). Probe `--surface` checks mechanism with brush fallback (client 10.98 in repo does not have “campfire” brush); 17 core tests PASS, all places reached in 32 of 32 seeds, in m19 in some seeds only 12 of 24 camps. Appearance and real campfire await user. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 26”.

## 2026-11-09 — G5: ruins and settlements (iteration 25) — PARTIAL

`ruinPercent` (ruin = building without doors with gaps in wall) and `campCount` (clearing with open tent), `SurfaceGrid::gaps` and `sites`, passable gaps in validator, resolver skips wall on gaps, “Ruins (%)” and “Camps” fields in Buildings tab. Ruins ≈ 29% of buildings, 24 camps per map, all places reached in 32 of 32 seeds after repair; probe `--surface` with Commit, Undo, Redo and without wall on gaps PASS; 17 core tests PASS. Appearance in GUI awaits user. [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md), “Iteration 25”.

## 2026-11-09 — G5: region graph (iteration 24) — PARTIAL

`buildRegionGraph`: regions (Land, Rock, Water, ≥ 16 tiles), neighborhood edges with weight and POI nodes (start, entrances, exits) with route distance from start; core and `style_render` only, no GUI. Manual test 14×6 and 17 core tests and both G1 probes PASS; in 32 of 32 seeds all POI have route, neighborhood graph is connected. From this iteration criteria and results G5 are in [G5_METRICS.md](procedural/G5_METRICS.md) and [G5_GATE_STATUS.md](procedural/G5_GATE_STATUS.md).

## 2026-11-09 — G5: reachability repair and seed rejection (iteration 23) — PARTIAL

`repairReach` (“Repair unreachable places” field, default checked) connects each component with doors or exit that is unreachable from start with a ford; `generateReachableSurface` rejects seeds in which POI cannot be reached. All POI reached in 32 of 32 seeds at `fordLength` 48 with repair (28 and 29 without) and at 0 with repair. 16 core tests and both G1 probes PASS. Appearance of repair fords (up to 256 tiles) awaits user. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 23”.

## 2026-11-09 — G5: POI and fixed start in validator (iteration 22) — PARTIAL

`poiReachabilityOf`: start closest to area center, entrances (doors) and exits (one per side); reachability from start. All POI reached in 32 of 32 seeds at `fordLength` 256 (m06 and m19), 19 and 20 of 32 at 0, 28 and 29 of 32 at 48. Manual tests and comparison with independent implementation 160 of 160; 16 core tests and both G1 probes PASS. Scope: one level surface, without window connection. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 22”.

## 2026-11-09 — generator window in tabs (iteration 21) — implemented, awaits user assessment

“Generate Terrain” window had one column of about 35 fields that didn't fit on 1080p (cut “Bush brush”, status overlapped fields). Fields are now in Area, Terrain, Paths, Buildings and Vegetation tabs; behavior unchanged. `rme` builds, 16 core tests and both G1 probes PASS; layout not verified in GUI. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 21”.

## 2026-11-09 — G5: sand/grass boundary steps (iteration 20) — PARTIAL

Added `shoreRadius` (smoothing window up to 17×17). Criterion −25% corners NOT met (radius 5: −10%, +2s generation), because corners are tile grid steps proportional to boundary length, not separate protrusions; single 1×5 protrusions filter removes (test). “Smooth beaches” window stays at radius 3. 16 core tests and both G1 probes PASS. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 20”.

## 2026-11-09 — G5: narrow sand spits between water (iteration 19) — PARTIAL

After four GUI renders: sand strips and peninsulas 1–2 tiles wide between lake and river (river cuts beach) are removed at “Smooth beaches” (`spit` 10 → 0 on 65,536 tiles). Sand/grass edge has border on verified map (1760 of 1760), so cause was geometric. Cost: some more cut buildings (fords repair them). Rectangular grass indents in sand (90° steps) not resolved. 16 core tests and both G1 probes PASS. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 19”.

## 2026-11-09 — G5: sand/grass boundary smoothing (iteration 18) — PARTIAL

“Smooth beaches” field (default checked, 2 passes of 7×7 majority filter at range 9 from lake). Sand/grass boundary length −22%, fine unevenness 2× less, sand amount unchanged; criterion 5× NOT met, and rule written beforehand (5×5) didn't smooth at all, so I used stronger one after measurement (deviation described). 16 core tests and both G1 probes PASS. User evaluates appearance. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 18”.

## 2026-11-09 — G5: paths reach goal and cross river (iteration 17) — PARTIAL

After GUI feedback: found 12h bug (`reachEnds` ends connected crosswise; visible only on long paths as straight diagonal lines and loops), fixed; paths reach real goal at `rockPasses`. At `fordLength` > 0 paths cross rivers with sandy ford. “Peak” measure proved unreliable (criteria 1 and 2 from record NOT met), I evaluate from renders and user; 16 core tests and both G1 probes PASS. Jagged sand and grass edges await separate diagnosis. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 17”.

## 2026-11-09 — G5: path goals through rock (iteration 16) — PARTIAL

At `rockPasses` path now seeks start and goal on opposite sides of rock band (3–30 tiles). Paths crossing rock in 10 of 32 seeds (was 3 and 8), criterion 24 of 32 NOT met; in hindsight at rock cost 0 is 21 and 27 of 32. Discovery: generator draws only 1–2 routes per 512×512 map at `pathLength` 40. No regression, 16 core tests and both G1 probes PASS. GUI appearance not verified. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 16”.

## 2026-11-09 — G5: paths through rock, “passes” part 1 (iteration 15) — PARTIAL

Rock from “rocky ground” brush is passable (19 of 4983 tiles blocks), so pass is not reachability need. Added `rockPasses` (paths can cross rock bands) and “Rock passes” field in window. Criterion “crossing paths in ≥ 75% seeds” NOT met (3 and 8 of 32; even at free rock cost 13 and 22 of 32), because paths connect random points and rarely must cross rock. 16 core tests and both G1 probes PASS. Option “blocking rock + passes” awaits. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 15”.

## 2026-11-09 — G5: more natural ford shape (iteration 14d) — PARTIAL

After user decision (“I prefer natural appearance”) longer fords (≥ 8 water tiles) have deflected axis (up to 4 tiles) and variable width 3/5; shorter unchanged. Property tests on wide water, probe on verified map (443 ford tiles, 0 blocking, 7 connected pockets), seed counts with closed buildings unchanged, 16 core tests and both G1 probes PASS. User evaluates “natural”. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 14d”.

## 2026-11-09 — G5: extended ford ends (iteration 14c) — PARTIAL

After user note (ford too narrow at start and end) ford ends now have sandy “inlet” with radius 3; land junction grows from 2 to 4–7 tiles. Probe checks blocking flags and connectivity on verified map (0 blocking, pockets connected), seed counts with closed buildings unchanged, 16 core tests and both G1 probes PASS. Appearance and game movement await user. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 14c”.

## 2026-11-09 — G5: fords of sand instead of path (iteration 14b) — PARTIAL

User note (“cannot cross this bridge”) valid: ford from Path tiles got blocking `shallow water` edge (5 of 6 tiles in probe). Fords now have Shore type (sand), probe checks tile blocking flag after Commit (0 of 6), range limit grew from 64 to 256 (at 256: 0 of 32 seeds with closed buildings in m06 and m19). 16 core tests and both G1 probes PASS. Appearance and game movement await user. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 14b”.

## 2026-11-09 — G5: fords through water (iteration 14) — PARTIAL

Core has `SurfaceTuning::fordLength` and `placeFords` (Path through river or lake connecting cut components), generator window “Ford length” field. 16 core tests and both G1 probes PASS (new `SURFACE_FORDS_E2E_PASS`). Criterion from record (≥ 90% seeds without closed buildings) NOT met for {8, 16, 32}: at 32 closed buildings in 5 of 32 seeds (was 10); in hindsight 48 gives 2 of 32. Ford appearance awaits user evaluation. Details: [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 14”.

## 2026-11-09 — G5: surface reachability validator (iteration 13) — PARTIAL

Core has `reachabilityOf` (4-connected components, pockets, closed buildings); 16 core tests and both G1 probes PASS, including cross test with union-find (160 of 160). Measurement on seeds 1001–1032: largest component median 993.5‰, but **10 of 32 seeds have buildings outside largest component** (rivers divide land; doors never blocked), so criterion “closed = 0 in ≥ 90% seeds” is NOT met. Validator not yet connected to generator window; repair/bridges/seed rejection next iteration. Details: [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 13”.

## 2026-11-09 — G5 part 1: door adapter (iteration 12e) — PARTIAL

Doors reach plan and preview via `DoorBrush` (profile: door type, plan: tile flags, window: “Doors” and “Door kind”); probe `--surface` with Commit/Undo/Redo PASS (6 and 38 doors), 15 core tests PASS. Fix: preview snapshot check now accepts `Door` class (without house ID). GUI awaits user. Details: [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 12e”.

## 2026-11-09 — G5 part 1: settlements (iteration 12), criterion NOT met — PARTIAL

Logistics (D20, D21): `g5_run_suite.py` (15 core tests + both G1 probes one command) and `g5_shape_table.py` (shape tables, two runs byte-identical) in repo; m06/m19 shape params stay manual (decision in D20). Measurement 12a in 19 references (`g5_reference_settlements.py`): clusters (nearest neighbor distance 4, ~2.8 buildings per cluster), buildings by path 6–8× more often than average tile, no water relation, doors toward path 20–26 percentage points more often than chance. Core: clusters, one door per building and routes to doors; criteria written beforehand **not met** (4–5 of 8 traits, 75% threshold) even after one constant change (12d). Paths are 4× longer than reference. Adapter (RME door brush), generator window and GUI: **not done**. Details and what not verified: [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), “Iteration 12”. Branch `feature/procedural-g5-settlements` (from PR #5 branch), local commits, no push and PR.

## 2026-11-08 — G4: building walls (iteration 11c) — PARTIAL

At user request building floors got walls (without doors): wall item layer in plan (`GroundPlan::walls`, `GroundProfile::wall`), validation in resolver and preview, alignment of items with existing `WallBrush::doWalls`, fields in generator window. Median outline tiles in reference range (28 vs 30 / 24). G1 regression `--commit` PASS. User confirmed in GUI: buildings look realistic, Preview/Validate/Place/Undo/Redo without issue. **Unverified: e2e probe on real client (G1 probe without walls).** [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

## 2026-11-08 — G4: settlements and structures, building foundations (iteration 11) — PARTIAL

Measurement 11a: buildings (wall outlines) are numerous in M06 and M19 (approx. 31–37 per 65,536 tiles, median 9 × 7, half with doors). Scope selected by user: foundations only. Implemented optional floor rectangles (`SurfaceKind::Floor`, `buildingDensity`, eight brushes in profile, fields in generator window): count and size within reference range, distribution shares met for m06, not for m19 (1.8 vs 1 percentage point). Regression G1 `--commit` PASS. **Unverified: GUI (window, Preview/Place/Undo with floors), appearance; no walls, doors, or roofs (not a complete building).** [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

## 2026-11-08 — G4: transitions between terrain types (iteration 10) — PARTIAL

New metric: composition of rock and earth boundaries (what they border). Generator had earth touching rock and beach proportionally to share, in references earth avoids them. Added grass border (2 tiles around beach entirely, around rock ~75%): m06 8 of 8 features in range, m19 2 of 8 (criterion not met; M19 has rock touching beach and water, which the heightmap does not produce). Changed pinned snapshot for Seed 1729. Core tests PASS, `rme.exe` rebuilt. Not verified: appearance in GUI, G1 regression. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

## 2026-11-08 — G1/G2 regression after G4 changes (iterations 3–9) — PASS with debt

On current code: `--commit` probe `passed: true` (stderr empty), `--surface` probe all assertions PASS; `passed: false` only due to 7 known `SetFocus` warnings (identical to G2 run from 2026-11-07, debt D1/D2). Paths (7 brushes) confirmed in GUI by user, without separate probe. User deemed paths pushed away from rock good. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

## 2026-11-08 — G4: optional paths (iteration 9) — PARTIAL

Measurement 9a: paths (pavement, gravel, sidewalk) can be measured only in M06 and M19 (0.5–0.7% of surface). Implemented optional paths (disabled by default): `SurfaceKind::Path`, `pathLength`, A* routes over land, resolver with 7th brush, fields in generator window. Metrics L/W/B in reference range for m06 and m19 (3 of 3). Core tests PASS. Smoothing (9c) and repulsion from rock (9d, path at rock −80%). User confirmed in GUI: Preview, Validate, Place and Undo/Redo with paths work. **Unverified: G1 `--commit` regression, terrain type share condition, metric B outside range (artifact).** [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

## 2026-11-08 — G4: rock massifs (iteration 8) — PARTIAL

Rock metrics (RB, RF, RS) recorded upfront. Generator rock is smoother and more merged than in references (RB 33–49 vs 121–333). Cause: raising `featureSize` to 192 removed fine noise detail (3 octaves). Change: additional height noise octaves when `featureSize` > 24 (for ≤ 24 byte-for-byte as before): RB 33→47 (m19) and 49→77 (m06), criterion **not met**; side effects within bounds (P′, shares, E_d). `rme.exe` rebuilt. User viewed m19 in GUI: “looks ok” (subjective assessment). Not verified: river invariant and G1 regression after noise change, m06 and winter in GUI. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

## 2026-11-08 — G4: wider beach (iteration 7c) — PARTIAL

Beach ring rule changed (noise threshold ≤ 40%, lake ≥ 90 tiles, `shoreWidth` up to 8): P′ (beach thickness) in reference range for m06 (5,912 vs 5,638) and m19 (5,871 vs 5,656). Profiles `g4-profiles/10.98-v9` (m06 and m19 `shoreWidth` 8) for visual assessment in GUI; pinned snapshot for Seed 1729 changed (rule change). Not verified: appearance, river invariant and G1 regression after change, beaches around rivers. Earth (D, F) unchanged. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

## 2026-11-08 — G4: earth patches and beach width (iteration 7) — PARTIAL

At user request (earth “dotted”, beaches thin): metrics D (earth boundary), F (density), P′ (beach thickness) recorded upfront. Measurement: generator has earth **smoother** than references (D 112–203 vs 421–451), so visual diagnosis does not match metric; beach is ~2.8× thinner (P′ ~1,980 vs 5,640). Beach ring rule change (`shoreWidth` to 8, noise threshold ≤ 80%) and adjustment of `transitionWidth`/`shoreWidth` gives D ~206 and P′ ~2,315: **criterion not met**, grids saturated. Further beach improvement requires model change (new parameter, profile schema). Core tests PASS. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

## 2026-11-08 — G4: large-scale structures (iteration 6) — PARTIAL

Core `scale_stats` (terrain type agreement at distances 4/16/64, excess over random layout; test PASS) and criteria recorded upfront (`5aa85a0`). Generator had nearly zero excess at distance 16 and 64 (references ~400 and 200–230). Change `featureSize` to 192 closes E4 and E16, but E64 at side 256 stays below range: **criterion 6b not met**; exploratively at side 512 all three fit in ranges (window 256 cuts lag 64). Profiles `g4-profiles/10.98-v8` (m06, m19 with featureSize 192, manual file change) for visual assessment in GUI. Not verified: P1 and river invariant for featureSize 192, appearance, area size dependency. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

## 2026-11-08 — G4: comparison with references and water channel metric (5a) — PARTIAL

Tool `style_render` and terrain type map export from probe allowed viewing generator alongside M06 and M19 ([G4_COMPARE.md](procedural/G4_COMPARE.md)): main difference is lack of large-scale structures (continents/sea), earth patches in grass, and wide beaches. Initial impression about rivers turned out false after measurement, and iteration 4 metric bypassed river networks connected to sea. New channel metric (13×13 opening, L/W/B/C, recorded upfront in `f0e04bc`): m06 2 of 4 features in range (too wide), m19 3 of 4 (too long, 832 vs 302). Tributaries are not the main gap; 5b (river generator change) not done. Core tests PASS. Not verified: appearance, large structures, M08/M10. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

## 2026-11-08 — G4: `cave` and `swamp` mapping, profiles v7 (after GUI test)

GUI test showed that `caves` are only earth, and `swamp` has plain water. Cause: `cave` and `swamp` brushes were in `Other` mapping, so generator did not know them (swamp: 21,344 `item swamp` tiles outside model, profile adapted to rest, mainly rock). Change at user request: `cave`→Rock (palette `dirt mountain ground`), `swamp`, `dark swamp`, `walkable swamp`, `item swamp`→Water (palette `swamp`). Tool `style_styles` got option `--palette-alias KEY=BRUSH`. New mapping v3 (locally), statistics for 19 maps recalculated, profiles v7: caves Dirt=earth/Rock=dirt mountain ground, swamp Water=swamp (64% water tiles). Full preparation with `swamp` brush as water: ok. User GUI test (caves, swamp, one preview each): “looks better”, accepted (subjective assessment, no metric). User also accepted remaining v7 profiles after viewing in GUI (subjectively). Not verified: impact on G4 assessment (results of iterations 1–4 pertain to keymap v2, not recalculated), correctness of `cave` as wall (in M14 ~1,510 tiles). Evidence for caves (M14: ~5k tiles at z ≤ 7, no basements) and swamp (M12/M13) remain very narrow.

## 2026-11-08 — G4: river shape (D9 water part) and river meandering (D6) — PARTIAL

Core `water_shape` (Zhang-Suen skeleton, tortuosity T, straight section share S, width) with tests; `SurfaceTuning::meander` (transverse wiggle of river path, ramp at outlets, island-river cleanup; meander 0 = previous result byte-for-byte); profile v6 carries adjusted meander, preview snapshot accounts for it. References with rivers: only M06 and M19. Result: T 1361 → 1514 ‰ (ref. 1626–1669), S 181 → 24 ‰ (ref. 0), P1 no drop, 0 island-rivers in 1200 seeds, but **criterion D6 recorded upfront not met** (22/31 seeds better < 75%; fitting saturated at meander 8; model changed after seeing 4a, same two maps). Core tests PASS, G1 `--commit` regression passed. Not verified: appearance (A/B), GUI window with new meander (rme.exe awaits rebuild), bifurcations, caves, mountains. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md), D6/D9 in [TECH_DEBT.md](procedural/TECH_DEBT.md).

## 2026-11-08 — G4: brushes with external border (ring) and decoration palette (schema v5)

Brushes with external border drawn outside (`earth`, `mountain`, `snowy mountain`, etc.) draw edge outside generated area; with such brushes a tile adjacent to an unwritable neighbor without ground stays empty, and the border lands inside area (1-tile ring); preview snapshot takes only tiles with border. Full preparation: 31 of 31 ground brushes pass (was 21). Profile also carries decoration palette (tree, bush, stone); window fills it in Tree/Bush/Stone fields. Correction: `snowy mountain` is not a stub. Regression G1 `--surface` and `--commit` PASS. [G4_GUI.md](procedural/G4_GUI.md).

## 2026-11-08 — G4: palette fix after “Borders would extend outside the allowed area” error

Causes: brushes from palette that generator rejects (`earth`, `mountain`, `snowy mountain` and 7 others: their borders extend outside area), and brushes from previous profile left in fields. Window now starts from default brushes and applies only brushes the client has with items; tool selects palette from brushes that passed full preparation (probe, 21 of 31). All 8 profiles pass full preparation, but palettes barely differentiate styles (winter: only `sea`). [G4_GUI.md](procedural/G4_GUI.md).

## 2026-11-08 — G4: brush palette in profile (schema v4)

After manual test (profiles looked almost the same, caves are one ground): profile carries most-used brush for each terrain type (rules upfront: brush without `item ` prefix, ≥ 200 tiles, ≥ 25% of type), window fills it in brush fields and reports omitted. For 10.98: m06, m19, m05, winter, caves have palette, swamp, m01, m15 do not (their grounds are items without brush, D13). Visual difference modest except winter (`snowy mountain`). Schema v4 invalidates v3 files. [G4_GUI.md](procedural/G4_GUI.md).

## 2026-11-08 — G4: style profiles in generator window (code ready, manual test pending)

**Style profile** list in Map → Generate terrain… (only for “Natural surface”), `*.profile` files from `procedural/styles` in local data directory, offered only for exactly this client and assets (hashes of `items.otb` and `Tibia.dat`). Selected profile sets surface and vegetation tuning in `GenerationConfig`; preview snapshot unchanged for existing configurations. `rme.exe` builds, tests PASS, offering verified in probe with real client 10.98 (8 of 8). Window not run by me. [G4_GUI.md](procedural/G4_GUI.md).

## 2026-11-08 — G4-F iteration 3: profiles per style (iteration criterion met in-sample, G4 from plan still not shown)

Eight styles (families with goal), profile per style with land coherence constraint; criteria recorded before run. A: profile better in S1 and S2 in 8 of 8 styles (in-sample), C: 6 of 8 recognized, P1 ≥ 95% everywhere; B (only case on unseen map, swamp, n = 2): better in both. Interpretation recorded upfront: this proves generator reproduces style terrain type distribution, not improvement on unseen maps. Two runs byte-for-byte identical. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

## 2026-11-08 — G4-F iteration 2: land coherence constraint, gate still NOT met

Fitting with constraint (candidate rejected below 97% land coherence of manual parameters), criteria recorded before run, result “after model change”: land coherence in all folds ≥ 95% of manual (critical metric fixed), but style still better in only 3 of 7 folds (M06, M19, swamp), worse in 2 (caves, M05), vegetation 2:2. Test folds used second time, so result is less independent. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md#iteracja-2--dopasowanie-z-ograniczeniem-spójności-lądu-2026-11-08-po-zmianie-modelu).

## 2026-11-08 — G4-F: G4 gate NOT met

Assessment on test folds, criteria from [G4_METRICS.md](procedural/G4_METRICS.md) recorded before run, two runs byte-for-byte identical. 7 evaluated folds: style better in 3 (M06, M19, swamp), worse in 2 (caves, M05), unchanged in 2; land coherence (critical) worse in 5 folds (down to 69% of baseline median in swamp). Result: not met. Objective function has no playability constraint, so fitting cuts land with lakes and rivers. Proposals for next iteration (new criteria before run), no profile hookup to GUI. [G4_GATE_STATUS.md](procedural/G4_GATE_STATUS.md).

## 2026-11-08 — G4-D: contextual vegetation and G4-F metrics recorded upfront (PARTIAL)

Doodad classification by brush name (Tree 13, Bush 19, Stone 13 brushes of 247), contextual statistics (grass/earth × water × rock), `DecorTuning` in generator (default byte-for-byte unchanged) and fitting closed; profile `g4-profile/3`. Vegetation evidence comes in practice from 3 maps (M05, M06, M19); on validation fitting improves 2 of 4 comparable folds, worsens 2. G3 no regression (190 identical files). Metrics, seeds 1001–1032, aggregation and G4 criterion recorded in [G4_METRICS.md](procedural/G4_METRICS.md) before any calculation on test set. [G4_DECOR.md](procedural/G4_DECOR.md).

## 2026-11-08 — D13: ground coverage (PARTIAL, 96 new keys accepted 2026-11-08)

Cause: ground without brush in `data/` 10.98 got empty key (650 item id, 789k tiles). Fix: normalizer option assigns key `item <name from client definition>`; G3 no regression (190 files byte-for-byte identical with option off). No ground: 116 ‰ → 37 ‰ tiles; useful maps on surface 5 → 9; content-based split without merges (previous merges were artifact of empty keys), 9 families; fitting improves validation in 6 of 8 folds (before 2 of 4), baseline wins in 2. [G4_COVERAGE.md](procedural/G4_COVERAGE.md).

## 2026-11-08 — G4-C: style goal and parameter fitting (PARTIAL, result largely negative)

Mapping from G4-B accepted. Goal from training maps (equal weight per family), deterministic fitting of 7 `SurfaceTuning` parameters on fixed grids, profiles `g4-profile/2`; tests PASS, two series byte-for-byte identical. Of 12 style maps only 5 (M05, M06, M11, M14, M19) have tiles in grounds with brushes on surface; rest is almost entirely “ground without brush”. Fitting reduces distance to training goal 2–10×, but on validation family improves 3–7% or worsens (2 of 4 folds choose baseline). Nothing calculated on test set. [G4_FIT.md](procedural/G4_FIT.md).

## 2026-11-07 — G4-B: profile schema and key→terrain type mapping (PARTIAL)

`g4-profile/1` (client, hashes `items.otb`/`Tibia.dat`, level and floors, sources with weights, mapping, integer statistics) with validation, refusal of mismatched client/assets/level and projection of stats through mapping; mapping by whole words of brush name, no ID. Test PASS. Mapping proposal for 10.98 (108 keys, 5 Unmapped = 2 ‰ tiles) **awaits user acceptance**; profiles not yet filled (G4-C). [G4_PROFILE.md](procedural/G4_PROFILE.md).

## 2026-11-07 — G4-A: manifest, family split, similarity test (PARTIAL)

Core: 16×16 window snapshots (shift-invariant, no position and floor), containment in ‰, common window filter, manifest `g4-manifest/1`, families by content and 5 leave-one-family-out folds; tool `reference_split`. Core test PASS. On 19 maps of 10.98: shape pairs 1000 ‰ (positive control), 5 folds with leakage ≤ 45 ‰; two series byte-for-byte identical, G3 reports unchanged. First run degenerated (9 maps in one family), so added common window filter (explicitly described). No test of generator result memorization or BlackTek split. [G4_SPLIT.md](procedural/G4_SPLIT.md). Next: G4-B (profile schema, key→terrain type mapping).

## 2026-11-07 — G4-0: items.otb compatibility with client 10.98 (PARTIAL)

Branch `feature/procedural-g4-style-learning` from `origin/master` (PR #4 merged). Independent script compared 25,548 items in `data/1098/items.otb` with plain 10.98 `Tibia.dat` in six features: 0 mismatches; header 3.57 on all 19 maps; two series byte-for-byte identical. Not proven that maps were created with this specific OTB (no second file). [G4_ITEMS_OTB.md](procedural/G4_ITEMS_OTB.md), D11 updated. Next: G4-A (manifest, family split, memorization test).

## 2026-11-07 — G3 closed with debt (user decision)

Gate G3 “Import and analysis of reference maps” closed as PASS with debt, without claiming all verified: D8 (remnants: loading `Map` is ~94% of peak memory, cancellation during normalization ~1.2 s), D9 (features from 4.2: rivers, caves, paths, settlements, microstructures, bottlenecks), D10 (no similarity/memorization test and set split), D11 (companion XML, containers, unconfirmed `items.otb`, no HTML, one machine). Gate criterion met for 20 maps (19 plain 10.98, 1 BlackTek): repeatable reports (two series, byte-for-byte identical), statistics and semantic map (PNG per floor, independently verified). Independent check: Map Statistics in RME match for tiles, items, and impassable tiles. [Gate status](procedural/G3_GATE_STATUS.md), [debt](procedural/TECH_DEBT.md). Next: G4 (style profiles, distribution fitting, holdout), first D10.

## 2026-11-07 — D7 closed by user decision

User closed debt D7 (G2): snapshots of seeds 4242, 7, and 1 assessed as OK. This is their visual assessment; recalculation of Surface memory estimate (40 B/tile) and river width smoothness test not done, recorded in [TECH_DEBT.md](procedural/TECH_DEBT.md) and [G2_GATE_STATUS.md](procedural/G2_GATE_STATUS.md). D5 and D6 remain.

## 2026-11-07 — D8: manual test in RME passed

User opened `forgotten.otbm` in plain RME after loader change and provided Map → Statistics: 340,595 tiles, 429,037 items, and 244,283 impassable tiles, identical to G3 reports (tiles, items, `blocking` feature). Confirms import and `blocking` feature by independent program and correct editor path operation without hook. Remaining field definitions differ (passability, moveable, containers), not error; details in [G3_REFERENCES.md](procedural/G3_REFERENCES.md#kontrola-niezależna-statystyki-rme-gui-ścieżka-edytora). D8: removed point “no test in RME”.

## 2026-11-07 — D8: G3 debt further closed (loader interrupt hook)

Added optional interrupt predicate to `IOMapOTBM` (separate change in `source/io`, 45 lines; without hook behavior unchanged) and used in import: memory limit applies during loading (overage 3–21 MiB instead of 293 MiB), and cancellation in loading takes 78–310 ms instead of 1.4–1.7 s. With hard control in loading, estimate before loading is lower bound (16 B/B), so M06 (71 MiB) passes at default limit 2,048 MiB (peak 1,900 MiB). Regression: 295 set files and complete `forgotten.otbm` byte-for-byte identical; editor path (without hook) gives identical tile count on 5 maps. Remnants: loading `Map` is 94% of peak (requires different loading method), cancellation during normalization ~1.2 s, no manual test in RME. Details: [G3_REFERENCES.md](procedural/G3_REFERENCES.md#limit-pamięci-i-anulowanie-d8). **To be done by user:** short test of opening map in RME (steps in reply).

## 2026-11-07 — G3: semantic map export and gate status

Semantic map is now saved as artifact: deterministic PNG (8-bit indexed, no dependencies) per floor plus `legend.json` (slots, colors, floor frames), in `local-data/` (private). Independent verifier (Python `zlib`) passed all PNGs for 19 plain 10.98 maps and `forgotten.otbm` (dimensions, CRC, filters, pixel count = tile count, “unknown” pixels = unknown cells), two series gave 105 byte-for-byte identical files, remaining reports unchanged. Clarified `unknown` in M01/M06 (id outside items.otb and “deprecated”). Added [G3_GATE_STATUS.md](procedural/G3_GATE_STATUS.md): gate criteria and gaps, closure decision belongs to user. Next: G3 decision, missing features from 4.2 (rivers, caves, paths, settlements, microstructures), similarity/memorization test.

## 2026-11-07 — D8: memory limit and import cancellation (closed with remnant debt)

Import has process memory limit (estimate 40 B/B before loading, measurement after; gzip via ISIZE trailer) and `stop_token` (before/after hashing and loading, every 4,096 tiles, every row in analyses). Measurements on M06 (71 MiB): rejection at 2,048 MiB, PASS at 3,328 MiB (peak 1,898 MiB, of which loading 1,792), cancellation delay 154–1,713 ms in import and 5–428 ms in analysis. Regression: 190 set files and complete `forgotten.otbm` byte-for-byte identical. Remnants: loader non-interruptible (loading and map destruction up to 1.7 s), estimate conservative (M06 does not fit in 2 GiB per estimate, though uses 1,898 MiB), overage possible during loading. Details: [G3_REFERENCES.md](procedural/G3_REFERENCES.md#limit-pamięci-i-anulowanie-d8). Next: estimate calibration on more maps, interruption point in loader (separate commit), missing features from 4.2.

## 2026-11-07 — G3-E: set of 19 plain 10.98 maps (PARTIAL)

Analysis G3-A…D run on 19 maps of client 10.98 (separate client from BlackTek: `Tibia.dat` has 23,725 vs 26,381 items). Two full series gave 190 byte-for-byte identical report files, sources and `Tibia.dat/spr` unchanged, 38 runs without stderr/dialogs. True `unknown` on two maps (M01: 90 of 14 ids, coverage 99.8%; M06: 48 of 15 ids). Peak memory for 71 MiB map (7.8M tiles): 1,899 MiB, 93% of 2 GB budget: debt D8. Error in my runner (starting RME loaded BlackTek because profile had no “10.98” entry) fixed and described. Feature soundness not assessed, no cancellation and memory limit. Details: [G3_REFERENCES.md](procedural/G3_REFERENCES.md). Next: memory limit and import cancellation (D8), missing features from 4.2, clarification of unknown in M01/M06.

## 2026-11-07 — G3-D: feature extraction and aggregate report (PARTIAL)

Added `extractFeatures` (ground neighborhoods, 16/32/64 blocks, features per floor, feature frequency vs distance from impassable ground) and JSON/MD/CSV renderers (integers, no time and locale), probe saves 7 files. `forgotten.otbm`: 100.0% coverage, two runs byte-for-byte identical in all files; fixture with `unknown`: 98.7%. Test on manually-counted 3×2 map PASS. Not done: rivers, caves, paths, settlements, microstructures, similarity test, HTML; only one reference. Details: [G3_FEATURES.md](procedural/G3_FEATURES.md). Next: G3-E (repeatability on several references, memory, cancellation), missing features from section 4.2.

## 2026-11-07 — G3-C: regions and topology (PARTIAL)

Added `analyzeTopology` (core): connected components, regions of same ground (impassable, enclosed) and floor features, no IDs and names, with cancellation. `forgotten.otbm`: 95,103 passable tiles, on floor 7 largest component 22,634 of 46,534 tiles, tile sums match, two runs byte-for-byte identical. Not done: bottlenecks, rivers/caves/paths, floor connections; counts not compared with independent implementation. Details: [G3_TOPOLOGY.md](procedural/G3_TOPOLOGY.md). Next: G3-D (feature extraction and aggregate report), bottlenecks, containers.

## 2026-11-07 — G3-B: semantic normalizer (PARTIAL)

Added `SemanticMap` (core) and `normalizeReference` (adapter): ground key from client brush and 16 structural features from item definitions, no literal IDs. `forgotten.otbm`: 3,028,669 cells, 77 ground keys, tile sum per key = 340,595, two runs byte-for-byte identical. Sum check found error (93 tiles without features treated as empty), fixed with `Present` bit. `unknown` path verified on synthetic fixture (8/8 checks, negative check); not verified on map with real missing items, containers or process memory. Details: [G3_NORMALIZER.md](procedural/G3_NORMALIZER.md). Next: G3-C (regions, biomes, topology).

## 2026-11-07 — G3-A: read-only import (PARTIAL)

Branch `feature/procedural-g3-reference-analysis`. Added SHA-256 and compatibility verdict (core), import adapter without `Editor` and probe on private copy. `forgotten.otbm`: matches 1098-BlackTek, 340,595 tiles, 0 unknown, two runs byte-for-byte identical, source unchanged. Not done: containers, companion XML, cancellation/memory limit, G3-B…E. Details and gaps: [G3_IMPORT.md](procedural/G3_IMPORT.md). Next: G3-B (semantic normalizer and `unknown`), containers, fixture with mismatched version.

## 2026-11-07 — G2 closed (user decision)

Gate G2 “Sensible surface and border” closed as PASS with debt, without claiming all verified: D5 (seam next to terrain), D6 (rivers mainly along axes), D7 (no final review of seed 4242, memory estimate not recalculated), client 1098-BlackTek only. Branch `feature/procedural-g2-surface`: 19 local commits, nothing pushed, no PR. [Gate status](procedural/G2_GATE_STATUS.md), [debt](procedural/TECH_DEBT.md).

## 2026-11-07 — G2: islands in lakes, small ponds, smoother rivers

After snapshots: fill islands <24 cells in lakes, shore rings dependent on lake size, rivers drawn along smoothed axis with gradually increasing width. Core test, `--surface`, `--commit` PASS, fingerprint Seed 1729 → 6783381837833077004. Appearance in RME and `rme.exe` after this change unverified (RME worked). [Details](procedural/G2_GATE_STATUS.md).

## 2026-11-07 — G2: snapshots of a few seeds (4242, 7, 1)

Snapshots saved in `docs/procedural/screenshots/`; user did not give a rating. Observations from snapshots: sand irregular and good, but on seed 7 sand island in lake, on seed 1 river with right angles and stepped width. Visual criterion for G2 still open. [Details](procedural/G2_GATE_STATUS.md).

## 2026-11-07 — G2: irregular lake shore

After reviewer note (uniform sand, grass should cut in): `shoreWidth` 3, first ring always, further by smooth noise. Sand tile distribution 1/2/3 steps from water 4253/3290/1273 (24 seeds 100×100). Core test, `--surface`, `--commit` PASS, fingerprint Seed 1729 → 7666204984290374682. Appearance in RME unverified. [Details](procedural/G2_GATE_STATUS.md).

## 2026-11-07 — D5: Surface next to existing terrain

Surface borders next to existing terrain caused preview refusal (change outside area). Reproduced by test `nextToTerrain` in `--surface`; fix: writable cells next to non-writable cells with ground stay empty (1-tile seam), `surfaceKeepClear` shared for preparation and preview (strict equality preserved). `SURFACE_NEXT_TO_TERRAIN_PASS` (100×100, with tile in center, 250×250), regressions `--preview`/`--commit`/core PASS, G1 commit lines unchanged. Seam and appearance in RME unverified. [Details](procedural/G2_GATE_STATUS.md).

## 2026-11-07 — G2: rivers from rocks with variable width, brush from list

After GUI test: river sources only from rocks, river width 2–4 tiles increasing with distance from source and with watersheds (after snapshot: first version changed width only at watershed), brush fields in dialog replaced with dropdown lists (ground / doodad). Core test, `--surface`, `--preview`, `--commit` on client 1098 PASS (D1 unchanged), fingerprint Seed 1729 → 1951698793224813922; slow dialog opening (user: dialog; ~70k items in 8 lists) → `wxComboBox` read-only loaded on first expansion, doodads only with single items; unmeasured and untested natively. Appearance and lists in RME unverified. [Details](procedural/G2_GATE_STATUS.md).

## 2026-11-07 — G2-D: distribution checks, river width, gate status

Core test now checks river width (2×2 window) and upper bound of shore width, plus 3 seeds 500×500. Check found single-tile tail at river source; fixed (southeast cell on expansion), pinned fingerprint Seed 1729 → 13556304753518616142. `--surface`, `--preview`, `--commit` on client 1098 passed, G1 commit lines unchanged, `pairs_without_border=0`. Gate G2 status **PARTIAL**: no visual review of several seeds in RME, D5 open. [Gate status](procedural/G2_GATE_STATUS.md).

## 2026-11-07 — G2: wider sand (shoreWidth 2) after GUI test G2-C

User: vegetation in GUI looks good, Undo/Redo works; sand around lakes too thin. Default `shoreWidth` 1 → 2 (range 0–4 unchanged), pinned fingerprint Seed 1729 100×100 changed to 4385766052247269449. `test_procedural_surface` PASS, `--surface` probe (100 and 250, with and without vegetation) PASS, `pairs_without_border=0`. User GUI test (500×500, vegetation enabled): sand, trees, stones, and rocks rated as “ok”; this is visual rating of one view, no Undo/Redo and no many seeds at this size.

## 2026-11-07 — G2-C: vegetation and terrain-dependent details

New decoration layer for Surface (option off by default): trees, bushes, and stones from doodad brush (single items only), rules dependent on terrain (trees only on grass and not at water, stones on earth and at rock). `GroundPlan::decor`, preview, commit with one Undo, dialog. `test_procedural_surface`, `--surface` probe (100 and 250, Undo and Redo), G1 regressions (`--preview`, `--commit`, core) PASS. Status **PARTIAL**: dialog and appearance in RME unverified, multi-tile doodads out of scope. [Report](procedural/G2_VEGETATION.md).

## 2026-11-07 — G2: appearance fixes after first GUI test

GUI test confirmed Natural surface, Undo and Redo (after RME restart, overlays “entry/spawn” disappeared, which did not come from generator). Notes: earth “choppy”, rivers too narrow. Changed smooth grass/earth transition with removal of patches < 24 cells and widening rivers to ≥ 2 tiles; `test_procedural_surface` and `--surface` PASS, fingerprints repinned. Status **PARTIAL**: visual effect awaits re-review in RME. [Report](procedural/G2_CORE.md).

## 2026-11-07 — G2-B2: surface v2 in Preview/Commit pipeline

`GenerationConfig` (algorithm, surface), `GroundProfile` (count, 3 or 6 brushes), preparation, preview hash, resolver, and dialog (“Terrain style”) handle Surface alongside unchanged Classic. G1 regressions passed: resolver, `--preview` and `--commit` with fingerprints matching G1 documents. `--surface` end-to-end (preparation, validation, commit as one Undo, Undo, Redo) works for 100×100 and 250×250 after raising Undo limit to 512 MiB; runner gave `passed: false` only by two `SetFocus` warnings in stderr (D1). Default Undo limit 40 MiB rejects 250×250 (`HistoryRefused`). Added test-only closing of “Warnings” window in probes. Status **PARTIAL**: dialog in GUI and appearance in RME not checked. [Report](procedural/G2_PIPELINE.md).

## 2026-11-06 — G2-B: surface resolver and borders on client 1098

`resolveSurfacePlan` (grass, dirt, rocky ground, sand, sea, sea profile) and existing `GroundBorderCalculator` passed probe on real client (`g2-surface-resolver-2-20261106`, `SURFACE_RESOLVER_PASS`; first run failed on empty client config before resolver code ran). Six runs 100×100–500×500, zero pairs of different brushes without border, but borders reach past area at neighbor (95 and 50 tiles), which G1 pipeline rejects: debt D5. Status **PARTIAL**: no hookup in Preview/Commit/dialog and result in RME, times are single series. [Report](procedural/G2_BORDERS.md).

## 2026-11-06 — G2-A: core surface v2

New generator `surface.h/.cpp` (lakes, rivers, rock, shores, grass/earth transition band) alongside unchanged v1. `test_procedural_surface` passed (24 seeds 100×100, 6 seeds 250×250, 92 injected allocation failures, mask has no effect on other cells); `test_procedural_core` still PASS. Status **PARTIAL**: no adapter or result in RME, readability and borders unrated. Times are single series. [Report](procedural/G2_CORE.md). G2 in progress.

## 2026-11-06 — G1: closed with debt

User decision: G1 closed with documented debt. Five gate criteria have evidence, resource thresholds 500×500 and 100×100 approved and met; open items D1–D4 are in [TECH_DEBT.md](procedural/TECH_DEBT.md). Caveats: Place 500×500 times come from EXE `15c25557…`, not current `b87d1abb…`; many evidence items are single series; PR #1 has no CI, and intermediate commits not built separately. Live sessions out of scope. Changed only status in `G1_GATE_STATUS.md`, Phase 1 list, and this file; entries below labeled “G1 IN PROGRESS” are history from measurement date. G2 not started.

## 2026-11-06 — G1: threshold approval and technical debt

Thresholds 500×500 and 100×100 approved (including Preview 100×100 ≤ 250 ms) and verifier deemed sufficient. Measured values fit in thresholds (assessment in `G1_GATE_STATUS.md`), with caveat that Place 500×500 times come from EXE `15c25557…`, not current. New `docs/procedural/TECH_DEBT.md` records as debt: `SetFocus` on close (D1), unexplained activation stack (D2), allocation failures outside tested places (D3), and unexplored memory jumps (D4). Live sessions out of scope.

## 2026-11-06 — G1: SetFocus on close and trace probe error

Trace from `--roundtrip --focus-trace` showed `SetFocus` at File → Exit comes from `MapTabbook::DeleteTab` with frame disabled by “Closing” window (Win32 error 87). Trace runs ended `0xC0000374` because `ProceduralFocusTrace` was deleted twice (`unique_ptr` and wx on close); after releasing ownership, exit code is 0. Live sessions deemed out of scope for G1. Activation stack from 2026-11-04 still unexplained. No change to production code introduced.

## 2026-11-06 — G1: Phase 1 list filled with evidence

Phase 1 list in `PROJECT_RME_PROCEDURAL_GENERATOR.md` showed unchecked items for which evidence already existed (resolver, commit with Undo/Redo, dialog, tests). Checked them with links to reports; one item (diff tests, cancellation, budget, invalidation tests) remains partial: no actual live network session and not all allocation failure sites. User deemed memory growth in 200-cycle series non-blocking. Gate G1 remains IN PROGRESS. Evidence itself not re-verified, only its mapping to list.

## 2026-11-06 — G1: 200-cycle memory trend series

`g1-leak-200-20261106` (self-driven, 500×500, EXE `b87d1abb…`): criterion set upfront met (slope WS and private 0.31 MiB/cycle, GDI/USER/handles no growth after warmup, stderr empty). Auxiliary boundedness test set before run (mean difference cycles 101–150 and 151–200 ≤ 5 MiB) not met: +8.6 MiB. Memory grows in steps and slower than first 30 cycles, but plateau not shown; cause not investigated.

## 2026-11-06 — G1: full OTBM round-trip on current binary

Repeated Place → Undo → Redo → Save → Close → Open for 100×100 (`g1-roundtrip-100-cur-c-20261106`) and 500×500 (`g1-roundtrip-500-cur-20261106`) on EXE `b87d1abb…`. Both: `ROUNDTRIP_PASS`, loaded 10,005 and 250,005 tiles, verifier `PARTIAL…FOCUS_WARNING` (`SetFocus` warning in stderr). Verifier ignores four Google Drive extension lines and saves them in `ignored_external_stderr`. Two earlier 100×100 attempts ended with operator step-order error (inferred from logs). Other OTBM versions and external readers not checked.

## 2026-11-06 — G1: SetFocus diagnosis in refusal run and 100×100 threshold approval

Approved thresholds 100×100 (Preview ≤ 250 ms value proposed, no separate confirmation). Win32 `SetFocus` gives error 87 when target or ancestor is disabled (separate test). `ProceduralFocusTrace` saves window state; run `g1-focus-state-b-20261106` showed disabled frame under generator modal dialog, and stack pointed to `LoadVersion` → `SelectBrushInternal` called from refusal probe step (`loadClient`). For activation and tab-delete stacks, disabled window remains hypothesis. Did not check focus return after dialog close or refusal run without `loadClient` step.

## 2026-11-06 — G1: Preview 100×100 measurement on staging build

`--resources` (3 × 100×100, 3 × 500×500, native input) passed and was verified (`PASS_G1_GUI_RESOURCE_BASELINE_ONLY`). Preview 100×100: ~110 ms, GUI gap ≤ 53 ms, WS growth ≤ 3.6 MiB, within baseline thresholds. Changed `g1_verify_dialog_probe.py`: `--resources` mode treated as staging build, old branch comparison with old build removed. **G1 IN PROGRESS.**

## 2026-11-06 — G1: memory trend repetitions (3 × 30 cycles)

Slopes repeatable: WS 0.77–0.79, private 0.90–0.95 MiB/cycle (limit 1.0), total growth ≈22 MiB, declining over time. By established criterion one pass and two fails due to GDI/USER component (single sample vs cycle 2; GDI oscillates ~12 objects, not growing). Criterion not changed. Cause of oscillation and memory jumps not investigated. **G1 IN PROGRESS.**

## 2026-11-06 — G1: self-driven memory trend probe

Added `test_procedural_dialog_leak.cpp` and `g1_run_leak_trend.py` (queued menu and button events, not native input). 10 cycles 500×500 fails criterion (WS +1.55, private +1.99 MiB/cycle), 30 cycles meets it at boundary (WS +0.79, private +0.95; limit 1.0). Growth is stepped and negative in cycles 21–30; handles, GDI, and USER constant. Result depends on cycle count and needs repetition. **G1 IN PROGRESS.**

## 2026-11-06 — G1: Preview 500×500 cancellation and three resource cycles

`--cancel-preview` passed fully (`PASS_G1_GUI_IN_FLIGHT_CANCEL_ONLY`) after five failed attempts due to operator error. Preview 0.76–1.74 s, GUI gap ≤154 ms, WS growth ≈85 MiB: within baseline thresholds. Memory after Cancel grows ≈1 MB WS per cycle, and threshold “no trend in 10 cycles” requires 10 cycles, so unrated. **G1 IN PROGRESS.**

## 2026-11-06 — G1: Cancel log confirmed, full cancellation run not

In `--cancel-preview` cancellation of Preview 500×500 in-flight passed (`RESOURCE_IN_FLIGHT_CANCEL_PASS`, log “Preview cancelled”), but run as whole failed at retry step due to operator error (`Cancel requires observed Validate`); three resource cycles not executed. Three earlier attempts failed by too short time window or dialog close by Esc. Criterion for operation logs confirmed for all dialog paths. **G1 IN PROGRESS.**

## 2026-11-06 — G1: 100×100 measurement on current build and Place/Validate logs

One `--roundtrip --roundtrip-timing` attempt (100×100): Place 63 ms, Undo 26 ms, Redo 20 ms, Save 86 ms; max GUI gap 90 ms; total growth 4.4 MiB working set and 5.4 MiB private. Verifier `PARTIAL…FOCUS_WARNING`. Log confirms “Preview validated” and “placed 10000 tiles as one undo step”. This single attempt vs baseline thresholds, not series; Preview 100×100 and Cancel logs still unproven. **G1 IN PROGRESS.**

## 2026-11-06 — G1: Preview refusal re-run and log verification

`--refusals` on current tree passed functionally (six cases, `REFUSAL_PASS`; verifier `PARTIAL_G1_GUI_PREVIEW_REFUSALS_FOCUS_WARNING`, only warning is known `SetFocus` 0x57). Two earlier failures resulted from map click ending probe via `_Exit(8)`; cause inferred from log, not code. Dialog log confirmed at runtime for refusals and Preview, not for Validate, Cancel, and Place. **G1 IN PROGRESS.**

## 2026-11-06 — G1: generator dialog operation logs

`TerrainGeneratorDialog::SetStatus` logs each status message via spdlog (info/warn for errors), and successful Place logs tile count. Covers Preview ready, Validate, Cancel, refusals, and Place. Canonical build passed (exit 0; Ninja from `C:\repo\vcpkg\downloads\tools`, not on this shell PATH). **RME not run and log content not checked at runtime**, so log criterion remains unverified. CTest on current tree: 7/7 PASS (including `test_procedural_core`). **G1 IN PROGRESS**.

## 2026-11-05 — G1: first Place, Undo, Redo, Save measurement for 100×100

Claude Code extended harness in separate worktree. After integration, native run on EXE `15c25557…` ended with code 0, and independent verifier returned **PARTIAL_G1_GUI_ROUNDTRIP_TIMING_FOCUS_WARNING**. Place observed at 84.34 ms. Max GUI timer gap was 113.71 ms, and working set and private commit growth 6.09 and 7.34 MiB respectively. Three retained 500×500 attempts re-verified against archived probe and runner sources. [Results, CSV and snapshot](procedural/G1_ROUNDTRIP.md#measurement-of-place-undo-redo-and-save-for-100x100). **G1 IN PROGRESS**.

## 2026-11-05 — G1: criteria review after 500×500 runs

Claude Code review via local bridge and independent manifest comparison confirmed no current resource measurement for 100×100. Place, Undo, Redo, Save measurement mode limited to 500×500. Preview refusal and invalidation attempts have evidence on build before preflight change. Additional SHA-256 check confirmed OTBM and five XML companions match from all time-series attempts with retained full 500×500 round-trip. [Criteria matrix and task order](procedural/G1_GATE_STATUS.md). **G1 IN PROGRESS**.

## 2026-11-05 — G1: three Place, Undo, Redo, Save measurements for 500×500

Two new native runs on synthetic OTBM 5 and private profiles confirmed repeatability on same build. Independent verifier for all three returned **PARTIAL_G1_GUI_ROUNDTRIP_TIMING_FOCUS_WARNING**. Place observed at 403.4–419.4 ms. Max GUI timer gap in Place window was 428.1–544.4 ms, and peak working set and private commit growth 110.23–110.56 and 114.82–115.25 MiB respectively. [Report, CSV and RME snapshot](procedural/G1_PLACE_TIMING_REPEATABILITY.md). **G1 IN PROGRESS**. Resource gate assessment awaits agreed thresholds.

## 2026-11-05 — G1: independent verification of 500×500 measurement

Mode `--roundtrip-timing` checked four CSV rows against GUI log and confirmed current build hashes. Result **PARTIAL_G1_GUI_ROUNDTRIP_TIMING_FOCUS_WARNING** retains known warning. Attempts on copies confirmed rejection of corrupted measurements. [Report](procedural/G1_ROUNDTRIP.md). Full **G1 IN PROGRESS**.

## 2026-11-05 — G1: repeatable verification of protection inside mask

Mode `--protected-selection` confirmed retained GUI run, 421 sources and 421 objects, and UID 32101 markers. Rejected copies with missing, wrong, or repeated marker and changed probe EXE. Result **PASS_G1_GUI_PROTECTED_SELECTION_ONLY**. [Reproduction](procedural/G1_DIALOG_MATRIX.md). Full **G1 IN PROGRESS**.

## 2026-11-05 — G1: protected tile inside exact mask

Native RME showed five selected fields and four changes. Tile (203,203,7) retained UID 32101, content, tile and ground identity, and selection membership after Place, Undo, and Redo. Refusal of stale selection also passed. **PASS_G1_GUI_PROTECTED_SELECTION_ONLY**, code 0, empty stderr. [Run report](procedural/G1_DIALOG_MATRIX.md). Full **G1 IN PROGRESS**.

## 2026-11-05 — G1: scope of protection evidence in selection test

Claude review in read-only mode and independent code and log check confirmed Place, Undo, and Redo for five empty selection fields in native GUI. Hole (202,202,7) is outside exact mask, and protected sentinel (900,900,7) lies outside entire area. GUI evidence of field protection belonging to mask remains open. In proposed run, field (203,203,7) still belongs to five selected fields, but has ground and unique ID. Preview should report four changes, and checks after Place, Undo, and Redo compare content, tile identity, and selection membership. Field (201,201,7) still serves to test rejection of stale selection. See [GUI matrix](procedural/G1_DIALOG_MATRIX.md). Full **G1 IN PROGRESS**.

## 2026-11-05 — G1: first Place, Undo, Redo, Save measurement for 500×500

Private native RME probe logged four operations on synthetic map. Place took 413.8 ms to observe history change. Largest timer gap was 544.4 ms, peak working set growth 110.23 MiB, and private commit 114.82 MiB. Canonical build and runner ended with code 0. One deferred `SetFocus` warning, status **PARTIAL_G1_GUI_TIMING_FOCUS_WARNING**. One run with 512 MiB history limit in private profile. [Method, results, and reproduction](procedural/G1_ROUNDTRIP.md#measurement-of-place-undo-redo-and-save-for-500x500). Full **G1 IN PROGRESS**.


## 2026-11-04 — G1: 100×100 round-trip on same build as 500×500

Native Preview/Validate/Place/Undo/Redo/Save/Close/Open passed. RME read 10,005 tiles, and comparisons confirmed 10,000 generated tiles, five fixture tiles, and five XML. File hashes match previous 100×100 attempt. Exit code 0, one deferred `SetFocus` warning, status **PARTIAL_G1_GUI_OTBM5_ROUNDTRIP_FOCUS_WARNING**. [Report of both sizes](procedural/G1_ROUNDTRIP.md). Full **G1 IN PROGRESS**.

## 2026-11-04 — G1: repeated 500×500 round-trip on current build

Native Place/Undo/Redo/Save/Close/Open passed after preflight check fix. RME read and displayed terrain. Comparisons confirmed 250,000 generated tiles, five fixture tiles, and five XML. OTBM and XML have identical hashes as previous 500×500 attempt. Exit code 0, one deferred `SetFocus` warning, status **PARTIAL_G1_GUI_OTBM5_ROUNDTRIP_FOCUS_WARNING**. [Report and snapshot](procedural/G1_ROUNDTRIP.md). Full **G1 IN PROGRESS**.

## 2026-11-04 — G1: re-measurement of Cancel after budget check fix

**PASS_G1_GUI_IN_FLIGHT_CANCEL_ONLY** on current EXE. Cancel interrupted Preview 500×500, repeat with seed 1728 passed Validate, and map and history retained state. Three full cycles gave max GUI gap 168.1 ms, time to preview 0.90–1.28 s, and working set growth 79.6–80.9 MiB. Process returned 0 with empty stderr. [Report, CSV and snapshot](procedural/G1_CANCEL.md). Full **G1 IN PROGRESS**.

## 2026-11-04 — G1: budget check before Preview task allocation

Core re-run detected coroutine frame allocation before refusal for small budget. Preflight checks moved before task creation. After fix, build passed, fingerprints 100×100 and 500×500 match historical results, and 80 sandbox failure attempts, 108 preparation, 112 commit, and 15 reservation attempts passed. Regular RME refused area outside map and performed Preview after coordinate fix. Core attempt returned 0 and retains status **PARTIAL_G1_STAGED_CORE_FOCUS_WARNING** due to deferred `SetFocus` warning. [Report and evidence summary](procedural/G1_STAGED_CORE.md). Full **G1 IN PROGRESS**.

## 2026-11-04 — G1: full save and load path for 500×500

Native Preview/Validate/Place/Undo/Redo/Save/Close/Open passed functionally. Place took one history step with private Undo limit 512 MiB. RME read 250,005 tiles, and comparison confirmed 250,000 generated tiles, five fixture tiles, and five XML. Process returned 0 with two noted `SetFocus` warnings, so run retains status **PARTIAL_G1_GUI_OTBM5_ROUNDTRIP_FOCUS_WARNING**. [Report and snapshot](procedural/G1_ROUNDTRIP.md). Full **G1 IN PROGRESS**.

## 2026-11-04 — G1: SetFocus warning call stacks

Private diagnostic run with symbols logged two calls. wxWidgets restored focus on window activation during modal dialog and selected remaining tab on app close. No stack passed through Preview preparation steps. Invalidation markers passed, but runner exceeded wait time before manual close, so no final verification of this run. [Report and stack paths](procedural/G1_FOCUS_TRACE.md). Full **G1 IN PROGRESS**.

## 2026-11-04 — G1: seed change and close during Preview

**PARTIAL_G1_GUI_PREVIEW_INVALIDATION_FOCUS_WARNING**. Native seed edit and Alt+F4 invalidated active Preview 500×500. In both cases observer saw no old result for two seconds. Repeat with seed 1728 and preview in new window worked, and map and history retained state. Build and file verification passed. Process ended with code 0 and one `SetFocus` warning, also without opening OTBM. [Report and snapshot](procedural/G1_INVALIDATION.md). Full **G1 IN PROGRESS**.

## 2026-11-04 — G1: re-load check after message close

Repeated OTBM 5 round-trip. Observer waited for return to main window and left map for manual inspection. RME displayed re-loaded terrain in sprite renderer. Comparisons of 10,000 generated tiles, five fixture tiles, and five XML passed. `SetFocus` warning appeared during creature import query, before observer end and manual process close. Status **PARTIAL_G1_GUI_OTBM5_ROUNDTRIP_FOCUS_WARNING**. [Report and renderer snapshot](procedural/G1_ROUNDTRIP.md).

## 2026-11-04 — G1: save and re-open of generated OTBM 5

**PARTIAL_G1_GUI_OTBM5_ROUNDTRIP_FOCUS_WARNING**. Native Place/Undo/Redo confirmed one history step and agreement of 10,000 generated tiles. After Save → Close → Open RME read same terrain, and independent verifier confirmed five intact fixture tiles and agreement of five XML files. Build passed, process ended with code 0 and one wxWidgets `SetFocus` warning. [Report and snapshot](procedural/G1_ROUNDTRIP.md).

Complete **G1 IN PROGRESS**. G2 remains unstarted.

## 2026-11-04 — G1: Preview refusals in native dialog

**PARTIAL_G1_GUI_PREVIEW_REFUSALS_FOCUS_WARNING**. Native Preview refused on multi-floor selection, busy selection session, unstarted live host, injected halo allocation error, and missing client. After restoring the client, 100×100 preview worked in the same dialog. Map and history preserved state. Build and check of 421 objects passed. Run ended with code 0 and one wxWidgets `SetFocus` warning. [Report, screenshot and reproduction](procedural/G1_REFUSALS.md).

Complete **G1 IN PROGRESS**. Next step includes generator save and re-opening OTBM with companions. G2 remains unstarted.

## 2026-11-04 — G1: cancellation during Preview preparation

**PASS_G1_GUI_IN_FLIGHT_CANCEL_ONLY**. Dialog performs preparation in batches on main thread. Native Cancel during 500×500 preview preserved map and history. Rerun with seed 1728 passed Preview and Validate. In three full cycles, largest GUI pause was 180.9 ms vs. 546.1 ms baseline. Working set growth was 79.9–81.3 MiB, and time to ready preview increased to 0.99–1.57 s. Canonical Windows x64 build and independent verifier passed. [Report, CSV and screenshots](procedural/G1_CANCEL.md).

Complete **G1 IN PROGRESS**. This run did not repeat Place/Undo/Redo and did not perform full refusal matrix or generator round-trip OTBM. G2 remains unstarted.

## 2026-11-03 — G1: baseline RSS and GUI blocking measurement

**PASS_G1_GUI_RESOURCE_BASELINE_ONLY**, complete **G1 IN PROGRESS**. [Report, CSV and screenshots](procedural/G1_RESOURCES.md). Three real cycles of Preview/Validate/Cancel for 100×100 and three for 500×500, without a second large result in observer. Preview 500×500 to READY observation: 523.5–528.4 ms; GUI timer pauses: 536.5–546.1 ms; peak WS of entire process 805.6 MiB, growth 79.3–80.5 MiB. After large Cancel WS 724.5/724.9/728.8 MiB; three cycles do not rule out leaks. All six cycles preserved map, sentinel, revision and empty history. Normal shutdown/exit 0/empty stderr/assets unchanged; 421 objects and hash CSV verified. Production and normal EXE remained unchanged; Codex without new Claude.

**Cancel during generation not implemented or passed.** Measurement includes Cancel after ready Preview and Validate. Estimator budget 512 MiB is not process RSS limit. Next point: staging/cancellation and dropping delayed results; then GUI refusals and final OTBM/companion round-trip. G2 unstarted, older audits preserved, no access to user maps/world/DB/Docker.


## 2026-11-03 — G1: Undo hints and readable small selection

Completed UX fix: refusal message provides current limits and path to Preferences, selection mode hides rectangle fields and shows captured range/tile count, minimap is centered by explicit `StretchBlit` rectangle. [Report, screenshots and hashes](procedural/G1_DIALOG_UX.md). Without new Claude run. Final Windows build exit 0; `g1-dialog-centering-rme-20261103` **PASS_G1_NATIVE_DIALOG_SELECTION_UX_ONLY**, normal shutdown/empty stderr/assets unchanged and 421 objects verified. Checked description, hidden fields, centering/resize and stable refusal/Place/Undo/Redo of five fields with holes.

Full 500×500 + selection matrix and 40 MiB refusal/512 MiB rerun passed earlier this step (`g1-dialog-polish-retry-rme-20261103`, **PASS_G1_NATIVE_DIALOG_UX_MATRIX_ONLY**), before final paint-only change. **This matrix was not repeated on final EXE**; first failed wrapped message comparison test kept as FAIL. Complete **G1 IN PROGRESS**. Next item: responsive cancellation/scheduling and RSS; then remaining GUI refusals and final generator OTBM/companion round-trip. G2 unstarted; earlier audits not restarted, shared changes and user data preserved.


## 2026-11-03 — G1: GUI 500×500 and exact selection

**PASS_G1_NATIVE_DIALOG_MATRIX_ONLY**, complete **G1 IN PROGRESS**. [Report and screenshots](procedural/G1_DIALOG_MATRIX.md). Real Preview/Validate/Place/Undo/Redo for 250,000 tiles and five unflagged members in 4×4 mask with holes; compared full context and sentinel, Undo restored tile identity and membership. Refusals of empty selection, brush, zero weights, bounds and old membership preserved source/history. On 40 MiB Undo, 500×500 Place correctly refused; in same private instance set 512 MiB via Preferences and reran successfully. Normal sources/EXE unchanged; current build exit 0, probe exit 0/normal shutdown/empty stderr/assets unchanged, 421 RME objects verified. Codex performed step without new Claude run.

Open: remaining GUI refusals/failures, refining Undo message and small mask presentation, responsive cancellation/RSS and final generator GUI/OTBM round-trip. G2 unstarted; shared changes and user data preserved. Next 3 tasks stated in report.


## 2026-11-03 — G1: native dialog and real GUI 100×100

**PASS_G1_NATIVE_DIALOG_100_ONLY**, complete **G1 IN PROGRESS**. [Report and screenshot](procedural/G1_DIALOG.md). Map → Generate terrain…: Preview/Validate/Cancel and Place as one Undo action; preview is minimap of specific tiles. Canonical build exit 0; `g1-dialog-final-rme-20261103` PASS/exit 0/normal shutdown/empty stderr/assets unchanged. Independent observer checked no mutation before Place, bad seed and invalidation, Cancel, and real Place/Undo/Redo with 10,000 tile comparison. Verifier confirmed 421 current RME objects. Claude implemented dialog and performed read-only review; Codex fixed layout/labels and ran tests. First incomplete inspection kept as FAIL, without passing.

Open: GUI selection/refusals/failures and 500×500, sprite preview, responsive cancellation/scheduling, RSS/other results and generator GUI/OTBM round-trip. G2 unstarted; shared changes and user data preserved, without commit/push.


## G1 — atomic commit and Undo/Redo, 2026-11-03

**PASS_G1_ATOMIC_TERRAIN_HISTORY_CORE_ONLY**, complete **G1 IN PROGRESS**. [G1_COMMIT.md](procedural/G1_COMMIT.md). New canonical Windows x64 build exit 0; `g1-commit-complete-rme-20261103`: exit 0, normal shutdown, empty stderr and unchanged curated assets. Verifier confirmed 421 current objects, 877 baseline sources and 22 intended differences. Claude performed map reservation and review, Codex integration and independent tests. Shared G0/Claude changes preserved; without commit/push and without access to user maps/world/DB/Docker.

Commit copies exact diff without RNG/autoborder to separate Action/Change/ActionQueue; all allocations before swap and publication. Undo/Redo swap full state and actual selection membership independent of flags, including null-source Redo and subsequent normal Selection Undo. Passed 112 commit faults, 15 reservations, 100/500 sizes, exact/short budget, stale/busy/live/Lua/thread/cancel, minimap and Selection preflight faults/retry, Lua callback and equal membership changes. Current full earlier probe also PASS. Separate fresh G0 queue regression: PASS functional; old constraints/GUI warnings described in report.

Open: GUI dialog/rendered preview and Commit/Cancel, responsive scheduling/RSS/other jobs and generator GUI/OTBM round-trip. Do not equate history core with whole gate. Next 3 tasks: (1) minimal G1 dialog with current request/mask/preview, (2) GUI Commit/Cancel and parallel result limiting, (3) full GUI/OTBM round-trip and G1 matrix closure. G2 unstarted.

## 2026-11-03 — G1: current offline target and card life

**PASS_G1_CURRENT_OFFLINE_TARGET_ONLY; complete G1 IN PROGRESS.** [G1_TARGET.md](procedural/G1_TARGET.md). New guard fetches current Editor fresh, compares existing mapGeneration, refuses live and busy/deferred selection, then composes prior validations. Selection variant also checks real membership. Returned pointer is borrowed for immediate continuation, without retained right to later commit.

Claude performed two files and documentation fix; Codex reviewed, integrated and independently tested. Windows x64 build exit 0, EXE `8282a1c7…`. `g1-target-rme-20261103`: PASS/exit 0/empty stderr/normal shutdown/assets unchanged, 420 current objects, 877 baseline sources G0 and ten expected G1 differences. Guard 100×100/500×500 and selected 500×500/4×4 without allocation; 22 refusals, map switching, shared views, last tab closure and new map, both unlinked live states. Without connect/bind/accept/network service. Full earlier regression also PASS. Startup warnings handled in same private instance via computer-use.

Open: atomic commit/failures/UndoRedo/empty tile membership, reentrancy and active Lua/history transaction, separate manual edit history, GUI dialog/preview/Cancel/scheduling, RAM/RSS/Undo/other results and GUI/OTBM round-trip. G2 unstarted; shared changes and user data preserved, without commit/push.

## 2026-11-03 — G1: independent asset catalog changes

**PASS_G1_GENERATION_ASSET_CATALOG_ONLY; complete G1 IN PROGRESS.** [G1_ASSETS.md](procedural/G1_ASSETS.md). New read of current values detects changes in definitions/ground brush/borders and sprite metadata also through preserved mutable references. Preview stores separate process-local fingerprint; validation checks it before source/preview using cached links. Does not change deterministic result fingerprints and does not scan assets in renderer hot path. Claude performed adapter; Codex reviewed and independently integrated/tested.

Windows x64 build exit 0, EXE `e17b2f82…`. Full probe `g1-assets-rme-20261103`: PASS/exit 0/empty stderr/normal shutdown/input assets unchanged, 419 current objects and 877 baseline sources G0 with ten expected G1 differences. 39 catalog mutations without epoch bump refused validation; after restoring values result passed again. Scan had zero allocations. Separate freshly linked border-order and snapshot tests also PASS, including 24 snapshot faults. Repeated full probe 100×100/500×500, 79 sandbox faults, 106 preparation, mask, validation and selection/Lua/UndoRedo.

Open: final editor/offline guard, atomic commit/failures/UndoRedo/empty tile membership, separate manual edit history, GUI dialog/rendered preview/Cancel/scheduling, RAM/RSS/Undo/other results and GUI/OTBM round-trip. Raw pixels/textures/GPU/cache/runtime animation and internals of other brushes outside this slice's fingerprint. G2 unstarted; user data and shared changes preserved, without commit/push.

## 2026-11-02 — G1: shared validation of prepared result

**PASS_G1_PREPARED_VALIDATION_ONLY; complete G1 still IN PROGRESS.** [G1_VALIDATION.md](procedural/G1_VALIDATION.md). `validatePreparedTerrain` checks config/profile/exact mask, epoch and client ID, full map snapshot and preview integrity without allocation or regeneration. Private source fingerprint binds preview to captured input and concrete plan. Success is momentary check, not retained right to commit; live membership checks separately. Independent asset edits without epoch bump still not fully covered.

Claude performed two validator files; Codex reviewed bridge/code, added preview binding and independently tested integration. Canonical Windows x64 build exit 0, EXE `47ab14b0…`. `g1-validation-rme-20261102`: PASS/exit 0/empty stderr/normal shutdown/assets unchanged, 418 current objects, 877 baseline sources G0 and ten expected G1 differences. Validation 100×100/500×500 had zero allocations and unchanged fingerprints. Passed 29 refusal cases, unload/reload client and full mask/budget/sandbox/selection/Lua regression. Startup required closing info dialog of private assets warnings via computer-use; continued same process, without restart.

Open: full stamping of independent asset mutations, final life/proper editor and offline mode check at commit, atomic transaction/failures/UndoRedo/empty tile membership/separate history, dialog/rendered preview, GUI Cancel/scheduling, RSS/Undo/other results and GUI/OTBM round-trip. G2 unstarted, shared changes and user data preserved; without commit/push.

## 2026-11-02 — G1: exact selection mask

**PASS_G1_EXACT_SELECTION_MASK_ONLY; complete G1 still IN PROGRESS.** [G1_MASK.md](procedural/G1_MASK.md). New adapter reads actual `Selection::getTiles()`, including unflagged empty tiles, preserves holes and refuses empty/multifloor/busy/foreign collection. Result has mask; revalidation without allocation checks exact current set and minimal bounds on same map. Does not replace snapshot/revision/assets check and does not grant commit right.

Claude performed two files in separate checkout; Codex found and assigned zero-padding check fix, integrated and independently built and tested. Canonical Windows x64 build exit 0, EXE `9b4c2334…`. `g1-exact-selection-rme-20261102`: PASS/exit 0/empty stderr/normal shutdown/assets unchanged. Two real tabs, 100×100 and 500×500 masks with one hole: 9,999 / 249,999 members, estimates 10,104 / 250,104 B. One capture allocation, zero revalidations; exact concrete diff in mask, source unchanged. Passed stable same-bounds swap, ownership, pointer substitution with same membership, limits/fault/cancel/thread/floor/identity and full earlier budget/sandbox/selection/Lua regression. Verifier: 417 current objects, 877 baseline sources G0, ten expected G1 differences.

Open: connecting mask to dialog and validation at commit, empty tile membership at publication/UndoRedo, atomic commit/failures/separate history, GUI Cancel/scheduling, RAM/RSS/Undo/other results, renderer-preview and GUI/OTBM round-trip. G2 unstarted; shared changes and user data preserved, without commit/push.

## 2026-11-02 — G1: shared preparation budget

**PASS_G1_JOINT_PREPARATION_BUDGET_ONLY; complete G1 still IN PROGRESS.** [G1_BUDGET.md](procedural/G1_BUDGET.md). `prepareTerrain` combines snapshot, semantic core, resolver and sandbox under one conservative limit; result has snapshot/plan/preview, and semantic grid vanishes before sandbox. Checked limits on cells/halo/actual changes, stop and error propagation do not publish partial result. Claude performed adapter in separate checkout; Codex assigned fixes, reviewed and integrated files and independently built and tested all.

Canonical Windows x64 build exit 0 (181 steps), EXE `a8ce2a3d…`. Clean full probe `g1-preparation-visible-rme-20261102`: exit 0, empty stderr, normal shutdown and unchanged assets. 100×100: estimate 7,234,198 B / peak tracked new requests 2,923,649 B; 500×500: 168,808,358 B / 69,507,073 B. After destruction zero tracked bytes. Exact limit and one-byte-less limit, independent path agreement, 106 preparation faults and full earlier sandbox/selection/Lua regression passed. Verifier confirmed 416 current objects, 877 baseline sources G0 and ten expected G1 differences. First run with wx SetFocus error in stderr kept as unclean; runner now requires empty stderr.

This closes shared preparation accounting, but does not guarantee RSS. Open: full application/source/assets/other results/future Undo, GUI Cancel/scheduling, exact mask and staleness, empty selection membership at commit, atomic commit/failures/Undo/Redo, dialog/rendered preview and GUI/OTBM round-trip. G2 unstarted; shared changes and user data preserved, without commit/push.

## 2026-11-02 — G1: selection isolation without refusing entire context

**PASS_G1_PREVIEW_SELECTION_ISOLATION_ONLY; complete G1 still IN PROGRESS.** [G1_SELECTION.md](procedural/G1_SELECTION.md). Shared `markSelectionChanged` notifies current editor only for real tile on its map. Copies and other maps do not invalidate cache; removed temporary `SelectedContext` from sandbox. Protected selected border changes still reject whole candidate as `OutOfMask`, including with identical IDs.

Windows x64 build `g1-selection-canonical-20261102.log` exit 0, EXE `7048a947…`. Full new probe `g1-selection-lua-rme-20261102` PASS/exit 0/normal shutdown/empty stderr/unchanged source assets. Two real editors, public oracle cache and Lua events, active/inactive map context, selected-border refusal with positive check and normal selection and Lua setter with Undo/Redo passed. Repeated real sandbox 100×100/500×500 and 79 allocation faults with both editors' cache observation. Verifier checked 415 current objects and 877 baseline sources G0: ten expected G1 differences, including new guard in TileOperations. Claude performed independent read-only review; Codex tests/integration. Both earlier harness FAILs kept.

Open: GUI exact mask and staleness, empty tiles membership at future commit, full budget/RSS, GUI Cancel/scheduling, atomic commit of exact diff/failures/Undo/Redo and separate draw history, dialog/rendered preview and GUI/OTBM round-trip. Normal action regression is not generator commit test. G2 unstarted, shared G0 changes and user data preserved; without commits/push.

## 2026-11-02 — G1: own sandbox and bordered diff

**Sandbox stage verified; complete G1 still IN PROGRESS.** [Contract, evidence and constraints](procedural/G1_PREVIEW.md). DrawOperations prepares own BaseMap via existing TerrainPlacement and GroundBorderCalculator; real changes outside mask/protection reject entire candidate, without map change. Claude implemented deterministic rule order and performed separate read-only review. Detected selection notifications from copy secured by `SelectedContext` refusal before updates; full selected context support remains open.

Canonical build after review PASS 0, EXE `fdfc4632…`. Final real probe `g1-preview-reviewed-rme-20261102` PASS/exit 0/normal shutdown/empty stderr: core → resolver → bordered sandbox 100×100/500×500, full repeats, 79 allocation faults, limits, Cancel during call, UID/AID/PZ and out-of-mask, irregular mask/temple, selected context, base-ground variant and stale assets after reload. Independent verifier confirmed 415 current objects and only nine expected differences vs. 877 baseline sources G0. Snapshot regression and separate real overlapping border rules test PASS; CTest core 1/1. No result is full G1 passing.

Next open requirements: selection without side effects, full work budget/RSS, GUI Cancel and scheduling, stale selection/mask/relevant assets before commit, atomic commit of exact preview through history, Undo/Redo and failures, separate manual drawing, dialog/rendered preview and GUI/OTBM round-trip 100×100/500×500. G2 unstarted. Shared G0 changes preserved; without commits/push and without interference with user maps, companions, Docker, database or world.

## 2026-11-02 — G1-B: own snapshot region and halo

**G1 still IN PROGRESS.** Added full own tile copies and separate TileLocation metadata in region with two-tile halo, 64-bit copy estimator and value comparison accounting for attributes/recursive containers/Creature/Spawn/opaque payloads. [Contract, evidence and constraints](procedural/G1_SNAPSHOT.md). Capture does not mutate map and refuses lossy copy against current definitions. Revalidation detects also direct, untracked data changes in context. Not yet sandbox/diff/commit nor full protection against all asset and dialog mask changes.

Claude implemented estimator and tests in separate worktree (run `668dec3c…`, review fixes `0e66a728…`). Codex read reports/diffs/new files and rejected attribute copying before accounting and application stand-ins; fixed before integration. New const view/getter do not change layout or existing Lua API. Codex implemented capture/equality, integration and extended real tests. Worker did not compile or run anything; without refusals and commits.

Focused build of three CPP PASS. First canonical wrapper had VCPKG_ROOT/vcvarsall ordering error: script switched to VS's vcpkg, cleared own CMakeFiles and started unnecessary dependency rebuild. Finished only verified own CMD tree 23128; log `g1-snapshot-canonical-20261102.log` remains interrupted, without PASS. Restored C:/repo/vcpkg with proper wrapper; canonical **PASS 0**, 414 steps, `g1-snapshot-recover-canonical-20261102.log`. Did not edit production build_ninja or user environment.

Real probe `g1-snapshot-rme-20261102` **PASS, normal exit 0, empty stderr**, without client, GUI/Application::OnInit and map files. Performed estimator zero-allocation test, payload growth, depth 64/65, null item, exact/exceeded budget, pre-Cancel, halo ownership, detection of UID/AID/opaque/creature saved/alternatives/spawn NPC radius/location counters and other map/revision, incompatible definition copies and **24/24** single allocation faults of capture without source mutation.

Sparse map 100×100 snapshot: 10,816 context tiles, 453,760 B estimate, 0.2661 ms; 500×500: 254,016 tiles, 10,661,760 B, 8.6326 ms. Both fit in estimated snapshot budget 64 MiB. Not full generator, dense map, tight heap/RSS nor speedup. No sustained OOM, practically impossible container overflow nor Cancel from GUI during work.

Verifier **PASS_G1_OWNED_SNAPSHOT_ONLY**, `g1-snapshot-verification-20261102.json`: current 414 probe objects, sources/test/EXE, proper vcpkg root, no worker refusals and 877 baseline sources; only six intended differences GroundBrush/VersionManager/Creature/ItemAttributes. Normal EXE `f6185abf…`; `git diff --check` and new files whitespace check PASS. Shared changes preserved. All own processes finished.

Next required G1 work: placement and shared autoborder in detached DrawOperations buffer, concrete diff/protection of its scope and full work budget; atomic commit/stale checks with separate Undo/Redo and failures; minimal dialog and real E2E/round-trip 100×100/500×500. G2 unstarted. Docker, database, world and forbidden map/companions untouched; no new commits.

## 2026-11-02 — G1-B: client resolver and Claude implementation

**G1 IN PROGRESS.** Added real plan Grass/Dirt/Water → named brushes of active client → deterministic placement IDs. [Contract and tests](procedural/G1_RESOLVER.md). Resolver checks client, input/grid/masks, variant catalog, items and own payload budget; does not change map or global brush/RNG. Does not yet perform DrawOperations placement/border sandbox or commit.

Claude implemented asset revision counter in separate clean worktree/branch, run `e14767b5-9984-4e39-8bf7-ddd8b20b88ce`. Codex checked report/stdout/stderr/status/full diff and independently load/unload paths, then integrated two VersionManager files. Worker did not run tests or commands; without refusals, commits and other changes. Capture stores revision, and resolver refuses input before reloading same client. Independent asset edits outside VersionManager still require check before commit.

Canonical build after integration **PASS 0**, 178 steps, `g1-resolver-revision-canonical-20261102.log`; normal EXE `969d4a85…`. Real probe **PASS, normal exit 0, empty stderr**, `g1-resolver-revision-rme-20261102/verification.json`: 100×100 and 500×500, repeatable placement IDs, mask/protection, UID/AID/PZ and temple without mutation, RNG, bad client/brush/item, budget, pre-Cancel, refusal from worker, no-op and forced reload, failed reload, explicit/empty unload and old input rejection. Original assets identical; failure affected only private catalog copy. Closed only private RME startup warning via computer-use.

First runtime **FAIL exit 9** preserved in `g1-resolver-rme-20261102`: detected too-tight cumulative weight condition. Fixed handling of correct zero-weight variants. Single resolver 10,000/250,000 positions: 0.2923/6.7105 ms; fingerprint 15173774556512220455 / 2039901658534265928. Not full generator measurement or complete G1 E2E.

Verifier `g1-resolver-verification-20261102.json` **PASS_G1_PLACEMENT_AND_ASSET_REVISION_ONLY**: currentness of 411 probe objects, sources, test and EXE; comparison of 877 baseline sources showed only four intended differences GroundBrush/VersionManager. `git diff --check` PASS. Shared changes preserved. Claude worktree remains preserved: application refused archiving primary checkout and attaching unmanaged source checkout; without manual removal.

Full G1 still requires snapshot tile/halo/relevant assets, DrawOperations sandbox and entire concrete diff, full budget/RAM, Cancel from GUI during work, stale map/selection/assets before commit, atomic commit and its failures, separate Undo/Redo, minimal dialog and E2E round-trip 100×100/500×500. G2 unstarted. Docker, database, world and forbidden map/companions untouched; no new commits. Tool state at final check: `usageLimited`, not `complete`; did not reduce scope or pass entire gate.

## 2026-11-02 — G1-B: own input and controlled ground selection

**G1 still IN PROGRESS.** Added `captureGenerationInput` on main thread: own config/extent/masks, identifier and revision of indicated map, conservative `empty_only` protection, payload limits and Cancel. Protected are entire occupied tiles and TileLocation metadata without Tile (spawn coverage, waypoint, temple, house exit). [Contract and boundaries](procedural/G1_INPUT.md). Not full snapshot with halo/assets nor stale preview validation.

`GroundBrush::getPlacementItemIdForRoll` factors out existing variant selection, also used by normal draw. Explicit roll does not touch global RNG; preserved historical `<` bounds and fallback. Core got safe default values, `SemanticGrid::valid()` and Cancel also in mask validation. Algorithm v1 fingerprints unchanged.

Canonical Windows x64 `build_ninja.bat` **PASS 0**, 31 steps; `g1-input-canonical-20261102.log`, normal EXE `76d29a1c…`. Core CTest **PASS 1/1 Release**, five real caught single allocation faults and exact retry; `g1-core-faults-tests-20261102.log`. Full semantic vectors 100×100/500×500 still repeatable; not adapter E2E nor full RAM measurement.

Probe on real RME objects **PASS, exit 0, empty stderr**, `g1-input-rme-entry-20261102/verification.json`: 12 synthetic positions, nine protection variants, no mutation/aliasing, budget, pre-Cancel, refusal from worker, 10,000 repeatable brush choices without RNG shift. Without client, map opening, Application::OnInit, GUI and main loop. First link had LNK2005 from two `main`; kept as FAIL in `g1-input-rme-20261102`. G1 builder used private input object, without editing production Application.

Verifier **PASS_G1_INPUT_AND_VARIANT_SLICE_ONLY**, `g1-input-verification-20261102.json`: 410 probe objects, source and EXE hashes; of 877 G0 manifest sources differ only two intentionally changed GroundBrush files. Remaining shared C++ preserved. Claude review was read-only, without tools, refusals and changes; initialization and grid notes incorporated. Does not replace independent tests.

Open: full snapshot/halo/assets and resolver DrawOperations/autoborder, entire work budget/Undo, Cancel during run, stale preview, atomic commit with application error, separate Undo/Redo, minimal dialog and real E2E with round-trip 100×100/500×500. G2 unstarted. Docker, database, world and forbidden map/companions untouched; without commits of shared changes.

## 2026-11-02 — started G1, first G1-A package

**G1 IN PROGRESS**, branch `feature/procedural-g1-core` continues existing G0 history with preservation of all shared uncommitted changes. Added C++ library `rme_procedural_core` and real tests without GUI: [contract and scope](procedural/G1_CORE.md). Core generates semantic grass/dirt/water, has explicit Z, stable local RNG streams, irregular mask and protection, boundary/weight/size/limit validation, fingerprint and stop_token. Does not use global brush, Lua, item ID or live map.

New CTest target `test_procedural_core` **PASS 1/1 in Release**, without `assert` disabled by NDEBUG. Log: `local-data/procedural-smoke/g1-core-tests-20261102.log`; target build 0. Full vectors 100×100 and 500×500 repeated for same seed, different seed changed result. Mask/protection preserved excluded tiles and did not shift RNG of others. Checked bad bounds/Z/weights/mask, unrepresentable size, exact/exceeded budget and cancellation before start. Fingerprint has control constant calculated independently on explicit bytes.

Single core generation: 100×100 0.1132 ms / output 10,048 B, 500×500 2.6514 ms / 250,048 B. This is one trial time on this MSVC x64, without speedup assessment and without full peak RAM measurement. `CoreBudget` covers only output grid; snapshot/halo/resolver/Undo budgets not yet implemented. **These trials do not pass G1 E2E.**

First canonical `build_ninja.bat` **PASS 0**, 411 steps and link, log `g1-core-initial-build-20261102.log`. First invocation mistakenly used old wrapper and overwrote local `g0-final-gate-canonical-20261102.log`; saved copy of this new log named G1. Prior real G0 log `g0-final-canonical-20261102.log` still contains `ninja: no work to do` and BUILD SUCCESSFUL; remaining G0 reports and archived EXE unchanged. Later G1 builds have own wrapper/log. Did not recover lost log from guesses.

Final rerun canonical build after last core/test change also **PASS 0**, `g1-core-canonical-20261102.log`; library and RME EXE linked. `git diff --check` PASS. Ast-grep found no `std::shared_ptr` in new core/test. Earlier C++ sources from G0 audit remained unchanged; added only new module and its targets.

Not yet performed: resolver/sandbox, protection fetch from RME snapshot, full work budget, commit/Undo/Redo, stale preview, live guard or dialog. Did not check bad_alloc injection, interruption during generation or other platforms. New GUI not run — package has no user-facing flow yet. Docker, database, world and forbidden map/companions untouched; without commits of shared changes.

Next three tasks: G1-B snapshot and controlled resolver via shared DrawOperations/autoborder; G1-C preview coherence/atomic commit and separate Undo/Redo; minimal dialog and real E2E 100×100/500×500 with assets and full files. G0 remains closed audit; do not repeat its closed tests.

## 2026-11-02 — PHASE 0 VERIFIED, G1 not started

Closed audit gate: repeatable Windows x64 build, description of real integration points and synthetic map round-trip. [Final report](procedural/G0_FINAL_20261102.md) contains assessment of all six phase 0 positions and G0-01…09, evidence sources, constraints and receiver outcome justification. Did not accept partial tests as full OTS world compliance.

- Canonical build `build_ninja.bat` **PASS 0**, `g0-final-gate-canonical-20261102.log`, `ninja: no work to do`; EXE `e8c3041e…`. Initially Ninja missing from PATH; set existing Ninja directory and VCPKG_ROOT only in process. CMake CMP0167 warnings and unused toolchain variable remain.
- New normal GUI `g0-draw-lua-clean-20261102` **PASS**: two real dried grass strokes via GUI, Lua in between, three separate entries, all three Undo/Redo and cleanup. Group actions true, automagic false. Normal exit 0, empty stderr, no fixture save. Existing test with automagic true keeps own separate historical scope.
- First `g0-draw-lua-final-20261102` passed history but had wx accessibility assertions during shutdown; kept with diagnostics, not clean shutdown. Check closed Script Manager panel and did not query accessibility during destruction. Did not establish full cause and did not change renderer/library/system.
- `g0-final-gate-complete-20261102/verification.json` **PASS_G0_AUDIT_EVIDENCE**, verifier 0: currency of key implementations vs. manifests, 35+67+1 full snapshot comparisons, full verifier of 4 snapshots/2 Save As and 4 negative checks, nested paths and zone. Rechecked 180 current sources/76 objects and 6 BlackTek and TOML results. Finished runtime audits not restarted, their reports not overwritten.
- G0-09 has finished audit with constraint: RME preserves spawn XML, current receiver changes radius and omits direction. **Did not pass** full semantic spawn compatibility, monster activation, normal server, DB/Docker or complete converted world. Constraint and future protection contract described in report and ARCHITECTURE.

In this continuation changed documentation and added only local verification artifacts; without editing production C++, new commits, operations on Docker/database/world or `forgotten.otbm` and companions. Shared uncommitted changes preserved. All own GUI and build processes finished. Did not perform full GUI/Lua/CTest suite, all clients and all failure/Cancel phases; do not declare full atomicity, power-loss resilience or generator determinism. No further required work in G0 audit gate. G1 remains TODO and not started.

Below is chronology of earlier assessments; their FAIL/PARTIAL/IN PROGRESS are historical and preserved.

## 2026-11-02 — current point completed, handoff to new chat

At user request finished current BlackTek point and closed scope of this session. Full state, commands and evidence boundaries: [HANDOFF_G0_20261102.md](procedural/HANDOFF_G0_20261102.md). **Did not mark full G0 as complete.**

- Current isolated build without unity **PASS, exit 0**, `g0-blacktek-current-sources-20261102/build-hostx64-result.json`, sources unchanged; MSVC 14.44.35207 HostX64. Created server EXE not run. Kept failed attempts with outdated vcxproj and C1060.
- Separate current IOMap/Zones probe **6/6**, runner 0; `runtime-current/verification-current.json`: **PASS_BOUNDED_CURRENT_RECEIVER**. Real loadMap/loadHouses, selected house/tile/nested items data, PZ in runtime cache, spawn XML and production read of generated TOML. Five negative checks have expected results; data and all artifacts preserved.
- Final verifier checked 180 sources and 76 objects vs. current server HEAD `4f82d1b…`, manifest/EXE, probe source, input/DAT/conversion hashes and independently TOML content. Old 6/6 remains historical; new results tied to current sources.
- Semantic difference: new converter preserves Rat at 160,160,7 and 90 s, but uses fixed zone 158…162/padding 2, without legacy radius=3/direction. Do not declare their behavior or monster activation, normal startup, DB, deployed Docker or reopening converted OTBM as complete world.

Final normal RME GUI already has full PASS with empty stderr (`g0-gui-single-tab-20261102`), and user confirmed physical visibility in prior run. All private processes/windows finished. Docker and forbidden map untouched, shared changes preserved, no new commits. Next three tasks for new chat: review final G0 matrix without audit restart → settle meaning of new receiver semantics for gate and spawn protection → close remaining requirements and only then mark G0 complete, without starting G1.

## 2026-11-02 — GUI passed, final audit discovered newer BlackTek sources

**G0 still IN PROGRESS.** User confirmed window and ID 1 row physically. `g0-gui-local-observation-20261102` finished real 0/1/0/1/0 and normal exit 0, one wx SetFocus with two empty tabs; launcher timeout did not finish child, and existing terminal process verified without restart via handle observer and `--verify-existing-house-gui`. Separate `g0-gui-single-tab-20261102` passed all five visible states, Redo menu, normal exit 0, empty stderr and full runner 0. Canonical `build_ninja.bat` rerun: exit 0, `ninja: no work to do`, EXE still `e8c3041e…`; preserved CMake CMP0167 warning, no compilation error. Did not change renderer; earlier blank images remain PARTIAL, cause undetermined.

Final source identity check showed **186/187 discrepancies** vs. old BlackTek manifest, with 85/85 still identical old objects. Current server checkout `4f82d1b1375f994106cc012f3058b3a248bd9a43` has clean `src`, 77 CPP/103 H and new Zones mechanism instead of old Spawns. Old 6/6 is correct result of historical snapshot, not evidence of current server. G0-09 requires update before closure.

`g0-blacktek-current-20261102`: first new build revealed outdated deleted CPP list in preserved vcxproj. Finished only verified own MSBuild tree (PID 35744), kept log and build result 1, no binary; sources did not change during attempt. **Build not passed.** New `g0-blacktek-current-sources-20261102` uses private override list matching current `premake5.lua` (`src/**.cpp`), without unity, own obj/bin and source manifest before/after. Compilation in progress; do not rerun after timeout observation alone. Did not change server project files/sources, running Docker or user world.

Next three tasks: finish isolated current build → adapt separate probe to real IOMap/Houses/Zones and check private XML with negative checks → finish final G0 matrix. Do not restart RME audit and do not change earlier FAIL/PARTIAL to PASS.

## 2026-11-02 — resumed observation, current House image is readable

User explained that after prior question did not see open RME; window closed earlier, so that answer does not resolve prior display. After resumption command ran normal canonical EXE `e8c3041e…` on new private run `g0-gui-local-observation-20261102`, PID 31092, window 7279474. New real CUA image shows House header, empty table, and after Add blue ID 1 row; terrain assets also visible. Prior symptom now absent, cause of change undetermined. Did not change renderer code or system settings.

**Run in progress, not PASS.** Recorded only 0/1; after Ctrl+N two empty tabs exist (active Untitled-2), nothing saved. State with one house left for answer to new physical screen question. Win32 confirmed full path and live process; separate observer holds handle until real exit, also when launcher exceeds observation limit. After such timeout do not run second copy. Then finish Undo/Redo menu/Undo, normal shutdown and final matrix review; prepared scope of possible current Draw/Lua/Draw repeat. G0 still not marked complete. Docker, forbidden map and shared changes untouched.

## 2026-11-02 — symptom reproduced outside RME, final GUI requires local observation

**G0 not passed.** Next round delivered new cause check instead of repeating data audit. Windows reports active console session 1, Intel UHD Graphics 32.0.101.7088, status OK, 1920×1080; disconnected RDP hypothesis not confirmed.

Three independent checks have manifests, archived sources, logs and normal exit 0 with empty stderr; `wgl-control-comparison-20261102.json` and `verify_wgl_controls_20261102.py` link their results:
- `g0-wgl-control-20261102`: zero RME objects, only wxWidgets/OpenGL 4.5. Native text visible, GL area gray; back has three exact RGB, front black, swap success without errors.
- `g0-native-wgl-20261102`: Win32/WGL alone, without wxWidgets/RME/NanoVG/assets. Intel OpenGL 4.6, same buffer result and successful swap, captured area white.
- `g0-native-wgl-generic-20261102`: separate process/window with software GDI Generic 1.1 format; front and back have exact RGB, real CUA image shows red/green/blue stripe. Not supported RME renderer or substitute GUI pass.

The hardware presentation/capture symptom thus also occurs outside editor code and wxWidgets. **Not determined whether the physical image is empty or lost in capture; driver fault not proven.** No answer to the physical screen question. Further safe verification of final GUI requires this observation or change to the external state of the graphics session. Without it, repeating the same runs will not close the gate and does not justify speculative production fix.

No drivers, registry, Windows settings, or production renderer changed. All private control windows closed normally and confirmed absent from inventory. Working Docker, forbidden map, and shared changes untouched. Prior PASS of data/I/O/BlackTek and G0 matrix limits still apply. Next three tasks: local observation of hardware presentation → final normal GUI on private run → review and close G0 matrix. Do not resume entire audit or repeat hardware checks without new information.

## 2026-11-02 — GUI presentation diagnostics, without full pass

**G0 still IN PROGRESS.** Continued existing results without re-auditing BlackTek/Lua/I/O. Kept shared changes and prior FAIL/PARTIAL; no commits, Docker operations, or forbidden map changes.

`g0-render-buffers-20261102/partial-verification.json`: separate test module observed actual Add → Undo → Redo from menu → Undo. Map and exact table data passed 0/1/0/1/0, five consecutive paints, rendering enabled, prior GL error 0. Normal exit code 0; one wx SetFocus warning. Examined front PNG 0/1 black, back contains header and row. Back buffer after swap not evidence of presentation. Runner correctly did not pass missing positive visual observation. Additional Ctrl+N created second empty tab; nothing saved. Module/EXE have manifest and preserved source copy. Normal canonical EXE remained `e8c3041e…`.

Attempts `g0-gui-screenclip-20261102`, `g0-render-observer-20261102`, and `g0-gui-visible-startup-20261102` kept as partial. Last confirmed that SW_SHOWNORMAL alone does not remove symptom; after Undo private process ended normally code 0. Screenclip did not deliver completed region screenshot. Do not replace prior actual GUI PASS with another result.

`g0-render-swap-20261102/partial-verification.json`: separate experiment 0/1/0, additional native SwapBuffers returned 1, GetLastError 0, GL error 0. In five own canvas/parent windows no WS_EX_COMPOSITED and WS_EX_LAYERED. Examined additional front black, back with header/row, CUA image still empty. This does not prove driver fault or capture issue and is not normal GUI PASS. Reverted house and closed normally: child 0, stderr empty, runner correctly 1 for incomplete protocol. Asked user about physical screen image; response did not arrive before closing this private attempt. GUI tests did not remain running.

Next three tasks:
1. Resolve difference between actual screen and capture; preserved buffers, GL state, and successful additional swap narrow question, do not justify speculative renderer change.
2. Confirm final protocol of normal GUI on new private run; do not repeat entire data audit.
3. Only then review final matrix and mark fulfilled phase 0 requirements; do not count diagnostics as full GUI PASS.

## 2026-11-01 — final I/O/Cancel PASS, gate still open by GUI assessment

**G0 IN PROGRESS.** [TEST_PLAN.md](procedural/TEST_PLAN.md) now has matrix of six phase 0 positions and G0-01…09, with concrete evidence and limits. Did not mark partial GUI as full pass.

- Fixed complete companion paths in resolver, backup, marker/rollback, and OTBM link. Final canonical EXE `e8c3041e3e7d6b06599a2c391fcc0599cfd945ebab2583e98cd958c0b4a45cfd`, build 0. `g0-nested-companions-final-20261101/verification-final.json` PASS: relative/absolute nested XML, actual write/restore error and external target limit, exact independent read; three reader negative checks PASS. Kept prior actual FAIL/PARTIAL and old helper limit, did not restart successful child just to change report.
- `g0-persistence-after-paths-20261101` PASS on same final code: four snapshots, two Save As rel/abs, waypoint BOTH, writer error, 402 positions/full remaining nodes/nested items/XML, four negative checks. Stderr empty.
- `g0-save-cancel-final-20261101` PASS: test event of actual Cancel button after target create/truncate, new and existing set, full state/bytes/cleanup restore, Cancel reset, both retry and RME reads; full independent comparison, child/runner 0, stderr empty. Not manual click or all phases.
- `g0-gui-after-paths-20261101`: normal final EXE, new empty map, Add/Undo and normal exit 0, but full visual protocol **PARTIAL**, runner 1. No visible GL row after Add, old canvas pixels, one SetFocus warning. `startup-shutdown-verification.json` passes only start/handling/close with diagnostics; does not replace full GUI PASS. Check `g0-gui-baseline-control-20261101` on unbuilt, previously verified `07d87869…` showed same symptom and exit 0. This does not establish cause.

Next scope is concrete: explain render/capture discrepancy on private profile, confirm required normal GUI, then accept final matrix. Do not restart BlackTek/Lua/I/O audit and do not mark them alone as full G0. Working Docker and forbidden map/companions untouched; shared changes not reverted, no new commits.

## 2026-11-01 — new files after incomplete rollback and XML paths

**G0 still IN PROGRESS.** `g0-removal-denied-final-20261101` passed code 0 and empty stderr: actual writer created new OTBM/spawn, blocked house XML caused write/restore error, and private deny DELETE/DELETE_CHILD prevented deletion of new targets. Complete V1 marker (9 records), house backup, both new files and editor metadata saved; next write rejected. Guard released lock and removed only own ACL denial. First improperly prepared scenario kept as FAIL, without overwriting evidence.

`g0-partial-rollback-crash-20261101` passed limited interrupt/restart protocol: observation of actual `Save failed`, verified private PID 39916 handle and expected code 18177. After removing only test ACL denial, normal EXE showed `Incomplete save` with two existing targets. Ctrl+S attempt on new empty map denied with visible marker path. Independent verifier confirmed all four kept files and marker byte by byte, no rejected target, and six unchanged inputs. Launcher ended only own process after PASS; was not normal shutdown or power loss test. Details, hashes, and commands in [TEST_PLAN.md](procedural/TEST_PLAN.md).

Subdirectory attempt revealed actual `wxFileName` error and backup/writer path inconsistency. Resolver now parses full path; backup/marker/rollback use same resolver as XML. First `g0-nested-companions-20261101` regression is **PARTIAL**: relative write/read and actual blocked rollback passed, absolute companion read failed because OTBM wrote basename instead of subdirectory. Fixed this link too, preserving attribute kind/order. Canonical builds, final nested runtime, and full fixture after serializer change passed. Exact results and limits in current TEST_PLAN matrix; full gate remains open due to last incomplete GUI assessment.

## 2026-11-01 — non-empty history publication and current normal GUI

**G0 still IN PROGRESS.** `g0_rme_queue_allocation_probe.cpp` uses test subclasses of existing virtual factories (no hook in production) to arm scalar C++ allocator after action commit, upon batch publish entry. Prepares real Lua history and checks entry identity/labels, index, memory counter, and actual Undo/Redo. Merge forced by timestamp window only in test subclass, without changing system clock.

- `g0-queue-allocation-before-20261101`: **REPRODUCED FAILURE, process 9**. After 14 entries first deque growth refusal kept entry count 14/14 but changed queue state; code incremented memory counter before rejected `push_back` (read after error 5850). Full snapshot baseline PASS; refusal attempt did not reach full further verification. Kept `reproduction.json`.
- `g0-queue-allocation-after-20261101`: **PARTIAL**. All 35 C++ cases completed assertions but independent reader rejected 14 snapshots after intended trimming in limit tests. Restoring test AID via setter 0 did not reproduce missing attribute. Kept `partial-verification.json`; oracle not loosened. Final test restores full snapshot field via normal action, which distinguishes fixture rebuild after deliberate eviction from tested Undo.
- `g0-queue-allocation-final-20261101`: **PASS, code 0 process/runner**, 35 cases and 35 independent full OTBM/XML comparisons, 35 `clear()` checks. Deque growth 9 cases/2 actual publish refusals; entry count limit 9/2; merge with Redo 8/1; memory limit 9/2. Total 7 rejected allocations kept entire old entries/labels/index/counter and map; executed existing Redo and Undo. Eight further GUI refusals were reported and left complete undoable action. Remaining attempts without injection hits also passed.

The `ActionQueue::addBatch` fix first plans Redo rejection and current trimming, then reserves space for merge or new deque slot. Only after success changes old entries and counter. `clear()` also zeroes `memory_size`. Does not change on-disk or Tile layout; **does not guarantee exceptions in `Action::commit` or for any non-Lua caller**. Kept prior one-time entry count trimming; limit set below current count is not immediately strict limit. Does not declare strict RAM budget or performance; slot/container preparation may temporarily require extra memory.

Canonical build **PASS, 2/2, code 0**, log `g0-queue-allocation-canonical-20261101.log`; normal EXE `07d8786909395c813adcba3afc09ed8da0f468f7d12eb623f46da1350ae630af`, `action_queue.cpp` `7efc3a44fe3d5e96c3c0e18ff8e66143efa01d3a9589b3ce945166a08e97ff63`. Final module `b011cebe54a6c0e38132c67935a2d9029a140075226d3d885688d1c6d4998c1f`, test EXE `fc61f91a1963b47cb26d9c9549f01786767e0d35ab80105cc84eb445b980238a`, 408 fresh objects. Manifest includes queue and its object. Context7 Microsoft C++ Docs and local MSVC header confirmed deque insertion guarantee; reservation before merge checked via ast-grep.

Regressions on same current base **PASS**:

1. `g0-house-history-after-queue-20261101`: eight markers conflict/retry/identity/Add/Edit/Delete/restore/cleanup/live guard without listen; full snapshot compared independently. EXE `af625038bd61c92d61f731e1f3172b7b4369e8974b0475733f8d540e66713d37`.
2. `g0-lua-allocation-after-queue-20261101`: repeated full series 67 cases, 67 comparisons, 400 fields as one action, Undo/Redo, private RME write and read, and independent reader. Same source test `4056f8f8…`, new EXE `eb11440879aa235f9864a44293449d3cc5ebc2f9f11ef6ae9d99b9477eeea13e`.
3. `g0-house-gui-after-queue-20261101`: copy of **normal** EXE without module. New empty map, House 0 → Add ID 1/rent 0/size 0 → Ctrl+Z 0 → Edit/Redo 1 → Ctrl+Z 0. Observations saved before Alt+F4; normal exit **0**, full runner PASS, stderr **empty**, no alert or save prompt. Title still had asterisk after Undo; did not infer from it about data state. No map saved. This is limited evidence of normal GUI/shutdown on current build, not test of all views/live.

All hashes of original fixture and copies unchanged. Queue module has 36 wx `SetFocus` warnings, explicitly recorded in report; not marked as visual GUI PASS. Similar Lua module limits still apply. Did not change Docker, database, user world, or forbidden map; shared changes kept. RME validation selector still review-only, so used above real tests.

Next 3 tasks: (1) force refusal of deletion of new private target on write rollback and check V1/files behavior; (2) complete explicit scenarios of partial write interrupt/recovery without touching user world; (3) final matrix of G0-01…09 requirements and gate assessment. Untested VM/constant OOM/active peering are limits of described evidence, not full atomicity pass.

## 2026-11-01 — Lua commit allocation errors, house, and rerun smoke 400 fields

**G0 still IN PROGRESS.** New separate `g0_rme_lua_allocation_probe.cpp` executed production `app.transaction` on active private fixture tab. Single scalar C++ `operator new` refusal armed only after five ground AID setters (also house field, nested container, and floors 6/7), before return from callback. Does not exhaust system memory or swap allocator in normal RME.

- `g0-lua-allocation-before-20261101`: **FAIL, code 9**. Without injection commit/Undo removed house ID fields 104/102/7 (detected only by independent snapshot, not test AID alone). On first allocation refusal rollback left **401 instead of 402 fields**, history 0/0. Kept logs, both snapshots, and `reproduction.json`; none is full PASS.
- `g0-lua-allocation-after-20261101`: **PARTIAL, code 9** after keeping rollback copy, house ID, and dirty state. All 58 saved snapshots passed independent full OTBM/XML comparison but case 57 reported error and restored map, keeping already published history 1/1. Kept `partial-verification.json` describes inconsistency, not series pass.
- `g0-lua-allocation-final-20261101`: **PASS of data/history of described scope**, full process/runner 0. **67 cases and 67 independent full comparisons**; baseline has 58 allocations. Of 58 actual refusals 54 ended in full rollback, history 0/0, restored house, and unchanged dirty state. Four refusals concerned GUI notifications: noted diagnostics, complete commit of five changes, one action, and correct Undo restoring full fixture. Remaining attempts without injection hits also executed full commit and Undo. After each case new empty transaction confirmed end of previous.

Fixes: `LuaTransaction` keeps own snapshots for accepting whole batch; uses separate copies for temporary base state restore. Rollback also restores earlier dirty state. Detaching house index does not zero house ID in historical object. `Editor::notifyStateChange` notes GUI observer `bad_alloc` instead of propagating as map operation error, especially after history publish. Changes fit within existing methods, without changing Tile format/layout. **Cost:** extra copy of existing changed field in Lua commit to end of publish; time/memory not measured and no speedup declared.

On same current objects repeated existing scripts `g0_smoke.lua`/`g0_verify_saved.lua`: actual new empty editor → 400 ground from loaded `grass` (4526), one action → Undo removes all fields and keeps empty halo → Redo restores result → new private save via `EditorPersistence::saveMap` and RME re-read. Independent reader compared exactly 400 positions/ground/empty stacks and metadata and both empty XML. Original fixture and three copies unchanged, no marker; executed script hashes checked.

Canonical build **PASS** (`g0-lua-notification-canonical-20261101.log`), normal EXE `f274e2ca51ca78c4a871b22a86acc9d5ac65f707305079e41018d65ce34caa3b`. Separate EXE `4554b08e33dc1e1a52e845a3701b11275cb255988c1cd86505be2b2ca1dda9dc`, 408 fresh objects, module `4056f8f861a5736347429c4b9f3dc1581f66b84efb5236778423b5db1961e1be`. Sources and normal EXE match final manifest. RME skill selector still review-only; executed proper build and real behavior tests.

**Limits:** series starts with empty history and checks five existing ground setters; does not cover Lua VM, persistent/concurrent OOM, non-empty history/Redo/limits, other mutators, and active peers. Module destroys test editors then uses `_Exit`; no global shutdown evidence. Stderr contains four expected notification failure reports and **69 wx `SetFocus` warnings** at serial tab create/close in one callback. Warning causes not determined; does not declare clean visual GUI. Kept them in log and `post-verification-diagnostics.json`, without changing original functional report. Closed only read common material warnings dialog of private profile. Docker and forbidden map untouched, shared changes kept.

Next 3 tasks: (1) test non-empty history/Redo and limits behavior on batch accept error; (2) remaining write interrupt/recovery phases and new target deletion error; (3) final G0-01…09 matrix and gate assessment based on current, sufficient evidence. Remaining explicit limits above are not full atomicity pass.

## 2026-11-01 — reproduced house loss on allocation error and fix

Test-only `g0_rme_house_allocation_probe.cpp` replaces scalar `operator new` only in separate EXE and arms single refusal in test thread directly before actual `Houses::addHouse`. On earlier objects (normal base `ed8c2b92…`, test EXE `102a637f…`) actual `std::bad_alloc` node insertion destroyed object taken by parameter by value. `g0-house-allocation-before-20261101`: code **9**, launcher **FAIL**, kept `reproduction.json` and log. This run did not reach further cases.

Fix limited to `Houses::addHouse` and declaration: parameter `unique_ptr&&`, first `try_emplace` empty node, then nothrow ownership transfer and metadata/counter update. Collision refusal also leaves object at caller. Does not change map format or action implementation; all existing call sites compile. Conditional move pattern `try_emplace` checked in Context7 Microsoft C++ Docs.

Canonical `build_ninja.bat`: **PASS, 155/155, code 0**, normal EXE `0677c3a73e5f52a23e498f78cd37020e59cf09341bf37cbd6e297b9f6a31be3d`. `g0-house-allocation-after-20261101`: same module source hash as in reproduction, 408 current objects, separate EXE `1cb5303dfc9822b18744f93be926d69c54ad13dfb5db8fa0f72031d7fb16e799`; **PASS process and launcher, code 0**. Confirmed actual exception, preserved owner/identity/generation/name/rent, empty map after error, unharmed ID counter, successful retry with same object, and safe duplicate refusal. Does not save map. All inputs and copies kept hashes. Manifest includes `house.cpp`, header, and object. No full proof of exceptions in `Action::commit`, metadata after insertion, and `ActionQueue`; this result concerns insertion into `Houses`.

History regression after fix **PASS**, `g0-house-history-allocation-fix-20261101`, 408 current objects, test EXE `720c55bfe6a91978a88f8abfea00199e49504b8ddea92b8c2d4db96c459ac3fc`, source `f1408cf54b8fc0c0118a31f96a04157e00e01e36bd40cf0f4c7f19619144db38`. Eight markers: conflict/retry Undo and Redo, substitute rejection, full Add/Edit/Delete chain, field/exit/identity restore, history cleanup, and live state lock without network open. Restored map snapshot passed RME re-read and independent full OTBM/XML comparison; inputs kept hashes. Process/launcher code 0. Module now also waits for `OnInit` end and active main window; destroys local editors before `_Exit`, does not test global shutdown. Test actions not executed in normal user session.

C++/Lua decision already recorded as chosen responsibility boundary based on noise/algo/geo tests, Draw/Lua/Draw, and transaction errors and regressions. Marked only this plan point as done. **G0 still IN PROGRESS**. Next three tasks: (1) full commit/rollback allocation errors Lua and queue; (2) remaining interrupt/recovery phases and new target deletion error; (3) final G0-01…09 matrix with existing evidence. Did not change Docker or forbidden map; kept common uncommitted changes.

## 2026-11-01 — Lua rotation and podium methods: PASS on private profile

Separate module `g0_rme_item_methods_probe.cpp` linked with 408 current RME objects (source freshness check by builder). Normal EXE not changed: base `ed8c2b92…`; test EXE `fd35b2522005c85611fb071cb3889d1eb1aa79316624d961bd7cd588375b07f5`, module source `cffd0188e11f4142ec8b6a3b12c12bef3a8f2229a27c5e2f203ced406c6b5348`. Artifacts: `local-data/procedural-smoke/g0-item-methods-20261101` (build manifest, inputs, log, observed exit 0, `verification.json` PASS).

Real Lua engine and active 402-field fixture editor executed three cases: `rotate` 1442→1476 by loaded definition, `setPodiumOutfit(128,3)`, and `setPodiumOutfit(129)` at prior direction 3. Each covered reference obtained before transaction, forced error after change and rollback without history entry, explicit commit and Undo/Redo, then implicit commit and Undo/Redo. Read-only C++ oracle compared all 12 `Outfit` fields, direction, and three podium flags. Negative lookType, direction -1/256, and method on plain item rejected without state/history change. Final Undo removed both test items, restored index 0, empty stack, and 402 fields.

**Limit:** available profile 1098 does not define podium. Only in new private XML copy was type of existing 1443 set to `podium`, using normal loader and `Item::Create` factory. This is proof of real `Podium` object binding/history, not podium support by standard profile or deployed BlackTek. Map not saved; podium serialization and global shutdown not checked (`_Exit` after test editor close). Read and closed normal material warnings dialog of private profile. Hashes of three source files and copies, base XML, and script remain unchanged; hash of intentionally changed private XML also checked after test. Docker and forbidden map not used; kept others' changes.

**G0 still IN PROGRESS. Next 3 tasks:** (1) reproduce allocation error in house ownership transfer path and in Lua commit/rollback, ensuring data and history preservation; plain `error()` does not prove it; (2) complete open write interrupt/recovery phases and new target deletion error on separate fixtures; (3) assemble final G0-01…09 matrix with existing artifacts and formal C++/Lua decision, without retesting passed cases and without claiming full atomicity.

## 2026-11-01 — correct closing tab selection and palette detach: PASS

Canonical build of current `editor_tabs.cpp` **PASS, code 0**. Final normal EXE SHA-256 `ed8c2b926d8c9c676d0b6e607d010708b9657cbfd2764fe0272a7aff10cf0b8d`; source `06ae6fe77fabc02f32f9b9833578a26d4d8e6da663b282fc4c9a31c793e9f231`. Fixed two reproduced errors: page selection from `wxAuiNotebookEvent::GetSelection()` instead of `GetInt()` and detaching borrowed palette objects before tab removal and re-assigning to remaining editor after it. Change covers only close handling, without map data/format mutation.

**PASS of actual GUI**, fresh `g0-house-close-final-20261101`, copy of normal EXE without test module:

- Click x of second clean tab closed that tab; `house-roundtrip.otbm` with house ID 1/rent 4321/size 1 remained.
- Adding house in next empty second tab marked it changed. Close attempt asked about `Untitled-3.otbm`. Cancel kept both tabs and new house. Retry and No closed only that second tab; fixture and its house remained.
- Closing last fixture emptied house list and town selection. Re-opening same private copy showed house, next close again cleared palettes. Edit without map opened nothing and did not crash.
- Normal Alt+F4: code 0 no alert. Launcher ended PASS, saved exit code before verification; all hashes of three inputs and copies unchanged, no new map/marker. Nothing saved.

Manifest, `run/house-ui-observation.json`, `observed-exit.json`, and `verification.json` contain scope of evidence. Prior PARTIAL runs kept without changing their assessment. `sorky-validate-change` skill selector has no RME route (review-only); thus executed canonical build and above real GUI test. Did not test closing shared views of same map or active live sessions. Still **G0 IN PROGRESS**: remaining Lua/atomicity and write interrupt cases and final assessment of all requirements not closed. `forgotten.otbm`, its companions, and running Docker BlackTek untouched; kept others' uncommitted changes.

## 2026-11-01 — wrong tab closed during regression

First palette fix build ended code 0, EXE `d7cb77055bdeb26fa41ac19bb552670c58985c4b03dfe34d03d45eb3978f0203`. Attempt `g0-house-close-fixed-20261101` confirmed empty palette and empty town selection after last map close, re-opening private fixture with house and again clearing after close. Click Edit without map opened no dialog nor ended process. But with two tabs clicking x of second (`Untitled-2`) closed first (`house-roundtrip`). Verifier rejected attempt due to explicit diagnostics; report remains **PARTIAL**, normal exit 0.

Source of second error is `evt.GetInt()` in `OnNotebookPageClose`. Local wxWidgets 3.3.1 defines `wxAuiNotebookEvent` as `wxBookCtrlEvent` whose page index is in `GetSelection()`, independent of event command integer field. Changed handler to this index with range check, including page selection to save question. Functional fix build passed; after indentation-only correction final build of current file is running. Regression of both fixes on that final build remains to do. All attempts concern only synthetic maps and private processes.

## 2026-11-01 — GUI Remove, rent editing, and stale palette indicator

Second fresh private normal RME instance (`g0-house-edit-gui-20261101`, old hash `29691821…`) repeated base Add/Undo/Redo/Undo cycle, then added empty house ID 2. Dialog rejected edit with no town (`Invalid town selected`); Cancel kept rent 0. Remove button and actual confirmation deleted house; Undo restored ID 2, Redo deleted again. Then opened only `run/house-roundtrip.otbm` — private copy of synthetic fixture (402 fields, G0 Town, G0 House). Just rent change via dialog gave 4321 → 4322, Undo → 4321, Redo → 4322. Closing that tab showed `Save changes` with name `house-roundtrip.otbm`; chose No. Changes of earlier empty tab also rejected. No map saved.

After last tab close palette still displayed ID 1/rent 4322/size 1. Code analysis confirmed borrowed references were re-assigned before `DeletePage`, and last page does not guarantee change event. This **PARTIAL, not full GUI PASS**; report `extra-gui-verification.json` explicitly preserves error. Normal Alt+F4 ended process code 0. Runner earlier exceeded 300 s; without restart opened Win32 handle of still-live PID 17936, checked full path and observed termination. Hashes of original fixture and all three copies remained unchanged.

Minimal fix in `MapTabbook::DeleteTab` detaches all palettes before removal and refreshes them after removal against remaining current editor, also with no map. Build and GUI regression of this fix in progress; older run remains as error evidence. Did not change user files, others' existing changes, or Docker BlackTek.

## 2026-11-01 — normal house add GUI, Undo/Redo, and close

Status: **G0 IN PROGRESS**. On fresh private profile ran copy of normal RME (`29691821bfcbd40da449162ec20015adee4471f4731e4c03b730d08d63907b8a`), without test module. New empty map, House palette: empty list → Add button showed ID 1/rent 0/size 0 → Ctrl+Z removed row → Edit/Redo restored same row → Ctrl+Z again emptied list. Alt+F4 ended process normally code 0, no save question and no wxWidgets alert. Map not saved. Normal material warnings at startup closed before test. Initial Ctrl+Y was not Redo; menu shows Ctrl+Shift+Z, proper test done from menu command. Redo shortcut action not confirmed.

Artifacts: `local-data/procedural-smoke/g0-house-gui-20261101/{gui-build-manifest.json,observed-exit.json,verification.json,run/house-ui-observation.json}`. Launcher reached observation read after `process.wait` and code 0 check but file not yet present (first save command pointed to unavailable Python alias). Saved actual observations and explicit exit code origin from this launcher path, then `--verify-existing-house-gui` verified already-ended run without restart. Launcher error itself remains noted; future runs save exit code before observation read. Verifier confirmed EXE copy hash, three hashes of original fixture and its copy, and no new map/marker.

**PASS of this GUI scenario and normal shutdown**, not all house operations. Still unchecked: Edit/Remove buttons, save warning after rent change via dialog, live peers, allocation failures, correct Lua rotation/podium, and all write interrupt/recovery phases. Prior house core tests and actual `IOMap::loadMap` BlackTek remain separate evidence of scope stated earlier. Did not change production C++, others' changes, user maps, or working Docker BlackTek.

## 2026-11-01 — actual first-save interrupt and normal GUI restart

Status: **G0 IN PROGRESS**. Private probe interrupted by Win32 `TerminateProcess` after reading actual complete V1 marker during `EditorPersistence::saveMap` of new empty map, before its removal. Observer checked EXE path and PID 12416; both processes (observer and launcher) confirmed expected code `18177` (`0x4701`). Standard runner reported this code as interrupt instead of PASS incomplete test. Marker kept bytes, three targets remained absent. Exact serializer instruction at stop not determined. First earlier attempt completed normally before interrupt and is not failure evidence.

On same private fixture ran copy of normal `build-ninja/rme.exe` (same checked hash `29691821bfcbd40da449162ec20015adee4471f4731e4c03b730d08d63907b8a`), without test module. Config has separate data and empty recent maps list; disabled only for this instance welcome, single-instance forwarding, and updater. Startup showed actual `Incomplete save` with three targets from interrupted write. After close created empty `Untitled-1.otbm`. Ctrl+S → new private `blocked-after-crash.otbm` ended `Save failed` with full saved marker path. After closing message tab still had same unmodified name; no target or companion created.

**PASS in this scope**, `g0-firstsave-crash-watch-20261101/{interruption.json,restart-verification.json,restart-ui-observation.json,restart-exit.json}`. Marker SHA-256 `b730eda50009ae9e36598bdb93c9fe481ee82aaa015ca61465093f8a577a7994`, matching before interrupt, after, and after retry write. Three input copies and original fixture kept hashes. After verification launcher deliberately ended only own private GUI process; did not check normal app shutdown. Scripts and repeat in `TEST_PLAN.md`.

Did not check interrupt in other phases, partially written targets, existing map with backups, recovery itself interrupt, or power loss. This is real test of one interrupt and following restart/write block, not full PASS crash recovery. Did not change production C++ in this attempt. `forgotten.otbm`, its companions, open user session, and Docker/world/base BlackTek untouched.

## 2026-11-01 — V1/legacy recovery after deferring map open

Status: **G0 IN PROGRESS**. Build of `OnEventLoopEnter` fix and linking separate module with 408 objects completed code 0. Normal EXE SHA-256 `29691821bfcbd40da449162ec20015adee4471f4731e4c03b730d08d63907b8a`; test EXE `3a421d9feccf0e9b8074fa009a0cadb4264d1255a7754b2446fb09656d5710f8`, module `ed66061311da43a76c762cf332a599278ce721e8920fbabb77cff75b1997f9a3`. Current source and normal EXE match with manifest confirmed after attempts.

- `g0-recovery-deferred-runtime-20261101`, full V1: **PASS**, code 0. Actual prompt listed only three private backups; after confirm restored all target bytes, removed marker and completed backups, then opened 402 fields. Read normal material warnings; prior progress dialog destructor alert did not occur.
- `g0-recovery-legacy-deferred-20261101`: **PASS**, code 0. Actual legacy warning and recovery prompt; three targets restored, 402 fields loaded, all three backups and marker kept byte-for-byte for check. Prior wxWidgets alert did not occur.
- `g0-firstsave-v1-20261101`: **PASS**, code 0. First write of actual empty editor again checked refusal with existing and empty marker, actual V1 writer with correct data, `noreplace` behavior, I/O error to nonexistent directory, full checked state rollback, and successful retry. During error message read actual V1: header, count 10, seven empty backups, three new targets, and footer; saved in `observed-marker-lines.json`. RME read and independent parser confirmed empty OTBM and both XML.

Each directory has `verification.json`, runtime log, manifest; startup cases also `startup-ui-observation.json` with actually read windows of private EXE. Hashes of source set and three copies unchanged. No test re-run on existing `run`. Closes indicated regression of opening recovered map and stated protocol scenarios, not actual process interrupt or persistence after power loss. Remaining GUI attempts, recovery errors, and final G0 gate audit still open. `forgotten.otbm`, its companions, and Docker/world/base BlackTek untouched.

## 2026-11-01 — recovery marker completeness

Status: **G0 IN PROGRESS**. New `recovery_marker.h` defines private `.saving.txt` format V1: version header, record count, seven backup positions and optional new targets and end marker. OTBM/XML unchanged. Writer checks data before file open and preserves `noreplace`. Reader limits size and record count, rejects incomplete/unknown formats and bad paths before restoring anything. Old format still recognized but cannot prove optional targets complete: recovery of known backups then keeps all copies and marker for manual check.

RME build after protocol change **PASS**, EXE `1f60b352a87c9815a165f8d62422f57a01cdab0c4bac7754e469538a42510ba0`. New CTest `test_recovery_marker` **PASS 1/1**: each truncated prefix of correct new marker, bad count/footer/version, extra data, old CRLF, UTF-8, bad records, and stream limit. Sources/hash/logs: `local-data/procedural-smoke/g0-recovery-marker-20261101/{unit-verification.json,CTest-LastTest.log}`.

Actual startup with truncated marker **PASS in refusal scope**: read `Incomplete recovery marker` dialog; all three targets, three backups, and marker remained byte-for-byte unchanged. Artifacts: `g0-marker-startup-final-20261101`. First earlier variant `g0-versioned-marker-runtime-20261101` invalidated: idle module could end process inside startup dialog. Kept `verification-premature.json`, current status UNVERIFIED. Fixed module requires `IsMainLoopRunning`, verifier also noted actual dialogs of private EXE.

Legacy startup reproduced three private targets, loaded 402 fields, kept three backups and marker. But wxWidgets assertion about `wxGenericProgressDialog` loop occurred during map open in `OnInit`; after Continue and closing known data warnings attempt ended code 0. Runner exceeded 180 s wait so not re-run: opened handle of still-live PID 18676, checked EXE path and observed final code 0. Mode `--verify-existing-startup` did only completed artifacts check. Report `g0-marker-legacy-final-20261101/verification.json` explicitly contains diagnostic limit — this is **not** clean GUI PASS.

Deferred recovered map open to existing `OnEventLoopEnter` instead of `OnInit` and added test condition to not start with disabled main window. Build of this final fix **PASS**. Linking new test module ongoing in `g0-recovery-deferred-runtime-20261101`; full marker runtime, legacy after this fix, and first-save retry on V1 remain undone. Still no test of actual process interrupt and power loss. `forgotten.otbm`, its companions, and Docker/world/base BlackTek untouched.

## 2026-11-01 — actual first save, refusal, and retry

Status: **G0 IN PROGRESS**. Separate `g0_rme_first_save_probe.cpp` linked with current 408 RME objects passed test **PASS, code 0** in `local-data/procedural-smoke/g0-first-save-runtime-20261101`. Creates actual new `Editor` with empty unnamed map of compatible version; does not toggle existing map flag. Actual `WriteCrashMarker` refused overwriting existing marker. `EditorPersistence::saveMap` rejected first write with existing and empty marker, no map change and no target file.

Write to nonexistent private directory forced actual I/O error. During message read actual marker: seven empty backup positions and three new targets OTBM/house/spawn. Saved this read: `run/data/user/data/observed-marker-lines.json`. Closed only `Error` message of private EXE; no normal RME windows handled. After function return metadata, unnamed, and dirty matched input state, marker removed, no target directory and files. Next first write to fresh directory succeeded: unnamed/dirty cleared, new companion names, no `~` backups and marker. RME read and independent parser confirmed empty map and correct empty house/spawn XML. Input set and copy hashes remained unchanged.

Manifest: EXE `31310090481d9f355defb8aa6cb53e335c6436deb3f6b601e673363d0763cbec`, module `f7ad5d0ccdea63511f03ba23e243fa1f7f0981a18be08f8b510e5236cf1b996f`; report `verification.json`. This test of first empty map save and I/O error and rollback, not actual process failure, power loss, or startup recovery of this attempt. Runtime still shows error message despite `showdialog=false`; test requires its close. Runner leaves PID for inspection after timeout and does not auto re-run process.

Runner removes last maps from copied config and rejects template containing `forgotten.otbm`. Did not open or change this map/companions, working Docker BlackTek, its world or base. Production sources after checked build did not change in this attempt. Actual write/recovery interrupt, corrupted/incomplete marker cases, and remaining plan gates remain open.

## 2026-11-01 — Undo of house add/remove and live metadata lock

Status: **G0 IN PROGRESS**. Kept prior working tree. `HousePalette::OnAddHouse` and `OnRemoveHouse` now create `Change`/`Action` instead of directly changing house set. Same `House` ownership transfers between map and history: Undo of delete preserves generation, properties, field list, and exit so prior actions remain executable. Fields keep remaining content and flags. Delete clears active brush pointers; changes mark map and proper regions changed. ID change in properties also updates exit index, whose absence extended check revealed. House metadata not distributed by current live protocol, so add/delete/edit blocked in UI and action queue in that state.

Build `build_ninja.bat` **PASS** after adding existing Ninja to PATH and `VCPKG_ROOT` only in process. First linking stopped by open old EXE; kept its process and state, moving file to `build-ninja/rme.pre-house-lifecycle-20261030.exe`, next build created new `rme.exe` (SHA-256 `f058557fda7acc87ba8635ea02bf164c408ef751eadc788d5c1100063df9f26f`). Did not close others' session or map.

Extended separate test module with 408 current RME objects **PASS, code 0**, run 11/01 (snapshot 08:43:42). Artifacts remain in directory named at prep 10/30: `local-data/procedural-smoke/g0-house-lifecycle-runtime-20261030`. `verification.json` requires eight runtime markers, unchanged hashes of three input files, and independent snapshot comparison after house restore. Confirmed:

- full chain add → edit ID/properties → delete, three separate Undo and three Redo; same object reproduced;
- delete existing house, field flag preservation, exit index delete and restore without dups, Undo refusal on ID conflict and retry after its delete;
- safe history delete with detached house and existing conflict and object substitute cases;
- refusal of add/delete/edit in actual `LiveManager`/`NetworkedActionQueue` state without mutation, dirty, and history entry, and return to offline edit after live state close. Test does not call `bind`, does not open port, does not create peers;
- restore snapshot read by RME and independent comparison of all 402 positions, remaining OTBM data and house/spawn XML with input.

Test EXE SHA-256 `6705c7a31d3c2401e446f4ae737febcfa447e904c851cf2cec5749cc92befb66`, module `702d8bae78972953ecb14524fb5ae32a2417811fa0c56e2a113074ef75e7699e`. Checked both and normal EXE match manifest before run. Did not yet test GUI buttons after this change, two-peer cooperation, or allocation errors. Test skips global app shutdown via `_Exit` after local editor destroy. Did not open or modify `forgotten.otbm` and its companions; working Docker BlackTek, its world and base untouched.

## 2026-10-30 — house history retry and object substitute

Status: **G0 IN PROGRESS**. New test module `g0_rme_house_history_probe.cpp`, linked separately with same 408 RME objects, passed actual `Action`/`ActionQueue` operations on private fixture. Undo conflict for ID 1 and Redo conflict for ID 2 kept both houses, field ID, and history index. After removing each conflict retry of operation worked, reproducing properties, generation, exit, field count, and house ID relation; Undo after clearing dirty again marked map changed. Delete of edited house stopped old Undo; new house with same ID had different generation and also remained untouched by old action.

Build and runner **PASS, code 0**. Artifacts: `local-data/procedural-smoke/g0-house-history-runtime-20261030`, log `run/snapshot-probe.log`, manifest and `verification.json`; source fixture and copy hashes unchanged. EXE SHA-256 `85fd8b952f006f5fb729499dac3c2c153a9f0af5ea93f212d7e0a4ed80a03c27`. Test does not save map and skips global app shutdown via `_Exit` after local editor destroy. Not additional dialog test or deleted house restore: existing add/delete still have no Undo. Live and all dialog fields still need work. Did not change production sources or open RME instance; Docker BlackTek untouched.

## 2026-10-30 — actual snapshot and Save As with BOTH

Status: **G0 IN PROGRESS**. Without production changes linked separate `snapshot_probe.exe`: 408 existing current RME build objects and wxWidgets test module. Module calls actual `MapBackupService::CreateSnapshot`, `EditorPersistence::saveMap`, and `EditorFactory::LoadFromFile` on private 402-field copy with house, spawns, UID/AID/PZ, and nested container. Does not run destructive `UnreachableCleaner`. Normal EXE, source fixture, and working Docker BlackTek untouched.

- **PASS snapshot:** four separate sets, also with actually observed name collision in same second; clean/dirty state and map/companion file names kept. Each result re-read in RME. Bad directory name rejected. Lock of new house XML forced actual serialize error; partial OTBM remained, message pointed directory, dirty and metadata restored.
- **PASS Save As:** relative and absolute input house/spawn names, output with new relative names bypassing two sentinels. `BOTH` kept waypoint in OTBM and separate XML, changed text/description and UID/AID. Target waypoint XML collision rejected write without new OTBM and without state change. All sentinels remained.
- **PASS independent check:** complete 402 positions, 401 unchanged fields, container semantics, remaining nodes and both XML; for Save As exactly expected ground and waypoint changes. Four negative checks found changed rent, map width, and corrupted waypoints in both formats. Runner checked unchanged copy and source fixture hashes.

Repeatable runner ended code 0 at 18:01:46; evidence in `local-data/procedural-smoke/g0-persistence-runner2-20261030/{build-manifest.json,verification.json,run/snapshot-probe.log}`. EXE SHA-256 `8dc5e943d334328f551a80b5d8a68fcf38ba5b29a102d8cbe845ae190d2530fa`. First runner stopped by missing sidecar `snapshot_probe.exe.manifest` and wxWidgets window; after reading warning closed it, process finished test. Fixed runner rejects missing manifest before run; second run required no interaction. Prior correct snapshot also read by isolated BlackTek loader (0), partial rejected on house XML (5).

This is separate integration test of real functions, not full GUI or app close test: after local editor destroy module runs `_Exit`. Did not check process/power failure, allocation exceptions, concurrent writers inside reserved directory, or complete first write/recovery protocol. House history scenarios, remaining Lua requirements, and final gate summary remain open. Details and commands: `TEST_PLAN.md`.

## 2026-10-30 — reopening map and Lua setter matrix

Status: **G0 IN PROGRESS**. Working tree preserved on `fix/save-as-companion-safety`. Continued previous attempt rather than repeating loader audit. Created separate profile in `local-data/procedural-smoke/g0-reference-profile-20261030`: copy of `data/1098`, shared data files, and `clients.toml` with single unchanged profile `1098-BlackTek`. Local `config.toml` points to this directory via `data_directory`. Did not change normal build data directory or map header. After previous test instance completed, ran same RME EXE from 12:31:05, SHA-256 `2cd4a8ad93f6d0c7410ca2f200df56f2b6f467f9f4d11b5f39fbc6301d331f67`.

- `test_reference_map_reopen.lua`: **PASS**, armed 17:35:44, result 17:36:17. Welcome opened 402 tiles, closed tab, File → Open opened same copy without profile change and without Lua reload. Old tile/item references rejected; new ones read fixture values. This closes this specific attempt, does not prove reuse of same memory address or all lifecycle transitions.
- New `test_item_setter_matrix.lua`: **PASS**, 17:38:10. 17 combinations: AID/UID/tier/text/description for ground, stack item 2148 and container root 1987, and count/subtype for stack item. Each case checked reference obtained before transaction, error propagation and rollback without history change, explicit commit with Undo/Redo, and standalone setter with separate Undo/Redo. Ten out-of-range numbers and `setPodiumOutfit` on non-podium rejected without history change. Final Undo removed temporary items and restored fixture data under test. Did not test items inside container, correct rotation/podium, allocation errors, or serialization of changed tier/text/description.
- Recorded actual console read in `lua-tests.log` and hashes and checks in `verification.json` in profile directory. OTBM and both XML are byte-identical with input copy, and scripts run from `build-ninja/scripts` match files in `lua_tests`. Did not save map. RME remains running on test copy after Undo to history index 0.

Did not change production sources or working BlackTek/Docker. Still require work: Save As/recovery tests and real snapshot, remaining house history scenarios, and final audit of all G0 gates. Next three scopes: (1) snapshot runtime and write error on copy; (2) missing Save As/recovery variants; (3) remaining Lua/house history requirements and final C++/Lua decision with full evidence table.

## 2026-10-30 — fresh isolated BlackTek loader

Status: **G0 IN PROGRESS**. Confirmed final linking in `g0-1098-20261026/blacktek-fresh/build.log`. New harness built with 85 current per-source objects, excluding old unity objects and `otserv.obj`. `otserv.cpp` compiled separately with changed `main` name, without `/FORCE:MULTIPLE`. Build tool rejects missing/newer CPP sources and headers; records hashes of sources, objects, and EXE. Existing uncommitted changes preserved on `fix/save-as-companion-safety`.

On private copies of RME-saved fixture `g0-container-20261030` actual `IOMap::loadMap`, `IOMap::loadHouses`, and `IOMap::loadSpawns` passed assertions: house ID 1, name, rent 4321, town, exit and linked tile; floors 6/7 with UID/AID/Z-index; bag → bag → 7 coins with AID 12042; spawn Rat, position, radius 3, 90 s, and south direction. Rat has minimal synthetic XML definition in test directory, loaded by real creature loader. Not a test of user creature definitions or their runtime execution in game.

Runner executed **6/6 PASS**: correct set and expected rejection of missing house XML, missing spawn XML, changed rent, empty spawns, and truncated OTBM. Empty spawn assertion is necessary: XML parser success alone does not prove creature load. Byte comparisons confirmed no copy mutation during read and preservation of three source files. Report: `local-data/procedural-smoke/g0-1098-20261026/blacktek-fresh/iomap-cases.json`; build manifest: `probe_build_manifest.json` in same directory. Sources and repeat instructions: `docs/procedural/smoke/g0_blacktek_*` and `TEST_PLAN.md`.

**Process limitation:** first attempt after successful assertions and correct local map destruction ended with access violation `0xC0000005` during final destruction of global server objects; exact cause not determined. Final harness calls `_Exit` only after return from test function (after local map and buffer destruction), skipping this phase. Result confirms read of indicated fixture by current Windows loader, not full server start/stop cycle or deployed Linux/Docker image compatibility. Running container, its database, and world were not restarted or modified.

Prepared `lua_tests/test_reference_map_reopen.lua` and started it in real RME on new copy `g0-reference-reopen-20261030/g0-metadata.otbm`, build EXE from 12:31:05. First stage reported `REFERENCE REOPEN ARMED` at 17:31:08, then closed tab. Attempt to reread via File → Open selected standard profile `10.98` and stopped on missing assets; directory selection cancelled. Second stage not executed, test **UNVERIFIED**. Map files not saved or header changed. Test instance remains without open map; Lua not reloaded. Need explicit profile selection on reopen in same session (e.g., isolated data directory with unambiguous profile), without substituting incompatible standard client OTB.

`git diff --check` and existing local documentation validator (UTF-8, closed blocks, links, and archived scripts) passed. Did not rebuild RME in this scope: did not change its production sources. Remaining open G0 tasks preserve current status. Next three scopes: (1) complete Lua reference/setter regression and map reopen; (2) complete Save As/recovery and snapshot tests and open house history scenarios; (3) compile all G0 requirement evidence and close C++/Lua decision without counting loader test as full gate completion.

## 2026-10-30 — reference lifetime and house validation

Status: **G0 IN PROGRESS**. On isolated map with profile `1098-BlackTek`, tab-switching test confirmed tile and item references are inaccessible on second map and work on return to first if objects exist. Separate attempt after closing first map also rejected access on second. Did not check same map reopen or nested items in container; Lua API does not expose its contents.

Audit of additional snapshot mechanism revealed collision risk: `MapBackupService::CreateSnapshot` creates name with second precision and writes OTBM and XML without reservation; on error deletes computed paths without checking if they belonged to earlier snapshot. Only found caller is `UnreachableCleaner::Clean`, which after separate confirmation removes tiles and clears history. This is code finding, not reproduced data loss case. Requires separate fix for safe reservation of whole set and tests of collision and write failure; timestamp alone or existence check before write does not close race.

Prepared snapshot change: new directory created via `create_directory` before serialization, with next suffix on collision; OTBM/XML set goes to this directory. On write error partial files remain and message points to directory. RME Windows x64 compilation and linking **PASS**. Have not yet tested actual snapshot, collision, write failure, or read of its files; fix is in progress and does not prove robustness against other processes writing inside already-created directory.

Reservation extracted to function used by `CreateSnapshot` and tested in `test_snapshot_directory`: eight concurrent reservations gave different directories, pre-existing directory and file preserved contents, and missing parent directory reported error. CTest Windows x64 **PASS 1/1**; rebuild and link RME **PASS**. This is reservation test, not full snapshot serialization, write error, or reread of it.

In separate profile tested synthetic interrupted first write marker: startup reported two new targets and preserved marker and contents of both files after program close. Paths were visually shortened in dialog; full paths found in accessibility tree. Details and limitations in `TEST_PLAN.md`; did not simulate real failure.

After replacing native message with existing text field Windows x64 build passed. Restart with same marker showed both complete paths wrapped in window; Ctrl+A selected full content. Did not check clipboard to avoid changing it. Marker remained in fixture.

On same isolated profile opened copy `guard-control.otbm` (400 tiles) and used Ctrl+S with first write marker. Editor refused save; SHA-256 of OTBM, both XML, marker, and both new targets unchanged. Marker path in message was initially shortened, so replaced with text field; build and retest GUI confirmed visible full path and same block. Did not test failure during real write.

On synthetic OTBM 3 copy tested nested container: bag 1987 → bag 1987 → 7 coins 2148 with AID 12042 at (100,100,7). RME with `1098-BlackTek` opened map, saved it, after restart reopened and saved; independent node read after each save confirmed these values. First byte comparison failed because RME changed count/AID attribute order; semantic comparison passed. Did not check full map state or other container content types.

Additional comparison before first and after second save confirmed 402 positions, identical nodes of remaining 401 tiles, and preserved XML trees of houses and spawns.

Then extended comparison to header and remaining OTBM nodes: for this fixture also identical. Five verifier tests passed, including negative attempts to modify metadata, tile, container, and house XML. External loader and attributes absent from fixture remain untested.

Attempt to run `IOMap::loadMap` in separate program with existing BlackTek build objects yielded no result: process ended before test entry confirmation, and loader object was older than source. Need fresh isolated build; standard server initialization performs connection and database updates before map load.

After limiting conflict check to house property action final Windows x64 build passed, and `test_item_implicit_undo.lua` on isolated `g0-metadata.otbm` confirmed AID change and Undo/Redo (`IMPLICIT ITEM UNDO PASS`, 07:25:14); test restored original value, then map was saved. On current build reran conflict: on copy `g0-house-conflict-final-20261030` changed house ID 1 → 2 and added new house with ID 1. GUI refused Undo with conflict message, and `test_house_history_conflict.lua` confirmed Lua Undo and history navigation do not change history position (`HOUSE HISTORY CONFLICT PASS`, 12:10:26). Copy saved after test. Adding and removing house still has no Undo support. During previous process close wxWidgets alert `GetWindow() != nullptr` occurred again; cause not determined. Separate attempt to select `New Palette` after opening copy ended with process disappearance; cause unknown, repeatability not confirmed.

**Work in progress:** prepared uncommitted history operation for house property edit, including ID change. On copy `local-data/procedural-smoke/g0-house-undo-20261030` Windows x64 build passed, and rent change 4321 → 6789, Undo, Redo, and XML save gave expected results; Undo after save restored 4321 and marked map as changed. ID change 1 → 2 and Undo/Redo worked in palette, and after save read ID 2 in XML and in tile node (104,102,7); after Undo and next save both files showed ID 1.

**Discovered regression requiring fix:** after repeating ID change to 2 added new house which took ID 1. Undo did not overwrite new house but also did not revert ID change and shifted history. Changes after house addition not saved. Prepared code not ready; must ensure consistent history behavior in conflict and investigate non-undoable house add/remove and collaboration mode.

In next fix queue checks condition before Undo/Redo, preserving history position on conflict; GUI reports problem in status bar, Lua returns error. Windows x64 build **PASS**, 197/197. On same isolated copy recreated ID conflict: GUI reported occupied ID and preserved both houses; `lua_tests/test_house_history_conflict.lua` **PASS** at 07:18:45 for `undo()` and `goToHistory()` with unchanged history index. After test state of two houses saved in test copy. Did not check retry after conflict removal or resolve non-undoable add/remove, restore deleted object, and live sync.

- House property validation executes before ID change: invalid town or rent breaks operation, and other house ID rejected even with duplicate warning off. This is limited fix, not Undo fix or modified map marking; these problems remain open.
- After fix Windows x64 build and linking **PASS**; selected CTest `test_lua_object_registry`, `test_lua_object_registry_sol`, `test_rollback_file` **PASS 3/3**. House validation not yet tested in GUI. `git diff --check` no errors, warnings on line endings.
- During test instance close wxWidgets assertion `GetWindow() != nullptr` in `GetDescription()` occurred again; cause not determined. Test map not saved.

## 2026-10-29 — Lua references and G0 regression

Status: **G0 IN PROGRESS**; changes remain uncommitted on branch `fix/save-as-companion-safety`. Historical results below preserve state from attempt date; current details and limitations are in [TEST_PLAN.md](procedural/TEST_PLAN.md).

- Lua bindings for tiles and items use references checking lifetime and current owner. Map item setters participate in transaction, standalone calls create Undo operation; clone outside map remains independent.
- Windows x64: last build and link **PASS**. CTest registry and Sol2 tests **PASS 2/2**. In editor on isolated copy with client `1098-BlackTek` passed rollback and ground history tests, standalone setter, stack item, iterators, and selection. These are described scenario tests, not full API verification.
- Save As and recovery in progress: checked selected collisions, Unicode, and single-file rollback; full atomicity, process failure, and new marker variant not confirmed. Discovered loss of unsaved house rent change remains unresolved.
- Floor, flag, UID/AID, spawn, separate house, and one zone round-trip is partial. External BlackTek parser read OTBM 3 nodes, but full loader and helper files not tested; tested BlackTek loader rejects OTBM 5.

Next three tasks: (1) check remaining reference operations, including containers and map switching; (2) complete and test safe Save As and recovery and resolve house change loss; (3) perform full round-trip and proper external receiver test, then audit all G0 gates.

## 2026-10-27 — manual history and extended round-trip

Status: **G0 IN PROGRESS**. Code base is `a71fe4b`; results below from this build. Created branch `docs/procedural-g0-validation` from `master`, then fast-forward to callback fix.

- **G0-06 PASS in tested range:** manual eraser 1×1 draw, Lua transaction, and manual `dried grass` brush draw created separate entries. Three Undo and three Redo checked snapshots of ground, ID stacks, flags, house ID and entity presence for 24×24 area and separate Lua tile; after sequential Undo fixture restored. `group_actions=true`, `use_automagic=true`, history limit 400, 40 MiB. First retry of eraser edit did not change content, so test assertion correctly rejected it; retried with different brush.
- **Partial G0-08 PASS:** synthetic tile on floors 6 and 7 with ground UID/AID and flag, spawn radius 3 and `Rat` (90 s) created, saved, and reread in RME with `1098-BlackTek`. Verification included 400 base grounds and halo; spawn XML separately parsed. Separate house fixture after restart preserved visible label, ID, rent, and size; XML also contains town ID and exit coordinates. Zones, containers, full attributes, and external receiver not verified.
- **G0-07 FAIL for existing Item:** `ground.actionId` change inside failed transaction not rolled back despite error propagation. Successful transaction with same change also did not create history entry. Diagnostics restored value without save. Need separate fix for Item mutation tracking and regression tests.
- **Nested transaction:** inner call rejection no longer reverts outer. Test on synthetic map confirmed single action and Undo/Redo of both changes; Item attribute problem remains open.
- **House change loss:** on synthetic fixture rent-only change did not mark map as changed; tab close without prompt left previous value in XML. Ctrl+S save before close worked. Need separate fix accounting for Undo.
- **Partial noise test PASS:** 400 simplex values in RME agreed between grid, repeat, and single samples; different seed changed result. Does not confirm brush determinism or full generator.
- **Partial algo/geo test PASS:** sample cave with seed repeated, Bresenham line had correct ends and continuity. Observed code lack explicit point table ordering for some operations; need further tests before use as stable contract.
- **Save As:** previously confirmed overwrite of source map XML on synthetic set. Prepared fix gives copy new, free XML names; for waypoints name derived from OTBM name required, collision breaks write. OTBM 3 test confirmed source and existing file preservation, first empty map write and waypoint XML read. OTBM 5 regression, cancel, and rollback remain before G1; see `procedural/TEST_PLAN.md`.
- During test session close wxWidgets assertion `GetWindow() != nullptr` in `GetDescription()` appeared; chose Stop after save and close. Cause and test relation not determined.
- Separate target `test_universal_rotator` built with `--parallel 2`, then `ctest --test-dir build-ninja -R "^test_universal_rotator$" --output-on-failure`: **PASS 1/1**, 0.20 s, code 0. Not full project suite.
- Detailed steps and results: [TEST_PLAN.md](procedural/TEST_PLAN.md). Preserved scripts can run only on isolated fixture copy.

## 2026-10-27 — G0-B: Lua callback error fix

Current status: **G0 IN PROGRESS**, specific ignored callback blocker removed. Branch `fix/lua-transaction-callback` created from `master` and completed with fast-forward of audit `b2bc871`.

- `LuaAPI::transaction` checks `sol::protected_function` result; callback error goes to existing rollback and returns to Lua. No changes to snapshot, `ActionQueue`, and serialization implementations.
- Final source Windows x64 build **PASS**, code 0, incremental with `--parallel 2`; EXE from 18:52:13 run in GUI.
- New test `lua_tests/test_transaction_rollback.lua` **PASS**, 18:53:46: three errors with preserved messages, six mutations rolled back, history without new entries after error. Successful transactions and separate Undo/Redo correct; fixture restored to 400 tiles.
- Original reproduction `g0_error_probe.lua` **PASS**, 18:54:08. Final verifier **PASS**, 18:54:30. SHA-256 OTBM before/after identical; test did not save map.
- Updated API documentation and regression run instructions. CodeGraph and Context7 used for mechanism check; `sorky-validate-change` selector has no route for RME, so applied real editor build and tests.
- Diff control and UTF-8, code blocks, and relative documentation links **PASS**. Preserved scripts match executed after CRLF/LF normalization; initial byte comparison rejected only line ending change after branch switch. New regression script matches copy run in RME.
- Still unverified: manual edit alongside transaction, many floors and complex metadata, nested transactions, map close/switch in callback, allocation failures, and external RME/OTS compatibility. Full Lua/CTest suite not run. Detailed logs: [TEST_PLAN.md](procedural/TEST_PLAN.md#g0-b--naprawa-callbacka-i-regresja-2026-10-27).

Next tasks: G0-06 with manual edit; fixture with metadata/many floors and round-trip; then close adapter decision before G1-A.

## 2026-10-27 — G0 audit and test results

Historical entry of audit before G0-B fix above: **G0 IN PROGRESS**, G1 not started. Changes at that time included documentation and manual diagnostic scripts, no production code changes. Detailed commands, versions, hashes, and evidence scope: [TEST_PLAN.md](procedural/TEST_PLAN.md).

- Windows x64 build **PASS**, code 0: after Ninja/PATH issue and PCH memory error resumed same target via `cmake --build build-ninja --target rme --parallel 2`. EXE 8026112 bytes, linking `[382/382]`; ran GUI 4.1.2.
- Profile `1098-BlackTek` (`dat_only`) loaded from user-indicated assets with signature validation on. `grass` range works; loader reports invalid ranges and missing other material IDs. Standard OTB not approved for this DAT.
- Synthetic Lua map edit: **400 ground + single Undo + single Redo PASS**. Halo remained absent. OTBM and two empty XML write, then explicit read with BlackTek profile: **PASS for this fixture**. Verifier confirmed all ground IDs, empty stacks/entities/house ID/flags and halo; grass visible in renderer.
- **Blocker: G0-07 FAIL.** Intentional `error()` after tile change in `app.transaction` does not propagate and does not revert change: `propagated=false; tile_present=true; tile_count=401`. Final diagnostics assert reported error. One manual Undo and verifier rerun restored 400 tiles; diagnostics change not saved.
- Normal File → Open picks standard profile `10.98` from header. Read test done via Welcome with explicit `1098-BlackTek`. After confirming path in Preferences process disappeared once without message; cause unknown. For further attempt used isolated `config.toml` and explicit cwd.
- CodeGraph MCP available 26.10; 27.10 transport closed, continued via working CLI `codegraph explore`. Context7 and `ast-grep` used in source audit. Skill `sorky-validate-change` indicated diff review for documentation.
- Still not done: separate history from manual edits, full metadata UID/AID/houses/spawns/zones and many floors, compatibility with external RME/OTS. Previous 33 PASS / 7 SKIP parser and 34 PASS / 6 FAIL with nonstandard client remain separate results.
- Final documentation check: UTF-8, closed code blocks, and relative links in 5 Markdown files **PASS**; three preserved scripts byte-identical with executed sources; both empty helper XML correctly parse as `houses`/`spawns`.

### Next three tasks

1. G0-B: fix callback result check and rollback in `app.transaction`; preserved [probe](procedural/smoke/g0_error_probe.lua) is error reproduction, not passed test.
2. Complete G0-06 and round-trip fixture with metadata/many floors; separately record compatibility result with target receiver.
3. Only after G0 close C++ adapter decisions (RNG, snapshot, atomic commit) and start G1-A.

## 2026-10-26 — start of PHASE 0

Historical entry: status during ongoing build. Final result and current tasks described in 27.10 entry above. Replaced earlier Git/CodeGraph constraints from preparatory review.

- Base: `5a01a8fe1c34465559701d50681cc2210ef32c67`, local initial snapshot. No equivalent upstream SHA determined. Initial working tree clean.
- Branch: `docs/procedural-phase0-audit`, created from `master` before changes.
- CodeGraph MCP working; CLI reported current index 3328 files / 21857 nodes / 56553 edges. Investigated draw paths, Undo, Lua, preview, and write; `ast-grep` confirmed brush fetch from GUI.
- Created [architecture and PR plan](procedural/ARCHITECTURE.md) and [test plan with evidence](procedural/TEST_PLAN.md). Specific risks: immediate batch part commit, GUI/RNG in drawing, incomplete empty tile definition, preview cursor dependency, and Lua callback error handling. Runtime conclusions remain unconfirmed until smoke test.
- Context7: Sol2 documentation `/thephd/sol2`, handling `protected_function_result::valid()`. This is upstream documentation; exact behavior of vcpkg version requires reproduction.
- Reproduction on installed Lua 5.5.0#1 / Sol2 3.5.0#1 with project config: ignored `sol::function` returns without exception despite `error()` and preserves earlier side effect; `valid()` detects error. Local C++ probe and command described in `TEST_PLAN.md`. Confirms transaction wrapper risk but does not replace true map/rollback/Undo test. Did not change production code.
- Build: first run of `build_ninja.bat` stopped on Ninja not in `PATH`. Found local Ninja 1.13.2 in vcpkg; reran same script with temporary `PATH` and `VCPKG_ROOT`. Found CMake 4.3.0-rc3, Visual Studio 18 Community, MSVC 14.51.36231. Built dependencies and configured RME, but compilation at default parallelism ended with `C3859/C1076` and Windows 1455 (no virtual memory for PCH). Resumed same target and build tree with `--parallel 2`; attempt in progress. Did not change pagefile, sources, or canonical script.
- Tests `tools/test_tibia_data.py -v`: 40 cases, **33 PASS / 7 SKIP**, exit code 0. Skipped integration tests requiring `Tests1098`.
- Read `data/1098/items.otb`: **PASS**, 26282 items, OTB version 3.57.62-10.98. Correct CLI: `tools/tibia_data.py --otb data/1098/items.otb info`; initial placement of `--otb` after `info` rejected by parser, then fixed.
- Initially found no complete DAT/SPR set, OTBM map, or ready EXE in checkout. User then indicated `C:\repo\ProjectSorky\otclient\data\things\1098` and `otclient_gl_x64.exe`. Inspection of indicated DAT/SPR with local OTB 10.98 passed: signatures `0x42A3` / `0x57BBD603`, 26282 items and 338944 sprites, no warnings. Only read data; OTClient EXE does not replace RME build.
- Further test revealed correct parsing and same signatures **do not prove mapping compatibility**: original tests run on copies of indicated assets and local OTB in ignored harness gave **34 PASS / 6 FAIL** (code 1, no SKIP). Errors concern expected fixture, flags/IDs, and sprite hashes. Did not change tests, original assets, or mapping to get PASS. Details and local log described in `TEST_PLAN.md`.
- CodeGraph confirmed existing mode `ItemDefinitionResolver::resolveDatOnly`; smoke candidate is profile `1098-BlackTek` from `data/clients.toml`. DAT ID 4526 corresponds to transient ground, standard OTB maps server ID 4526 to different client ID (4515). `dat_only` choice requires confirming load and materials in GUI; standard OTB not approved for this client.
- GUI, Lua 20x20, rollback, Undo/Redo, RME round-trip, and external receiver compatibility: **not done**. Data tool tests do not replace these proofs.

### Historical build tasks

1. Bring canonical build attempt to final result and record log/limitations.
2. Confirm complete asset set and run smoke test per G0-01–G0-09 in `docs/procedural/TEST_PLAN.md`.
3. Based on reproduction close C++/Lua boundary, RNG, snapshot, and atomic commit decisions; only then start G1-A.

## 2026-10-26 — plan analysis and drafting

Session scope: prepare `PROJECT_RME_PROCEDURAL_GENERATOR.md` for implementation. Plan version: 1.1. Did not start generator implementation; source review is partial G0 preparation, not gate completion.

### Done

- Read plan, `AGENTS.md`, `RTK.md`, CMake/vcpkg config, script instruction fragments and build script.
- Checked integration points: `DrawOperations`, `ActionQueue`, `LuaTransaction`, `noise`/`algo`/`geo` registration, terrain demo, RNG ground/doodad, map identity and OTBM helper file write.
- Refined MVP, minimal GUI, snapshot and conflicts, adapter determinism, area protection, rollback, memory budget and test criteria. Details and source locations: section 1.0 of plan.
- Context7: `/websites/wxwidgets/3.3`, worker–GUI communication and event receiver lifetime documentation. This is documentation verification, not RME test.

### Evidence and limitations

- `rtk git status --short --branch`: no Git repository; directory has no `.git`, has `.gito`. Did not change metadata, create branch or commit, SHA not determined.
- No `.codegraph/`: per instructions did not query nonexistent index or run indexing.
- `rtk proxy ast-grep --version`: 0.45.1. Structural query `g_gui.GetCurrentBrush()` confirmed three draw overloads depend on current GUI brush.
- Used `rg` and source reads via `rtk proxy`. Did not assume API declarations prove runtime correctness.
- Document verification in PowerShell: correct UTF-8 read without substitution chars, closed code blocks, sections 0–14 and 12 references to existing source files/scripts — OK. First attempt to verify via `python` did not run (no interpreter under that command); verified via PowerShell.
- Build, GUI run, Lua 20x20 smoke test, Undo/Redo, OTBM round-trip, regression tests and benchmarks: **not run**. Changes include documentation only.

### Phase status

| Phase | Status | Commit | Evidence/tests | Blockers or dependencies | Next task |
|---|---|---|---|---|---|
| 0 | IN PROGRESS | Audit `b2bc871`, fix `a71fe4b` | Build, tile creation rollback and mixed history PASS; partial round-trip floors, UID/AID, flags and spawn PASS | Direct AID change not tracked; Save As overwrites shared XML; houses/zones and external receiver unverified | Fix both errors and remaining fixture |
| 1 | TODO | — | — | G0 | Single floor MVP |
| 2 | TODO | — | — | G1 | Terrain and borders |
| 3 | TODO | — | — | G2 and references | Reference analysis |
| 4 | TODO | — | — | G3 and holdout | L1/L2 profiles |
| 5 | TODO | — | — | G4 | Multi-floor topology |
| 6 | TODO | — | — | G5 | Ranking and feedback, optional ML |
| 7 | TODO | — | — | G6 | GUI expansion |
| 8 | TODO | — | — | G7 | Broader measurement and reliability |
| 9 | TODO | — | — | G8 | Beta and release |

### Historical preparatory review tasks (current plan above)

1. Determine copy origin, proper Git checkout access and SHA; create branch from `master` and save local changes. Use CodeGraph after index made available.
2. Confirm toolchain, available assets and test map; perform canonical Windows x64 build and record result, without declaring success in advance.
3. Run smoke test on copy of complete files (20x20, Undo/Redo, write/read), place audit results and integration plan in `docs/procedural/ARCHITECTURE.md` and `docs/procedural/TEST_PLAN.md`.
