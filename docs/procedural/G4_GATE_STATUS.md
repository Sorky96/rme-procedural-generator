# G4 — gate status “Learning L1/L2 style”, 2026-11-08

**G4 NOT MET in measured scope.** G4-F evaluation run once, on criteria from [G4_METRICS.md](G4_METRICS.md) written and committed before execution (evaluator `f251ba7`). Two runs produced bit-identical reports (8 s each, one machine). No criterion or threshold was changed after seeing the result.

**Gate from plan:** “measurable and repeatable improvement in selected style metrics over manual presets, without degradation of critical playability metrics.”

## Result (client 10.98, surface z ≤ 7, side 256, seeds 1001–1032, baseline/candidate pairs)
Difference = candidate − baseline, smaller = better. S1/S2 in ppm (max 2,000,000 each), S3 in thousandths ‰, P1 in ppm. “Better” = median < 0 and candidate wins in ≥ 24 of 32 seeds.

| Fold (test) | Maps | S1 shares | S2 adjacency | S3 vegetation | P1 median: baseline → candidate |
| --- | ---: | --- | --- | --- | --- |
| caves (M14, M16) | 2 | worse (+440 140) | worse (+425 428) | better (−4 833, 3 words) | 999 730 → 889 678 (89 %) |
| M01 | 1 | undecided (0) | undecided (0) | N/A | 999 730 → 999 730 |
| M05 | 1 | worse (+1 022 156) | worse (+1 011 538) | worse (+85 916) | 999 730 → 946 054 |
| M06 | 1 | better (−274 079) | better (−276 892) | better (−2 071, 24/32) | 999 730 → 933 535 |
| M15 | 1 | N/A: no profile (M17 validation no data) | | | |
| M17 | 1 | N/A: no test target (0 natural tiles) | | | |
| M19 | 1 | better (−817 977) | better (−788 629) | better (−8 396) | 999 730 → 947 825 |
| swamp (M12, M13) | 2 | better (−320 283) | better (−325 744) | N/A | 999 730 → **689 523** |
| winter (M11, M18) | 2 | undecided (0) | undecided (0) | N/A | 999 730 → 999 730 |

M01 and winter: validation step chosen is baseline, so candidate is the same generator (difference exactly 0).

## Gate criteria (registered ahead) and result
| Criterion | Requirement | Result |
| --- | --- | --- |
| S1 shares | better ≥ 2/3 evaluated folds (7) | 3 of 7: **no** |
| S2 adjacency | better ≥ 2/3 | 3 of 7: **no** |
| No fold worse in both | 0 | 2 (caves, M05): **no** |
| S3 vegetation | more better than worse (folds with context) | 3 better, 1 worse (4 folds): yes |
| P1 walkability coherence | in each fold candidate ≥ 95 % of baseline median | 5 of 7 folds below 95 %: **no** |

**Result: not met** (S1, S2, no fold worse in both, P1 failed).

## What follows from this
- Fitting improves style on 3 of 7 evaluated folds (M06, M19, swamp) and worsens on 2 (caves, M05), and changes nothing on 2. Improvements are large where the test target resembles the training mix (M19: −818K ppm of 2M possible).
- **Critical playability metric is worse.** Fitted parameters (lake to 300 ‰, rock to 320 ‰, up to 8 rivers) cut land: median of largest coherent walkable component drops from 99.97 % to 68.9 % (swamp), 88.9 % (caves), 93–95 % in others. Improvement in share distance is thus bought at the cost of land coherence. This is a flaw of the objective function itself (see below), not a tool error.
- Objective function (L1 distance of shares and adjacencies) contains no land coherence or any playability constraint, so nothing prevents fitting from reaching extreme grid limits.

## Caveats to the result
- **Data:** 7 evaluated folds are one to two maps each; three folds with vegetation are 3 maps. Ranges are scatter across 32 generator seeds, not uncertainty in map selection.
- **Leak:** test family of fold k was validation family of fold k−1 (G4_METRICS.md); result is somewhat less independent than the name “test” suggests.
- **Interpretation of metric recording:** G4_METRICS.md mentions “sum across seeds” in S1/S2 description and pairs per seed in aggregation; evaluator counts distances per seed (necessary for pairs). Not counting cumulative variant.
- **S3** was computed on folds where test target has context ≥ 200 tiles (24 of 3–24 words).
- **Not measured:** visual similarity, naturalness of rivers and mountains, clustering at 16/32/64, human judgments, dungeons, BlackTek, `border_error_rate`, generator output memorization vs. reference (D10), 100 % correct IDs and zero changes outside mask (those depend on commit path, not tuning, but not checked in GUI).
- **Profile is not wired to GUI** (surface and vegetation tuning lack fields in `GenerationConfig`/dialog), so no manual test in RME yet; as long as nothing there to choose, it would be pointless.

## What remains (proposals, not executed)
1. Add land coherence (P1) as a hard constraint or component of fitting objective function and repeat G4-F. Any such change is a new iteration, not a fix of this result: record new criteria before execution and mark result “after criterion change”.
2. Per-style profiles (coast, grass, mountains) instead of one blend, and more maps with identified ground.
3. Narrow parameter grids or add regularization toward manual values.

<a id="iteracja-2--dopasowanie-z-ograniczeniem-spójności-lądu-2026-11-08-po-zmianie-modelu"></a>

## Iteration 2 — fitting with land coherence constraint (2026-11-08, “after model change”)

**G4 still NOT MET.** Criteria recorded before execution ([G4_METRICS.md](G4_METRICS.md), section “Iteration 2”, commit `83607af`); metrics, seeds, folds and gate rule unchanged. Only the model changed: candidate in `SurfaceTuning` fitting is rejected if median land coherence on seeds 1–6 is lower than 97 % of median of manual parameters (`fc41d6b`). Two fittings (36 s) and two evaluations bit-identical.

| Fold (test) | S1 shares | S2 adjacency | S3 vegetation | P1 median: baseline → candidate |
| --- | --- | --- | --- | --- |
| caves | worse (+354 676) | worse (+347 019) | better (−5 000) | 999 730 → 958 154 |
| M01, winter | unchanged (candidate = manual) | | N/A | unchanged |
| M05 | worse (+836 959) | worse (+832 825) | worse (+85 167) | 999 730 → 958 154 |
| M06 | better (−410 079) | better (−342 307) | **worse (+2 523)** | 999 730 → 958 154 |
| M19 | better (−815 337) | better (−781 036) | better (−8 312) | 999 730 → 966 000 |
| swamp | better (−320 068) | better (−312 398) | N/A | 999 730 → 958 154 |
| M15, M17 | N/A (as in iteration 1) | | | |

| Criterion | Iteration 1 | Iteration 2 |
| --- | --- | --- |
| S1 better ≥ 2/3 of folds (7) | 3 of 7: no | 3 of 7: no |
| S2 better ≥ 2/3 | 3 of 7: no | 3 of 7: no |
| Fold worse in both = 0 | 2: no | 2: no |
| S3 more better than worse | 3 : 1: yes | 2 : 2: **no** |
| P1 each fold ≥ 95 % baseline | 5 folds below: no | **0 below: yes** (958 154–999 730 at threshold 949 744) |
| **Gate** | not met | **not met** (S1, S2, no fold worse in both, S3 fail) |

**Conclusion:** constraint fixed critical playability (land coherence ≥ 95.8 % of manual in all folds, earlier down to 69 %), not destroying improvement in the three folds where test target resembles training mix (M06, M19, swamp: still −0.3 to −0.8M ppm). Did not improve two folds where style does not improve: caves (target is ground only) and M05 (target is almost all grass), where generator fitted to the mix is farther from target than manual parameters; in M06 vegetation changed from better to worse (+2 523 at scatter −3 286…+6 381, nearly zero). Plain style blend does not describe individual family, confirming direction “per-style profiles”.

**Iteration 2 caveats:** test folds used second time, after model adaptation to iteration 1 result; 97 % value chosen ahead, but entire constraint idea comes from observing iteration 1 result. Remaining caveats as in iteration 1 (one to two maps per fold, validation/test leak, no visual assessment, dungeons, BlackTek, generator memorization, no GUI wiring).

## Iteration 3 — per-style profiles (2026-11-08, “after model change”)

Criteria recorded before execution ([G4_METRICS.md](G4_METRICS.md), “Iteration 3”, commit `975fba4`; tool `269da1b`). Two runs (44 s) bit-identical. **Iteration 3 criterion met in measured scope, but this is in-sample result and does not meet G4 gate from plan** (interpretation recorded ahead).

**A — own style (in-sample), 8 styles:** profile better than manual parameters in S1 and S2 in **all 8** (32/32 seeds in each), none worse; vegetation (S3) better in all 4 styles with comparable context (caves, m05, m06, m19). Land coherence P1 ≥ 95 % in all 8 (95.2–100 %).

**B — map excluded from learning within style (only case, swamp, n = 2):** profile from M13 to M12 and from M12 to M13 is better than manual in S1 and S2 (32/32 seeds; −320 252 / −307 163 and −493 180 / −487 248 ppm), P1 95.9 % and 95.2 % (just above 95 % threshold). This is the only result on unseen map, but M12 and M13 are the same declared family (file names), two cases insufficient for generalization conclusion.

**C — distinguishability:** 6 of 8 styles recognized (own profile closest to own target). Not recognized: m01 and swamp (m06 and m19 profiles just barely closer: m01: 2 821 743 own vs 2 811 131 m06 profile). Profiles m15, m19, swamp and winter are similar because fitting reaches grid limits (lake 300 ‰, rock to 320 ‰, rivers 0–8, shore 3–4).

| Iteration 3 criterion | Requirement | Result |
| --- | --- | --- |
| A | ≥ 2/3 styles better in S1 and S2, none worse in both | 8 of 8, 0 worse: yes |
| C | ≥ 2/3 recognized | 6 of 8: yes |
| P1 | each profile ≥ 95 % baseline | 0 below: yes |
| B | no threshold, failure if worse in S1 and S2 | 0 failures |

**How to read this (caveats, none trivial):**
- A and C are fitting to the data the profile came from. Manual parameters are far from most targets (e.g. “caves” target is almost pure ground: 3.0M ppm baseline vs 12 712 profile), so “better” is easy to achieve; this proves **generator can reproduce share distribution of a given style**, not that style is faithfully imitated.
- Result measures share distribution, adjacency of types, vegetation and land coherence. **Does not assess appearance** (shapes of rivers and mountains, clusters, naturalness), and rivers still run mostly along axes (D6). Fitted values reach grid limits, so profile is a grid boundary, not optimum.
- Five styles have one map with target; m01 is pure water, caves is pure ground, so profile of such style describes one ground type.
- No style profile files recorded yet (tool reports tuning in JSON), no GUI/`GenerationConfig` wiring.
- Still not measured: dungeons, BlackTek, generator output memorization vs. reference (D10), `border_error_rate`, 100 % correct IDs and zero changes outside mask in full pipeline.

**G4 gate status from plan (unseen maps):** not demonstrated. Only evidence on unseen map (B, swamp, n = 2) is favorable, but too narrow. To demonstrate it, need at least several styles with ≥ 2 independent maps with targets.

## Iteration 4 — river shape (D9 water part, D6) (2026-11-08): criterion D6 NOT MET

Criteria recorded before measurement ([G4_METRICS.md](G4_METRICS.md), “Iteration 4”, commit `59b860c`); implementation `99f617c`. References with rivers (≥ 5 components): only M06 (29 rivers, T 1626 ‰, S 0 ‰) and M19 (8 rivers, T 1669 ‰, S 0 ‰); remaining 10 maps with water extraction do not qualify (2 qualify, so evaluation possible but on two maps, two cross folds). Generator without meandering: T 1361 ‰, S 181 ‰ (31 of 32 seeds with river).

| Variant | Meander (fitted) | T median | S median | T: better/worse seeds (verdict) | S: better/worse (verdict) | P1 (candidate/baseline) | river-island |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 4a (first model) | 8 (grid max) | 1398 | 70 | 18/9 (undecided) | 15/6 (undecided) | 99.997 % | 0 |
| 4b (after model change) | 8 (grid max) | 1514 | 24 | 22/6 (undecided) | 16/5 (undecided) | 99.972 % | 0 |

Medians of distance differences from reference (4b, fold M06→M19): T −58 ‰ (p10 −219, p90 +54), S −77 ‰ (p10 −281, p90 +79); fold M19→M06 analogously (−56 / −77). Measurable conclusion: rivers are more sinuous (T +153 ‰ vs. reference 1626–1669) and nearly without long straight segments (S 181 → 24 ‰ at reference 0), but criterion (better in T and S by rule: median < 0 and ≥ 75 % seeds) not met in either fold: best seed fraction 22/31 = 71 %, and undecided cases result mostly from ties. Met: P1 ≥ 95 % (candidate ≈ baseline) and invariant (0 failures in 1200/1200 seeds: river always reaches lake/edge).

**Caveats (significant).**
- 4b is model change after seeing 4a result (wave change, continuity, interpolated stamping, river-island cleanup); criterion and seeds same, but this is adaptive reuse of same two maps. 4a is original result; 4b does not replace it as proof.
- Fold is two-layer: fitting on one map with rivers, test on other; both yielded meander 8, so fitting saturates at grid boundary, optimum not found.
- Measurement is grid skeleton (approximation); thresholds (segment ≥ 12, river component) chosen before measurement without sensitivity knowledge.
- Did not assess appearance (blind A/B), bifurcations, lake sizes, dungeons or BlackTek. Reference S = 0 means no 12-tile straight skeletons in references; part of difference may come from skeletonization itself.
- Rivers in v6 profiles have meander 8 (v5 profiles no longer supported).
- **Correction (2026-11-08, [G4_COMPARE.md](G4_COMPARE.md)):** iteration 4 river metric skips water components ≥ 20,000 tiles (sea), so large rivers connected to sea did not enter reference or meander fitting. References (M06 W ≈ 1.6) describe narrow streams, generator rivers wider (W ≈ 4.4–6.6). Iteration 4 result concerns only such streams.

## Iteration 5a — water channels, baseline measurement (2026-11-08)

Definitions and criterion recorded before measurement ([G4_METRICS.md](G4_METRICS.md), “Iteration 5”, commit `f0e04bc`). References (profile v7, map v3): qualify M06 (218 components), M08 (72), M10 (72), M19 (82), M07 and M09 have 1 each (no). By record, I compare m06→M06 and m19→M19; M08 and M10 have identical values (same family), not entered for evaluation. Generator: seeds 1001–1032, side 256, channel in 32/32 seeds in both profiles; two runs bit-identical.

| Feature | m06: generator | M06 | band | m19: generator | M19 | band |
| --- | --- | --- | --- | --- | --- | --- |
| L (skeleton per 65,536 tiles) | 455 | 384 | 257–576: in band | 832 | 302 | 202–453: **out** |
| W (width, thousandth tile) | 4,679 | 2,898 | 1,942–4,347: **out** (too wide) | 5,705 | 4,337 | 2,906–6,506: in band |
| B (branches / 100 skeleton tiles, ×1000) | 26,987 | 41,134 | 27,560–61,701: **out** (slightly) | 32,744 | 38,913 | 26,072–58,370: in band |
| C (connecting to open water or edge, ‰) | 916 | 614 | 411–921: in band (at border) | 750 | 865 | 580–1,298: in band |
| In band | **2 of 4: not passed** | | | **3 of 4: passed** | | |

**Conclusion (status description, not gate).** Channel quantity and branching in generator are same order of magnitude as references; exceptions are too-wide channels in m06 (4.7 vs 2.9 tiles) and 2.7× too much channel length in m19. Measurement does not indicate lack of tributaries as main difference between generator and references; earlier visual impression (rivers without network) came from viewing angle chosen for most land and from shared noise scale this metric does not describe.

**Caveats.** In-sample fitting (profile from same map). Two references with rivers, two evaluated. Channel is approximation (13×13 opening, square favors axes; also includes narrow inlets and lake arms). Did not assess appearance, large-scale structures, or rivers in M08/M10. 5b (generator change) not executed because 5a showed no clear tributary gap.

## Iteration 6 — large-scale structures (2026-11-08): criterion NOT MET in recorded layout

Definitions and criterion recorded before measurement ([G4_METRICS.md](G4_METRICS.md), “Iteration 6”, commit `5aa85a0`); `scale_stats` core with test (PASS, values computed manually). References (excess agreement E_d above random layout, ‰): M06 E4 471, E16 394, E64 233; M19 E4 578, E16 425, E64 197. Seeds 1001–1032, side 256, two runs bit-identical.

| Variant | Profile | E4 | E16 | E64 | E16 and E64 in band |
| --- | --- | --- | --- | --- | --- |
| 6a (profile v7: featureSize 24 / 12) | m06 | 454 | 147 | −12 | **0 of 2** |
| | m19 | 310 | 16 | −1 | **0 of 2** |
| 6b (featureSize fitted from grid 256 → 192 on seeds 1–6) | m06 | 571 | 399 (in band) | 121 (band 156–350: **out**) | **1 of 2** |
| | m19 | 606 | 399 (in band) | 102 (band 132–296: **out**) | **1 of 2** |

**Conclusion.** 6a showed gap quantitatively: generator has nearly zero agreement at distance 16 and 64 above chance (references ~400 and 200–230). Changing just `featureSize` to 192 closes gap for E4 and E16, but E64 stays below band in both profiles, so **6b criterion not met**.

**Additional observation (not recorded ahead, explicitly exploratory: side 512, 8 seeds, no evaluation).** At side 512, featureSize 192 gives E4/E16/E64 = 614/496/261 (m06) and 638/477/247 (m19), all within bands. Thus E64 deficit at side 256 comes largely from lag 64 cutoff by window 256 (caveat recorded ahead), and E_d depends on area size. Value 192 is not therefore final result: fitting depends on chosen side, and in GUI user picks any area.

**Caveats.** In-sample fitting on two references; metric does not distinguish one large land from many large patches; did not check P1 (walkability coherence) or river invariant for featureSize 192 (additional 6b conditions), so 6b incomplete for this reason too. User will assess appearance in GUI on profiles `10.98-v8` (only m06 and m19 have featureSize 192; this is manual profile file change, not `style_styles` output).

## Iteration 7 — dirt patches and beach width (2026-11-08): criterion NOT MET

Definitions, change P to P′ and model change recorded before measurement ([G4_METRICS.md](G4_METRICS.md), “Iteration 7”, commits `6aa735e`, `1073167`, `79e54ad`); core `PatchStats` in `scale_stats` with test (PASS, values computed manually).

**7a (side 512, seeds 1001–1032, profile v8).** References M06 / M19: D 421 / 451, F 749 / 625, P′ 5 638 / 5 656. Generator m06: D 112, F 975, P′ 1 987; m19: D 203, F 951, P′ 1 979. In band: m06 1 of 3 (F), m19 0 of 3. Original definition P (beach thickness relative to all Shore cells) gave for references 33 795 and 22 366 (sand far from water), so it was replaced with P′ before generator evaluation.

**7b (fitting on seeds 1–4, side 512, change to shore ring rule: noise threshold ≤ 80 %, required lake ≤ 270 cells, limit `shoreWidth` 8).** Descent of coordinates indicated both profiles at grid boundaries: `transitionWidth` 16 000 and `shoreWidth` 8. Values after fitting: m06 D 210, F 948, P′ 2 316; m19 D 206, F 939, P′ 2 315. **Bands still not met**: D (band 282–632 and 302–676) is below, P′ (3 777–8 457 and 3 790–8 484) is below. Change to recorded ring rule raised P′ only from ~1 980 to ~2 315, because rings above the third form with low probability (noise threshold 80 %). Relative to reference (~5 600, i.e. beach ~5–6 cells), more than twice as much is still missing.

**Conclusions and caveats.**
- Metrics D and F show that the land boundary in the generator is **smoother** (lower D), not more ragged than in references (which have many small land patches). This does not match visual assessment “dotted land”; D and F do not measure fine “dots” (components ≤ 4 cells). Fitting relative to D would increase raggedness, so it is not obvious that it improves appearance for the user.
- Beach: full fitting requires model change (new parameter or different ring rule, profile schema change), not yet recorded.
- In-sample fitting on two references; core tests PASS (including changed limit `shoreWidth` ≤ 8); profile v8 in `local-data` was not changed.

### Iteration 7c — beach (2026-11-08): P′ in band for both profiles

Criteria recorded before measurement ([G4_METRICS.md](G4_METRICS.md), “Iteration 7c”). Beach ring rule: noise threshold min(5 + 10·ring, 40) %, required lake min(30·3^(ring−1), 90) cells. `shoreWidth` chosen from {0..8} on seeds 1–4 (side 512): 8 for m06 and m19 (P′ grows from 1 000 at 1 to 5 835 / 5 807 at 8, references 5 638 / 5 656). Evaluation on seeds 1001–1032, side 512, profile `g4-profiles/10.98-v9`:

| Profile | P′ generator | reference | band | result |
| --- | --- | --- | --- | --- |
| m06 | 5 912 | 5 638 | 3 777–8 457 | **in band** |
| m19 | 5 871 | 5 656 | 3 790–8 484 | **in band** |

Shore share grows from ~1 % to 3.4 % (goal of change); remaining shares decline by less than 3 percentage points (m06 Grass 41.2 → 39.9 %, m19 Grass 28.1 → 26.4 %), so additional condition met. **River invariant not remeasured** (beach is laid before rivers, which overwrite cells, so beach rule does not affect it; not verified by measurement), G1 regression `--commit` not run after rule change. Core test PASS after changing pinned Seed 1729 100×100 snapshot from 6783381837833077004 to 9975948746684599016 (rule change alters result for `shoreWidth` ≥ 1, as recorded; earlier snapshots in G2 documents are historical).

**Caveats.** In-sample on two references; P′ is a thickness measure (not appearance); in references beach includes sand around puddles and rivers, which generator does not do at rivers; visual assessment in GUI is user's responsibility. Land (D, F) was not changed in this step (D remains below band), awaits direction choice.

### m19 profile correction (2026-11-08, by user request after visual assessment)

After viewing v9, user found that grass and dirt in m19 form “narrow strips” (dotted mosaic), while m06 has solid patches. `transitionWidth` for m19 lowered from 16 000 (upper fitting grid boundary) to 6 000 as in m06: profile `g4-profiles/10.98-v10` (manual file change, not `style_styles` output). User's visual decision takes precedence over metric: D (grass/dirt boundary) for m19 drops relative to 7a/7c, thus farther from reference M19 (451), which is conscious agreement. Other profiles with `transitionWidth` 16 000 (m05, winter) were not changed.

## Iteration 8 — rock massifs (2026-11-08): criterion NOT MET

Definitions, result 8a and model change recorded before measurement ([G4_METRICS.md](G4_METRICS.md), “Iteration 8”, commits `ee52877`, `76a859e`); core `PatchStats` extended for rock (test PASS, values computed manually). References M06 / M19: RB (rock boundary) 333 / 121, RF (compactness ≥ 500) 167 / 862, RS (dust < 20) 36 / 15.

| Variant (v10, side 512, seeds 1001–1032) | m06 RB / RF / RS | m19 RB / RF / RS | in band |
| --- | --- | --- | --- |
| 8a (3 octaves height noise) | 49 / 998 / 0 | 33 / 1000 / 0 | m06 0 of 3, m19 2 of 3 |
| 8b (more octaves for `featureSize` > 24, up to 8; 6 for 192) | 77 / 988 / 0 | 47 / 993 / 0 | m06 0 of 3, m19 2 of 3 |

**Conclusion.** Generator rock is smooth and merged into large blocks (boundary 3–10 times shorter than in references; reference rock M06 is fragmented, M19 is compact but with jagged edge). Extra octaves raised RB by ~40–55 %, but not enough for band; **criterion not met**. Cause of gap: raising `featureSize` to 192 in iteration 6 removed fine detail from rock, lake, and beach edges (noise had 3 octaves).

**Side effects of 8b (control, not criterion).** Kind shares within ±5 percentage points vs. v10 (m06 Grass 390 ‰, Dirt 173 ‰, Shore 45 ‰; m19 Grass 277 ‰, Dirt 127 ‰, Shore 45 ‰), P′ 5 549 / 5 530 (still in band), E4/E16/E64 at side 512: m06 604 / 448 / 224 (ref 471 / 394 / 233), m19 647 / 496 / 236 (ref 578 / 425 / 197). Noise change affects profiles with `featureSize` > 24 (m06, m19, winter); for ≤ 24 result is byte-for-byte as before (pinned Seed 1729 snapshot unchanged, test PASS).

**Caveats.** Two references; rock M06 is only ~5 % of area; river invariant and G1 regression after noise change not checked (rivers depend on height, so impact possible); appearance to be assessed by user in GUI. Remaining gap (rock boundary) would require fine-grained disturbance of rock threshold, not yet recorded.

**User assessment (2026-11-08, GUI, m19 500×500 after iteration 8b):** “looks ok” (edges, rock and beach after adding octaves); subjective assessment, does not change metric result (iteration 8 criterion still not met).

## Iteration 9a — paths: can they be measured in references (2026-11-08)

Definition (list of ground keys and path component) recorded before measurement ([G4_METRICS.md](G4_METRICS.md), “Iteration 9”, commit `468b8ee`); function `extractKeyShapes` with test (PASS, values computed manually), probe output `paths-surface.txt`. All 19 reference maps (mapping v3), floor ≤ 7; no evaluation, description only.

| Map | path components | L (skeleton per 65,536 tiles) | W (thousandths tile) | B (branches, ×1000 per 100 tiles) | path tiles | share of present tiles |
| --- | --- | --- | --- | --- | --- | --- |
| M06 | 22 | 70 | 3 642 | 30 200 | 12 905 | 0.46 % |
| M19 | 5 | 78 | 4 606 | 26 259 | 7 346 | 0.72 % |
| other 17 | 0 | — | — | — | ≤ 2 | ≈ 0 |

**Conclusion.** Paths (by narrow key list: cobblestone, gravel, pavement) can be measured in two maps (M06: 22 components, M19: exactly 5, at boundary of qualification); occupy 0.5–0.7 % of area and have order of ~70–80 skeleton tiles per 65 536 tiles (e.g. in 512×512 area ~300 tiles, several segments of tens of tiles each). Condition recorded ahead (≥ 2 qualifying maps) is met, so path generation is not blocked by data, but foundation is very narrow.

**Caveats.** Recognition by brush name (heuristic); paths from other ground (e.g. beaten earth, sand) invisible; M19 value based on 5 components; path generator does not exist, so generator baseline is zero by definition. Generation would require new surface kind passing through resolver, generator window (path brush field), profile (schema), and preview snapshot.

## Iteration 9b — optional paths (2026-11-08): metric in band, GUI integration unverified

Model, criteria and goals recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 9”, commits `468b8ee`, `4ea15a3`). Implemented: `SurfaceKind::Path`, `SurfaceTuning::pathLength` (0 = disabled, not part of style profile), A* routes over Grass/Dirt/Shore between random land points (distance 48–160, cost with noise, width 3–4), resolver with seventh brush (profile with 7 brushes; with 6 rejects grid with paths), “Path length” and “Path brush” fields in generator window (default 0, `cobblestone` brush), preview snapshot with `pathLength`.

**Measurement (seeds 1001–1032, side 512, profile v10; `pathLength` chosen on seeds 1–4 from {40, 80, 160, 320}: 40 for m06, 80 for m19; path channel as in 9a: area ≥ 40, skeleton ≥ 20, width ≤ 7):**

| Profile | seeds with paths | L (ref.) | W (ref.) | B (ref.) | in band |
| --- | --- | --- | --- | --- | --- |
| m06, `pathLength` 40 | 32/32 | 53.5 (70) | 3 679 (3 642) | 28 494 (30 200) | **3 of 3** |
| m19, `pathLength` 80 | 32/32 | 92.5 (78) | 3 746 (4 606) | 29 639 (26 259) | **3 of 3** |

Core tests PASS (`paths`: disabled gives byte-for-byte result as before — pinned Seed 1729 snapshot unchanged; path never overwrites rock, lake or river; deterministic; `pathLength` 401 rejected).

**What was NOT verified.** Full GUI path: `rme.exe` not linked because RME runs (file lock), so generator window with path fields, resolver with seven brushes, Preview/Validate/Place/Undo with paths were not run; G1 `--commit` regression not run nor palette probe after resolver change; additional condition “kind shares ±1 percentage point” not measured separately (paths occupy 0.5–1 % of area); paths have no targets (settlements, gates), do not connect to bridges, do not get vegetation; targets come from two maps (M19: 5 components), so “in band” means agreement with two references, not realism.

### Iteration 9c — path smoothing (2026-11-08): L and W in band, B out of band (intentionally)

Criteria recorded after measurement (process lapse described in [G4_METRICS.md](G4_METRICS.md), “Iteration 9c”; criteria as in 9b and presented to user before change): diagonal moves, cost at rock, moving average smoothing. Measurement (seeds 1001–1032, side 512, v10): **m06** (`pathLength` 40): L 56.5 (ref. 70) in band, W 4 546 (ref. 3 642) in band, **B 3 697 (ref. 30 200) out of band**; **m19** (80): L 89.5 (78) in band, W 4 526 (4 606) in band, **B 5 700 (26 259) out of band**. Before change B was 28 494 / 29 639 (in band). Share of path tiles at rock: median 0 ‰ / 13 ‰ (previously 0 / 9 ‰), so unchanged (this case is rare).

**Conclusion.** B decrease does not mean fewer path branches, only fewer skeleton “branches”: metric B counts skeleton tiles with ≥ 3 neighbors, and ragged, stepped path outline produces many (skeletonization artifact), as in manually drawn reference paths. Smoothed paths are closer to what user wanted (smooth curves; view of 200×200 section confirms smooth curve width 3–4), but **criterion B from 9b not met**; accepted user's visual decision over metric. Core tests PASS (path never on rock/lake/river, deterministic, disabled = byte-for-byte as before). Caveats as in 9b; visual assessment in GUI after smoothing is user's responsibility.

### Iteration 9d — path repulsion from rock (2026-11-08): criterion met

Criterion recorded before change ([G4_METRICS.md](G4_METRICS.md), “Iteration 9d”, commit `f8ae580`): share of path tiles with Rock within radius 3 (mean and median across seeds) at least half lower; L and W in band; core tests PASS. Change: A* cost grows with rock proximity (60/40/25/12 for distances 1/2/3/4). Result (seeds 1001–1032, side 512, v10, `pathLength` 40 / 80):

| Profile | rocknear median before → after | rocknear mean before → after | L (ref.) | W (ref.) |
| --- | --- | --- | --- | --- |
| m06 | 0 → 0 ‰ | 36.9 → 8.2 ‰ | 55.5 (70), in band | 4 434 (3 642), in band |
| m19 | 87.5 → 10 ‰ | 99.4 → 16.7 ‰ | 91.0 (78), in band | 4 655 (4 606), in band |

Mean decrease: m19 ~83 %, m06 ~78 %. Core tests PASS. **Confirmed by user in GUI (2026-11-08):** Validate and Undo/Redo with paths passed; user chose own path brush (dark), which explains appearance in 9c screenshots. Caveats: baseline median already low (rare case, hence mean control); B still out of band (artifact described in 9c); visual assessment after 9d user's responsibility; kind shares after adding paths not measured separately.

### G1/G2 regression after iteration 3–9 changes (2026-11-08)

Probes on real client 10.98 from private profile (`g1_run_resolver_probe.py`, current code: profiles v6–v10, meander, paths, new beach rule, more noise octaves):
- `--commit` (`g4-reg-commit-20261108a`): `"passed": true`, `input_assets_unchanged: true`, `stderr` empty; PREVIEW_PASS, COMMIT_PASS, COMMIT_SIZE_PASS ×2, PREVIEW_BASE_GROUND/EXCLUDED/MASK_EPOCH_PASS, error tests (preparation, validation, selection) PASS.
- `--surface` (`g4-reg-surface-20261108a`): all assertions PASS (SURFACE_RESOLVER_PASS, SURFACE_RESOLVER_SIZE_PASS ×6, SURFACE_E2E_PASS ×2, SURFACE_VEGETATION_E2E_PASS ×2, SURFACE_NEXT_TO_TERRAIN_PASS ×3, SURFACE_BORDER_STATS ×6 no pairs no border); `"passed": false` only because `stderr` not empty: 7 wxWidgets `SetFocus` warnings (Win32 error 87), **identical in number and size (1 113 bytes) as in historical G2 run `g2g-surface-182831`**, i.e. known debt D1/D2, not result of these changes.

**What regression does NOT cover.** Probes use profile with 6 brushes (no paths); 7-brush path (paths) confirmed by user in GUI (Preview, Validate, Place, Undo/Redo), without separate probe. Performance measurements not repeated.

**What regression does NOT cover.** Probes use profile with 6 brushes (no paths); 7-brush path (paths) confirmed by user in GUI (Preview, Validate, Place, Undo/Redo), without separate probe. Performance measurements not repeated.

## Iteration 10 — transitions between kinds (2026-11-08): m06 8 of 8, m19 2 of 8; criterion NOT MET for m19

Definitions and subsequent changes recorded before execution ([G4_METRICS.md](G4_METRICS.md), “Iteration 10”, commits `4f5ff75`, `5ec7c07`, `354d9a7`); core `PatchStats` extended for rock and dirt boundary composition (test PASS, values computed manually).

**References (boundary composition):** M06 rock RD/RG/RS/RW 33/938/24/4, dirt DG/DR/DS/DW 951/32/12/4; M19 rock 99/502/251/147, dirt 728/155/27/88.

| Variant (v10, side 512, seeds 1001–1032) | m06 in band | m19 in band | notes |
| --- | --- | --- | --- |
| 10a (no change) | 6 of 8 | 3 of 8 | dirt touches rock and beach proportionally to share (RD 254–322 vs 33–99; DS 145–194 vs 12–27) |
| 10b (full grass border 2 cells around Rock and Shore) | 8 of 8 | 2 of 8 | RD 0 (m19 ref. 99) out of band, DR 0 (155) fell out of band |
| 10c (partial border around Rock, ~75 %; full around Shore) | **8 of 8** (RD 36 vs 33) | **2 of 8** (RD 43 vs 99 out of band; DR 21 vs 155 out; DS 0 in band) | chosen variant, fixed, no parameters |

**Conclusion.** For m06, rule “dirt does not touch rock and beach” reproduces boundary composition (8 of 8, RD 36 vs 33). For m19 it is insufficient: reference M19 has much more rock-dirt contact and, most importantly, **rock touches beach and water (RS 251, RW 147)**, which generator does not (RS 0), because rock is highest and lake is lowest height cells. This would require height model change, not local rule, so **not done**. Criterion from record (RD and DS in band for both profiles) not met for m19; per record, no further parameter search for metrics. Kind shares at boundaries (m06 Grass 403, Dirt 161; m19 294 / 115; change < 5 percentage points).

**Behavior change.** Border rule changes Surface results for all profiles and for `pathLength` 0; pinned Seed 1729 100×100 snapshot changed from 9975948746684599016 to 17159159863505677800 (test PASS, added test for border rule: no Dirt within 2 of Shore, some Dirt–Rock contact remains). **Not checked:** appearance in GUI, G1 regression after this change (change is in core, not resolver), in-sample on two references.

**User assessment (2026-11-08, GUI, m06/m19 500×500 with grass border):** “looks ok”; subjective assessment, does not change metric result (boundary composition criterion for m19 still not met).

## Iteration 11a — settlements and structures: buildings in references (2026-11-08)

Definitions recorded before measurement ([G4_METRICS.md](G4_METRICS.md), “Iteration 11”, commit `1a16d26`); function `extractBuildingShapes` with test (PASS, values computed manually), probe output `buildings-surface.txt`. Building = 8-connected component of tiles with Wall or Door feature (≥ 12 cells, bounding box ≥ 4 × 4), floors ≤ 7; all 19 maps; description only.

| Map | buildings | per 65 536 cells | median sides (longer × shorter) | wall cells | doors per building | with doors |
| --- | --- | --- | --- | --- | --- | --- |
| M06 | 1 313 | 31 | 9 × 7 | 30 | 1.66 | 51 % |
| M19 | 582 | 37 | 9 × 7 | 24 | 3.44 | 61 % |
| M05 | 13 | 37 | 11 × 8 | 39 | 2.15 | 85 % |
| M15 | 5 | 48 | 11 × 6 | 19 | 0.20 | 20 % |
| M01 | 47 | 94 | 5 × 4 | 16 | 0 | 0 % (mostly fences/walls) |
| M11, M12 | 1–2 | — | — | — | — | — |
| remaining 12 | 0 | — | — | — | — | — |

**Conclusion.** Buildings can be measured in five maps (≥ 5 buildings: M01, M05, M06, M15, M19), and in M06 and M19 they are numerous: ~1 building per 2,000 cells (i.e. ~125 in 512×512 area), typically 9 × 7 cells, ~30 wall cells, half with doors. Generator does not create any structures, so baseline is zero by definition.

**Caveats.** Wall feature includes fences and standalone walls (M01, part of M15), so “building” is a shape heuristic; interiors, roofs (higher floors) and furniture are outside the measure; evidence mainly from M06 and M19. **What generation requires (not done):** walls and doors are not ground, but items on a cell requiring wall brush with automatic element matching and door brush; current plan (`GroundPlan`) carries only ground and doodads, so preview, validation and Undo require extension with a structural items layer.

## Iteration 11b — building foundations (2026-11-08): features in band, share condition not met for m19

Model, criteria and density constants recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 11”, commit `ae1c33a`). Implemented: `SurfaceKind::Floor`, `SurfaceTuning::buildingDensity` (0 = disabled, not part of profile), floor rectangles 7–11 × 5–9 (random orientation) with 2-cell margin on Grass/Dirt, resolver with eight brushes (index 6 path, 7 floor), “Buildings” and “Floor brush” fields in generator window (default 0, `stone floor`), preview snapshot with `buildingDensity`.

**Measurement (seeds 1001–1032, side 512, v10; density 30 for m06, 40 for m19, constants above):**

| Profile | buildings per 65 536 (ref.) | longer side (ref.) | shorter side (ref.) | in band |
| --- | --- | --- | --- | --- |
| m06 | 30 (31) | 9 (9) | 7 (7) | **3 of 3** |
| m19 | 40 (37) | 9 (9) | 7 (7) | **3 of 3** |

Additional constraint “category shares ±1 percentage point vs. state without buildings”: m06 max change 0.8 percentage point (met), **m19 1.8 percentage point (Grass 294.5 → 276.5 ‰) — not met**: floors consume ~3–4% of land surface, so grass share declines, which follows from design, not error. Core tests PASS (disabled = byte-for-byte as before; floor only on former Grass/Dirt; rectangle letters; value 101 rejected; deterministic). G1 regression: probe `--commit` `passed: true` (stderr empty), probe `--surface` all assertions PASS (stderr as in historical run: 7 `SetFocus` warnings, 1 113 bytes).

**Caveats.** Floor without walls, doors, and roofs is not a building; features in band result from given dimensions (7–11 × 5–9 chosen for reference 9 × 7), so this is implementation check, not independent proof; random placement (no roads and settlement clusters, paths do not lead to buildings); comparing floor outline with wall outline is an approximation. **Not verified:** generator window with new fields, Preview/Validate/Place/Undo with floors (path with 8 brushes; probes use 6), appearance in GUI.

## Iteration 11c — building walls (2026-11-08): metric in band, appearance and corner correctness await user

Scope, model, and criteria recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 11c”, commit `6a3048a`; user decision: walls with floor, doors not required). Implemented: `GroundProfile::wall` (wall brush) and `GroundPlan::walls` (wall item ID for each outline cell of foundation; outline = Floor cell with 4-neighbor of different kind), validation in resolver (wall brush exists, is not wall decoration, item is not ground or meta-item) and in preview (item belongs to brush, lies on plan cell, no wall outside mask), placement after vegetation and element alignment via existing `WallBrush::doWalls` on preview buffer (no second engine), plan and preview snapshot with walls, “Walls” and “Wall brush” fields in generator window (active when “Buildings” > 0).

**Measurement (seeds 1001–1032, side 512, v10, density 30 / 40 as in 11b).** Median outline cells per building: m06 28 (ref. wall 30, band 20.1–45), m19 28 (ref. 24, band 16.1–36): **in band in both**. Core test PASS (outline = 2·(longer+shorter)−4 cells for each foundation). G1 regression: probe `--commit` `passed: true`, stderr empty; probe `--surface` all assertions PASS (stderr as historically: 7 `SetFocus` warnings, 1 113 bytes). `rme.exe` linked.

**What was NOT verified.** Full GUI path with walls (window, Preview, Validate, Place, Undo/Redo), correct alignment of wall elements (horizontal, vertical, corners) by `WallBrush::doWalls` on user client, behavior with protected cells and mask; G1 probe uses profile without walls, so new layer does not yet have e2e test on real client outside user GUI; outline metric results from dimensions given in 11b (implementation check, not independent proof). Building without doors and roof is not gameplay entry.

**User assessment (2026-11-08, GUI, building walls, `Buildings` > 0, `Walls` enabled):** “looks like real buildings”; Preview, Validate, Place, Undo, and Redo passed without issues (confirmed by user, not probe). Thus closed previously unverified GUI path with eight brushes and wall layer; correctness of corner alignment assessed visually by user.

## Bug fix: order of `SurfaceTuning` fields and profile parser (discovered 2026-11-08 during full test run before PR)

**Bug.** In iteration 9 I added `pathLength` field to `SurfaceTuning`, then in 11b `buildingDensity`, **before** field `meander`. Profile parser initialized tuning positionally (`SurfaceTuning {v[0] … v[10]}`), so recorded profile value `meander` (8) went from iteration 9 to `pathLength`, from 11b to `buildingDensity`, and `meander` stayed 0. Test `test_procedural_style_profile` (round trip tuning) from 13fac4d was red; **did not run full test suite then**, only single tests (my oversight).

**Effects.** (1) GUI: window after profile load always overwrites `pathLength` and `buildingDensity` with field values, so unwanted paths and buildings did not arise, but **from iteration 9 until now profiles did not apply river meander** (rivers in screenshots after 9 are rivers without meander). (2) Measurements: `style_render` with v10 profiles had `meander` 0 from 9 to 11, plus small paths in iteration 10 (`pathLength` 8 from wrong mapping), in 11 `buildingDensity` given explicitly. Iteration 9 measurements (paths) gave `pathLength` explicitly.

**Fix.** Field order restored (first eleven as in profile, then `pathLength`, `buildingDensity`), parser assigns fields by name. All 14 core tests PASS.

**Remeasurement with correct profile loading (seeds 1001–1032, side 512, v10):** paths 9d: m06 L 53 / W 4 381 (previously 55.5 / 4 434), m19 L 89 / W 4 618 (91 / 4 655), “path at rock” mean 8.2 / 16.9 ‰ (8.2 / 16.7): no change in bands; transitions 10c: **m06 7 of 8** (previously 8 of 8: DW 55 vs 4 fell 5 ‰ out of band ±50 ‰, because rivers with meander touch land more), m19 2 of 8 (unchanged); buildings 11b: unchanged (30 / 9 / 7, 40 / 9 / 7), shares m06 0.8, m19 1.7 percentage point. Iteration 10 criteria for m06 (8 of 8) given earlier were computed without meander; correct value is 7 of 8.

## Iteration 12a — settlements: clusters, paths and doors in references (2026-11-09): PARTIAL, descriptive measurement

Definitions, constants, and bands recorded before implementation and measurement ([G4_METRICS.md](G4_METRICS.md), “Iteration 12”, commit `46b7024`; verified by grep before implementation). Code: `settlement_stats` (core; test with manually counted values PASS, commit `ff142e6`), probe output `settlements-surface.txt`, one command `docs/procedural/smoke/g5_reference_settlements.py`. All 19 maps (copies in `local-data/`, runner hashes source before and after), floors 0..7, client 10.98, mapping `accepted-10.98-v3`. Two runs of M06 gave byte-identical file (`cmp`); other maps once.

| Map | buildings | G | Kn (×1000) | Ks | Ki ‰ | Kb ‰ | Bp / Cp ‰ | Bv / Cv ‰ | Dh ‰ | Dn | Db ‰ | Dp / Dp0 ‰ (\|S\|) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- | ---: | ---: | ---: | --- |
| M06 | 1 313 | 4 | 11 167 | 1 | 265 | 661 | 93 / 14 | 287 / 316 | 512 | 2 | 231 | 855 / 594 (69) |
| M19 | 582 | 4 | 11 955 | 1 | 189 | 714 | 209 / 25 | 245 / 268 | 613 | 4 | 563 | 750 / 546 (76) |
| M01 | 47 | 4 | 34 204 | 2 | 148 | 765 | 0 / 0 | 319 / 169 | 0 | N/A | N/A | N/A (0) |
| M05, M11, M12, M15 | 13, 1, 2, 5 | N/A (< 30 buildings) | | | | | | | | | | |
| other 12 | 0 | N/A | | | | | | | | | | |

Three maps qualify (≥ 30 buildings): M06, M19, M01. M01 appears by 11a to be fences rather (0 doors, 0 path cells per list 9a), so for clusters and spacing it is supporting evidence, not settlement pattern. Door and orientation features come from M06 and M19 only.

**What follows (descriptive, without generator assessment).**
- **Clusters.** Nearest neighbor typically lies at 4-cell spacing (M06, M19, M01), and 66–71% of buildings (M06, M19) belong to clusters ≥ 3; solitary ones are 19–27%. Clusters number ~11–12 per 65 536 cells vs. 31–37 buildings per 65 536, so ~3 buildings per cluster on average, but cluster median is 1 (many singletons, few large). Random placement from 11b at density 30–40 per 65 536 likely gives far fewer near neighbors; this is to be measured in 12c, I do not assume.
- **Path.** Buildings lie near path 6.6× (M06: 93 vs 14 ‰) and 8.4× (M19: 209 vs 25 ‰) more often than average land cell. Relationship is clear, though absolutely only every 11th (M06) and 5th (M19) building has path cell within ≤ 3: paths per list 9a are only part of roads.
- **Water.** No relationship: Bv ≤ Cv in M06 (287 vs 316) and M19 (245 vs 268); M01 (319 vs 169) is only map with larger Bv, but those are fences. Conclusion: generator does not need “building near water” model, it suffices that building not stand on water (already ensured by Grass/Dirt margin).
- **Doors.** Doors present in 51% (M06) and 61% (M19) of buildings, median door cells is 2 and 4, and only 23% and 56% of door cells lie on outline edge. So “doors” in reference is often many cells (door-windows, gates, internal doors); assumption “one entry per building” is **intentional idealization** (deviation recorded upfront), not reference reflection.
- **Door orientation vs. path.** Among buildings with edge doors and nearby path (|S| = 69 in M06, 76 in M19) door side is path side in 855 ‰ and 750 ‰ of cases, vs. 594 ‰ and 546 ‰ at random side: doors face path 26 and 20 percentage points more often than chance. Sample uncertainty (one map, n ≈ 70): ~±40 ‰ (1 SD from binomial), so direction is reliable, magnitude is not.

**What was NOT verified.**
- Independence: all from M06 and M19 (D17), M01 supporting only; no third map with houses, so no separate validation.
- Scatter across maps (one value per map; only binomial uncertainty given for Dp).
- Paths outside list 9a (trampled earth, trodden grass), thus actual building-road count: Bp is lower bound.
- Whether “building” means house: Wall/Door in reference includes fences, gates, and walls (D19); distances 3, 8, 12 are constants upfront, sensitivity not explored.
- Floors > 7, interiors, roofs, bridges.
- Generator measurement: generator baseline on these measures (12c) not yet computed; design 12b not written.

## Iteration 12b–12d — clusters, doors and paths to doors in core (2026-11-09): PARTIAL, criterion NOT MET

Scope from user: doors + paths to doors + clusters. Model, constants and criteria recorded before implementation and before each measurement ([G4_METRICS.md](G4_METRICS.md), “Iteration 12”: definitions `46b7024`, model 12b `7bccd95`, result 12c and record 12d `99c60d7`; records verified by grep). Implemented in core (commit `49e6737`): buildings in clusters (random anchors, rest added within 3–6 cells of already placed), one door per building (`SurfaceGrid::doors`, center of side facing nearest path within ≤ 60, else random side), Path step before doors and route to nearest path (shared `PathRouter` extracted from `drawPaths`). Adapter (RME door brush, plan, preview, generator window) **not done**.

**Generator measurement (seeds 1001–1032, side 512, profiles v10; settings m06 pathLength 40 / buildingDensity 30, m19 80 / 40; median; two runs byte-identical).**

| Feature | ref m06 | 12b (30% anchors) | 12d (60%) | ref m19 | 12b | 12d |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| G (spacing) | 4 | 4 ✓ | 4 ✓ | 4 | 4 ✓ | 4 ✓ |
| Kn (×1000) | 11 167 | 5 875 ✗ | 9 375 ✓ | 11 955 | 4 750 ✗ | 7 500 ✗ (band from 8 010) |
| Ks | 1 | 2 ✗ | 2 ✗ | 1 | 3 ✗ | 2 ✗ |
| Ki ‰ | 265 | 62 ✗ | 150 ✗ | 189 | 37 ✗ | 72 ✗ |
| Kb ‰ | 661 | 850 ✗ | 741 ✗ (Δ 80) | 714 | 928 ✗ | 875 ✗ |
| Bp − Cp (direction) | +79 | +924 ✓ | +910 ✓ | +184 | +908 ✓ | +896 ✓ |
| Bv − Cv (direction) | −29 | −130 ✓ | −127 ✓ | −23 | −98 ✓ | −86 ✓ |
| Dp − Dp0 (direction) | +261 | +503 ✓ | +527 ✓ | +204 | +495 ✓ | +509 ✓ |
| **features passed** | | 4 of 8 | **5 of 8** | | 4 of 8 | **4 of 8** |

**Result: criterion (≥ 75 % of evaluated features) NOT MET in either 12b or after change 12d, in both profiles.** Did not further tune constants (record from ahead). Change 12d (60 % anchors) improved Kn and Ki, did not restore Ki or Ks; in code now is 60 % (closer to reference than 30 %), noted as decision after model change.

**What is met from assumptions.** Exactly one door per building, doors on outline, Path step, spacing ≥ 3, doors vanish when cell stops being Floor after mask (test `settlements`); `buildingDensity` 0 unchanged: pinned Seed 1729 snapshot and PNG `pathLength` 80 / `buildingDensity` 0 for 4 seeds **byte-for-byte** identical to core before changes (comparison of two binaries); 15 core tests PASS (`g5_run_suite.py --probes`: both G1 probes PASS after rebuilding `rme`).

**Result changes (intentional).** For `buildingDensity` > 0: different building placement, Path before doors and routes; in `--surface` probe wall cell count for 100×100 changed from 150 to 164 (250×250: 1 038 unchanged); `SURFACE_RESOLVER_SIZE` snapshot (without buildings) unchanged. Test `buildings` loosened: besides Floor allowed Path (Grass/Dirt/Shore → Path).

**Deviations where criterion not evaluated.** (1) **Paths:** length (L per 65 536) grows from 53 / 89 (9d) to 299 / 344 with buildings enabled (ref 70 / 78), because each building gets route to nearest network; bands 9b no longer apply to this config. To resolve: fewer routes (e.g. only to Path cell within ≤ 25, or shared cluster routes) or `pathLength` 0. (2) **Bp = 1000 by design** (step), so this feature has direction only. (3) Dn = 1 and Db = 1000 are idealizations (ref 2–4 and 23–56 %).

**What was NOT verified.** Adapter and GUI (door brush via `DoorBrush`, Preview/Validate/Place/Undo/Redo with doors and steps, settlement appearance); “without route” (percentage of buildings whose step does not connect to network) not counted; sensitivity to constants 3–6, 12, 60; independent maps (D17); impact on category shares (Path grows); performance; behavior with mask and protected cells outside `settlements` test.

## Iteration 12e — door adapter (2026-11-09): PARTIAL, probes PASS, GUI awaits user

Scope and controls recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 12e”, commit `c0f7883`; record verified by grep). Implemented (commit `c96a238`): `GroundProfile::door` (kind from closed list: Normal door, Locked door, Magic door, Quest door, Normal door (alt), Archway; mapping to `DoorType` in `door_types.h`, no item IDs), `GroundPlan::doors` (flag on outline cell; in plan snapshot only at doors), validation in resolver, preparation and preview, placement of `DoorBrush::draw` after `WallBrush::doWalls` (refusal `InvalidItem` when wall brush lacks such doors for placement), “Doors” and “Door kind” fields in generator window (active when walls enabled).

**Found and fixed obstacle.** RME converts door element to `Door` class object; preview snapshot check (`hashItem`) accepted only `Item` class and rejected preview (`PreviewError::InvalidItem`). Permitted `Door` without assigned house door ID; nothing else was loosened.

**Results.** New probe case `--surface` (`SURFACE_DOORS_E2E_PASS`): 100×100 6 doors (6 buildings), 250×250 38 doors (38 buildings); after Commit exactly one door element per planned cell and none outside, one history position, after Undo zero doors and cell count as before, after Redo complete again. Refusals: doors without wall brush, unknown kind. Plan snapshot with doors differs from plan without doors, ground and walls of plan identical. `g5_run_suite.py --probes`: 15 of 15 core tests, both probes PASS on rebuilt `rme` (`SetFocus` warnings in `--surface` now 11 instead of 9: two cases added from `NewMap`; only this known kind stderr).

**What was NOT verified.** RME generator window (new fields, appearance, door direction vs. path, Preview/Validate/Place/Undo/Redo with doors on user client) awaits user assessment; probe uses `stone wall` brush and “Normal door”; for other wall brushes and door kinds preview may refuse (no doors of that kind in brush); house door IDs not assigned; routes to doors remain ~4× longer than in reference (see 12d).

## Iteration 12f — door facing nearest building (2026-11-09): implemented per user note, awaits user assessment

Rule recorded before change ([G4_METRICS.md](G4_METRICS.md), “Iteration 12f”). Measurement after change (seeds 1001–1032, side 512, v10; two runs byte-identical): G, Kn, Ks, Ki, Kb **unchanged** vs. 12d (building placement does not depend on doors), Dp 1000 (as before), path length L 303 / 337 (was 299 / 344). Criterion 12c still not met (5 of 8 in m06, 4 of 8 in m19). 15 core tests and both G1 probes PASS after rebuilding `rme` (door probe: 6 and 38). **Not verified:** by unit test that adjacent building doors without path face each other (noted in assessment but not written); improvement on actual screenshot (user assessment).

**User assessment after 12f (2026-11-09, GUI, two screenshots, Undo/Redo and Validate work):** doors and paths to doors deemed okay, user wants to proceed. My notes from those screenshots (not measured): building without nearby path gets only short step without route (screenshot 1 at bottom, screenshot 2 near right building); left building doors on screenshot 2 face distant path (range 60), not neighbor 25 cells away; long paths 9d end in rounded "blob" mid-land; paths occupy much surface (L ~4× reference, 12d). User overall assessment takes precedence over criterion 12c metric (still unmet); discrepancy noted.

## Iteration 12g — shorter paths to doors (2026-11-09): L in band at `pathLength` 0; cost: ~2/3 buildings without route

Criteria and selection rule recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 12g”; grep confirmed). New constant `doorTrailReach`: route from step only to Path cell within ≤ R from building center. Measurement result of variants (seeds 1001–1032, side 512, v10, `pathLength` 0; median; two runs byte-identical):

| R | L m06 (ref 70, band 47–105) | L m19 (ref 78, 52–117) | no route m06 / m19 (‰) |
| --- | ---: | ---: | ---: |
| 16 | 20.5 | 25.5 | 696 / 672 |
| **24 (selected per rule)** | **48** | **72.5** | 671 / 637 |
| 36 | 116 ✗ | 151 ✗ | 629 / 597 |
| previously (60) | ~300 | ~340 | n/d |

Selection R = 24 (largest satisfying L ≤ 1.5·ref in both profiles). G, Kn, Ks, Ki, Kb, Bp, and Dp unchanged vs. 12d/12f (placement same). **At default profile settings** (`pathLength` 40 / 80 as in GUI suggestions) L is 108 / 177, because random long paths 9d are added; m06 result just above band, m19 above.
**Cost:** “no route” (step without path to other doors) is ~64–67% of buildings; in references only 9–21% of buildings have nearby path (Bp 93 / 209 ‰), so percentage is same class but results from short reach, not reference model. 15 core tests and both G1 probes PASS after rebuilding `rme`. **Not verified:** appearance in GUI (user assessment takes precedence); other `doorTrailReach` values outside three from record; `style_render` now has `access` row (percentage no route) counted only when doors present.

## Iteration 12h — paths to doors must lead somewhere (2026-11-09): stubs removed, L out of band in m19; awaits user assessment

Criteria and selection rule recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 12h”; grep confirmed). Changes: (1) routes to doors stamped to step and goal (`reachEnds`; long paths 9d unchanged), (2) step only when goal in reach or later partner in reach; solitary building doors exit straight to ground; door side toward partner.

Measurement (seeds 1001–1032, side 512, v10, `pathLength` 0; median; two runs byte-identical):

| `doorTrailReach` | L m06 (≤ 105) | L m19 (≤ 117) | stubs m06 / m19 (‰) | doors without step m06 / m19 (‰) |
| --- | ---: | ---: | ---: | ---: |
| 24 | 174 ✗ | 277 ✗ | 25 / 18 | 104 / 43 |
| **16 (selected per rule, 24 fails)** | **96 ✓** | **174 ✗** | **33 / 31** | 208 / 103 |

Criterion (a) L: **not met in m19** (174 vs. ≤ 117), met in m06; criterion (b) stubs ≤ 100 ‰: **met** (was ~650 ‰ “isolated” in 12g). G, Kn, Ki, Kb, Dp unchanged; Bp 791 / 897 ‰ (not 1000, solitary buildings have no step). 15 core tests and both G1 probes PASS after rebuilding `rme`; `settlements` test changed intentionally: step before doors no longer required (for side ≥ 250 at least one required). **Not verified:** appearance in GUI (user assessment takes precedence); reaches other than 16 and 24; “leads somewhere” beyond stub metric (neighbor route may be bent or wide).

**User assessment after 12h (2026-11-09, GUI screenshots, two areas, `rme` after 12h): “looks better”.** Paths connect adjacent building doors and no stubs visible. Visually confirms direction, but this is general assessment (no numbers), and L metric in m19 stays outside recorded band (174 vs. ≤ 117) – metric/visual discrepancy noted; visual takes precedence. Still open: criterion 12c as a whole unmet, no sensitivity study of constants (60%, 3–6, 60, 12, 16/24).

## Iteration 13 — surface reachability validator (2026-11-09): PARTIAL, buildings criterion NOT MET

Criteria recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 13”, commit `645ce8e`; grep confirmed). Implemented `reachabilityOf(SurfaceGrid, walls)` (`source/procedural/core/reachability.{h,cpp}`): measures, changes nothing, no repair or seed rejection.

**Tests.** New `test_procedural_reachability`: layouts counted manually (lake splits land 20 + 25; building without doors closed, with doors reachable, without walls reachable; doors to water; Preserve; errors and `stop_token`) and comparison with independent implementation (union-find) on 80 random 16×16 grids with and without walls: 160 of 160 matching, two runs identical. 16 core tests and both G1 probes PASS after rebuilding `rme`; Seed 1729 snapshot unchanged (test `surface`). Working memory: 9 B/cell (labels 4 + queue 4 + flag 1), **criterion from record (≤ 8) was too tight**: this is deviation, value is estimate not measurement.

**Measurement** (seeds 1001–1032, side 512, profiles v10, walls and doors, median [min..max], two runs byte-identical; `g5_shape_table.py`, row `reach`):

| | m06 (`pathLength` 40, 30 buildings/64k) | m19 (80, 40) |
| --- | ---: | ---: |
| passable cells | 177 340 | 177 084 |
| largest component (‰) | 993.5 [647..999] | 993.5 [651..1000] |
| outside largest per 65 536 cells | 266 [4..15 759] | 264 [0..15 495] |
| pockets ≥ 16 | 2 [1..9] | 2.5 [0..9] |
| closed buildings / buildings | 0 [0..52] / 120 | 0 [0..77] / 160 |
| seeds with closed > 0 | **10 of 32** | **10 of 32** |

With `pathLength` 0 numbers nearly same (e.g., m06 closed [0..50], m19 [0..84]).

**Criterion 4 (closed = 0 in ≥ 90% seeds): NOT MET** (22 of 32 = 69% of seeds without closed). Diagnostics added post-measurement (post hoc, row `doorBlocked` in `style_render`, not from criterion record): **no doors blocked** (0 of 32 seeds); closed buildings lie on land cut from largest component by river or lake (seeds with largest component 647–989 ‰). Closure thus results from land division, not door error; fix requires bridges, fords, or seed rejection (next iteration).

**Not verified:** Rock and Shore passability on client (assumed, D22); “POI reachable” and “fixed start” (do not exist in generator, N/A); floors and Z transitions; adapter and GUI (validator not yet called from generator window or RME probe, only from `style_render`); state for `buildingDensity` 0 outside snapshot test; validator performance (not measured).

## Iteration 14 — fords through water (2026-11-09): PARTIAL, criterion NOT MET for recorded value (32), met post hoc from 48

Criteria and value selection rule recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 14”, commit `15d5f66`; grep confirmed). Implemented `SurfaceTuning::fordLength` (appended to struct, 0..64, 0 = disabled), `placeFords` in core (BFS through River and Lake, Path on route and water edge, components counted by `walkableLabelsOf`), “Ford length” field in generator window (profile must have seventh path brush), plan snapshot with `fordLength` only when set.

**Tests.** `test_procedural_reachability` extended with manually counted layouts: river 3 wide (reach 2: unchanged; reach 3: 6 Path cells, river 18, one component 142), pocket 12 cells without ford, lake 4 wide (reach 3: unchanged; 4: 8 Path cells), determinism, second run unchanged. 16 core tests PASS, Seed 1729 snapshot unchanged. Probe `--surface`: new case `SURFACE_FORDS_E2E_PASS` (side 250, first seed from 1729 with ford: 6 Path cells only on water, Preview, Validate, Commit as one step, Undo, Redo, plan snapshot changes); probe `--commit` PASS; `rme` rebuilt.

**Measurement** (seeds 1001–1032, side 512, profiles v10, walls and doors, settings as in iteration 13; seeds with closed buildings / total closed; median ford cells):

| `fordLength` | m06 seeds / sum | m19 seeds / sum | largest component, min (‰) | ford cells (median, max) |
| --- | ---: | ---: | ---: | ---: |
| 0 | 10 / 121 | 10 / 159 | 647 / 651 | 0 |
| 8 | 9 / 69 | 9 / 82 | 733 / 730 | 8, 42 |
| 16 | 9 / 69 | 9 / 82 | 733 / 731 | 14, 122 |
| **32 (selection per rule)** | **5 / 24** | **5 / 31** | 919 / 919 | 36, 307 |

Criterion (≥ 29 of 32 seeds without closed, i.e., ≤ 3 closed, in both profiles): **NOT MET for any value from recorded set {8, 16, 32}**; per rule selection is 32 with verdict NOT MET (5 of 32 seeds, 69 → 84% without closed). **Post-hoc measurement (not from record):** 48 → 2 of 32 seeds (m06 and m19; 2 closed buildings total each), 64 → 1 / 2 of 32; these values would meet criterion, but not in recorded set, so do not change verdict; ford cells at 48–64: max 330–373 per 262 144 cells. Two runs byte-identical (`g5_shape_table.py --extra fordLength=32`).

**Conclusion.** Fords suffice when water no wider than reach; remaining closed buildings lie beyond water wider than 32 cells. Default in window 0 (disabled); value for manual assessment: 48.

**Not verified:** ford appearance in GUI (user assessment takes precedence); whether Path cell on water is passable in game (depends on path brush, D22); bridges with separate item (out of scope); impact of masks and protection (Preserve can break ford); ford performance (not measured; in 32-seed sweep generation with fords took 6–7 s vs. 4 s without, but two generator calls per seed in `style_render`); `test_universal_rotator` does not link with full `ninja` (different target, not touched, not tested).

## Iteration 14b — sand fords, limit 256 (2026-11-09): PARTIAL, per user note; criteria 1–4 met, appearance and game walking not verified

Criteria recorded before change ([G4_METRICS.md](G4_METRICS.md), “Iteration 14b”, commit `95a3971`; grep confirmed). **Iteration 14 bug (user note: “can't walk that bridge”):** criteria checked connectivity in model, not cells after placement. Probe measurement showed 5 of 6 ford cells blocking: ground `cobbled pavement`, edge `sand`, and blocking element `shallow water` (id 4648), water edge overlaid by bordering, because path brush has lower priority than water. Result from iteration 14 (Path on water) I mark as **incorrect**; seed numbers from that iteration concern model, not walking.

**Change.** Ford tiles are Shore kind (sand, same brush and bordering as beaches); fords no longer require a seventh brush (adapter and window revert to requiring path brush only at `pathLength`); limit `fordLength` 64 → 256 (validation and window field; 64 was my arbitrary choice, not measurement-driven).

**Criterion 1 (probe `--surface`): met.** After Commit, 0 of 6 blocking ford tiles; all have ground (new assertion `FORD_TILES`, blocking property from client 10.98 data). **Criterion 2: met**, numbers identical to 14 (seeds with locked buildings out of 32: m06 / m19 at 8: 9 / 9; 16: 9 / 9; 32: 5 / 5; 48: 2 / 2; 64: 1 / 2). **Criterion 3 (report):** `fordLength` 256 → 0 of 32 seeds with locked buildings in both profiles (sum 0); ford cells max 621 / 627 of 262 144 tiles; `style_render` time 6.4 s (m06) and 7.0 s (m19), same as at 32 (6.4 / 6.9 s) and 64. **Criterion 4: met:** 16 core tests (ford tests switched to Shore, numbers unchanged); both G1 probes PASS; Seed 1729 footprint unchanged.

**Not verified:** ford appearance in GUI (sandy shoal, not bridge); game walking (assertion concerns blocking flag from client data, not movement; adjacent water tiles get sand edges); how wide a ford makes sense (at 256 very long shoals across the sea are possible, range for user assessment); masks and preservation (Preserve can break a ford); bridges with separate item.

## Iteration 14c — extended ford ends (2026-11-09): PARTIAL, criteria 1–4 met, appearance and game walking await user

Criteria recorded before change ([G4_METRICS.md](G4_METRICS.md), “Iteration 14c”, commit `5091210`; grep confirmed). Change: at `fordLength` > 0, water within Chebyshev distance ≤ 3 of the first and last ford route cell becomes sand (`fordMouthRadius`); center unchanged.

**What is known about user attention.** New probe assertion (on approved tiles, side 250, `lakePermille` 250, `fordLength` 256): 0 ford tiles and 0 land tiles next to it with blocking flag; 3 pockets connected by ford in model are also connected on map (4-neighbor movement). By client flags, ford was passable in 14b already; I read the complaint as too narrow contact with land (corner of edge) and fixed that. If the user checked game walking, I do not know the cause.

**Criteria 1 and 2 (manual tests): met.** River width 3 (20×8): ford 12 sand cells (was 6); contact 4 + 4 (was 2 + 2); one component 148. Grid 20×16 (route in middle): 30 sand cells; contact 7 on wide-land side and 4 on narrow-land side (limited by land, not ford). Lake width 4, range 4: 16 sand cells. **Criterion 3:** probe `--surface` PASS (180 ford tiles, 0 blocking, 0 disconnected). **Criterion 4: met:** seeds with locked buildings identical to 14b (range 8/16/32/48/64/256: m06 9/9/5/2/1/0, m19 9/9/5/2/2/0 out of 32); ford cells grow (median range 48: 182, max 624; range 256: median 256–272, max 930); time unchanged (6.4–7.6 s); two runs byte-identical; 16 core tests and both G1 probes PASS; Seed 1729 footprint unchanged.

**Not verified:** appearance (larger sandy patches at ends may look unnatural; radius 3 is my choice); game walking; whether 4–7-tile contact is enough in user assessment; masks and preservation; bridges with item.

## Iteration 14d — more natural ford shape (2026-11-09): PARTIAL, criteria 1–4 met, “natural” awaits user assessment

Criteria recorded before change ([G4_METRICS.md](G4_METRICS.md), “Iteration 14d”, commit `776a3f7`; grep confirmed). Change (routes ≥ 8 water cells only; shorter like 14c): ford axis with transverse jitter up to 4 cells (0 at ends, noise from seed and absolute coordinates); width 3 or 5 varying every ~8 cells; mouth radius 3 at ends; safeguard: if axis does not connect ends through passable cells, a straight route is added. `placeFords` takes seed and area start (`FordShape`).

**Criterion 1: met** (manual tests 14c unchanged). **Criterion 2: met** (test on water 64×24, route 48 cells): one component through ford; same seed gives same ford, different seed different shape; only water converted to sand; in middle third each column has 3 to 9 sand cells; average sand row changes by ≥ 2 rows (shape not a straight line). **Criterion 3: met:** probe `--surface` with larger lakes (`lakePermille` 450, 443 ford tiles, 7 pockets): 0 blocking ford tiles and next to it; all pockets connected on approved map. **Criterion 4: met:** seeds with locked buildings identical to 14c (range 8/16/32/48/64/256: m06 9/9/5/2/1/0, m19 9/9/5/2/2/0 out of 32); ford cells: median at range 48: 180; at 256: 287–306, max 1059; generation time unchanged (6.4–7.0 s); two runs byte-identical; 16 core tests and both G1 probes PASS; Seed 1729 footprint unchanged.

**Not verified:** whether ford looks natural (user assesses; constants 4, 8, 40,000, widths 3/5 are my choices); how many fords in probe and measurement had route ≥ 8 cells and how many times the straight-route safeguard triggered (not counted); game walking (probe checks blocking flags); masks and preservation; bridges with item.

**User assessment after 14d (2026-11-09, GUI): “Yes, that's good”.** Natural ford shape confirmed visually (general assessment, no numbers; game walking not checked). Still open: bridges with item, rock passes, region graph and POI, seed rejection, fords for masks and preservation.

## Measurement before passes (2026-11-09): in this profile rock is passable

Probe `--surface` (side 250, `lakePermille` 450, profile `grass/dirt/rocky ground/sand/sea`, client 10.98) counts blocking flag of tiles after Commit by cell kind (row `KIND_BLOCKING`): **Rock 19 of 4983 blocking (0.4 %)**, Shore 0 of 7705, Grass 116 of 17 293 (0.7 %); example rock tile: ground `gravel` with elements `rocks`, `grass`. Conclusion: rock from “rocky ground” brush **is not obstacle**, so “pass” solves no reachability cut (model D22 for Rock, Shore and Grass confirmed on this profile, except single tiles; cause of 19 + 116 blocking tiles not investigated). Pass could only be path route through rock (today `PathRouter` does not cross Rock) or would require rock to block (mountain walls, beyond generator scope).

## Iteration 15 — paths through rock (2026-11-09): PARTIAL, criterion 2 NOT MET

Criteria recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 15”, commit `eae361d`; grep confirmed). Implemented `SurfaceTuning::rockPasses` (0/1, appended to struct, not part of style profile): when 1 `PathRouter` can enter Rock cells for additional 40 (constant `passCrossCost`), Rock cells under route and border become Path; “Rock passes” field in generator window (active when “Path length” > 0); plan snapshot with `rockPasses` only when set; `style_render` has row `cut` (Path cells with Rock within distance ≤ 3 on both sides in one axis, per 65 536).

**Criterion 1: met** (`rockPasses` 0: 16 core tests; Seed 1729 footprint unchanged). **Criterion 3: met** (two runs byte-identical; both G1 probes PASS; probe `--surface`: 443 ford tiles, 0 blocking, 7 connected pockets).

**Criterion 2: NOT met.** Seeds 1001–1032, side 512, profile v10, `pathLength` 40 / 80, `buildingDensity` 30 / 40:

| `rockPasses`, cost | m06: `cut` median (max), seeds with `cut` > 0 | m19: `cut` median (max), seeds with `cut` > 0 | L median m06 / m19 |
| --- | ---: | ---: | ---: |
| 0 | 0 (0), 0 of 32 | 0 (1), 1 of 32 | 166 / 295 |
| **1, cost 40 (recorded)** | 0 (6), 3 of 32 | 0 (15), 8 of 32 | 162 / 296 |

Required: median ≥ 2× larger than at 0 and `cut` > 0 in ≥ 24 of 32 seeds; result 3 and 8 of 32. **Additional measurement after the fact (not recorded):** cost 15 → 5 and 9 of 32; cost 0 (rock free) → 13 and 22 of 32 (median `cut` m19 9). Even free rock passage does not give 24 of 32, so **cost is not the bottleneck**: routes connect random land points and rarely require passage through a rock band. Route length (L) no regression (162 / 296 vs. 166 / 295).

**Conclusion.** Function works and breaks nothing, but in these seeds rock passes are barely visible; to see them, routes would have to deliberately connect areas separated by rock (change to route endpoint selection, separate iteration). GUI appearance not checked. **Not verified:** appearance; blocking of Path tiles on rock (probe does not touch them because `rockPasses` is off in probe); measurement in references (deviation described in record); option 2 (rock blocking with passes): not started.

## Iteration 16 — path targets on far side of rock (2026-11-09): PARTIAL, criterion 1 NOT MET

Criteria recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 16”, commit `f258df4`; grep confirmed). Implemented in `drawPaths` (only `rockPasses` = 1): every other attempt seeks start and target “through rock” (up to 64 random land cells as start, from each up to 8 random directions, up to 48 steps; sequence 3–30 Rock cells without water, then land, target 4 steps farther); without success old random target. At `rockPasses` 0 random stream and result unchanged.

**Discovery during diagnosis:** at `pathLength` 40, the target length (160 cells on 512×512) is already filled by the first route, so generator draws only **1–2 routes on map**; “every other attempt” in first version (one start attempt) gave 4 and 6 of 32 seeds; searching start among 64 cells raised it to 10 and 10.

**Criterion 3: met** (new test `surface` `rockPasses`: deterministic, changes result, value 2 rejected; 16 core tests, Seed 1729 footprint unchanged; two runs byte-identical; both G1 probes PASS). **Criterion 2: met:** L (median) 159 / 307 within limits 249 / 443.

**Criterion 1: NOT met** (seeds 1001–1032, side 512, profile v10, rock cost 40 recorded):

| | m06 `cut` median (max), seeds with `cut` > 0 | m19 `cut` median (max), seeds with `cut` > 0 |
| --- | ---: | ---: |
| `rockPasses` 0 | 0 (0), 0 of 32 | 0 (1), 1 of 32 |
| `rockPasses` 1, iteration 15 (random targets) | 0 (6), 3 of 32 | 0 (15), 8 of 32 |
| **`rockPasses` 1, iteration 16** | **0 (16), 10 of 32** | **0 (25), 10 of 32** |

Required: median ≥ 2× and `cut` > 0 in ≥ 24 of 32 seeds; is 10 of 32. **Additional measurement after the fact (not recorded):** rock cost 15 → 13 and 13 of 32; cost 0 → 21 of 32 (median `cut` 3) in m06 and 27 of 32 (median 8) in m19; L 153–304. At recorded cost 40, A* prefers detour over 3–30 rock cells, so with chosen targets routes often detour anyway. Conclusion: bottleneck is now **rock cost and that map gets only 1–2 routes**, not targets; 75 % criterion would require cost near 0 (m19 yes, m06 not yet) or forcing passage through rock, which changes route character.

**Not verified:** GUI appearance (I don't know whether rock passage looks like a mountain pass or like a cut corridor 3–4 tiles); blocking of Path tiles on rock; measurement in references; option 2 (rock blocking).

## Iteration 17 — paths reach target and cross river (2026-11-09): PARTIAL, criteria for measure 1 and 2 NOT MET, but measure proved unreliable

Criteria recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 17”, commit `610853c`; grep confirmed). Implemented: 17a `drawPaths` stamps to actual start and target at `rockPasses` 1; 17b at `fordLength` > 0 router (paths and door routes) passes through River cells (cost +25); water under route becomes sand (Shore, with mask so later path does not make it Path), only when route actually crosses river. Answer to (2): “Path brush” is list of all ground brushes; `dirt` can be chosen today.

**Bug found on images, not in metric (from iteration 12h):** `route_` runs from target to start, and `reachEnds` connected `start` with start of `smooth_` (target end) and end of `smooth_` with target, i.e., **crossed over**. At short door routes (≤ 16) invisible; at long paths gives long straight diagonal lines and loops (confirmed by render of seed 1003 before and after fix). Fixed (`line(target, start of smooth_)`, `line(end of smooth_, start)`). **Side effect: changes results at `buildingDensity` > 0** (door routes have correct ends); Seed 1729 footprint (`buildingDensity` 0) unchanged; seed counts with locked buildings unchanged; sums slightly different (e.g., range 8: 71 / 90 instead of 69 / 82).

**Measurement** (seeds 1001–1032, side 512, profile v10; seeds with measure > 0 out of 32, m06 / m19):

| setting | `tipRock` > 0 | `tipWater` > 0 |
| --- | ---: | ---: |
| `rockPasses` 0, `fordLength` 0 | 0 / 1 | 2 / 4 |
| `rockPasses` 1, `fordLength` 0 | 16 / 24 | 10 / 17 |
| `rockPasses` 0, `fordLength` 48 | 2 / 0 | 1 / 4 |
| `rockPasses` 1, `fordLength` 48 | 13 / 23 | 4 / 6 |

**Criterion 1 (`tipRock` > 0 in ≤ 3 of 32 seeds at `rockPasses` 1): NOT met** (16 and 24). **Criterion 2 (`tipWater` half smaller at `fordLength` 48): NOT met for `rockPasses` 0** (2 → 1, 4 → 4); met only at `rockPasses` 1 (10 → 4, 17 → 6; this result recorded as additional, not from record). **Conclusion on measure:** “tip” (Path cell with ≤ 3 Path neighbors) also counts step-like edges of wide path in rock and ends near border; grows when path crosses rock; does not distinguish “broken in rock” from “properly crosses”. **I do not treat it as proof**; images and user assessment decide. Criterion 3: met (16 core tests; both G1 probes PASS; probe `--surface` still 0 blocking ford tiles and next to it; pockets connected; two runs byte-identical). Seeds with locked buildings for `fordLength` 8/16/32/48/64/256: m06 9/9/5/2/1/0, m19 9/9/5/2/2/0 (unchanged).

**Not verified:** whether path through river looks good and crosses to far side in all cases (checked only render of 2 seeds in kind colors, not GUI); criterion (1) outside measure; blocking of sand tiles under river path (probe does not cover them); jagged edges of sand and grass (point 4 of notes, separate diagnosis); effect of endpoint fix on door routes in GUI.

## Iteration 18 — sand / grass edge smoothing (2026-11-09): PARTIAL, criterion 2 (5×) NOT MET; rule changed after measurement

Criteria recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 18”, commit `e8be5a2`; grep confirmed). Implemented `SurfaceTuning::shoreSmoothing` (0..4 passes, appended at end, not part of style profile; 0 = byte-for-byte as before); `smoothShores` in core (majority filter; call after beach rings, before `grassRim`); “Smooth beaches” checkbox in generator window (default checked = 2 passes); measure `shore` in `style_render`.

**Result of rule recorded upfront (5×5 window, cells ≤ 3 from lake), seeds 1001–1032, side 512, m06 / m19, `shoreSmoothing` 2 vs. 0:** (`spurs` + `notches` + `checker`) median 18 → 21 / 22 (did not drop); `boundary` 627 → 585 (−7 %); `sand` 2990 → 3069 (+3 %). **Criterion 2 NOT met** (required ≥ 5× fewer). Baseline is low (18 of 65 536 cells), so measure does not show “raggedness” visible on screenshot; it is not lone cells but step-like boundary.

**Rule change after measurement (deviation from record, explicit):** I tried 7×7 window and range ≤ 9 from lake (beyond beach rings to `shoreWidth` 8) and adopted that rule; choice after seeing results, not from record. Result (`shoreSmoothing` 2 vs. 0): (`spurs` + `notches` + `checker`) 18 → 9 / 10 (2× fewer); `boundary` 627 → 486 / 485 (−22 %); `sand` 2990 → 3012 / 2987 → 3016 (+0.7 %); 4 passes give 9 and 464. **Criterion 2 still NOT met** (2× instead of 5×); `boundary` not greater and `sand` in ±15 % met.

**Criterion 1: met** (`shoreSmoothing` 0: Seed 1729 footprint and 16 core tests unchanged). **Criterion 3: met** (new test `testSmoothShores`; manual numbers for 7×7 rule: strip unchanged, hole filled, lone sand → grass / dirt; ring 0 stays; deterministic; 0 passes unchanged). **Criterion 4:** 16 core tests and both G1 probes PASS after rebuilding `rme`; two runs byte-identical (G1 probes run at `shoreSmoothing` 0, so do not cover smoothed result).

**Not verified:** whether edge looks good (user assessment; I cannot assess from measure); effect on style profiles (beach composition in profiles counted only via `sand`); grass / dirt and rock / grass boundary (not smoothed); smoothing on approved map in probe; whether GUI screenshot shows same effect (images from `style_render` too small to assess).

## Iteration 19 — narrow sand spits between water (2026-11-09): PARTIAL, criteria 1–4 met, appearance awaits user

Criteria recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 19”, commit `d82b1df`; grep confirmed). **Diagnosis after four GUI screenshots after iteration 18:** (a) narrow sand spits and peninsulas 1–2 tiles wide between water (river cuts beach ring overlaid before it); filter from 18 did not touch them because it counted Lake as sand and did not see rivers; (b) rectangular grass notches into sand (90° steps on straight sections). Measurement on approved map (row `SAND_GRASS_EDGE`): sand / grass edge **does have** bordering elements (1760 of 1760 sand tiles near grass; grass 106), so lack of bordering is not the cause.

Implemented `trimSpits` (only at `shoreSmoothing` > 0, after river overlay): Shore cell in horizontal or vertical sequence ≤ 2 Shore cells bounded at both ends by water becomes water (River if either end is River, else Lake); measure `spit` in `style_render`.

**Criterion 1: met** (`shoreSmoothing` 0: 16 core tests; Seed 1729 footprint unchanged). **Criterion 2: met:** `spit` (median of 65 536 cells, seeds 1001–1032, side 512, m06 / m19, `shoreSmoothing` 2 vs. 0) **10 → 0** (max 19 / 20 → 1 / 2); baseline > 2 so criterion applies; `sand` 2990 → 3001 and 2987 → 3002 (+0.4–0.5 %); `boundary` 627 → 485 (−23 %). **Criterion 3: met** (new test `testTrimSpits`: width-2 strip vanishes, width-3 strip stays; single water square vanishes; sequence with grass on one end stays; sequence between river and lake becomes river; 20 + 10 = 30 river cells). **Criterion 4: met:** 16 core tests and both G1 probes PASS after rebuilding `rme`; two runs byte-identical.

**Cost:** cut sand spits increase count of locked buildings. Seeds with locked buildings / sum (m06, m19; `shoreSmoothing` 0 → 2): `fordLength` 0: 10 / 121 → 11 / 132 and 10 / 159 → 11 / 180; `fordLength` 48: 2 / 2 → 3 / 4 and 2 / 2 → 2 / 5; `fordLength` 256: 0 → 0 in both. Fords (range ≥ 48) fix almost all.

**Unresolved:** (b) rectangular notches and 90° steps of grass into sand on straight sections (this is boundary shape from noise threshold on tile grid, not noise; removal requires different boundary shape); user appearance assessment; whether screenshot 3 (grass notch) still looks the same (did not change it); G1 probe still does not cover smoothed result (runs at `shoreSmoothing` 0). **Not verified:** appearance on approved map after `trimSpits`.

**User assessment after 19 (2026-11-09, GUI): “Better”.** Edges after removing narrow sand spits look better (general assessment, no numbers). Still open: rectangular grass notches into sand (90° steps); paths through rock and river assessed only by renders; bridges with item; rock blocking with passes; region graph and POI; seed rejection.

**User assessment (2026-11-09, GUI): “Paths look ok”** (paths through rock, through river, and with corrected ends; general assessment, no numbers).

## Iteration 20 — steps and 90° notches at sand / grass boundary (2026-11-09): PARTIAL, criterion 2 NOT MET; disabled by default in window

Criteria recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 20”, commit `efcfa40`; grep confirmed). Implemented `SurfaceTuning::shoreRadius` (0..8, appended at end; 0 = radius 3 as in 19); `smoothShores` with radius parameter (window (2r+1)², range from lake 3r); measure `corners` in `style_render`.

**Criterion 1: met** (`shoreSmoothing` 0 and `shoreRadius` 0: Seed 1729 footprint and 16 core tests unchanged). **Criterion 3: met** (new `testShoreRadius`, 20×15: protrusion depth 1, length 5 stays at r = 3 (e.g., 26 sand of 49); vanishes at r = 5 (60 of 121); straight boundary unchanged at r = 5; r = 0 means 3; deterministic). **Criterion 4: met** (16 core tests and both G1 probes PASS; two runs byte-identical).

**Criterion 2: NOT met.** `corners` (2×2 windows with 1 or 3 sand cells; of 65 536; median; seeds 1001–1032, side 512, m06 / m19; `shoreSmoothing` 2):

| radius | `corners` | `boundary` | `sand` | `style_render` time on 32 seeds |
| --- | ---: | ---: | ---: | ---: |
| (no smoothing) | 573 / 572 | 627 / 626 | 2990 / 2987 | 3.3 s |
| 3 (state from 19) | 460 / 459 | 485 / 484 | 3001 / 3002 | 4.6 s |
| 4 | 435 / 435 | 471 / 469 | 3010 / 3011 | 5.8 s |
| **5** | **415 / 415** | 455 / 453 | 2993 / 3004 | 6.8 s |
| 6 | 404 / 403 | 447 / 447 | 3006 / 3017 | 9.0 s |

Required: −25 % vs. radius 3; is −10 % (radius 5) and −12 % (radius 6); `boundary` and `sand` meet conditions. **Conclusion:** corner count grows with boundary length (corner-to-boundary ratio ≈ 0.9–0.95 at each radius), so this is ordinary tile-grid steps, not separate protrusions; majority filter removes single protrusions (test), but does not change step-like boundary character. Longer time (+2 s at radius 5 on 512×512; measurement from one run of 32 seeds) is cost of range-3r loop around each lake cell.

**Decision:** “Smooth beaches” checkbox in window stays at radius 3 (`shoreRadius` 0); radius 5 gives −10 % corners for +2 s and does not change appearance at scale visible in screenshot. `shoreRadius` available in core and in `style_render`. **Not verified:** whether radius 5 looks better in GUI (not enabled in window, not assessed); effect on appearance of screenshot 3; locked buildings (not measured).

## Iteration 21 — “Generate Terrain” window layout (2026-11-09): implemented, awaits user assessment

Criteria recorded before change ([G4_METRICS.md](G4_METRICS.md), “Iteration 21”, commit `2f078ca`; grep confirmed). Window fields went into `wxNotebook` tabs: **Area**, **Terrain** (with “Smooth beaches”, moved from between fords and rock), **Paths**, **Buildings**, **Vegetation**; preview, help text, status and buttons outside tabs. Controls created on current tab page (`host`), names and defaults unchanged.

**Criterion 1: met** (`rme` builds; 16 core tests and both G1 probes PASS; probes do not open window). **Criterion 3: met by count of `terrain-` names:** 39 before, 40 after (added `terrain-sections` for tabs); none disappeared. **Criteria 2 and 4: not checked**, require GUI assessment (window height on 1080p screen, no clipping, no status overlapping fields, Preview / Validate / Place / Undo behavior and field-enable logic).

**Not verified:** window appearance (native `wxNotebook` in dark theme); keyboard tab switching and mnemonics; whether tabs have sensible height (Terrain has 11 rows, most); whether inactive fields (e.g., weights at Natural surface style) are readable.

**User assessment after 21 (2026-11-09, GUI): “Looks ok”.** Window layout in tabs confirmed visually (criterion 2 met per user; Preview / Validate / Place / Undo behavior not separately described).

## Iteration 22 — POI and fixed start in reachability validator (2026-11-09): PARTIAL, criterion 2 met for `fordLength` 256

Criteria recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 22”, commit `089c45d`; grep confirmed). Implemented `poiReachabilityOf` in `reachability.*`: start = passable cell nearest area center (need not be in largest component); entries = door cells; exits = one per side (passable border cell nearest side center); point reached when in start component. Pure counting function; generator and GUI unchanged; row `poi` in `style_render`. Scope: surface of one floor (exits Z and basement: separate stage; default decision, user did not answer scope question).

**Criterion 1: met.** Manual tests (lake: start (3,2) from tie, 4 exits, 3 reached, start component 20 of 40; building with doors: entry and 4 exits reached; no doors, start in closed room reaches no exit, component 9; side with water no exit; grid with only water no start; empty grid invalid) and comparison with independent implementation (search from start) on 80 random grids in two modes: 160 of 160 matching. **Criterion 3: met** (16 core tests and both G1 probes PASS; Seed 1729 footprint unchanged; two runs byte-identical).

**Criterion 2: met for `fordLength` 256.** Seeds 1001–1032, side 512, profile v10, `buildingDensity` 30 / 40, `pathLength` 40 / 80; seeds with **all** entries and exits reached from start (m06 / m19); sum of unreached entries / exits:

| `fordLength` | m06 | m19 | unreached entries (m06 / m19) | unreached exits (m06 / m19) |
| --- | ---: | ---: | ---: | ---: |
| 0 | 19 of 32 | 20 of 32 | 121 / 159 | 13 / 13 |
| 48 | 28 of 32 | 29 of 32 | 2 / 2 | 3 / 3 |
| **256** | **32 of 32** | **32 of 32** | 0 / 0 | 0 / 0 |

Required: ≥ 29 of 32 in both profiles at 256; is 32 of 32 (at 0 and 48 report without band; at 48 m06 has 28). Smallest share of start component in passable cells: 647 ‰ at 0; 978 ‰ at 48; 999 ‰ at 256.

**Caveats (from record and new):** start “nearest center” is my choice (on real map, start is city or temple, which generator does not create); POI today are only doors and border exits (no ruins, camps, or list points); exits on sides do not know what lies in adjacent areas; **range 256 means very long shoals across sea**, so criterion met costs appearance (assessed earlier only at 48); no repair or seed rejection yet (next plan point); validator not connected to window; passability model from D22. **Not verified:** Z exits and basement; floor other than 7 (core is floor-independent, not tested); POI in window or RME probe.

## Iteration 23 — reachability repair and seed rejection (2026-11-09): PARTIAL, criteria 1–4 met, repair appearance awaits user

Criteria recorded before implementation ([G4_METRICS.md](G4_METRICS.md), “Iteration 23”, commit `a47cec8`; grep confirmed). Implemented: `SurfaceTuning::repairReach` (0/1, appended to end, not part of style profile; 0 = byte-for-byte as before) and repair mode `placeFords` (`FordShape::repair`): after regular fords each component containing entry or exit and different from start component gets ford through water up to 256 cells (start counted once, regardless of component size, shape and mouth as in 14d); `generateReachableSurface` (seed rejection: attempts `seed`, `seed + 1`, … to `maxAttempts`, first grid with all POI reached, else first attempt with `allReached` = false); shared `fixedStartOf` and `exitCellsOf` in `reachability.*`; “Repair unreachable places” field in Paths tab (default checked); `repairReach` in plan snapshot only when set.

**Criterion 1: met** (`repairReach` 0: Seed 1729 footprint and 16 core tests unchanged). **Criterion 2: met** (manual tests 24×9: river width 6; normal ford range 3 does nothing; repair gives 24 sand cells and 30 river; after it entry and 4 exits reached; deterministic; island without doors and exits stays alone; 2-cell room with doors in water gets ford; test `reachableSurface`: first attempt; seed arithmetic; determinism; `maxAttempts` 1 returns first grid with `allReached` = false; `maxAttempts` 0 rejected). **Criterion 4: met** (16 core tests and both G1 probes PASS after rebuilding `rme`; two runs byte-identical).

**Criterion 3: met.** Seeds 1001–1032, side 512, profile v10, `buildingDensity` 30 / 40, `pathLength` 40 / 80 (rejection attempts counted on seeds 1001–1039; `maxAttempts` 8); all POI reached from start; m06 / m19:

| `fordLength`, `repairReach` | all POI reached | first attempt fails | average attempts |
| --- | ---: | ---: | ---: |
| 0, 0 | 19 / 20 of 32 | 13 / 12 | 1.53 / 1.50 |
| 48, 0 | 28 / 29 of 32 | 4 / 3 | 1.12 / 1.09 |
| **48, 1** | **32 / 32 of 32** | **0 / 0** | **1.00 / 1.00** |
| 0, 1 | 32 / 32 of 32 | 0 / 0 | 1.00 / 1.00 |
| 256, 0 | 32 / 32 of 32 | 0 / 0 | 1.00 / 1.00 |

Required: ≥ 31 of 32 in both profiles at `fordLength` 48 with repair; is 32 of 32. Repair works without normal fords too (`fordLength` 0). **Caveat on measure “ford cells”:** row `reach` counts difference vs. same grid at `fordLength` 0, but since iteration 17 paths cross rivers only at `fordLength` > 0, so difference also includes route change (median 13,495–13,735 in m19), not only fords; for repair without normal fords is 0 (difference does not see repair). Comparison “no more than at 256 alone” met only formally (median m06 571 vs. 674, m19 13,495 vs. 13,735; max 25,457 vs. 25,850); **I do not treat it as proof**.

**Not verified:** appearance of repair fords (range to 256 gives very long shoals; in GUI you assessed only fords to 48); seed rejection in window (function in core only); effect of masks and preservation (Preserve can break a ford); Z exits and basement; generation time (`style_render` on 39 seeds: 8.6–9.2 s with repair vs. 8.1–8.7 s without, but single measurement).

**User assessment after 23 (2026-11-09, GUI): “Looks good”.** Repair fords (up to 256 cells) look good per user (general assessment, no numbers; game walking not checked).
