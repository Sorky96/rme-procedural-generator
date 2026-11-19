# G2 — “sensible surface and borders” gate status, 2026-11-07

**CLOSED by user decision on 2026-11-07 (PASS with debt, not an unqualified PASS).** Gate: “terrain readable in RME and no known critical border errors”. Evidence: core tests and probes on the real 1098-BlackTek client (pairs_without_border=0, G1 regressions unchanged), user GUI reviews (vegetation, Undo/Redo, 500×500, dialog, generation next to terrain, seed 7 after fixes), and screenshots of seeds 4242, 7 and 1 (4242 and 7 before the final fixes, 1 after them). Accepted qualifications were recorded as debt: rivers still mostly follow axes and seed 4242 has no final review (D6, D7), a seam next to existing terrain (D5), and only client 1098-BlackTek, no performance measurements and no walkability review with trees and stones. The sections below record successive steps and evidence in chronological order. Earlier “PARTIAL” statuses in tables and sections are historical.

**Update 2026-11-07 (after closing G2):** D7 was closed by user decision. The user considered screenshots of seeds 4242, 7 and 1 OK (user review, not a test). Surface memory-estimate recalculation and the river-width smoothness test were not performed. D5 and D6 remain as debt ([TECH_DEBT.md](TECH_DEBT.md)).

## G2 plan criteria and evidence

| Criterion | Evidence | Status |
| --- | --- | --- |
| Natural rivers | test_procedural_surface: every river ends in a lake or at the area edge, has a minimum length, and every river tile lies in a 2×2 window made of river or lake (test added 2026-11-07, see below). Appearance review: user, after rivers were widened (“Looks better”). | PASS core; appearance assessed from one view |
| Lakes, rocks | Same tests: minimum lake and rock-patch size, upper share limits, 24 seeds 100×100, 6 seeds 250×250, 3 seeds 500×500, every feature appears in every seed set. | PASS core |
| Shores | Shore is at most shoreWidth steps from the lake (test added), and land touches a lake only through shore. Default is two tiles after GUI test. | PASS core; appearance: user review, 500×500 |
| Biome transitions | Smooth grass/dirt transition with noise scale 10 and removal of patches below 24 cells (transitionBand test: 88 hard versus 246 soft edges). Dirt share in open land: 0.24–0.39 (100×100), 0.28–0.32 (250×250), 0.29–0.31 (500×500). | PASS core; appearance: user review |
| Semantic mapping with the existing autoborder only | --surface resolver: pairs_without_border=0 in all six runs (seeds 1–4 at 100×100, 1729 at 250×250 and 500×500). No custom border logic was added. | PASS (1098 client) |
| Vegetation and terrain-dependent details | [G2_VEGETATION.md](G2_VEGETATION.md); core test: no decoration on water, shore or rock, no trees near water; E2E with Undo/Redo. User GUI review: “Trees look good”. | PASS tests; appearance assessed |
| Distribution tests | Above plus decoration: tree share on land ≈3.1% (24 seeds 100×100) and 3.2% (6 seeds 250×250). | PASS |
| No unsupported items | Resolver and preview check every item (definition, clientId, not meta; decorations also not ground or border); E2E: placed items match the plan for every cell. | PASS (1098 client) |
| Visual tests on several seeds | **Not performed.** Only individual user views exist; no screenshots from several seeds are in the repository. | OPEN |
| No known critical border errors | pairs_without_border=0. D5 (borders outside the area next to an external neighbor) was solved for Surface by leaving cells next to existing ground empty, with test SURFACE_NEXT_TO_TERRAIN_PASS. | PASS tests (1098 client); seam appearance not assessed |

## Changes in this step

- Added core-test checks for river width and the upper shore-width bound, plus a 500×500 review (three seeds).
- The river-width check found a one-tile tail at the river source (seed 1, 100×100, cell 51,50). Expansion added only the eastern and southern neighbors, so the final route cell lacked a full 2×2 block. Each route cell now also adds the south-eastern neighbor (lakes unchanged). Side effect: rivers are slightly thicker on diagonals. The pinned Seed 1729 100×100 fingerprint changed from 4385766052247269449 to 13556304753518616142. Algorithm version remains 2 (unreleased).

## Changes after GUI review on several seeds (2026-11-07)

User feedback: appearance was OK. Rivers should thicken after merges and narrow toward their source, must not flow from “empty places”, and brushes should be selected from lists rather than typed.

- **River sources:** rock cells only. The route starts at the first non-rock cell below rock, so a river exits the rock edge. Without rock there are no rivers. Test: every river touches rock.
- **River width (second version, after user screenshot: “rivers still not fixed”):** the first version thickened a river only after tributary merges, so a single river had exactly two tiles. Tile size is now the number of cells traveled from the farthest source plus 15 for each incoming route: below 20 → 2 tiles, below 45 → 3, from 45 → 4. The source at rock is thinnest, the river grows gradually toward its mouth and a merge adds a step. Tributaries only flow in, the river does not split downstream. River tiles in a full 3×3 block: 1232 of 4971 (24 seeds 100×100), 453 of 1869 (6 seeds 250×250), 183 of 735 (3 seeds 500×500). Rock and lakes are not overwritten, so a block can be clipped next to rock (the test allows this). Working-memory estimate (40 B/cell) was not recalculated after adding two uint16 vectors per cell.
- **Fingerprint:** Seed 1729 100×100: 1951698793224813922 (previously 13556304753518616142). Rivers now occur in 24 of 24 100×100 seeds (previously 23).
- **Dialog:** brush fields are dropdown lists for selection only (ground brushes for grass/dirt/water/rock/shore, doodads for trees/bushes/stones), with default choices grass, dirt, clear water, rocky ground, sand, trees, bushes and small rocks when present in the loaded client. The list does not check whether a brush makes sense for its role (for example, water as grass); the resolver still validates this.
- Probes after the change: g2d3-surface-160136 and g2d3-commit-160136 (--surface records passed: false only because of D1; exit code 0, empty stderr), G1 commit rows unchanged, pairs_without_border=0, halo_tiles_modified 101 (100×100 seed 1) and 55 (250×250), 0 in the other runs (D5).
- **Slow dialog opening (confirmed by the user as a dialog issue):** data contains 15 741 doodad brushes and 4 756 ground brushes (counted from XML in brushes/ and data/), and eight lists were filled on every opening, about 70 000 entries in total. Lists are now wxComboBox controls for selection only: before expansion they contain one item (the default or first), and the full list loads on first expansion. The doodad list is limited to brushes with single variant-0 items because only those are accepted by the resolver. Opening time was not measured before or after. The change is based on item counts, not measurement, and lazy loading on expansion was not checked in native RME (there may be flicker or a delay when first expanding a 15 000-entry list).
- **User GUI test after lazy loading:** “It is OK” (dialog opens without problems and lists work), with no timing measurement and no separate river review for seeds 1, 7 and 4242.
- **Not verified:** appearance of new rivers and list behavior in native RME; a dialog with a client lacking the default names (first list item); dialog performance with many brushes.

## D5: generation next to existing terrain (2026-11-07)

- **Cause:** after ground is placed, GroundBorderCalculator also calculates borders for neighboring tiles (the halo, and with selection holes and protected tiles, inside the area too). If the area edge has a higher-priority brush than the existing neighbor, the border lands on the neighbor tile and the G1 pipeline intentionally rejects every change outside the writable area, without clipping.
- **Reproduction (before the fix):** the new nextToTerrain test in --surface (100×100 seed 1 surrounded by grass) refused preparation, and after the first half of the fix it refused InvalidPlan (preview requires strict equality: “item exists ⇔ cell is writable”).
- **Solution (Surface only):** surfaceKeepClear(snapshot) identifies writable cells with a ground-bearing, non-writable cell in their 8-neighborhood (outside the area, outside the mask or protected). Those cells remain empty (Preserve in generator and resolver, without an item), so border tiles are at least two steps from external terrain. Preview allows an empty writable cell only when the same function identifies it (strict equality preserved). Classic and the G1 OutOfMask test are unchanged. With an empty map and empty surroundings, output is identical to before (probes, fingerprints).
- **Test result** (d5d-surface-162711): SURFACE_NEXT_TO_TERRAIN_PASS for 100×100 seed 1 (396 seam cells, 9604 placed), 100×100 seed 1 with an existing center tile (404 / 9595) and 250×250 seed 1729 (996 / 61504). Preparation and validation pass, cells next to existing ground are empty, all remaining cells are placed, existing terrain is unchanged after commit, Undo and Redo. Regressions: --preview and --commit PASS (d5e-*-162844), G1 commit rows unchanged, core test PASS.
- **Cost:** a one-tile empty seam next to existing terrain (must be filled manually), with no smooth transition at the neighbor side. Surface preparation memory estimate increased by 2 bytes per cell plus overhead (bytes in SURFACE_E2E_PASS 100×100: 13385154 → 13405218).
- **User GUI test (2026-11-07):** generation next to existing terrain, “Looks good” (visual review; GUI Undo/Redo not reported separately).
- **Not verified:** interaction with vegetation next to terrain (not tested); neighbor types other than grass (test ring is grass); tiles containing only items without ground (not counted as a neighbor); clients other than 1098-BlackTek.

## Irregular shore (2026-11-07, external reviewer note)

Note: concept good, irregular sand, grass should intrude in places; it looks “random”. Interpretation: sand around a lake had constant width (lake outline), while a natural beach should narrow and widen.

- **Change:** shoreWidth defaults to 3 (previously 2). The first ring at a lake is always sand. Each later ring appears only where smooth noise (grid every 6 cells, separate shore_s2 stream) exceeds a threshold that grows with the ring (35%, 65%, 95% of the range). Result: the beach is wide in some places and grass intrudes to the first ring elsewhere.
- **Distribution** (sand tile count 1/2/3 steps from water): 4253/3290/1273 (24 seeds 100×100), 6126/4763/1863 (6 seeds 250×250), 12984/10185/4085 (3 seeds 500×500). The test requires rings 2 and 3 to exist and be rarer than the previous rings.
- Seed 1729 100×100 fingerprint: 7666204984290374682. Core test, --surface (with SURFACE_NEXT_TO_TERRAIN_PASS) and --commit PASS (g2f-surface-171132, g2f-commit-171132), pairs_without_border=0, G1 commit rows unchanged.
- **Not verified:** appearance in RME and whether it matches the review intent (“grass clumps in sand” could also mean isolated grass clumps inside sand, which this change does not make: grass intrudes only from outside); effect on vegetation (trees stay two steps from shore, so wider sand means fewer trees near water).

## Visual review of several seeds (2026-11-07, user screenshots)

Screenshots 250×250, Natural surface, vegetation enabled, zoom about 44–46%, order 4242, 7, 1: [seed 4242](screenshots/g2-surface-seed-4242-20261107.webp), [seed 7](screenshots/g2-surface-seed-7-20261107.webp), [seed 1](screenshots/g2-surface-seed-1-20261107.webp). The user sent screenshots without a review. The observations below are from the screenshots alone and are not a user assessment. The visual criterion is **not passed**.

Good: irregular sand that intrudes in places; trees stay away from water; river sources at rock (4242, 1); smooth grass/dirt transitions and rock borders without visible gaps.

Needs improvement:

- **Seed 7:** a small sand square (about 2×2 tiles) in the middle of a large lake (a one-cell land island surrounded by lake gets a shore ring); small ponds (seed 7 and 1, left side) have a sand ring larger than the water itself.
- **Seed 1:** a river turns at a right angle and circles a green island by rock; width changes in block steps (2 → 3 → 4), looking rectangular rather than like a channel.
- The river on seed 1 leaves the right edge of the view (the view does not include the whole area, so its start was not assessed).

## Fixes after screenshots of seeds 4242, 7 and 1 (2026-11-07)

- **Islands in lakes:** a land patch fully surrounded by a lake (not touching the area edge) and smaller than 24 cells becomes lake (islandCells). Test: every land patch in a lake has at least 24 cells or touches the edge.
- **Lake-dependent shore rings:** ring r ≥ 1 requires a lake with at least 30·3^(r−1) cells (30, 90, 270), so small ponds get a narrow beach (lake size propagates through rings and saturates at 65535).
- **Smoother rivers:** the route is smoothed with a moving average (up to four cells per side, fixed-point 1/16-cell arithmetic, no smoothing at ends or merge cells, so a tributary touches the river), and a square with half-side 1–2 cells is drawn around each point, growing smoothly with river size (traveled cells plus 15 for each incoming route, full range at 60). Turns are no longer right angles and width grows gradually. A river may cross rock, otherwise it would split into two parts. Diagonal sections are wider horizontally than straight sections due to the square stamp.
- **Bugs along the way:** the first version broke the tributary connection to the river (test “River ends inside the land”), fixed by pinning merge cells and allowing rock crossing.
- Seed 1729 100×100 fingerprint: 6783381837833077004. Core test PASS (206 allocation points with injected failure, all reported; 24+6+3 seeds); --surface (with SURFACE_NEXT_TO_TERRAIN_PASS) and --commit PASS (g2g-surface-182831, g2g-commit-182831), pairs_without_border=0, G1 commit rows unchanged. An ASCII screenshot of one seed also reviewed: diagonal rivers without right angles (temporary code, removed).
- **Not verified:** appearance in RME after this change (screenshots above predate it); rme.exe was not rebuilt in this step because a running RME locked the file; working-memory estimate (40 B/cell) was not recalculated after adding vectors (lake sizes, routes, merges); river-width distribution and smoothness are not measured by a test.

## GUI retest after fixes (2026-11-07, rme.exe rebuilt)

User: “7 looks OK, 1 like on the screen”; no report for seed 4242. Screenshot of seed 1: [seed 1 after fixes](screenshots/g2-surface-seed-1-after-fixes-20261107.webp) (zoom 37%). Seed 7 is a verbal assessment without screenshot.

Assistant observations from seed-1 screenshot: no island in the lake, small ponds on the left have a narrow sand ring, river turns are rounded and width grows toward the mouth (about 2 → 4 tiles), and the tributary touches the river. Long straight horizontal and vertical sections remain (for example, horizontal section by rock in the upper right and a vertical section in the center). The river is less angular than before but still mainly follows axes. It is unknown whether the user considers this acceptable (“like on the screen” can mean either “good” or “unchanged”).

Visual criterion status: seed 7 OK (user assessment), no unambiguous assessment for seed 1, seed 4242 after fixes not assessed.

## Probe results (profile g0-reference-profile-20261030, private runs, hidden window)

| Probe | Run | Result |
| --- | --- | --- |
| core | test_procedural_surface | PASS; 151 allocation points with injected failure (all reported), 7 for decoration |
| --surface | g2d-surface-153056 | all PASS markers (SURFACE_RESOLVER_SIZE_PASS, SURFACE_E2E_PASS, SURFACE_VEGETATION_E2E_PASS, SURFACE_RESOLVER_PASS); runner recorded passed: false because of stderr (D1), exit code 0 |
| --preview | g2d-preview-153056 | passed: true |
| --commit | g2d-commit-153056 | passed: true; TERRAIN_COMMIT_SIZE_PASS rows 100×100 (8881480 / 3965894 / 80195 / 6608263949644166501) and 500×500 (218763816 / 98785798 / 1987447 / 173161415496114986) identical to [G1_COMMIT.md](G1_COMMIT.md) |

Border statistics: halo_tiles_modified = 95 (100×100, seed 1) and 50 (250×250, seed 1729), 0 in the other runs.

## Not verified

- Visual review of several seeds in RME (user step below).
- Dialog and appearance after the latest river change (rivers have not yet been viewed after adding the south-eastern cell).
- Clients other than 1098-BlackTek.
- D5, effects of trees and stones on walkability, and performance measurements beyond one-off core-test numbers (generation_ms is not a before/after comparison).
- Exact lower shore-width bound (the test checks only the upper bound; shore may be narrower where land is rock or river).

## GUI test to perform (several seeds)

For seeds 1, 7 and 4242, size 250×250, Natural surface, vegetation enabled: preview and generate, then assess rivers (no isolated single tile), lake shores, grass/dirt transitions, rock and borders. Save each seed screenshot to docs/procedural/screenshots/, then this document can be updated to PASS for the visual criterion.
