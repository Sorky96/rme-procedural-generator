# G4-B — style-profile schema and ground-key → kind mapping, 2026-11-07

**PARTIAL.** Schema, validation, serialization, rejection of incompatible client/assets/level and statistic projection through mapping are built and tested. **The 10.98 mapping proposal awaits your approval** (list below). Until approved it is not data on which the generator may be fitted. Profiles are not yet populated from references (that is G4-C).

## Included

- source/procedural/core/style_profile.*:
  - StyleKind: Grass, Dirt, Rock, Shore, Water, Other (valid ground without a generator kind: floors, roofs, stairs, hazards and biome grounds), Unmapped (no rule matches, reported without guessing), NoGround.
  - proposeKeyMap with whole-word brush-name rules (no IDs, case-insensitive, priority order: qualifiers roof/floor/hole…, then Water, Rock, Dirt, Grass, Shore). Rules are code data (defaultKeyRules), not item IDs.
  - serializeKeyMap/parseKeyMap (g4-keymap/1, manually editable), StyleProfile (g4-profile/1: client, SHA-256 of items.otb and Tibia.dat, surface/underground level with floor range, weighted sources, mapping, integer statistics), validateProfile, parseProfile, checkProfile.
  - accumulateKindStats: tiles per kind and neighboring pairs (east/south) for a selected floor range. A map key absent from the mapping is an error, not Unmapped.
- style_keymap tool (core only): builds keymap.tsv and a Markdown review table from G3 ground_keys.csv reports.
- test_procedural_style_profile PASS: rules on 25 data names (including “dirt mountain ground” = Rock, “sandstone” ≠ “sand”, “grassland” ≠ “grass”, “grass pitfall” = Other, “snow” = Unmapped), full round-trip, unambiguous serialization, refusals (other schema version, missing section, unknown kind/level, extra line, duplicate key/client, tab in key, floor range inconsistent with level, empty fields), compatibility (client, items.otb, Tibia.dat, floor outside level), manually calculated statistics on a 3×2 map plus floor 8 (including tile without ground, missing neighbor, cancellation, reversed range).

## 10.98 mapping proposal (approval required)

108 keys from 19 maps. The table uses 12 style maps and all floors (not only surface). File: local-data/procedural-smoke/g4-keymap/a/keymap-review.md (two byte-identical runs).

| Kind | Keys | Tiles | Share (‰) |
| --- | ---: | ---: | ---: |
| Grass | 7 | 790 325 | 80 |
| Dirt | 8 | 4 398 960 | 445 |
| Rock | 8 | 672 583 | 68 |
| Shore | 5 | 176 750 | 17 |
| Water | 3 | 1 649 192 | 167 |
| Other | 71 | 1 009 754 | 102 |
| Unmapped | 5 | 23 733 | 2 |
| NoGround | 1 | 1 149 443 | 116 |

**Decisions to confirm (assumptions, not client facts):**

1. sand (171k tiles, mainly two maps) = **Shore**. In desert maps it is the main ground, not a strip beside water.
2. sea, clear water, walkable sea = **Water**. The key does not say whether it is lake or river; Lake/River split requires shape features (D9) in G4-C.
3. cave, swamp, lava, tar pit = **Other** (no generator kind). earth and dirt = **Dirt**: earth has 4.2 million tiles in three maps, mainly M06, making Dirt 44.5% of tiles. Per-map weights in G4-C address this.
4. **Unmapped** (23.7k tiles, 2‰): snow, ice, white gravel, melting ground, ice closed stone pile. Should snow/ice be a winter-biome kind or remain outside the model?
5. **NoGround = 11.6% of tiles** (tile without ground from a brush): what these are (for example, groundless items or walls) was not investigated and should be reviewed before use.

## Not verified

- Rules are a name heuristic. Nothing was checked in item definitions or materials (for example, whether earth is the same material as dirt in BlackTek). No BlackTek mapping exists.
- Table statistics use all floors, not surface. The z≤7 profile is calculated from SemanticMap only in G4-C.
- There is no data-filled profile yet, so refusal on a real profile and time/memory measurement of accumulateKindStats on a large map are not tested.
