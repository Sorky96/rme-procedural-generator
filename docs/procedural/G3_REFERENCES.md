# G3-E — repeatability on the reference set, 2026-11-07

**PARTIAL (G3-E).** The complete G3-A…D pipeline was run on 19 maps from the ordinary 10.98 client and earlier on one BlackTek map. Reports are repeatable (two complete byte-identical series). Memory was measured, and a limit and cancellation were added for the largest map (near the 2 GB budget), including during loading (D8, with residual debt). The G3 gate is satisfied for these maps within the described scope, but G3 is not closed. See “Not verified”.

## Two different clients (important)

“10.98” (ordinary) and “1098-BlackTek” are **different data sets**: ordinary 10.98 Tibia.dat has 23 725 items, BlackTek has 26 381 (header signatures are identical, 42A3/57BBD603, so they must not be distinguished by signature). Ordinary 10.98 maps were analyzed only with the Tibia.dat/spr from their folder and the “10.98” dat_otb entry. forgotten.otbm was analyzed only with BlackTek. Assets were never mixed.

An error in my first approach (reported by the user): the G0 profile had one BlackTek entry in clients.toml, so the “10.98” entry from config.toml had no match and RME startup fell back to the first client (BlackTek) and tried to load it. Fix: the ordinary 10.98 profile is the repository data/ directory (it has the “10.98” entry). The runner refuses when the requested client is absent from clients.toml, and the probe refuses when the loaded client differs from the requested one (CLIENT_MISMATCH).

## Set (10.98 client, 19 maps)

Maps are private. Aliases below map to names only locally in local-data/. All are OTBM 3 with items 3.57, runs finish unattended, and stderr and wx-log.txt are empty (38 runs).

| id | file MiB | header dimensions | tiles | items | unknown ground/items | distinct unknown IDs | coverage | floors | ground keys | peak memory MiB |
|---|---|---|---|---|---|---|---|---|---|---|
| M01 | 0.44 | 256×256 | 38 588 | 60 435 | 43 / 47 | 14 | 99.8% | 11 | 6 | 244 |
| M02 | 0.01 | 2048² | 393 | 554 | 0 / 0 | 0 | 100% | 4 | 1 | 236 |
| M03 | 0.01 | 2048² | 503 | 1 225 | 0 / 0 | 0 | 100% | 1 | 8 | 236 |
| M04 | 0.03 | 2048² | 2 218 | 4 278 | 0 / 0 | 0 | 100% | 3 | 12 | 236 |
| M05 | 0.22 | 2048² | 24 832 | 29 815 | 0 / 0 | 0 | 100% | 5 | 27 | 242 |
| M06 | 71.08 | 2000² | 7 835 705 | 8 520 654 | 0 / 48 | 15 | 100% | 16 | 74 | **1 899** |
| M07 | 5.97 | 5000² | 760 126 | 792 422 | 0 / 0 | 0 | 100% | 1 | 2 | 451 |
| M08 | 10.27 | 5000² | 1 322 539 | 1 354 835 | 0 / 0 | 0 | 100% | 1 | 2 | 555 |
| M09 | 5.97 | 5000² | 760 126 | 792 422 | 0 / 0 | 0 | 100% | 1 | 2 | 452 |
| M10 | 10.12 | 5000² | 1 322 539 | 1 322 539 | 0 / 0 | 0 | 100% | 1 | 2 | 553 |
| M11 | 0.05 | 2048² | 4 388 | 6 930 | 0 / 0 | 0 | 100% | 4 | 10 | 238 |
| M12 | 0.21 | 2048² | 23 729 | 30 041 | 0 / 0 | 0 | 100% | 2 | 3 | 241 |
| M13 | 0.13 | 2048² | 14 343 | 17 323 | 0 / 0 | 0 | 100% | 2 | 1 | 240 |
| M14 | 0.06 | 2048² | 5 245 | 8 077 | 0 / 0 | 0 | 100% | 4 | 8 | 238 |
| M15 | 0.07 | 2048² | 6 718 | 9 329 | 0 / 0 | 0 | 100% | 2 | 4 | 238 |
| M16 | 0.07 | 2048² | 7 422 | 9 268 | 0 / 0 | 0 | 100% | 4 | 6 | 237 |
| M17 | 0.21 | 2048² | 19 005 | 29 667 | 0 / 0 | 0 | 100% | 5 | 2 | 241 |
| M18 | 0.02 | 2048² | 1 907 | 3 393 | 0 / 0 | 0 | 100% | 3 | 8 | 237 |
| M19 | 19.35 | 2048² | 1 888 858 | 2 630 326 | 0 / 0 | 0 | 100% | 16 | 104 | 717 |

Peak memory is the maximum PeakWorkingSetSize of the complete process (including client, assets and GUI, about 235 MiB for startup alone), read every 50 ms. Series B values were within ±1 MiB.

## What this proves

- **Repeatability:** two complete series, 190 files (3 JSON reports and 7 analysis files per map) byte-identical, 0 differences. Source maps and Tibia.dat/spr have identical hashes before and after every run (the runner stops on a difference).
- **Unknown path on a real map:** M01 has 43 unknown grounds and 47 unknown items (14 distinct IDs), M06 has 48 unknown items (15 IDs). Reports include IDs and counts, and M01 analysis coverage is 99.8%. This closes the gap in G3_NORMALIZER.md (“unknown only on fixture”). Causes are explained in “Why these IDs are unknown”.
- **Scale:** 7.8 million tiles and 16 floors (M06) in 7.1 s wall time for the complete run.

## Why these IDs are unknown

Every one of the 29 unknown IDs was checked in data/1098/items.otb with tools/tibia_data.py. All 14 IDs from M01 (30 845–41 132) **do not exist in items.otb** (they are newer than this client, whose OTB has 26 282 entries), and all 15 IDs from M06 (4 422–4 437) have a “deprecated” group in items.otb (entries without a client ID). Therefore unknown comes from map data, not an importer or client mismatch. The report includes the IDs and counts, and the normalizer does not guess their class.

<a id="limit-pamięci-i-anulowanie-d8"></a>
## Memory limit and cancellation (D8)

D8 (no import memory limit or cancellation) was closed on 2026-11-07 in two steps. The final state and before → after comparison follow (10.98 client, M06 = 71 MiB map, 7.8 million tiles, one run per measurement, same EXE and machine).

**Process memory limit** (ReferenceImportLimits.maxProcessBytes, 2 048 MiB by default in the runner) works in three places:

1. **Before loading:** current working set plus the lower estimate estimateLoadBytes (uncompressed file size × 16; for gzip, the ISIZE trailer size). It refuses only maps that cannot fit even at the lowest observed density (23.4–36.1 B per byte for files ≥ 5 MiB, never below 18). The first version used an upper bound of 40 B/B and rejected M06 at 2 GiB even though it fits.
2. **During loading (new):** IOMapOTBM has an optional abort predicate (setAbortCheck) checked by the calling thread every 5 ms; worker threads stop at the next tile. The predicate returns true when stop_token is requested or working set exceeds the limit. The limit therefore also applies during loading.
3. **Before building the semantic model:** measured working set plus analysisWorkingBytes (6 B/cell + 16 B/cell for the largest floor).

| case | before (upper estimate, no interruption) | after (lower estimate + loading interruption) |
|---|---|---|
| M06, limit 2 048 MiB | refusal before loading | **PASS**, peak 1 900 MiB (loading 614 ms, normalization 1 303 ms) |
| M06, limit 1 500 MiB | (not applicable: estimate rejected it earlier) | interrupted during loading: memory_budget_exceeded, peak **1 503 MiB**, loading 503 ms |
| M06, limit 1 000 MiB | refusal before loading | refusal before loading (lower estimate 1 137 MiB + 234 MiB) |
| M06, estimator reduced to 1 B/B, limit 1 200 MiB (loading-control test only) | (with 1 500 MiB limit) peak **1 793 MiB**, excess about 293 MiB | interrupted during loading, peak **1 221 MiB**, excess 21 MiB |

**Cancellation** (stop_token) is checked before and after hashing, during loading (every 5 ms by the predicate), after loading, every 4 096 tiles during normalization, and every row in topology, features and images. Delay from request to return from normalizeReference on M06 (one run per point):

| request after | stage | before | after |
|---|---|---|---|
| 0–150 ms | before loading (hashing) | 154–304 ms | 150–300 ms |
| 300–600 ms | **during loading** | 1 433–1 713 ms | **78–310 ms** |
| 700 ms | loading (end) | 1 433 ms | 650 ms |
| 1 000–2 200 ms | normalization | 1 196–1 217 ms | 1 164–1 207 ms |
| 3 000 ms | topology | 428 ms | 557 ms |
| 4 500 ms | topology | 5 ms | 5 ms |

In import, delay includes destroying the loaded map on return (about 1.2 s for a fully loaded 7.8-million-tile map). An interruption during loading destroys only a partial map, hence the lower values. About 1.2 s remains during normalization.

**Regression:** after the changes, the full 19-map set with the default 2 048 MiB limit (M06 also passes) produced 295 files (190 analysis and 105 semantic files) byte-identical to the series before the loader changes. The PNG verifier passed 100% of checks, forgotten.otbm (BlackTek) was unchanged, and stderr and wx-log.txt were empty. **Editor path (without the hook):** the same loading without a predicate produced the same tile count as hooked import on five maps (M19 1 888 858, 5000×5000 water map 1 322 539, maps 19 005 and 38 588, forgotten.otbm 340 595).

**Residual debt (D8 in TECH_DEBT.md):** (1) loading alone is still about 94% of the peak (1 793 of 1 900 MiB for M06), so a map much larger than 71 MiB will not fit in 2 GiB; changing this requires a different loading method than Map (for example, a streaming importer directly into the semantic model), which was not implemented; (2) cancellation delay during normalization is about 1.2 s (destroying the loaded map); (3) the lower estimate means a sparse, large map can load up to the limit before interruption (time loss, not memory: excess 3–21 MiB); (4) calibration uses 19 maps from one client, memory measured only on Windows and single runs; (5) the source/io change also affects the editor (path without hook), verified only by equal tile counts, not by a manual GUI test.

<a id="kontrola-niezależna-statystyki-rme-gui-ścieżka-edytora"></a>
## Independent check: RME statistics (GUI, editor path)

The user opened forgotten.otbm in ordinary RME (after the loader change, without the abort hook) and supplied the **Map → Statistics** window (2026-11-07). Comparison with this pipeline's reports (BlackTek client):

| quantity | RME (editor) | G3 pipeline |
|---|---|---|
| tiles | 340 595 | 340 595 |
| items | 429 037 | 429 037 |
| non-walkable tiles (RME) / tiles with blocking feature | 244 283 | 244 283 |
| walkable tiles (RME) / walkable fields by G3 | 96 312 | 95 103 |
| movable items (RME counts items) / tiles with loose_item | 1 361 | 1 238 |
| non-empty containers (RME) / tiles with container (including empty) | 5 | 431 |

The first three values are identical, independently confirming import, item counting and the blocking feature, and that editor loading works after the loader change. Differences in the remaining rows come from different definitions, not an error: RME considers every unblocked tile walkable, while G3 also requires ground and no unknown content (96 312 − 95 103 = 1 209 tiles without ground and without blocking; these were not checked individually). RME counts movable items, G3 counts tiles containing such an item. RME counts only containers with content, G3 every container. Remaining entries (39 085 detailed tiles, 42 depots, Action/Unique ID items, houses and towns) were not compared because the pipeline does not count them.

## Not verified

- Whether classifications are sensible for these maps (for example, whether features distinguish styles) was not assessed, and they were not viewed in RME. Features and reports are measured, not evaluated.
- It was not checked whether data/1098/items.otb is exactly the items.otb used to create these maps. Compatibility (OTBM 3, items 3.57, 0 unknown on 17 of 19 maps) suggests it, but is not proof. Unknown IDs in M01 and M06 are explained separately above, which does not rule out an items.otb mismatch on other maps.
- Results from both series are from one machine and one EXE. Another system locale was not tested.
- Times are one complete set of measurements (two series, the same result within 0.1 s wall), not a before/after benchmark. Individual stages were not separated.
- Import cancellation is interruptible during loading (loader hook), and delay was measured. About 1.2 s during normalization remains (map destruction), as described above.
- Features from section 4.2 of the plan are still missing (rivers, caves, routes, settlements, microstructures, similarity test). No train/validation/test split or inter-map similarity evaluation was performed (G4 scope).
- Companion files (house, spawn, zones) are not loaded.
