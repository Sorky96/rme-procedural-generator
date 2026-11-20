# G3-B — semantic normalizer, 2026-11-07

**PARTIAL (G3-B), unknown path verified on a synthetic fixture.** A reference map is converted to a structural semantic representation (ground key plus a per-tile feature mask, per floor). Regions, biomes and topology are G3-C. The G3 gate is not passed.

## Model

- Core source/procedural/core/semantic_map.* (without editor dependencies): SemanticMap stores a dense rectangle per floor (uint16 ground key, uint32 feature mask, about 6 B/cell). create refuses ranges outside floors 0–15, duplicate floors and cell-budget overflow. set refuses cells outside the rectangle. The Present bit is set by set(), so a tile without any other feature still exists.
- 16 features: has_ground, blocking, blocks_missiles, blocks_path, wall, door, table, carpet, border, floor_change, teleport, magic_field, container, doodad, loose_item, unknown.
- Adapter reference_import.* (normalizeReference): features come from loaded-client item definitions (ItemFlag, isWall/isDoor/isTable/isCarpet/isBorder, getDoodadBrush, container, teleport, magic field). **No item ID is hardcoded.** The ground key is the loaded client's ground brush name (getGroundBrush()->getName()); ground without a brush has an empty key. Keys are interned alphabetically, so the table does not depend on tile iteration order. Items without a definition or invalid in OTBM receive the unknown feature and are included in the G3-A unknown_ids report.
- Core test test_procedural_semantic_map: PASS (exact and exceeded budgets, floor order, duplicate/empty/bad range, key order, boundaries, tile without features, summary repeatability).

## Result for forgotten.otbm (1098-BlackTek client)

Source: SHA-256 45031779…2304. Local semantic-report.json (SHA-256 5C9C1024…2B4C) is in local-data/, not the repository.

- 3 028 669 cells in rectangles across 16 floors (11% tile coverage); probe budget 200 million cells.
- Features (tiles): has_ground 329 479, blocking 244 283, blocks_missiles 176 850, blocks_path 1 178, wall 13 572, door 683, table 494, carpet 2 678, border 52 045, floor_change 613, teleport 27, magic_field 152, container 431, doodad 16 916, loose_item 1 238, unknown 0.
- 77 ground keys (including empty: 20 196 tiles, of which 11 116 have no ground); sum of tiles by key 340 595 = tile count.
- The blocking feature separates water and lava without names: sea 45 033/45 034, clear water 1 289/1 289 and lava 3 232/3 232 blocking tiles, while grass is 6 374/24 405 (placed obstacles block tiles).

## Bug found and fixed

The first version treated a tile with no feature as an empty cell. The sum by key (340 502) did not match the tile count (340 595, missing 93). Fix: an explicit Present bit set by set() and a test that a tile without features still exists. The sum matches after the fix.

## Repeatability

Two runs before and two after the fix produce byte-identical semantic-report.json and reference-report.json (0D538B97…77A0, the same as in G3-A). Runs finish unattended, stderr and wx-log.txt are empty, and wall time is 4.6–5.0 s (one complete measurement, no breakdown).

## Not verified

- The unknown path on a real map: forgotten.otbm has zero unknown items, so unknown was checked only on the synthetic fixture below, not on a map with real missing values.
- Items inside containers contribute no features (only the container itself). Companion files (house/spawn/zones) are not loaded.
- Two different brushes with the same name would merge into one key; it was not checked whether this occurs.
- doodad relies on the doodad_brush definition, which the code describes as “not necessarily a doodad brush”; the accuracy of this feature was not manually assessed.
- It was not assessed whether 16 features are sufficient for G3-C/D or whether the 77 ground keys group biomes well; that is G3-C work.
- Memory: the model is about 18 MB calculated from the cell count, but process RSS and Map-loading peak were not measured. Time is one measurement, not a benchmark. stop_token cancellation does not exist (see G3_IMPORT.md).

## Fixture with unknown values (2026-11-07)

g3_make_unknown_fixture.py builds a new OTBM 3, g3-unknown.otbm, from synthetic g0-metadata.otbm (400 grass tiles, floor 7), with IDs 60000–60003 absent from the client: two grounds (60000, 60002), the same unknown item three times on a stack (60001, on three tiles, once with unknown ground), and one tile without ground containing only an unknown item (60003). Expected values are written to g3-unknown-expected.json, and g3_verify_unknown.py compares them with the reports. Fixture files are in local-data/ and not in the repository.

Result (1098-BlackTek client, two runs, byte-identical reports, SHA-256 of semantic-report.json 4EDB6690…CF6A):

- 8/8 verifier checks PASS: 400 tiles on floor 7; unknown_ids exactly 60000×1, 60001×3, 60002×1, 60003×1; unknown_ground 2 + unknown_items 4 = 6; unknown feature on 5 tiles; has_ground 399; sum of tiles by ground key = 400 (three with empty key: two unknown grounds and the tile without ground).
- Negative check: the same verifier on the forgotten report reports seven errors (code 1), so it can detect a mismatch.
- Observation: an unknown ground is counted as ground (unknown_ground), because Item::isGroundTile() includes invalid items “similar to ground”. This is loader behavior, not a normalizer decision.

Evidence limits: IDs 60000+ are unknown only because client 1098 lacks them (the result itself confirms this, but it is not an independent check), and the fixture has one missing-item form (inline in a tile). Missing items nested in containers and ITEM nodes with attributes were not tested.
