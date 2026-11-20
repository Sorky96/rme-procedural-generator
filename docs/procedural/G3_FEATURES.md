# G3-D — feature extraction and aggregate report, 2026-11-07

**PARTIAL (G3-D).** An aggregate report (JSON, MD, CSV) and the features from section 4.2 of the plan are implemented within the scope below. The G3 gate (“repeatable report, statistics and semantic map for every reference map”) is satisfied only for one reference (forgotten.otbm) and a fixture, so it is not passed.

## Included

- Core source/procedural/core/features.* (without the editor, item IDs or names):
  - **Ground adjacency matrix:** tile pairs to the east and south (west and north are transposes), dense, refusal above 512 keys.
  - **16/32/64 blocks** aligned to absolute coordinates, including negative coordinates: decile histograms of occupancy, walkable-field share and non-walkable-ground share.
  - **Per-floor features:** tile count for each of 16 features.
  - **Feature frequency by distance** (4-neighbor, multi-source BFS) from the nearest non-walkable-ground tile: doodad, wall, border, ranges 1, 2, 3–4, 5–8, 9–16, 17+/no source.
  - stop_token cancellation per row and every 4096 queue pops; working memory about 5 B per cell of the largest floor.
- Core source/procedural/core/feature_report.*: renderJson, renderMarkdown, renderCsv (tables: ground_keys, floors, adjacency, scales, distance). Integer values only (shares in per mille), newline characters, no time, addresses or locale. The report is aggregated, with no tile coordinates or raw content. It includes analysis coverage (tiles without unknown content) and the unknown list.
- Adapter makeReportInput; the probe writes seven files to run/analysis/.
- test_procedural_features PASS on a manually calculated 3×2 map (adjacency matrix, scale histograms, feature counts, distance ranges), negative x blocks, no source, refusals (too many keys, incompatible topology, cancellation), render repeatability, CSV quoting and JSON escaping.

## Result for forgotten.otbm (1098-BlackTek client)

Local files are in local-data/ and not in the repository. Two runs produce all seven files byte-identically (including reference-analysis.json, 42 712 B, SHA-256 252029EE…; reference-analysis.md, 3 900 B, 874CEB62…). Runs finish by themselves, stderr and wx-log.txt are empty, and the complete run takes 4.7–4.8 s wall time (report generation was not separated).

- Analysis coverage: 340 595 of 340 595 tiles without unknown content (100.0%). For the fixture with unknown values: 395 of 400 (98.7%), with four different unknown IDs in the report.
- Distance from non-walkable ground (example of a useful feature): the border share falls from 68.2% (distance 1), through 46.1% (2), to 17.1% (9–16); doodads have a 5.8% share immediately next to non-walkable ground and about 12–14% at distances 2–16.
- 16×16 blocks: 2 540 non-empty, of which 33.6% are at least 90% non-walkable ground and 2.5% are at least 90% walkable fields; for 32 and 64, respectively 24.7%/1.0% and 15.9%/0.5%.
- Most common ground transitions: cave–cave (alternate border) 6 541, cave (alternate border)–earth (stone border) 6 307, (no brush)–cave 4 117, grass–sea 2 426.

## Not verified

- Only one reference map and a synthetic fixture. There is no map comparison or evidence that features distinguish styles (that is G4 work), and the planned multi-reference test is missing.
- Section 4.2 features still missing: river length/tortuosity/width/branching, lake and island sizes as separate features (only G3-C ground regions exist), forest density and clusters, mountain wall height and wall-to-plateau ratio, cave chambers/corridors/cycles/bottlenecks, route networks, bridges, settlement and building placement, microstructures (repeated fragments), and the similarity/memorization test.
- There is no HTML report variant. CSV and MD are written as UTF-8 without BOM. Compatibility with a particular spreadsheet application was not checked.
- Distance feature frequency assumes that “non-walkable ground” (at least 90% blocking tiles, G3-C threshold) represents water, lava and rock, without separating them. Distance is calculated through the floor rectangle with four-neighbor connectivity, ignoring obstacles and other floors.
- The adjacency matrix counts only neighbors on the same floor. The “(no brush)” key mixes tiles without ground and ground without a brush.
- Repeatability was checked in one environment (same EXE and client, two runs). Another system locale and loading order were not checked. Renderers use integer values only, so locale should not matter, but this was not tested.
- Memory: 5 B/cell is a code estimate, process RSS was not measured. Time is one complete measurement, not a benchmark. Cancellation was tested in the core test only as a request before work started.
