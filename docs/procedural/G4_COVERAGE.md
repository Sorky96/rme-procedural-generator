# G4 — ground coverage (D13), 2026-11-08

**PARTIAL.** The cause of D13 was found and removed with a normalizer option. Coverage increased from 5 to 9 useful surface maps. The user **accepted the new assignments (96 item … keys) on 2026-11-08** (local file g4-keymap/accepted-10.98-v2.tsv, SHA-256 406f21ab…, identical to the proposal used for the results below). This is acceptance of a name-based heuristic, not verification in client definitions.

## Cause

The semantic-map ground key is the name of the brush that owns a ground item (getGroundBrush()). In 10.98 data, many real grounds do not belong to any brush: earth variants (IDs 8133 and 10164, together 156k tiles, each ID in one map), mountain, sand, gray sand, swamp, lava, flowing water, and “wall” grounds in cave maps (stone wall, dirt wall, sandstone). All received an empty key (“no ground”). Measured: 650 distinct item IDs and 788 876 tiles (of 14 million) across 19 maps, 88% in M06 and M19.

## Fix

- normalizeReference has NormalizeOptions::nameUnbrushedGrounds (disabled by default). A ground without a brush receives key item <name from the client definition>. Nothing is guessed by ID; the name comes from the loaded client. Import summary also includes groundWithoutBrush (ID, count, name, client ID, blocking), and the probe writes ground-without-brush.csv (aggregates, no coordinates).
- The word wall → Other rule in defaultKeyRules comes before dirt/earth because “dirt wall” is a cave wall, not dirt. Test extended.
- **No G3 regression:** with the option disabled, 190 report files for 19 maps are byte-identical to original G3 (0 differences) on the final binary. With the option enabled, two runs (with and without mapping) produced identical analysis files and fingerprints.

## Result

- NoGround tiles on style maps: 1 149 443 (116‰) → 365 646 (37‰). Keys: 108 → 204 (96 new item … keys, 108 old keys unchanged in kind). New Unmapped: 33 keys (including item basalt, item rock soil, item small brook, item wasteland, item sandy tile, item stone tile), together 5‰ of tiles.
- Maps with at least 1000 natural tiles at z≤7: M01 (15 153, water only), M05, M06, M11, M12 (3 436), M13 (6 808), M14, M15 (6 297), M19 = **9** (was 5). M16 (83), M17 (0 natural: 8 774 Other and 7 305 Unmapped, including a city underwater), and M18 (693) remain below the threshold.
- **Split changed** (fingerprints count ground names, so windows with empty keys stopped joining maps): 0 content merges (was 4), 9 families and 9 folds, test–rest leakage ≤ 11‰ (was ≤45‰). The earlier G4-A result (a seven-map family) was largely an empty-key artifact and remains in history as the first run.
- **Fit** (same grids, seeds 1–6, side 160; L1 distance to validation target, ppm): validation improved in 6 of 8 folds with a target (−4% to −78%), and manual baseline wins in 2 folds (M01 fold: 1 648 968 vs 3 275 353; winter fold: 3 002 414 vs 3 876 681). Earlier improvement was 3–7% in 2 of 4 folds. Two style_fit runs were byte-identical. Test fold M15 has no validation target (M17 validation has 0 natural tiles).

| Fold (test) | Validation: baseline → selected step |
| --- | ---: |
| caves (M14, M16) | 3 710 733 → 2 815 964 |
| M01 | 1 648 968 → 1 648 968 (baseline) |
| M05 | 2 186 449 → 2 092 742 |
| M06 | 3 582 804 → 2 666 482 |
| M15 | no validation target |
| M17 | 2 399 716 → 525 138 |
| M19 | 3 473 698 → 2 828 419 |
| swamp (M12, M13) | 3 407 994 → 2 377 098 |
| winter (M11, M18) | 3 002 414 → 3 002 414 (baseline) |

- Fitted values still reach grid edges (lake 300‰, rock 320‰). This fits shares, not appearance.

## Accepted 2026-11-08

The 96 item … keys in local-data/procedural-smoke/g4-keymap/proposed-10.98-v2.tsv were reviewed (…/c/keymap-review.md). As in G4-B, natural-kind tiles are Grass 790 733, Dirt 4 566 080, Rock 728 390, Shore 213 570, Water 1 666 777, Other 1 489 624 and Unmapped 49 920. Decisions: item gray sand and item sand = Shore (like sand), item earth = Dirt, item mud = Dirt, item flowing water = Water, item swamp/item lava = Other, item rock soil = Unmapped.

## Not verified

- That an item name from client definitions identifies its ground kind (for example, that every mountain is rock). Rules remain a word heuristic.
- That data/10.98 contains every brush used by these maps. A missing brush may have been the author's deliberate choice.
- Nothing changed in the editor path. The option works only in reference import. There is no GUI test.
- New profiles and the split were calculated on one machine.
