# G3 — reference-map import and analysis gate status, 2026-11-07

**CLOSED by user decision on 2026-11-07 (PASS with debt, not an unqualified PASS).** The gate criterion is satisfied for 20 maps (19 ordinary 10.98 and one BlackTek), but the planned scope is not complete. The gaps below were accepted as debt (D9–D11 in TECH_DEBT.md, with remaining D8).

**Planned gate:** “A repeatable report, statistics and semantic map are generated for every reference map.”

## Criteria and evidence

| Criterion | Evidence | Status |
| --- | --- | --- |
| Repeatable report | Two complete series of 19 maps: 190 files (3 JSON reports + 7 analysis files per map) byte-identical, 0 differences (G3_REFERENCES.md); forgotten.otbm identical before and after changes (G3_FEATURES.md) | PASS (one machine, one EXE, no other-locale test) |
| Statistics | Matches the independent Map → Statistics window in RME: tiles, items and non-walkable tiles identical for forgotten.otbm (G3_REFERENCES.md#independent-rme-gui-editor-statistics-check). Import, unknown with coverage, per-floor features, regions and components, ground adjacency, 16/32/64 blocks, features versus distance from non-walkable ground; JSON, MD and CSV (G3_NORMALIZER.md, G3_TOPOLOGY.md, G3_FEATURES.md) | PASS within the described scope |
| Semantic map | SemanticMap in memory and a written artifact: deterministic PNG per floor plus legend.json (G3_SEMANTIC_MAP.md). Code-independent verifier: 20 maps × all floors, every check PASS; two series of 105 files byte-identical | PASS |
| Local OTBM import and item/asset compatibility | G3_IMPORT.md; separate “10.98” and “1098-BlackTek” clients, with runner and probe refusing the wrong client | PASS |
| unknown without guessing | Fixture with missing IDs (8/8) and real unknown values on M01 and M06, explained by 14 IDs outside items.otb and 15 deprecated IDs (G3_REFERENCES.md) | PASS |
| Read-only import, private maps | Map and Tibia.dat/spr hashes before and after every run; reports only in git-ignored local-data/; repository contains aggregates | PASS |
| Memory limits, cancellation from the start | D8 closed with residual debt: limit enforced during loading too (hook in IOMapOTBM), cancellation during loading 78–310 ms; Map loading is 94% of the peak; editor path checked manually by the user | PASS with debt |

## Gaps accepted as debt

- **Features from plan section 4.2:** missing rivers (length, tortuosity, width, branching), caves (chambers, corridors, cycles, bottlenecks), forest (clusters), mountains (walls to plateau), routes and bridges, settlements and buildings, microstructures, bottlenecks and connections between floors. Ground regions are not named biomes because names would be guesses.
- **Similarity/memorization test** (plan 4.3) and train/validation/test split (4.4): not done, input for G4.
- **Reasonableness review:** features and semantic maps are measured. Apart from one viewed image (floor 7 of forgotten.otbm), they were not assessed visually or substantively.
- **HTML** report format: absent (JSON, MD and CSV were selected).
- **Companion files** (house, spawn, zones) are not loaded. Items in containers contribute no features.
- **Items.otb:** it was not confirmed that data/1098/items.otb is exactly the file used to create the ordinary 10.98 maps (matching suggests it, but does not prove it).
- **Technical debt from this stage:** D8 (remaining: Map loading 94% of peak, cancellation during normalization about 1.2 s), see TECH_DEBT.md. Repeatability was checked on one machine.
- **Commits and PR:** G3 changes are in commits on feature/procedural-g3-reference-analysis. CI is disabled, and intermediate commits were not built separately.

## Required GUI test (user)

G3 does not change the RME interface, so this stage has no GUI test. The user should review the PNG files in local-data/…/run/analysis/semantic/ (floors) together with legend.json. They can be opened in any image viewer.
