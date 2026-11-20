# G4-D — contextual vegetation rules (doodad classification by brush name), 2026-11-08

**PARTIAL, vegetation evidence is thin.** The mechanism (name classification, contextual statistics, decoration tuning, closed fitting) works and is repeatable. On validation, vegetation fitting improves distance in 2 of 4 folds with comparable context and worsens it in 2. Nothing was calculated on the test set.

## Included

- doodad_class: whole-word brush-name rules → Tree / Bush / Stone / None (None means neither vegetation nor generator stone: furniture, fire fields, statues, rubble). Among 247 style brushes: Tree 13 brushes (20 032 objects at z≤7), Bush 19 (148 919), Stone 13 (19 835), None 202 (44 291). Review list: local-data/procedural-smoke/g4-keymap/d/doodad-review.md.
- Three new semantic-map feature bits (TreeDoodad, BushDoodad, StoneDoodad, beyond the reported 16). G3 reports are unchanged (they count only 16 features): 190 files for 19 maps are byte-identical to the original (0 differences) on the final binary.
- decoration: DecorTuning (six chances in ‰: tree scale, trees near water, bush, bush near water, stone on dirt, stone near rock). Default DecorTuning gives the exact previous result (test on three seeds); out-of-range values are rejected (InvalidParameters). Forest-cluster patterns (noise) and two-step range are unchanged.
- decor_context: counts of grass and dirt tiles (real: from semantic maps through the accepted mapping; generator: from DecorGrid) in eight contexts (grass/dirt × near water or shore × near rock, two-step four-neighbor range, same definition on both sides). Target from multiple maps (each family equal, each map within a family equal, maps below 200 tiles in a context excluded), distance (mean difference of 24 rates in ‰ across comparable contexts), closed fit (each chance = reference rate in its context; tree scale = reference rate / built-in-rule rate on the same surface).
- Profile g4-profile/3 adds decor, target and tuning. style_fit fits vegetation from training maps, while validation maps only measure it.
- Tests: test_procedural_decor PASS (including fit approaching at least 4× closer to a target generated from a known configuration), test_procedural_style_profile PASS, test_procedural_surface PASS unchanged. Two style_fit series are byte-identical (35 s each).

## Vegetation data in references (surface, style maps)

In practice decoration exists only in **M06, M19 and M05** (grass). M12 and M14–M18 together have fewer than 400 grass/dirt tiles. In total (‰ of tiles): grass far from water and rock: trees 22, bushes 154, stones 4 (626k tiles); grass near water: trees 11, bushes 78 (89k); dirt far from rock: stones 18 (119k); dirt near rock: stones 30 (5k). Built-in generator rules: bushes 40‰ (60‰ near water), stones on dirt 30‰ (80‰ near rock), trees 0 near water. The “trees near water = 0” rule is therefore weakly, but not completely, true in references (11‰ on grass).

## Validation fit (distance = mean difference of rates in ‰, summed over comparable contexts)

| Fold (test) | Validation | Terms | Built-in → fitted |
| --- | --- | ---: | ---: |
| M01 | M05 | 6 | 176 → 414 (worse) |
| M05 | M06 | 21 | 550 → 1 008 (worse) |
| M17 | M19 | 24 | 1 839 → 1 559 (better) |
| winter | caves (M14, M16) | 3 | 19 → 3 (better) |
| other 4 folds | – | 0 | no comparable context |

Fitted tuning for all style maps: tree scale 437, trees near water 11‰, bushes 172‰, bushes near water 107‰, stones on dirt 13‰, near rock 29‰.

## Not done / not verified

- **Name classification is a heuristic.** Accepted examples include bushes such as mushrooms, tall grass, flowers, bamboo plant and lilypads; stones include kamienie and staglamites; leaves, debris, ramp and sand pile are None.
- Routes (needed for a “route” rule) and biome transitions were not extracted (D9).
- Water/rock range is fixed at two steps; other values were not studied.
- Closed fitting omits second-order dependencies (for example, a bush sampled after a tree), so generated rates are slightly lower than requested.
- Appearance impact was not visually reviewed. Tuning is not connected to GUI/GenerationConfig (changing that structure would change preview fingerprints).
- The small number of maps with vegetation (3) does not support a meaningful uncertainty interval.
