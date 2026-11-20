# G4-F — metrics and criteria set BEFORE evaluation on test set, 2026-11-08

**Status: recorded, not yet run on test set.** Until this file was committed, no values were computed or viewed on test maps (test family of each fold). Training and validation values from G4-C/D were viewed. **Leak caveat:** test family of fold k is validation family of fold k−1, so I saw fitting distances from fold k−1 to test family target of fold k (G4_FIT.md, G4_COVERAGE.md, G4_DECOR.md). Fold k tuning, which will be evaluated, was not measured on that target, but test target is not fully “unseen”; this weakens test independence and enters result caveats. Change of any rule below after running on test requires separate entry and marking result as “after criterion change”.

## Comparison
- **Baseline (manual presets):** default `SurfaceTuning` and default `DecorTuning`.
- **Candidate:** `SurfaceTuning` and `DecorTuning` from `fold-N.profile` (learned on training, step chosen after validation).
- Folds from `reference_split` (default parameters), client 10.98, map `accepted-10.98-v2`, surface only (z ≤ 7). Fold is **evaluated** if it has test target (at least one test map with ≥ 1000 natural tiles) and training target. Others reported as N/A, not as improvement or degradation.
- Generation: side 256, seeds **1001–1032** (32 seeds; disjoint from fitting seeds 1–6), no mask, vegetation enabled, same seed for baseline and candidate (pair).

## Style metrics (measure: smaller = better)
1. **S1 shares:** L1 (ppm) between shares of 5 natural types in result (sum across seeds) and shares of test target.
2. **S2 adjacency:** L1 (ppm) of 5×5 adjacency matrix of result vs. test target.
3. **S3 vegetation:** `decorDistance` (average of 24 stakes differences in ‰ across contexts where test target has ≥ 200 tiles); fold without comparable context = N/A for S3.
Test target counted like training targets: `buildTarget` / `buildDecorTarget` (families equally, maps in family equally, minimum 1000 natural tiles).

## Critical playability metric
4. **P1 walkability coherence:** share of largest 4-connected component of walkable tiles (Grass, Dirt, Shore) among all walkable tiles of result, separately for each seed (in ppm). No walkable tiles = N/A for that seed.
Remaining playability conditions from plan (100 % correct IDs, zero changes outside mask, border_error_rate) do not depend on tuning values (profile changes only numbers within range validated by generator, commit path control not changed), so **not measured by this evaluator**; this is assumption to verify by full pipeline test in GUI (step G4-GUI).

## Aggregation and uncertainty
- For each evaluated fold and metric: pairwise difference (candidate − baseline) for each of 32 seeds; reported: median, 10th and 90th percentile of difference, count of seeds where candidate is better.
- Fold is “better” in metric if median difference < 0 **and** candidate better in ≥ 24 of 32 seeds (75 %). Fold is “worse” if median > 0 and candidate worse in ≥ 24 of 32. Otherwise “undecided”.
- Comparisons across folds: count of better / undecided / worse folds and median span. No single summary number; folds have varying map counts (1–2), so not averaging them into one value.

## G4 criterion (style and playability part)
G4 is **met in measured scope** if simultaneously:
- for S1 and S2: better is at least 2/3 of evaluated folds and no fold is “worse” in both;
- S3: more better folds than worse (among those with comparable context);
- P1: in each evaluated fold median P1 of candidate ≥ 95 % of median P1 baseline.
Otherwise result is **not met** (with count of which conditions failed), not “partially met”. Data constraints (one to two maps per fold, 3 maps with vegetation) I enter as caveat to result regardless of direction.

## What this evaluation does not cover
Visual similarity, naturalness of rivers and mountains, clustering at scales 16/32/64, human judgments, dungeons, BlackTek client, context rules beyond vegetation, and test of generator output memorization vs. reference.

## Iteration 2 (recorded 2026-11-08 after viewing iteration 1 result; before its execution)

**What changes: model, not metrics or gate criterion.** Metrics S1, S2, S3, P1, seeds 1001–1032, aggregation, “better/worse” rule, G4 criterion and fold set are exactly those from iteration 1. Changing `SurfaceTuning` fitting:
- **Land coherence constraint in fitting:** candidate (one parameter value in search) is rejected if median land coherence P1 on fitting seeds (1–6, side 160) is lower than **97 %** of median P1 of manual (default) parameters on same seeds. Margin 97 % (instead of 95 % from criterion) chosen ahead to leave buffer for side and seed change. Rejected candidate does not participate in selection; start (manual parameters) always meets constraint.
- Training target, step selection after validation, and vegetation fitting unchanged.

**Caveats this iteration inherits and deepens:**
- Iteration 2 designed after viewing iteration 1 results on same test folds. Test folds thus used second time and result is not independent test in G4 sense; this is evaluation after model adaptation to known weaknesses. Result marked “after model change” and does not replace iteration 1, just sits alongside.
- Test seeds same, test maps same; difference concerns tuning only.
- If candidate fails constraint in any step, fold profile is identical to manual parameters (result “undecided”), which is allowed and honest result.

## Iteration 3 — per-style profiles (recorded 2026-11-08, before its execution)

**What changes:** instead of one profile learned on family blend, one profile per style. **Data do not permit evaluation on unseen maps except one case** (see below), so this iteration has three parts of varying evidential power and does not replace G4 criterion from plan.

**Definitions.**
- *Style* = family from G4-A split (declared ∪ by content), having at least one style map with target (≥ 1000 natural tiles on z ≤ 7; `buildTarget`). 8 styles with targets (m01, m05, m06, winter, caves, m15, m19, swamp); m17 has no target.
- *Style profile* = `SurfaceTuning` fitted to style target with land coherence constraint 97 % (as in iteration 2; train = validation = style, final step result) and `DecorTuning` from fitting closed on style contexts (contexts below 200 tiles embedded).
- Fitting: seeds 1–6, side 160. Evaluation: seeds 1001–1032, side 256, seed pairs as before.

**Part A — own style mapping (in-sample).** For each style: style profile vs. baseline (manual parameters) in metrics S1, S2, S3 and P1 relative to target **of same style**. “Better/worse” rule and S1–S3, P1 definitions as in iteration 1. *This is evaluation on data the profile came from.*

**Part B — map excluded from learning within style (leave-one-map-out).** For styles with at least two maps with targets: profile fitted on remaining maps of style, evaluation on excluded map, vs. baseline. From current data only **swamp (M12, M13): two cases** qualify. Result reported separately, without numeric threshold (n = 2 insufficient for generalization conclusion).

**Part C — style distinguishability.** Matrix: for each pair (style profile i, style target j) median across 32 seeds of S1 + S2. Style j is *recognized* if its own profile has smallest median among all 8 profiles. Also reported baseline as extra row (not candidate for recognition).

**Iteration 3 criterion (all simultaneously):**
- A: profile better than baseline in both S1 and S2 in at least 2/3 of styles, and no style worse in both;
- C: at least 2/3 of styles recognized;
- P1: in each style profile (A and B) median P1 ≥ 95 % of median baseline;
- B: no threshold; if any of two cases is “worse” in S1 and S2 together, I record it as failure.

**Interpretation ahead:** meeting A, C and P1 means only that per-style profiles map styles they came from, without loss of land coherence. **Does not** prove improvement on unseen maps and **does not** meet G4 gate from plan; that remains evaluated by part B, which with n = 2 settles nothing. Result marked “after model change”, test folds from iterations 1–2 remain used.

## Iteration 4 — river shape (D9 water part, D6), recorded 2026-11-08 before implementation and before any river measurement

**Scope.** D9 for water bodies only: rivers (length, width, sinuosity, straight segments) and lake sizes. Still out of scope (D9 debt remains): bifurcations, caverns (chambers, corridors), mountains (walls to plateau), paths, settlements, microstructures, narrow passages, floors. D6: generator rivers run along axes.

**Definitions (constants, set beforehand).**
- Water = Water kind (lake, river, sea; without Shore) at z ≤ 7. In references via accepted mapping, in generator as Lake and River.
- Water component = 4-connected component, per floor. *Skeleton* = Zhang-Suen skeletonization (standard, two passes, 8-neighborhood) of the component.
- *River component*: area ≥ 40 tiles, skeleton length L ≥ 20 tiles, mean width area / L ≤ 7. Other components: lakes (area < 20,000) and sea (area ≥ 20,000; omitted).
- Features of river component: tortuosity T = 1000 · L / max(bounding box side) in ‰; proportion of straight segments S = ‰ of skeleton tiles lying in a sequence ≥ 12 consecutive tiles in one row or column; width W = 1000 · area / L (thousandths of a tile).
- Reference value of feature = median across all river components of maps (aggregate). Generated value for seed = median across river components of that seed; seed without river component is reported as “no river” and does not enter comparisons.
- Reference map *qualifies* if it has ≥ 5 river components. If there are fewer than two such maps, the D6 assessment is reported as impossible, not passed.

**Model change.** New parameter `SurfaceTuning::meander` (0..8 tiles, default 0 = current result unchanged byte for byte): transverse shift of the river route center line by smooth noise along the route, zero at ends and near tributary mouths. Tuning `meander` from grid {0,1,2,3,4,5,6,8}: minimizes |T_gen − T_ref| + |S_gen − S_ref| on seeds 1–6, side 160.

**Evaluation (two cross-folds: tuning on one qualifying map, evaluation on the other).** Seeds 1001–1032, side 256, baseline (meander 0) / candidate pairs; distance difference |gen − ref| for T and for S; “better/worse” rule as in iteration 1 (median difference and ≥ 75% of seeds).

**D6 criterion (all simultaneously, in both folds):**
1. candidate better than baseline in T and in S;
2. walking connectivity P1 candidate median ≥ 95% of baseline median;
3. generator invariant: each river cell is 4-connected via river cells to a lake or area edge, in 100% of seeds (river does not become an island).
Failure to meet any = D6 incomplete, with enumeration. Assessment in objective features does not replace visual assessment (blind A/B is a separate step).

**Caveats set beforehand:** evidence from at most a few references (probably M06 and M19), shape measurement is grid skeleton (approximation), threshold “straight segment ≥ 12” and river component definition selected before measurement, but without knowledge of their sensitivity.

## Iteration 5 — water channels along with rivers connected to sea (recorded 2026-11-08 before implementation and before measurement)

**Reason.** Iteration 4 metric omitted water components ≥ 20,000 tiles (sea), so large rivers connected to sea did not enter the reference ([G4_COMPARE.md](G4_COMPARE.md)). Iteration 5 measures *channels*: narrow parts of water regardless of whether they connect to sea. First, baseline measurement of reference and generator (5a), then possible generator change (5b) evaluated with the same criteria.

**Definitions (constants, set beforehand).**
- Water = Water kind (lake, river, sea; without Shore) at z ≤ 7, per floor. In references via accepted mapping v3, in generator as Lake and River.
- *Open water* = morphological opening of water mask with 13×13 square (erosion, then dilation; tiles outside area count as no water). *Channel* = water minus open water (narrow parts, width ≤ 12). Square favors axes; this is an approximation.
- *Channel component*: 4-connected component of channel; counted if area ≥ 40 tiles and skeleton length (Zhang-Suen, as in iteration 4) L ≥ 20.
- Features (on counted components, aggregate per map or seed): **L** = sum of skeleton lengths per 65,536 tiles of area (tiles present in reference; area cells in generator); **W** = 1000 · area / L (thousandths of a tile); **B** = number of skeleton branches (skeleton tile with ≥ 3 skeleton neighbors in 8-neighborhood) per 100 skeleton tiles; **C** = proportion of channel components that touch open water or area edge, in ‰.
- Reference map *qualifies* if it has ≥ 5 counted components. Without at least two such maps, evaluation is impossible, not passed.

**Baseline measurement (5a).** References: values L, W, B, C of each qualifying map (no averaging outside range). Generator: profiles m06 and m19, seeds 1001–1032, side 256, median across seeds (seed without channel component: “no channel”, counted separately).

**Criterion (for 5a and 5b, the same).** For style profile X with reference M_X (m06→M06, m19→M19; this is in-sample tuning, not test on unseen map): feature is *in band* if generator median lies in [0.67·ref; 1.5·ref]. Profile “passed” if ≥ 3 of 4 features are in band and seeds with channel ≥ 75%. Result 5a is status description, not gate. Generator change 5b is passed if both profiles are passed and the following remain met: P1 (walking connectivity) median ≥ 95% of baseline and invariant “each river cell reaches a lake or edge” in 100% of seeds (if 5b changes this invariant, I record the new one before change).

**Caveats set beforehand.** Two references with rivers (M06, M19), threshold 13×13 and band thresholds selected without knowledge of values, channel is an approximation (small lakes ≤ 12 width and bays will also enter measurement), no visual assessment. If 5a shows that channel definition does not separate rivers from bays in the section I examined, I report it and change the definition **before** 5b with recording in this file.

## Iteration 6 — large-scale structures (recorded 2026-11-08 before implementation and before measurement)

**Reason.** Visual comparison ([G4_COMPARE.md](G4_COMPARE.md)): generator produces uniform mosaic at one scale, references have large masses of land, sea, and rock. Current G4 measure (proportions, 1-distance neighborhood) does not describe this.

**Definitions (constants, set beforehand).**
- Natural kinds: Grass, Dirt, Rock, Shore, Water (in generator Lake and River together as Water). Cells of other kind (Other, Unmapped, NoGround, no tile, Preserve) take part in no pair.
- *Concordance at distance d*: ‰ of cell pairs (x, y) and (x+d, y) and (x, y) and (x, y+d), both natural, of the same kind, d ∈ {4, 16, 64}; computed on floors 0..7 aggregate for reference and on one area for generator seed.
- *Random concordance* A0 = Σ_k s_k² ‰, where s_k is the proportion of kind k among natural cells. *Excess* E_d = concordance(d) − A0, in ‰ (difference in concordance above random layout with the same proportions), so measure does not depend on ground proportions alone.
- Reference = value E_d of map; generator = median E_d across seeds.

**Baseline measurement (6a).** Profiles m06 and m19, seeds 1001–1032, side 256, profile v7; references M06 and M19 (m06→M06, m19→M19, in-sample as in iteration 5).

**Criterion (for 6a and 6b).** Features E_16 and E_64 *in band* if generator median lies in [0.67·ref; 1.5·ref]. Profile “passed” if both E_16 and E_64 are in band. E_4 I report without assessment (check that fine scale does not break: median E_4 of generator in band as above is an additional condition for 6b).

**Model change (6b, only if 6a shows gap).** Tuning `SurfaceTuning::featureSize` from expanded grid {24, 32, 48, 64, 96, 128, 192, 256} on seeds 1–6, minimizing |E_16 − ref| + |E_64 − ref| (rest of tuning unchanged). Evaluation on seeds 1001–1032. If feature size change alone is not enough, further steps (e.g., additional noise scale) I record here separately before execution. Additional conditions 6b: P1 (walking connectivity) median ≥ 95% of baseline, river invariant in 100% of seeds.

**Caveats set beforehand.** Two references, in-sample tuning, no visual assessment (concordance at lag does not distinguish “one large land” from “many large patches”), references are large maps with human elements (outside natural cells), and generator window (256) cuts lag 64 at edge. If 6a shows that reference values depend strongly on chosen map fragment, I report it and change definition before 6b.

## Iteration 7 — dirt patches and beach width (recorded 2026-11-08 before implementation and before measurement)

**Reason.** User observation in GUI (profile v8, area 500×500): dirt in grass is a jagged “dotted” patch instead of solid patches; beaches are thin (in reference M19 wide). User requested improvement of both points.

**Definitions (constants, set beforehand).** Kinds as in iteration 6 (natural cells: Grass, Dirt, Rock, Shore, Water; rest omitted; Lake and River as Water).
- **D — dirt boundary density**: 1000 · (number of 4-adjacent Dirt–Grass cell pairs) / (number of Dirt cells), aggregate. Large D = ragged, jagged dirt.
- **F — dirt compactness**: ‰ of Dirt cells lying in 4-connected components of Dirt with area ≥ 100.
- **P — beach thickness**: 1000 · (number of Shore cells) / (number of Shore cells with 4-neighbor Water). 1000 = beach of one-cell thickness.
- Feature N/A if denominator is small: Dirt < 1000 cells (D, F), Shore < 1000 cells (P).
- Reference = aggregate value of map (floors 0..7, cells present); generator = median across seeds.

**Baseline measurement (7a).** Profiles m06 and m19 in version v8 (featureSize 192), seeds 1001–1032, **side 512** (closer to GUI use and consistent with observation from iteration 6 that side 256 cuts off scale); references M06 and M19 (m06→M06, m19→M19; in-sample).

**Criterion (for 7a and 7b).** Features D, F, P (if not N/A) *in band* when generator median lies in [0.67·ref; 1.5·ref]. Profile “passed” if all non-N/A features are in band. Additional conditions for 7b: (1) proportion of each of 5 kinds in generator median differs from baseline (7a) by at most 5 percentage points; (2) river invariant in 100% of seeds; (3) P1 (walking connectivity) median ≥ 95% of baseline, if P1 measurement tool is available for side 512, and if not, I report it as unchecked.

**Model change (7b, only if 7a shows gap).** Tuning `transitionWidth` from grid {0, 500, 1000, 2000, 4000, 8000, 16000} and `shoreWidth` from grid {0..8} (raising validation upper limit from 4 to 8, outer rings as before drawn by noise), separately for each profile, on seeds 1–4 side 512, minimizing sum of relative errors |gen − ref| / ref over non-N/A features. Evaluation on seeds 1001–1032. If tuning alone is not enough, further change (e.g., noise smoothing earth edge) I record here separately before execution.

**Caveats set beforehand.** Two references, in-sample; D and F are shape measures, not visual assessment; narrowing grass/dirt transition changes kind neighborhood tuned earlier (condition 1 is proportions only, not neighborhood); “beach” is Shore kind from key→kind mapping (sand), in references sand outside water also enters Shore (e.g., deserts), so P measures only Shore cells at water relative to all.

**Change in P definition (recorded 2026-11-08 after reference measurement 7a, before generator measurement and before any model change).** Reference measurement showed that P from original definition is useless: values 33,795 (M06) and 22,366 (M19), because most Shore cells in references are sand far from water (deserts, sand islands), not beaches. I replace it: **P′ = 1000 · (Shore cells reachable from Shore cells adjacent to Water via 4-adjacent Shore cells in at most 8 steps, including starting cells) / (Shore cells with 4-neighbor Water)**. P′ = 1000 means beach of one-cell thickness. Rest of criterion unchanged (band [0.67·ref; 1.5·ref], N/A for < 1000 reachable cells). Reference values of P′ I will see only after this recording; earlier P remains in description as unusable measurement, not as passed.

## Iteration 7 — results 7a and recording model change 7b (2026-11-08, before its execution)

**7a (values measured before generator change, seeds 1001–1032, side 512, profile v8).** References (M06 / M19): D 421 / 451, F 749 / 625, P′ 5,638 / 5,656. Generator m06: D 112, F 975, P′ 1,987; m19: D 203, F 951, P′ 1,979. In band: m06 1 of 3 (F), m19 0 of 3.

**Observation contrary to earlier diagnosis.** According to numbers, dirt boundary in generator is *smoother* (lower D) and its compactness *greater* (higher F) than in references, which have more small dirt patches. Earlier description “dotted dirt in generator” was based on observation and is not confirmed by D and F metrics; metric does not measure size of individual “dots” (see caveats). Beach is about 2.8 times thinner than in references (P′).

**Model change 7b.** (1) As recorded above: tuning `transitionWidth` from grid {0, 500, 1000, 2000, 4000, 8000, 16000} and `shoreWidth` from grid {0..8}, separately for each profile, on seeds 1–4, side 512, minimizing sum of relative errors over non-N/A features D, F, P′ (coordinate descent: first `transitionWidth`, then `shoreWidth`). (2) Because beach rings above third never form (noise threshold `5 + 30 · ring` percent exceeds 100% from ring 4, and required lake size grows 3×), raising limit `shoreWidth` to 8 alone changes nothing. I therefore change the rule: noise threshold of ring = min(5 + 30 · ring, 80)%, required lake size = min(30 · 3^(ring−1), 270) cells; results for `shoreWidth` ≤ 2 remain byte-identical, and for 3 ring 3 threshold changes from 95% to 80%. Validation limit `shoreWidth` from 4 to 8.

**Additional caveats set beforehand.** Tuning relative to D and F may *increase* dirt raggedness (D grows), which may contradict user's visual assessment; result should go to visual assessment in GUI before repair is considered effective. D and F do not measure number of fine “dots” (components ≤ 4 cells), if such observations prove needed, I add measure separately with recording.

## Iteration 7c — beach (recorded 2026-11-08 after result 7b and after user choice “beach first”, before implementation and measurement)

7b saturated the grids and P′ reached only about 2,315 (goal about 5,640, band 3,777–8,457). Instead of a new parameter (change to profile scheme) I change the ring rule for all beaches: ring noise threshold = min(5 + 10 · ring, 40)% (was min(5 + 30 · ring, 80)%), required lake size = min(30 · 3^(ring−1), 90) cells (was up to 270). Ring 0 unchanged. Consequence: results for `shoreWidth` ≥ 1 change (ring 1: threshold 15% instead of 35%), and previously tuned profiles have wider beach without file change.

**Choice of `shoreWidth` per profile** (without changing rest of tuning): value from {0..8}, for which median P′ across seeds 1–4 (side 512, profile v8) is closest to reference (M06 5,638, M19 5,656); evaluation on seeds 1001–1032. Criterion: P′ in band [0.67·ref; 1.5·ref] in both profiles. Additional conditions: kind proportions (median) differ from baseline 7a by no more than 5 percentage points (Shore of course grows, so for it threshold is 10 points, since it is the goal of change), river invariant 100% of seeds. `transitionWidth` remains as in v8 (I do not change dirt in this step).

## Iteration 8 — rock massifs (recorded 2026-11-08 before implementation and before measurement)

**Reason.** User selected “Mountains and rock” from list of further D9 features ([PROJECT_RME_PROCEDURAL_GENERATOR.md](../PROJECT_RME_PROCEDURAL_GENERATOR.md), PHASE 4). Scope: shape of rock massifs on surface (z ≤ 7). Multi-floor walls to plateau remain in PHASE 5.

**Definitions (constants, set beforehand).** Kinds as in iteration 6 (natural cells; Rock = Rock kind). Rock cells in 4-connected components.
- **RB — rock boundary density**: 1000 · (number of 4-adjacent Rock–(other natural kind) pairs) / (number of Rock cells). Smaller = compact, smooth rock.
- **RF — massif compactness**: ‰ of Rock cells lying in components with area ≥ 500.
- **RS — rock dust**: ‰ of Rock cells lying in components with area < 20.
- Feature N/A when Rock cells < 1000. Reference = aggregate value of map (floors 0..7); generator = median across seeds.

**Baseline measurement (8a).** Profiles m06 and m19 in version v10, seeds 1001–1032, side 512; references M06 and M19 (in-sample, m06→M06, m19→M19).

**Criterion (8a and 8b).** Each non-N/A feature *in band* [0.67·ref; 1.5·ref]; profile “passed” if all non-N/A features are in band. Band for feature with reference close to 0 (e.g., RS below 20 ‰) I replace with absolute threshold: generator value ≤ ref + 20 ‰. Additional conditions for 8b: kind proportions (median) relative to 8a ±5 percentage points; river invariant in 100% of seeds; core tests PASS.

**Model change (8b, only if 8a shows gap).** Tuning from grids: `rockPermille` remains (proportion from profile), `minRockCells` {8, 24, 64, 160}; possibly rock boundary smoothing (new rule recorded separately before execution, if `minRockCells` change alone is not enough). Tuning on seeds 1–4 side 512, evaluation on 1001–1032.

**Caveats set beforehand.** Two references; M06 has little rock (about 5%), so RB/RF/RS for it may be uncertain; rock in references includes “mountain”, “rocky ground” and related brushes per mapping, without walls and multi-floor structures; shape metrics, not visual assessment (belongs to user).

### Iteration 8 — result 8a and recording model change 8b (2026-11-08, before its execution)

**8a (v10, side 512, seeds 1001–1032).** References M06 / M19: RB 333 / 121, RF 167 / 862, RS 36 / 15. Generator m06: RB 49, RF 998, RS 0; m19: RB 33, RF 1000, RS 0. In band: m06 0 of 3, m19 2 of 3 (RF, RS). Generator rock is **very smooth and merged into large blocks** (boundary 3–7 times smaller than in references); reference rock M06 is fragmented (RF 167), M19 compact, but with 3.7 times longer boundary.

**Cause (from code, recorded before change).** Height noise has fixed 3 octaves: step, step/2, step/4 (weights 4:2:1). After raising `featureSize` to 192 in iteration 6, the finest octave has step 48 and weight 1/7, so rock, lake, and beach edges lost fine detail; this is a side effect of iteration 6, not measured before.

**Model change 8b.** Height noise gets additional octaves when `featureSize` > 24: number of octaves = min(8, max(3, ⌊log2(featureSize/6)⌋ + 1)), weights 2^(n−1−octave), divisor 2^n − 1; for `featureSize` ≤ 24 result byte-identical as before (3 octaves 4:2:1). Moisture noise (dirt/grass) unchanged, to avoid increasing dirt raggedness which user rated negatively. Evaluation on seeds 1001–1032, side 512; criterion as in 8a; additional conditions: proportions ±5 percentage points relative to v10, river invariant, core tests PASS; additionally I report again E4/E16/E64 from iteration 6 and P′ from iteration 7c (not as criterion, as control of side effects).

## Iteration 9 — paths (recorded 2026-11-08 before implementation and before measurement)

**Reason.** User selected “paths” from list of further features (D9, D15; [PROJECT_RME_PROCEDURAL_GENERATOR.md](../PROJECT_RME_PROCEDURAL_GENERATOR.md), PHASE 4: paths). Generator does not create paths at all, so baseline measurement of generator is zero by definition. This step (9a) establishes **whether paths can be measured in references** and how they look, before anything is designed.

**Definitions (constants, set beforehand).**
- *Path tile*: tile whose ground key (brush name or `item <name>` for ground without brush) is one of: `cobblestone`, `dark cobblestone`, `grassy cobblestone`, `item cobblestone`, `white gravel`, `pavement`, `item pavement`. List selected from key names occurring in references (not from observed shapes); intentionally without floors and tiles (`stone floor`, `gray stone tiles`, `wooden floor`, `tiles`, roofs), because those are building floors, not paths.
- *Path component*: 4-connected component of path tiles, per floor (z ≤ 7); counted if area ≥ 40 tiles, skeleton length (Zhang-Suen, as in iterations 4–5) L ≥ 20 and width area/L ≤ 7 (narrow and long shape; wider patches are squares, not paths).
- Features per map (aggregate): number of components; **Ls** = sum of skeleton lengths per 65,536 tiles present; **Ws** = 1000 · area / L; **Bs** = branches per 100 skeleton tiles (×1000); **Us** = proportion of path tiles in tiles present (‰).
- Reference map *qualifies* if it has ≥ 5 counted components. If fewer than two maps qualify, path assessment is impossible and **I do not begin generator implementation** (I report it).

**Measurement 9a.** All 19 reference maps (mapping v3), no assessment, description only; result decides whether 9b (path generation and assessment with bands as in iteration 5) has any basis at all.

**Caveats set beforehand.** Path recognition by brush name is a heuristic (paved tile can also be a square or interior); path in reference can be made of different ground (e.g., packed earth) and then is not visible; only M06 and M19 have such grounds in greater quantity.

### Iteration 9 — result 9a and recording 9b: optional paths (2026-11-08, before implementation)

**9a (references, path component metric).** M06: 22 components, L 70, W 3,642, B 30,200, 0.46% of area; M19: 5 components, L 78, W 4,606, B 26,259, 0.72%; rest of maps: 0. Paths measurable only in M06 and M19.

**User decision (2026-11-08):** simple optional path (disabled by default).

**Model (9b).** New cell kind `SurfaceKind::Path`; `SurfaceTuning::pathLength` = target length of path skeleton per 65,536 area cells (0 = disabled, 0..400); is not part of style profile (profile scheme unchanged), user sets it in generator window. Paths form after rivers and beaches: routes between random land cells (Grass or Dirt) at distance 48–160, searched A* over Grass, Dirt and Shore cells (Shore cost higher), with noise in cost for tortuosity; rivers, lakes and rock are impassable (no bridges in this step); width 3–4 cells (3×3 stamp with one-sided noise widening); path overwrites only Grass, Dirt and Shore. While skeleton length below target, more routes are added (attempt limit); `pathLength` 0 gives result byte-identical as before. Resolver: profile with 7 brushes (sixth index: path brush) handles Path cells; profile with 6 brushes rejects grid containing Path; generated paths get no vegetation.

**Evaluation (seeds 1001–1032, side 512, profile v10 with m06 and m19).** `pathLength` chosen so L is as close as possible to reference of given profile (grid {40, 80, 160, 320} on seeds 1–4), measurement by same path component definition as in 9a (area ≥ 40, skeleton ≥ 20, width ≤ 7) on Path cells. Criterion: L, W and B in band [0.67·ref; 1.5·ref] (ref: m06 70 / 3,642 / 30,200; m19 78 / 4,606 / 26,259), at least 75% of seeds with ≥ 1 component; additional conditions: proportions of remaining kinds (median) relative to state without paths ±1 percentage point, path never on Rock, Lake, River (core test), river invariants and pinned fingerprint Seed 1729 (at `pathLength` 0) unchanged, core tests PASS.

**Caveats set beforehand.** Goals from two maps (M19 with exactly 5 components); generator has no path goals (settlements, gates) and does not connect paths with bridges; paths are random routes across land, without claim to realism; path brush chosen by user (default `cobblestone`, if exists in client).

### Iteration 9c — path smoothing (record made AFTER measurement: process oversight)

**Process caveat.** Recording of 9c criteria was to be done before code change, but the command recording it ended with syntax error and recorded nothing; I noticed this only after measurement. Content below was presented to user in chat before change (agreement: “Yes.”), and criteria are the same as in 9b, so they were not selected to fit the result, but formally **this is not prior recording**.

Reason: user observation in GUI (m19, `pathLength` 80) showed paths turning in steps at right angles and one running along rock edge. Change: (1) diagonal moves in A* (cost 14 instead of 10, octile heuristic), (2) additional cost for cells adjacent to rock, (3) smoothing route with moving average (window ±6) and stamping along segments between smoothed points. Path still overwrites only Grass, Dirt, Shore. Evaluation: criteria as in 9b (L, W, B in band of reference, `pathLength` chosen on seeds 1–4 from {40, 80, 160, 320}, evaluation on seeds 1001–1032, side 512), plus control of proportion of path cells at rock.

### Iteration 9d — stronger path repulsion from rock (recorded 2026-11-08 before change, after baseline measurement)

Reason: user (after GUI 9c): path around massif runs close to rock; “should repel more”. User also confirmed that Validate and Undo/Redo with paths passed. Baseline state (v10, seeds 1001–1032, side 512, after 9c): proportion of path cells with Rock cell within radius 3 (Chebyshev), median across seeds: m06 0 ‰, m19 87.5 ‰ (control “rocknear”).

Change: cell cost in A* grows with proximity to rock instead of fixed +25 for 4-neighborhood: addition 60 / 40 / 25 / 12 for distance 1 / 2 / 3 / 4 from Rock (Chebyshev distance, computed once per area). Rest unchanged.

Criterion: (1) rocknear (mean and median across seeds) in m19 at least half lower than in baseline state; (2) L and W still in reference band (as in 9b); (3) core tests PASS, path never on Rock/Lake/River, disabled = byte-identical. B does not enter criterion (skeletonization artifact described in 9c). Caveat: median is already low in baseline state (path at rock is rare case), so “half” applies to small number; visual assessment belongs to user.

## Iteration 10 — transitions between kinds (recorded 2026-11-08 before implementation and before measurement)

**Reason.** User selected “transitions” (PHASE 4: biome transitions, [PROJECT_RME_PROCEDURAL_GENERATOR.md](../PROJECT_RME_PROCEDURAL_GENERATOR.md)). Current G4 measure compares only general neighborhood (distribution of kind pairs across whole map). Transition means here: **what borders given kind** (e.g., whether rock touches dirt, grass, sand or water).

**Definitions (constants, set beforehand).** Kinds as in iteration 6 (natural cells; Lake and River as Water). Pair = two 4-adjacent cells both natural, of different kinds.
- *Rock boundary composition*: among pairs where one cell is Rock, proportion (‰) of pairs where the other is Dirt (**RD**), Grass (**RG**), Shore (**RS**), Water (**RW**).
- *Dirt boundary composition*: among pairs with Dirt cell (other cell other than Dirt) proportion (‰) of pairs with Grass (**DG**), Rock (**DR**), Shore (**DS**), Water (**DW**).
- Composition N/A when number of pairs of given boundary < 1000. Reference = aggregate value of map (floors 0..7); generator = median across seeds.

**Baseline measurement (10a).** Profiles m06 and m19 v10, seeds 1001–1032, side 512; references M06 and M19 (m06→M06, m19→M19, in-sample). First, description and gap assessment only, without generator change.

**Criterion (for 10a and possible 10b).** Feature *in band* when generator median lies in [0.67·ref; 1.5·ref], and for feature with ref < 50 ‰ when |gen − ref| ≤ 50 ‰. Profile “passed” if ≥ 6 of (non-N/A) 8 features are in band. Possible generator change (10b) will be recorded separately before execution; additional conditions for it: kind proportions ±5 percentage points, core tests, pinned fingerprint Seed 1729 unchanged for `pathLength` 0 (unless change is explicitly recorded as changing results).

**Caveats set beforehand.** Two references, in-sample; boundary composition does not distinguish whether contact is smooth or ragged (described by D/RB from iterations 7–8); in references transition through “Other” element (buildings, roads) is outside measure; values where most boundary is water (Shore/Water) depend on proportion of water and beach, already tuned in iterations 6–7c.

### Iteration 10 — result 10a and recording change 10b (2026-11-08, before its execution)

**10a (v10, side 512, seeds 1001–1032).** References M06 / M19: rock boundary composition RD/RG/RS/RW = 33/938/24/4 and 99/502/251/147; dirt boundary composition DG/DR/DS/DW = 951/32/12/4 and 728/155/27/88. Generator m06: RD 254, RG 717, RS 0, RW 29; DG 741, DR 55, DS 145, DW 47 — in band 6 of 8 (outside: RD and DS). Generator m19: RD 322, RG 661, RS 0, RW 15; DG 614, DR 148, DS 194, DW 44 — in band 3 of 8 (outside: RD, RS, RW, DS, DW).

**Gap analysis.** (1) Dirt in generator touches rock and beach roughly proportionally to its share (RD ≈ dirt share in land, about 30%), but in references dirt **avoids** rock (RD 33–99) and beach (DS 12–27): around rock and beach is grass. (2) In M19 rock touches beach and water (RS 251, RW 147), in generator almost never (RS 0, RW 15), because rock is highest, lake lowest cells of height; this cannot be fixed with local rule without changing height model, so 10b does not address it.

**Model change 10b (point 1 only).** After rock and beach determination (before rivers) each Dirt cell at Chebyshev distance ≤ 2 from Rock or Shore cell changes to Grass (“grass border”). No parameters (fixed rule, applies to all profiles); changes Surface results (also for `pathLength` 0), so pinned fingerprint Seed 1729 100×100 will be intentionally changed.

**Criterion 10b.** RD and DS in band in both profiles (m06 and m19); features in band in 10a remain in band; kind proportions (median) relative to 10a ±5 percentage points; core tests PASS; river and path invariants unchanged (tests). RS and RW for M19 are not criterion (see gap 2).

**Caveats set beforehand.** Grass border reduces dirt proportion and its raggedness at rock; visual assessment belongs to user; in-sample on two references.

### Iteration 10 — result 10b and recording 10c (2026-11-08, before execution of 10c)

**10b (full grass border 2 cells around Rock and Shore; v10, side 512, seeds 1001–1032).** m06: 8 of 8 features in band (RD 0, DS 0 and DR 0 fit in absolute band ±50 ‰ around reference 33/12/32). m19: 2 of 8 (RD 0 vs. ref. 99 outside band [66, 148]; DR 0 vs. 155 outside band, and was in band in 10a; DS 0 in absolute band). **Criterion 10b not met for m19**: border completely removes dirt-rock contact, but in M19 it is nonzero. Kind proportions in bounds (m06: Grass 403, Dirt 160, Shore 45; m19: 295 / 114 / 45 ‰; change < 5 percentage points vs. 10a).

**Change 10c.** Border around Rock is partial: Dirt cell at Chebyshev distance ≤ 2 from Rock changes to Grass when value noise (step 4) of cell is below 75% of range (change probability about 75%); border around Shore remains full (ref. DS 12–27 is close to zero). Value 75% chosen to leave about one-quarter of contact (references RD 33–99 with 254–322 in 10a); selected after seeing 10a and 10b (adaptively, one value, no grid search).

**Criterion 10c.** As in 10b: RD and DS in band in both profiles, features in band in 10a remain in band (incl. DR in m19), proportions ±5 percentage points, core tests PASS. If DR in m19 falls out of band (possible: RD and DR depend on same contact, but have different denominators), I report it as not met and **do not** search for further metric parameters; choice between full and partial border then goes to user's visual assessment.

## Iteration 11 — settlements and structures, part 1: measurement of buildings in references (recorded 2026-11-08 before implementation and before measurement)

**Reason.** User selected “settlements and structures” (last point from list; PHASE 5 of plan: structures, settlements, ruins, camps). Generator does not create any buildings, so baseline state is zero by definition. This step (11a) establishes **how buildings look in references and whether they can be measured**; decision on generation scope (walls, doors, floors via RME brushes) is made only after its result.

**Definitions (constants, set beforehand).** Floors z ≤ 7. *Wall cell*: tile with Wall or Door attribute. *Building*: 8-connected component of wall cells; counted if it has ≥ 12 cells and bounding box ≥ 4 × 4. Features per map: number of buildings; **Bd** = buildings per 65,536 tiles present; median of longer and shorter bounding box side (**Bl**, **Bs**); median number of wall cells (**Bw**); mean number of Door cells per building (**Bdoor**, ×1000); proportion of buildings with ≥ 1 door (‰, **Bdp**). Reference map *qualifies* if it has ≥ 5 buildings; without ≥ 2 such maps measurement is impossible and structure generation has no data basis.

**Measurement 11a.** All 19 reference maps, description only; result determines the scope of possible 11b (specification written separately before execution). No generator evaluation.

**Upfront caveats.** Wall attribute comes from client item definitions (wall brush); freestanding walls, fences, and cave walls also have it, so “building” is a shape heuristic; interiors, roofs (higher floors), and furniture are out of scope; references are mainly M06 and M19.

### Iteration 11 — result 11a and specification 11b: building foundations (2026-11-08, before implementation)

**11a (references).** M06: 1,313 buildings, 31 per 65,536 tiles, median 9 × 7, ~30 wall cells, 1.66 doors per building; M19: 582, 37, 9 × 7, 24 cells, 3.44 doors; M05, M15, M01 fewer and different (M01 is mostly fences). Measurement possible in 5 maps.

**User decision (2026-11-08):** scope is “building foundations only”, without walls and doors.

**Model 11b.** New cell kind `SurfaceKind::Floor` and parameter `SurfaceTuning::buildingDensity` = target number of buildings per 65,536 cells of area (0 = disabled, 0..100, default 0; not part of style profile). After paths: random floor rectangles, longer side 7–11 (uniform), shorter 5–9 (uniform, not larger than longer), random orientation; rectangle with 2-cell margin must lie entirely on Grass or Dirt (no Path, Shore, Rock, Lake, River, Floor); up to 40 attempts per building, omitted if no space. Floor uses floor brush (generator window, default `stone floor` if exists in client); resolver: 8-brush profile (index 7: floor; index 6: path, always filled when floor enabled) accepts Floor cells; 6- and 7-brush profiles reject grids with Floor. No walls, doors, or roofs; floor gets no vegetation or paths.

**Evaluation (seeds 1001–1032, side 512, profiles v10; `buildingDensity` 30 for m06 and 40 for m19, constants set beforehand, no tuning).** Floor component: 4-connected Floor component. Features: number of components per 65,536 cells (ref. m06 31, m19 37), median longer bounding box side (ref. 9, 9), median shorter (ref. 7, 7). Criterion: all three features within band [0.67·ref; 1.5·ref] in both profiles; additional conditions: Floor only on former Grass or Dirt (core test), no building overlap, type proportions (median) relative to state without buildings ±1 percentage point, `buildingDensity` 0 gives byte-for-byte result as before (pinned Seed 1729), tests PASS.

**Upfront caveats.** Floor without walls is not a building; comparing floor bounding box with wall bounding box in reference is an approximation (wall encompasses outline, floor interior, so floor is often 2 cells smaller); placement is random (no roads, no settlement clusters); building count from two maps.

### Iteration 11c — building walls (recorded 2026-11-08 before implementation; user decision: “I prefer with walls, doors not required”)

**Scope.** Building outlines are added to building foundations (11b). Walls are items on a tile (not ground): the plan gains a wall item layer, placed like vegetation after ground border calculation, and wall element fitting (horizontal, vertical, corners) is done by the existing RME mechanism (`WallBrush::doWalls`) on the preview buffer, without a separate engine. Doors: out of scope (building has continuous outline).

**Model.** `GroundProfile::wall` = wall brush name (empty = no walls; only allowed when foundations enabled); `GroundPlan::walls` = wall item ID (first element of brush, `WallBrush::getPlacementItemId`) for each *outline cell*: Floor cell with a 4-neighbor of different kind (for 9×7 rectangle: 28 cells). Resolver checks that brush exists, yields non-zero ID, and item is not ground or meta-item; preview checks that each wall item belongs to profile brush (`hasWall`), lies on cell with plan ground, and plan has no walls outside mask. Generator window: “Walls” toggle and wall brush; walls only when “Buildings” > 0. Preview footprint and plan contain wall layer.

**Evaluation.** (1) Core metric: median number of outline cells per building (Floor cells with 4-neighbor non-Floor) within band [0.67·ref; 1.5·ref] relative to median wall cells in references (m06 30, m19 24), seeds 1001–1032, side 512, density as in 11b (30/40). (2) Core test: outline has 2·(longer+shorter)−4 cells for each rectangle. (3) Verification on real client in GUI by user (Preview, Validate, Place, Undo/Redo with walls; corner correctness). (4) G1 regression `--commit` and `--surface` without result changes. Additional condition: no walls outside mask and on protected cells (preview control).

**Upfront caveats.** Metric (1) results from sizes set in 11b (for rectangle outline is 28 cells) and is an implementation check, not independent proof; wall fitting (corners, T-junctions) depends on RME mechanism and client, to be verified by user; building without doors is not a game entrance; roofs and interiors absent; G1 probe does not yet include paths with walls.


## Iteration 12 — settlements and structures, part 2: clusters, doors and door trails (recorded 2026-11-09 before implementation and measurement)

**Scope (user decision 2026-11-09):** doors (one entrance per building, door brush via existing RME mechanism), trails leading to doors, building clusters instead of random placement. This record includes **12a: definitions and measurement in references** and **generator evaluation criteria (12c)**. Design (12b) will be recorded separately, after result 12a, before implementation.

**Definitions (constants, set beforehand).** Floors 0..7, each floor separately. *Building* as in 11a (8-connected component of Wall or Door tiles, ≥12 bounding box ≥4×4); *bounding box* = rectangle [x0..x1] × [y0..y1] of component. On generator side, building is 4-connected Floor component (its bounding box is the outline bounding box). *Spacing* between two buildings on same floor = max(dx, dy), where dx = max(0, a.x0−b.x1, b.x0−a.x1), similarly dy (directly adjacent buildings have spacing 1). *Trail tile* = ground key from list 9a (`cobblestone`, `dark cobblestone`, `grassy cobblestone`, `item cobblestone`, `white gravel`, `pavement`, `item pavement`); on generator side Path cell. *Water tile* = Water/Lake/River kind per same mapping (keymap) as iterations 4–10. *Side* of building = one of four bounding box edges (N, E, S, W); *edge door* = Door tile on bounding box edge, its side is that edge (corner counts for both). *Exit outward* from door = successive tiles in direction perpendicular to side, outside bounding box.

Features per map (collectively; N/A when map has <30 buildings):
- **G** — median (over buildings with at least one neighbor on floor) of smallest spacing to another building.
- **Cluster** = single-linkage component of buildings with spacing ≤12 (constant). **Kn** = clusters per 65,536 present tiles (×1000); **Ks** = median number of buildings in cluster; **Ki** = share of solitary buildings (cluster 1) in ‰; **Kb** = share of buildings in clusters ≥3 in ‰.
- **Bp** = share of buildings having trail tile within Chebyshev distance ≤3 from bounding box (‰); control **Cp** = share of present tiles not water or trail, having trail tile within distance ≤3 (‰). **Bv** = share of buildings with water tile within distance ≤8 from bounding box (‰); control **Cv** = same for present tiles (no water). Conclusion about building–trail or building–water relation only as Bp relative to Cp and Bv relative to Cv.
- **Dh** = share of buildings with ≥1 Door tile (‰); **Dn** = median number of Door tiles per building among buildings with doors; **Db** = share of Door tiles lying on bounding box edge (‰) — remainder are interior doors or window-doors counted by heuristic.
- **Dp** (door orientation relative to trail): set S = buildings with ≥1 edge doors and trail tile within distance ≤3 from bounding box. *Trail-facing side* = side for which from any tile of its edge, exit outward in ≤3 steps reaches trail tile. **Dp** = share of buildings in S where at least one side with doors is a trail-facing side (‰). Control **Dp0** = average over S of |trail-facing sides| / 4 (‰) — expected result with random entrance side. Conclusion about orientation only as Dp relative to Dp0. N/A when |S| < 30.

**Measurement 12a.** All 19 reference maps, description only, no generator evaluation; results with counts (buildings, |S|). Map *qualifies* for cluster and spacing features when it has ≥30 buildings (expected: M06, M19; M05 with 13 buildings does not qualify). If <2 maps qualify, cluster evaluation is impossible and I report this before design.

**Generator evaluation criteria (12c; seeds 1001–1032, side 512, profiles v10 m06 and m19 with settings as in `g5_shape_table.py`: m06 pathLength 40/buildingDensity 30, m19 80/40; building density threshold values are not tuned after seeing result).** Feature is *within band* when generator median lies in [0.67·ref; 1.5·ref]; for ‰ features (Ki, Kb, Bp, Bv, Dh, Db, Dp) and for ref<50: |gen−ref|≤50‰ (absolutely). Evaluated features: G, Kn, Ks, Ki, Kb, Bp, Bv (each also relative to controls Cp, Cv: direction of difference Bp−Cp and Bv−Cv matches reference), Dp (direction Dp−Dp0 matches reference). **Intentional deviations, not counted toward band:** Dn (intent: exactly one entrance per building) and Db (all doors on edge). Profile “passes” if ≥75% of evaluated (non-N/A) features are within band or have matching direction. Additional conditions: each building has exactly one door (core test), doors lie on outline cell with external neighbor being passable cell (not water, not rock), path exists from door to trail (Grass/Dirt/Path/Shore cells) in ≤25 steps or building reported as unreachable (percent reported), with buildingDensity 0 and pathLength 0 result is byte-for-byte as before iteration (pinned Seed 1729), 14 core tests and both G1 probes (`g5_run_suite.py --probes`) PASS, two runs byte-identical.

**Upfront caveats.** Reference features come from two qualifying maps (D17), so bands are in-sample and don't prove generality; Wall/Door in reference includes fences, walls, and gates (some “buildings” are not houses), interior doors and double doors undercount Db and overcount Dn; distances 3, 8, and 12 are preset constants, not tuned; Cp/Cv control is simplest area control, doesn't account for buildings naturally standing on land far from rock; door orientation relative to trail depends on which trail tiles reference has in keys 9a (beaten earth trails invisible); floors >7 and interiors out of scope; visual assessment is user's and takes precedence over metric (discrepancy noted).

### Iteration 12 — result 12a, baseline generator state and specification 12b: settlements (2026-11-09, before implementation)

**12a (references: M06 / M19 / auxiliary M01).** Nearest neighbor spacing G = 4; clusters approx. 11–12 per 65,536 tiles (Kn 11,167 / 11,955), cluster median 1, solitary (Ki) 265 / 189 ‰, in clusters ≥3 (Kb) 661 / 714 ‰; buildings near trail: Bp 93 / 209 ‰ vs. control Cp 14 / 25 ‰; water independent (Bv ≤ Cv); edge doors facing trail: Dp 855 / 750 ‰ vs. Dp0 594 / 546 ‰ (|S| = 69 / 76). Details and caveats: [G4_GATE_STATUS.md](G4_GATE_STATUS.md), “Iteration 12a”.

**Generator baseline (seeds 1001–1032, side 512, v10, median [min..max]; `g5_shape_table.py`).** m06: G 10 [8..11], Kn 16,875, Ks 1, Ki 358, Kb 442, Bp 0 [0..33], Cp 10, Bv 83, Cv 189, Dh 0. m19: G 6 [5..7], Kn 11,750, Ks 2, Ki 131, Kb 762, Bp 15, Cp 17, Bv 125, Cv 173, Dh 0. Doors and orientation: N/A (no doors). Two runs byte-identical.

**Model 12b (constants set beforehand, not tuned after seeing result; works only when `buildingDensity` > 0, at 0 result is byte-for-byte as before).**
1. *Clusters.* Buildings placed sequentially: **30%** of attempts are anchors (random placement as before), **70%** are attachments: randomly select one already-placed building, one of its four sides, and spacing **3–6 tiles** (uniform; 2-cell margin for Grass/Dirt remains, so minimum spacing is 3), offset along side random so bounding boxes share extent. Building sizes and attempt limit (40 per building) unchanged; failed attachment counts as attempt.
2. *Doors.* One door tile per building, at center cell of side outline facing target (target: nearest Path tile at Chebyshev distance ≤60 from building center, at time building is placed in sequence; without target, random side). `SurfaceGrid::doors` (cell indices) is new field; does not change cell kinds. Door cell is discarded if after mask and protection it is no longer Floor.
3. *Trails to doors.* Cell before door (step) becomes Path; from step a route follows same procedure as trails 9d (A*, smoothing, stamping 3–4 cells; shared function extracted from `drawPaths` without changing its result) to target from point 2. Building without target gets only step and feeds network: later buildings connect to nearest Path tile. Route rejected by length limit (3 × Chebyshev distance) or no path = building “without access”; their percentage is reported.
4. *Door brush* (adapter, part 2 of stage 12; separate from core): `GroundProfile::door` = RME door brush name; door placed by existing `DoorBrush` on wall cell after `WallBrush::doWalls`; no separate engine. Not part of this numeric decision.

**Additional conditions 12b (core).** `buildingDensity` 0: Seed 1729 fingerprint unchanged (`pathLength` 0 and > 0); `pathLength` > 0 with `buildingDensity` 0 unchanged byte-for-byte; for `buildingDensity` > 0 results **intentionally change** (new placement, steps and trails), so pinned fingerprints for such cases will be updated explicitly in commit; exactly one door per building and door on Floor outline cell; step is Path; 14 core tests + new test PASS; `g5_run_suite.py --probes` PASS after rebuilding `rme`; two runs byte-identical.

**Evaluation 12c** per criteria recorded above (bands, seeds 1001–1032, profiles m06 and m19, `buildingDensity` 30 and 40, `pathLength` from `g5_shape_table.py`). Remaining: GUI (door brush, appearance, Preview/Validate/Place/Undo/Redo) for user assessment.

**Upfront caveats.** Percentage 30/70, spacing 3–6, reach 60 and trail width chosen from references M06/M19 (G 4, approx. 2.8 buildings per cluster), not tuned on generator seeds; changing these constants after seeing result requires new recording and “after model change” label. Measurement of G and Kn is sensitive to constants 3–6 and 12 (sensitivities not explored). Doors in generator are one entrance (idealization), references have more (Dn 2 / 4).

### Iteration 12 — result 12c (model 12b) and recording 12d: anchor percentage (2026-11-09; recorded before constant change and re-measurement)

**Result 12c for model 12b (core commit with 30% anchors; seeds 1001–1032, side 512, v10, median).** Criterion “≥75% of evaluated features in band or with matching direction” **not met in both profiles (4 of 8)**.

| Feature | m06 ref → gen | m06 | m19 ref → gen | m19 |
| --- | --- | --- | --- | --- |
| G | 4 → 4 | in band | 4 → 4 | in band |
| Kn | 11 167 → 5 875 | **out** (band 7 482–16 750) | 11 955 → 4 750 | **out** (8 010–17 932) |
| Ks | 1 → 2 | **out** (0.67–1.5) | 1 → 3 | **out** |
| Ki | 265 → 62 | **out** (\|Δ\| 203 > 50) | 189 → 37 | **out** (152) |
| Kb | 661 → 850 | **out** (189) | 714 → 928 | **out** (214) |
| Bp − Cp | +79 → +924 | direction matches | +184 → +908 | matches |
| Bv − Cv | −29 → −130 | direction matches | −23 → −98 | matches |
| Dp − Dp0 | +261 → +503 | direction matches | +204 → +495 | matches |

Intentional deviations (outside band by design): Dn 1 (ref 2 / 4), Db 1000 (ref 231 / 563), Dh 1000. Additional core conditions: one door per building, doors on outline, step is Path, spacing ≥3, `buildingDensity` 0 unchanged (Seed 1729 fingerprint and PNG `pathLength` 80 / `buildingDensity` 0 byte-identical as before change), 15 tests PASS.

**Observations outside criteria.** (1) Bp = 1000 by construction (each building has step), so does not distinguish from reference as Bp = 93 / 209; direction is met trivially. (2) Trail length (L per 65,536) grows from 53 / 89 (9d, without buildings) to 243 / 280 with buildings, because each building gets route to nearest network; references have 70 / 78 (whole map, per list 9a). Trails thus occupy clearly more than in reference. (3) Buildings too clustered: Ki 62 / 37 vs. 265 / 189 ‰.

**Change 12d (one constant, recorded before re-measurement; result marked “after model change”).** `clusterAnchorPercent` 30 → **60**. Justification solely from table: references have 19–27% solitary buildings and clusters with median 1; generator at 30% anchors has 4–6% solitary. Value 60 chosen as round midpoint toward expected value, not tuned on seeds; if it also fails criterion, I do not search further without new user decision. Other constants (spacing 3–6, reach 60, 12 in cluster definition) unchanged. Evaluation criteria same as in 12c (above). Trail length (observation 2) in this iteration not changed or evaluated; reported as deviation.

**Upfront caveats.** Change after seeing result in-sample on same seeds and maps (D17); improving Ki/Kn with one constant does not prove generality. Ks and Kb may not change as expected, depending on cluster size distribution, not one parameter.

### Iteration 12e — door adapter (recorded 2026-11-09 before implementation; user decision: “Adapter and door GUI”)

**Scope.** Doors from core (`SurfaceGrid::doors`) go to plan and preview by existing RME mechanism, no separate engine; core results and criteria 12c unchanged.
- `GroundProfile::door` = door type from closed list of names (Normal door, Locked door, Magic door, Quest door, Normal door (alt), Archway), mapped to `DoorType`; empty = no doors (continuous wall as before). Allowed only with wall brush (profile with 8 brushes). No item IDs in code: door element selected by wall brush profile via `DoorBrush`.
- `GroundPlan::doors` = flag per cell (one per building, on outline cell with wall); appended to plan fingerprint only with doors, so plans without doors keep current fingerprint.
- Preview: after `WallBrush::doWalls` on each door cell `DoorBrush(type).draw` on preview buffer; if `canDraw` is false (wall brush has no doors of this type for this orientation), preview refuses with error instead of silently leaving wall. Door elements of wall brush already in allowed set (`getRelatedItems` includes doors).
- Generator window: “Doors” checkbox (active when walls enabled) and door type list.

**Evaluation.** (1) Tests: plan with doors has different fingerprint than without, plan without doors byte-identical as before; rejections (doors without wall, unknown type, profile without 8 brushes). (2) G1 probes `--commit` and `--surface` PASS after rebuilding `rme`; probe will include case with walls and doors (preview, commit, Undo, Redo, door count = building count within mask). (3) GUI: user assessment (appearance, door direction vs. trail, Undo/Redo); their visual choice has priority.

**Upfront caveats.** Whether selected wall brush has doors of given type for all orientations depends on client and brush data; failed `canDraw` ends preview with error. No automatic house door ID assignment (out of scope). Core does not know door type.

### Iteration 12f — building door side without trail (recorded 2026-11-09 before change; reason: user visual assessment in GUI)

**User observation (GUI, screenshot).** Left upper building doors face left, and trail runs from right and bottom. Reason in model 12b: building which at processing time has no Path tile yet in range 60 gets **random** door side, and later buildings connect to its step, so trail circles the building.

**Change (one rule; does not change building placement, so G, Kn, Ks, Ki, Kb unchanged).** When no target as Path tile at range 60, door side faces **center of nearest other building** (any, including later in placement order) at Chebyshev distance ≤60; random side only if no building in range. Routes: unchanged (building without Path in range gets only step, later ones connect to it).

**Evaluation.** Criteria 12c unchanged; expectation: Dp and other cluster features do not worsen, and doors of two adjacent buildings face each other (test: for pair of buildings without trail, each door side faces the other). Visual user assessment decides; discrepancy with metric noted.

**Upfront caveats.** Facing nearest building does not guarantee facing target trail; change from one screenshot observation.

### Iteration 12g — shorter trails to doors (recorded 2026-11-09 before implementation and measurement; user decision: “Shorten trails”)

**Reason.** With buildings enabled, trail length (L per 65,536 tiles) is approx. 300–340 vs. 70 / 78 in references (M06 / M19), because each building gets route to nearest network within range 60 from its center.

**Change.** New constant `doorTrailReach` (Chebyshev distance from building center to nearest Path tile): route from step determined only if target lies in this range; otherwise building gets only step (as before without target). Door side still selected at range 60 (`doorTargetReach`, rule 12f). Building placement unchanged.

**Value selection (rule set beforehand).** Measure variants `doorTrailReach` ∈ {16, 24, 36} (seeds 1001–1032, side 512, profile v10 m06 and m19, `buildingDensity` 30 / 40, **`pathLength` 0**, because random long trails 9d alone give L 53 / 89 and depend on user setting). Select **largest** value for which median L ≤ 1.5·ref in both profiles (≤105 for m06, ≤117 for m19; ref 70 / 78). If none qualify, select 16 and report criterion not met. Also report **percentage of buildings without access** (step in Path component 8-connected where exactly one door exists; at `pathLength` 0 means: step without route to neighbor) and Bp, Dp, G, Kn, Ks, Ki, Kb (should be unchanged: same placement).

**Additional criteria.** `buildingDensity` 0 unchanged (Seed 1729 fingerprint); 15 core tests and both G1 probes PASS after rebuilding `rme`; two runs byte-identical; visual user assessment decides (their choice has priority over metric).

**Upfront caveats.** Shorter range leaves more buildings without route (cost of shortening, measured as percentage without access); L counted only for `pathLength` 0, with long trails 9d sum will be larger by their contribution; band for L from two maps (D17); range {16, 24, 36} set beforehand, not tuned.

### Iteration 12h — trails to doors must go somewhere (recorded 2026-11-09 before implementation and measurement; reason: user visual assessment after 12g, three screenshots)

**User observations (GUI, `pathLength` 0).** (1) Single steps without route look like stubs; (2) routes between neighboring buildings end just before target (e.g., screenshot 3: rounded patch under building breaks 2 tiles before stub of other door), so they don't look like they lead anywhere. Causes: (1) step always created, even with nothing to connect to (12g); (2) route smoothing by moving average shortens ends, and stamping doesn't reach start and end points.

**Changes (both concern only buildings; `buildingDensity` 0 and long trails 9d unchanged byte-for-byte).**
1. `PathRouter::connect` gets option `reachEnds` (enabled only for door trails): in addition to smoothed line, segments from step to line start and from line end to target are stamped.
2. Step created only when building has target: Path tile at ≤`doorTrailReach` (24) from center (as before) **or** unprocessed building in that range (center-center, Chebyshev) whose door will attach: then step is marker waiting for partner, and partner finds it as target. Building without any target gets no step (door opens directly onto grass/dirt).

**Evaluation (seeds 1001–1032, side 512, v10, `pathLength` 0, `buildingDensity` 30 / 40).** (a) L (median) ≤1.5·ref (≤105 m06, ≤117 m19) at `doorTrailReach` 24; if not, value 16 (only alternative, set beforehand); (b) **stubs**: percentage of doors whose step is only door in its Path component 8-connected ≤100‰ (goal: 12g had approx. 650–670‰ “isolated”, most of which are stubs); (c) report percentage of doors without step (buildings without target) and G, Kn, Ks, Ki, Kb, Dp (should stay unchanged); (d) tests: 15 core tests + G1 probes PASS after rebuilding `rme`, two runs byte-identical, all doors still on outline, step (if any) is Path before door; (e) visual user assessment decides.

**Upfront caveats.** Pairs of buildings can connect by narrow or winding route (A* with fixed costs); building whose partner fails to get route (length limit, no path) stays with stub (what (b) measures); change from three screenshots of one user; range 24 not examined for sensitivity.

### Iteration 13 — surface reachability validator (recorded 2026-11-09 before implementation and measurement; user decision: “Reachability validator first”)

**Scope.** Core gets pure function `reachabilityOf(SurfaceGrid, walls)` (`source/procedural/core/reachability.{h,cpp}`), which only **measures** (no repair and no seed rejection; separate iteration after measurement). Does not change generator result or fingerprints.

**Walkability model (assumption, not verified on client).** Walkable: Grass, Dirt, Rock, Shore, Path and Floor interior. Unwalkable: Lake, River, Preserve (cell not generated, so unknown), and when `walls` enabled, Floor outline cell (Floor with 4-neighbor of different kind or outside area — same definition as in resolver) **without door**; door cell is walkable. Without walls, outline is walkable. 4-connectivity (as `topology`).

**Output.** Walkable cell count, components, largest component, largest share in ‰, cells outside largest, “pockets” (components ≥16 cells outside largest), buildings (4-connected Floor components), sealed buildings (doors outside largest component or no door with `walls`), reachable buildings.

**Why no band from references.** References (floor 7) have largest component approx. 10% of walkable area (e.g., 74,945 of 718,346) because maps are islands and areas separated by unknown; generator window 512×512 is not comparable. Reference values **N/A** as band; criteria are absolute.

**Criteria.**
1. Hand-counted tests: lake dividing land into two components (numbers from diagram), building with door (reachable), without door with `walls` (sealed), same data without `walls` (reachable), Preserve cells unwalkable, empty grid, `stop_token`.
2. Cross-check: on ≥50 random 16×16 grids result equals independent test implementation (different algorithm: union-find) in component count, largest, and sealed buildings.
3. Determinism: two runs identical; working memory ≤8 B/cell (estimate, not measured).
4. Measurement (seeds 1001–1032, side 512, profile v10 m06 and m19, `buildingDensity` 30 / 40, `pathLength` default from profile and 0, walls and doors enabled; median [min..max]): largest component share (‰), cells outside largest per 65,536, pockets, sealed / buildings. **Criterion for walkability: buildings only:** sealed = 0 in ≥90% of seeds (medianly 0). Other numbers baseline for bridges and passes, no band (rivers dividing land legal).
5. All core tests and both G1 probes PASS; `buildingDensity` 0 unchanged byte-for-byte (Seed 1729 fingerprint `17159159863505677800`).

**Upfront caveats.** No POI or “fixed start” yet from G5 (start/POI in generator don't exist): “POI reachable” = N/A, only required points are building entrances. Rock and Shore walkability assumed, not verified on client (D22 to record). Floors and Z passages out of scope (generator operates on one floor).

### Iteration 14 — fords across water (recorded 2026-11-09 before implementation and measurement; user decision: “Bridges/fords”)

**Scope.** Chose **fords**, not bridges: ford is Path tile passage placed on river or lake, so uses seventh brush (trail) and needs no new brush or item IDs. Bridges with separate object (wood, railings) need bridge brush in profile, out of scope (to TECH_DEBT).

**Change (only with new field `SurfaceTuning::fordLength`, appended to structure end; 0 = disabled = result byte-for-byte as before).** After rivers, trails and buildings: while walkable component ≥16 cells (`reachabilityPocketCells`) exists outside largest, with range ≤`fordLength` cells of water (River, Lake; 4-movement, water only) to another walkable component, smallest such component (order: smallest label in row-major) gets ford: shortest route through water (BFS, ties by fixed neighbor order) stamped as Path with water neighbors of route (approx. 3 width). After each ford components recount. Walkability model as iteration 13 (walls enabled). Fords created before mask and protection applied (Preserve cells stay Preserve).

**Criteria.**
1. Hand-counted tests: two land halves divided by 3-width river — `fordLength` 3 connects them (1 component, 3-cell water route), `fordLength` 2 does not; no water in range = unchanged; two runs identical; `fordLength` 0 = fingerprint as before change (Seed 1729: 17159159863505677800).
2. Measurement (seeds 1001–1032, side 512, profile v10 m06 and m19, settings as iteration 13, `fordLength` ∈ {8, 16, 32}): seeds with sealed buildings (`sealed` > 0), largest component (‰), Path cells added by fords per 65,536 tiles. **Walkability criterion: ≥29 of 32 seeds without sealed buildings (≥90%) in both profiles.** Value selection: smallest of {8, 16, 32} meeting criterion in both; if none, value 32 and verdict NOT MET.
3. All core tests and both G1 probes PASS after rebuilding `rme`; at `fordLength` 0 tests with fingerprints unchanged.
4. Adapter/GUI: “Ford length” field in generator window (0 = disabled), profile must have seventh trail brush (as with `pathLength`); preview, Undo and Redo with fords (probe `--surface` case); user appearance assessment has priority.

**Upfront caveats.** Ford as Path looks like gray path through water, not bridge; not verified whether Path tile on water is acceptable to player (walkable in game depends on trail brush). Ford can cut long river in many places (one river divides land into many components); ford count grows with components. Mask and protection can break ford (Preserve). Validator only measures walkability model (D22).

### Iteration 14b — fords of sand instead of trail, range limit 256 (recorded 2026-11-09 before code change; reason: user note after 14, GUI screenshot)

**User observation:** “Can't cross that bridge”; question, why limit reach to 64. **Diagnosis (measurement in probe, side 250, first seed with ford).** 5 of 6 ford tiles blocking. Tile has ground `cobbled pavement`, edge `sand` and blocking element `shallow water` (id 4648, water edge overlaid by auto-border because trail brush has lower priority than water). Error of my iteration 14: criteria checked model connectivity, not tiles after commit.

**Change.** Ford tiles are kind **Shore** (sand), not Path: same brush and same border rules as beaches, which in RME lie at water without blocking edges on own tile. Validator model walkability unchanged (Shore and Path walkable), so numbers from iteration 14 (sealed seeds, largest component) should remain same; ford no longer needs seventh brush (adapter and window back to requiring trail brush only with `pathLength`). `fordLength` limit grows from 64 to **256** (validation and window field); 64 was my arbitrary choice, without measurement justification.

**Criteria.** (1) Probe `--surface`: after Commit **no ford tile is blocking** and each has ground (new assertion; this criterion failed in 14). (2) Sealed buildings count for `fordLength` 8/16/32/48/64 identical as iteration 14 (m06 9/9/5/2/1, m19 9/9/5/2/2 of 32 seeds); if differs, record difference. (3) Reach 256: measure `style_render` time on 32 m06 seeds (currently 6–7 s per 32 vs. 4 s without fords) and sealed building seeds; no band, report only. (4) 16 core tests, both G1 probes PASS, Seed 1729 fingerprint unchanged; core ford tests change from Path to Shore (hand-count unchanged except kind name).

**Upfront caveats.** Ford of sand looks like shallows/sand bar, not bridge; bridge with separate object still out of scope. Whether bare `sand` on water is walkable on client checked by assertion only (blocking property from client data), not game walking. Adjacent water tiles may get sand edges (normal border).

### Iteration 14c — extended ford ends (recorded 2026-11-09 before code change; reason: user note after 14b, GUI screenshot: “too narrow at start/end”)

**User observation:** ford of sand (14b) is fairly wide in middle, but too narrow at start and end to cross. **What I know from measurement (new assertion in probe, side 250, lakePermille 250, `fordLength` 256, 71 ford tiles, 3 pockets).** No ford tile or land tile nearby has blocking flag, and each pocket connected by ford in model is connected on committed map (4-movement). So by client flags ford is walkable; user complaint I read as narrow bottleneck at land junction (ford ends in edge corner, so contact is 1–2 tiles), not flag error. If user tested in-game walking and still cannot, cause unknown to me.

**Change (only with `fordLength` > 0).** For first and last cell of ford route all water cells (River, Lake) at Chebyshev distance ≤**3** become Shore (constant `fordMouthRadius = 3`, within area). Ford middle unchanged (3 width). Component connectivity unchanged (only more sand at ends).

**Criteria.**
1. Hand-counted tests (3-width river, 20×8, range 3): ford-land junction edge (ford cells 4-adjacent to land) is **4** at each end (rows 0..3 at upper area edge) instead of 2; Shore cell count in whole ford changes from 6 to number counted from diagram (recorded in test); 4-width lake analogously.
2. In area middle (not at edge) junction is 7 at each end (test on 20×16 grid with 3-width river, route in middle row).
3. Probe `--surface`: still 0 blocking ford or nearby tiles, pockets connected on committed map.
4. Sealed buildings count for `fordLength` 8/16/32/48/64/256 unchanged vs. 14b (m06 9/9/5/2/1/0, m19 9/9/5/2/2/0); ford cells grow (report, no band); 16 core tests, both G1 probes, Seed 1729 fingerprint unchanged (at `fordLength` 0 nothing changes).
5. User appearance assessment has priority; I cannot judge from metric whether “can cross” in their sense.

**Upfront caveats.** Radius 3 is my choice (without reference measurement; no sand fords in references); larger sand “patches” at ends may look unnatural; adjacent water tiles get sand edges.

### Iteration 14d — more natural ford shape (recorded 2026-11-09 before code change; user decision after 14c: “I prefer natural appearance”)

**Reason.** After 14c ford is straight line of uniform width and edges like from ruler; user does not know if natural, prefers natural look. **Assessment of “natural” is user's; criteria below only measure shape is not straight line and doesn't break anything.**

**Change (only for fords with water route ≥ 8 cells; shorter ones remain as in 14c, so 14c hand-counted tests unchanged).** (1) Ford axis is not the shortest BFS path, but a line from route start to end with transverse deflection `offset = (noise − 0.5) · 2 · A · sin(π·p/L)`, `A = min(4, L/6)` cells, noise from `valueNoise` (generator seed, absolute coordinates of base point, step 8), so deflection is 0 at both ends. (2) Variable width: around each axis cell, water at Chebyshev distance `d` becomes sand, `d = 2` when noise (separate stream) > 40,000 of 65,536, else `d = 1` (width 3 or 5, variable smoothly every ~8 cells). (3) Ends as in 14c (mouth radius 3). (4) Safety check: after building axis, it is verified whether start connects to target via passable cells; if not, a simple BFS route from 14c is additionally stamped (guarantees progress and loop termination). `placeFords` gets optional argument with seed and area start (default 0).

**Criteria.**
1. 14c hand-counted tests (river 3, lake 4, grid 20×16, pocket 12) pass unchanged.
2. Test on wide water (grid 64×24, land x 0..7 and x 56..63, water 48 columns, `fordLength` 64): one component after ford; determinism (two runs and two different seeds yield different shapes); in central third of length each column has 3–9 sand cells; sand center of mass in column shifts ≥ 2 rows between columns (shape is not a straight line); sand only where water was.
3. Probe `--surface`: 0 blocking ford tiles and nearby, pockets connected on committed map.
4. Seeds with sealed buildings identical as in 14c (range 8/16/32/48/64/256: m06 9/9/5/2/1/0, m19 9/9/5/2/2/0 of 32); ford cells and generation time reported (no band); two runs byte-identical; 16 core tests, both G1 probes, Seed 1729 fingerprint unchanged (`fordLength` 0 changes nothing).

**Upfront caveats.** Constants (A, 8, 40,000, width 3/5) are my choices without reference measurement (references have no fords); deflected axis may pass through non-water cells (not changed); whether it looks natural is user's assessment; safety check (4) may add a straight route, then shape will be less natural.

### Iteration 15 — trails through rock, “passes” (part 1) (recorded 2026-11-09 before implementation and measurement; user decision: both options, rock pass first)

**Scope and deviation from “measure references first” rule.** Measurement on committed map (G4_GATE_STATUS, “Measurement before passes”) showed rock is walkable, so pass is not accessibility need here, only trail shape. **I did not measure in references how many trails cross rock** (requires new probe code); this is deviation from rule, so criteria below are absolute, no reference band (N/A). User option 2 (rock blocking and passes as only crossings) is **separate later iteration** (needs blocking brush in profile and walkability model change); not here.

**Change (new field `SurfaceTuning::rockPasses`, 0/1, appended to end; 0 = as before, byte-identical).** At 1, path router (`PathRouter`, also door trails) can enter Rock cells at extra cost 40 per cell (base step 10–20), so trail crosses rock band only where detour is longer (narrowest band point as saddle substitute; elevations not available in router). Rock cells under trail and its border become Path.

**Measure.** `cut` = number of Path cells with Rock at distance ≤3 on both sides in one axis (left and right or top and bottom), i.e., trail “cutting” through band; counted per 65,536 cells, median [min..max], seeds 1001–1032, side 512, profiles m06/m19, `buildingDensity` 30 / 40.

**Criteria.**
1. `rockPasses` 0: all core tests and Seed 1729 fingerprint unchanged; `style_render` result identical as before.
2. `rockPasses` 1, `pathLength` from profile (40 / 80): median `cut` at least 2× larger than at 0 and `cut` > 0 in ≥ 75 % of seeds (24 of 32); trail length L (median) no more than 1.5× value at 0 (no shape regression).
3. Two runs byte-identical; 16 core tests and both G1 probes PASS; probe `--surface` still 0 blocking ford tiles, pockets connected.
4. User appearance assessment has priority.

**Upfront caveats.** Cost 40 and reach 3 are my choices without reference; trail through rock is wider than a real pass (stamp 3–4 tiles) and may look like a carved corridor; Path cells on rock use trail brush (its edges relative to rock were not checked for blocking, as with fords; this will be in probe if criterion 3 shows it); reachability unchanged (rock is passable).

### Iteration 16 — trail targets on rock far side (recorded 2026-11-09 before implementation and measurement; user decision: “Trail targets through rock”)

**Reason.** In iteration 15 trails could cross rock, but rarely did (3 and 8 of 32 seeds; even with free rock 13 and 22) because trail ends are random land points. Bottleneck is target choice, not cost.

**Change (only `rockPasses` = 1 and `pathLength` > 0; at 0 result and random stream byte-identical as before).** In `drawPaths` every other attempt (even attempt numbers) searches for target “through rock”: from random land cell (Grass or Dirt) in random of 8 directions, up to 8 attempts, max 48 steps: along the way no water allowed, must encounter sequence of 3–30 Rock cells, and after them a land cell; target is that cell shifted 4 steps further, if still land. If unsuccessful, attempt uses previous random target. Router (rock cost 40, from 15) unchanged.

**Criteria (no band from references, N/A as in 15).** Seeds 1001–1032, side 512, profile v10, `pathLength` 40 / 80, `buildingDensity` 30 / 40, `rockPasses` 1, measure `cut` as in iteration 15:
1. Median `cut` ≥ 2× larger than at `rockPasses` 0 **and** `cut` > 0 in ≥ 24 of 32 seeds, in both profiles.
2. Trail length L (median) ≤ 1.5× value at `rockPasses` 0 (166 / 295 → ≤ 249 / ≤ 443).
3. `rockPasses` 0: 16 core tests and Seed 1729 fingerprint unchanged, `style_render` result identical; two runs byte-identical; both G1 probes PASS; new test `surface`: `rockPasses` 1 is deterministic and yields different result than 0.
4. User appearance assessment has priority.

**Upfront caveats.** Constants (3–30 rock cells, 48 steps, 4 steps past rock, every other attempt) are my choices; some seeds may not have rock band in reach, then 75 % is unattainable and I will report it; trail through rock is as wide as regular trail (3–4 tiles); trail brush on rock was not checked for blocking.

### Iteration 17 — trails reach rock targets and cross water (recorded 2026-11-09 before implementation and measurement; reason: user comments on iteration 16, five GUI screenshots)

**User comments.** (1) Trail through rock “doesn't reach anywhere, ends in rock”. (2) Would prefer trail of `dirt` or choice. (3) Trail reaching river should cross to other side. (4) Jagged sand and grass edges at lake (separate issue, below).

**Diagnosis (1).** `PathRouter::connect` smooths the route with moving average (window 6), which shortens both ends by several cells; in iteration 12h I fixed this with `reachEnds` option for door trails only. Trails from `drawPaths` do not have it, and target “on other side of rock” lies 4 cells past rock, so trail ends in rock.
**Response (2).** Field “Path brush” is a list of all ground brushes; `dirt` can be selected today without code changes. I do not change default (cobblestone) unless user decides otherwise.

**Changes.**
- 17a: `drawPaths` stamps to actual start and target (`reachEnds`) **only at `rockPasses` = 1**; at 0 byte-identical as before.
- 17b: at `fordLength` > 0 router (trails and door routes) may pass through **River** cells (not Lake) with additional cost 25 per cell; water cells under trail and border become **Shore (sand), not Path** (Path on water gets blocking water edge, see 14b) and are marked with mask, so later trail does not change them to Path. At `fordLength` 0 byte-identical as before. Passage width is not limited beyond cost (rivers are narrow), `fordLength` is not its limit in this iteration.

**Measure (new, in `style_render`).** Trail “tip” = Path cell with ≤ 3 Path cells among 8 neighbors (trail end cap). `tipRock` = tips with ≥ 3 Rock cells among 8 neighbors; `tipWater` = tips with ≥ 3 River or Lake cells among 8 neighbors; counted per map. Seeds 1001–1032, side 512, profile v10, `pathLength` 40 / 80, `buildingDensity` 30 / 40.

**Criteria.**
1. `rockPasses` 1: number of seeds with `tipRock` > 0 after change ≤ 3 of 32 in both profiles (baseline state before change I record).
2. `fordLength` 48 (alongside `pathLength`): number of seeds with `tipWater` > 0 at least half smaller than at `fordLength` 0 (baseline state I record); seeds with sealed buildings for `fordLength` 8/16/32/48/64/256 I report (may change; no band).
3. `rockPasses` 0 and `fordLength` 0: Seed 1729 fingerprint and `style_render` result unchanged, 16 core tests; probe `--surface`: 0 blocking ford tiles and nearby, pockets connected; both G1 probes PASS; two runs byte-identical.
4. User appearance assessment has priority.

**Upfront caveats.** Tip is a heuristic (may also count stamp corners); trail passage through river is sandy ford 3–4 tiles (not bridge); door routes may also pass through rivers; not verified whether trail through river looks good; point (4) (jagged sand/grass edges) **is not in this iteration** — requires separate diagnosis.

### Iteration 18 — sand / grass edge smoothing (recorded 2026-11-09 before implementation and measurement; reason: user note, GUI screenshot, zoom 163%: “jagged sand and grass edges, should not look like that”)

**Diagnosis (from code, not measured on image).** Beach rings grow one cell per ring (4-neighborhood) and each outer ring clipped independently by noise threshold (`valueNoise`, step 6); sand/grass boundary thus sum of thresholds, not smooth curve: sand islands of several cells, notches and steps. Border (RME auto-border) between sand and grass doesn't smooth this at multi-cell scale.

**Change (new field `SurfaceTuning::shoreSmoothing`, 0..4 passes, appended at end; 0 = as before, byte-identical; not part of style profile; generator window: checkbox “Smooth beaches”, default checked = 2 passes).** After beach rings, before `grassRim`: for each pass, for cells of Shore, Grass and Dirt kinds at Chebyshev distance ≤ 3 from Lake cell: cell becomes Shore if in 5×5 window majority of Shore, Grass, Dirt and Lake cells are Shore or Lake (rock, river and outside area not counted), else land (Grass or Dirt, depending on Grass / Dirt majority in window; tie: Grass). **Ring 0 (cells 8-adjacent to Lake) stays sand** (beach always at water). Passes computed on a copy (no cell-order effect).

**Measure (new, in `style_render`, row `shore`).** `spurs` = Shore cells with ≤ 2 Shore or Lake cells among 8 neighbors; `notches` = Grass or Dirt cells with ≥ 6 Shore or Lake cells among 8 neighbors; `checker` = 2×2 windows with two diagonal Shore cells and two diagonal Grass or Dirt cells; `boundary` = Shore cells with 4-neighbor Grass or Dirt; `sand` = Shore cells; all per 65,536 cells. Seeds 1001–1032, side 512, profile v10 m06 and m19, profile default settings plus `buildingDensity` 0, `pathLength` 0.

**Criteria.**
1. `shoreSmoothing` 0: all core tests and Seed 1729 fingerprint unchanged, `style_render` result byte-identical as before.
2. `shoreSmoothing` 2: median (`spurs` + `notches` + `checker`) at least 5× smaller than at 0 in both profiles; `boundary` (median) no larger than at 0; `sand` (median) within ±15 % of value at 0 (beach neither disappears nor grows).
3. Hand-counted tests (new test): solitary Shore cell outside range 3 from water returns to grass; grassy hole in sand band at water is filled; simple sand band without disturbance remains unchanged; ring 0 remains; result deterministic; mask and protection do not change (handling as before).
4. 16 core tests, both G1 probes PASS after rebuilding `rme`; two runs byte-identical; generator window field “Smooth beaches” (default checked).
5. User appearance assessment has priority; from measure I cannot judge whether edge “looks good”.

**Upfront caveats.** This changes beach shape for all who check the box (style profiles were tuned without smoothing: beach composition in profiles may shift slightly, I measure through `sand`); window 5×5 and majority threshold are my choices; boundary at single-tile level may still have 1–2 cell steps (grid resolution); does not smooth grass / dirt or rock / grass boundary.

### Iteration 19 — narrow sand tongues between water (recorded 2026-11-09 before implementation and measurement; reason: user note “still looks jagged” + four GUI screenshots after 18)

**What appears in screenshots and what I measured.** (a) Narrow strips and peninsulas of sand 1–2 tiles wide between water (screenshot 4: 1-tile strip between lake and river, lone sand squares in water), formed when river (overlaid after beaches) cuts beach ring. (b) Rectangular grass “notches” in sand with straight, 90° edges (screenshot 3). Measurement on committed map (probe `--surface`, row `SAND_GRASS_EDGE`): **sand / grass boundary has border elements** (1760 of 1760 sand tiles at grass have elements, e.g. `rocks`, `grass`; grass 106), so lack of border is not the cause. Filter from iteration 18 did not affect (a) because it **counted Lake as sand** (peninsula in water had majority “sand”), did not count rivers and worked only within reach 9 from lake; (b) are 90° steps on straight segments that majority filter does not remove (not noise).

**Change (only with `shoreSmoothing` > 0, after river placement; at 0 byte-identical as before).** `trimSpits`: Shore cell lying in horizontal or vertical sequence **≤2** consecutive Shore cells **bounded on both ends by water** (Lake or River) becomes water (River if either end is River, else Lake). One pass per snapshot. Fords (iteration 14) run after this step, so accidental land cut is repaired by `fordLength`.

**Measure (`style_render`, row `shore`, new field `spit`)** = number of Shore cells meeting condition above, per 65,536. Seeds 1001–1032, side 512, profile v10, `buildingDensity` 0, `pathLength` 0.

**Criteria.**
1. `shoreSmoothing` 0: Seed 1729 fingerprint and 16 core tests unchanged, `style_render` result as before.
2. `shoreSmoothing` 2: median `spit` at least 10× smaller than at 0 (baseline state I record), `sand` (median) within ±15 %; if baseline `spit` is ≤ 2, criterion is **N/A** (measure does not describe screenshot).
3. Hand-counted tests: 2-width Shore strip through full height between water disappears; 3-width strip remains; single sand square in water disappears; sequence of 2 with grass at one end remains; determinism.
4. Seeds with sealed buildings at `fordLength` 0 and 48 I report before and after (may change, no band); 16 core tests and both G1 probes PASS; two runs byte-identical.
5. User appearance assessment has priority.

**Upfront caveats.** (b) 90° steps on straight segments are not in this iteration (boundary on tile grid always has steps; removing them would require different boundary shape than borne by noise threshold); removing narrow strip may cut land passage (fords can repair this); river appearance at edge may change.

### Iteration 20 — steps and 90° notches at sand / grass boundary (recorded 2026-11-09 before implementation and measurement; user decision: “We can tackle steps now”)

**Diagnosis (from screenshot 3 after iteration 18 and reasoning, not from image measurement).** Sand / grass boundary has rectangular protrusions of depth 1 and length several cells, and long straight segments. Majority filter 7×7 from iteration 18 does not remove them: protrusion of depth 1 and length 5 wins in 7×7 window (5 grass cells of 7 in column), so it stays. Window of height 11 loses to such protrusion (5 of 11), so removes protrusions of length ≤ 5.

**Change.** New field `SurfaceTuning::shoreRadius` (0..8, appended at end; 0 = radius 3 as before; makes sense only at `shoreSmoothing` > 0; not part of style profile). `smoothShores` gets window radius (window (2r+1)×(2r+1)) and reach from lake `3r` (before 9 for r = 3). Generator window: “Smooth beaches” sets `shoreRadius` 5 (window 11×11, reach 15). Rest of rule unchanged (ring 0 untouched, Lake counts as sand, Rock and area edge not counted, tie → Grass).

**Measure (`style_render`, row `shore`, new field `corners`).** `corners` = 2×2 windows with exactly 1 or 3 Shore cells and rest Grass or Dirt (window without Lake, River, Rock, Path, Floor and outside area), per 65,536 cells; each protrusion or notch adds four such windows, straight segment none. Seeds 1001–1032, side 512, profile v10 m06 and m19, `buildingDensity` 0, `pathLength` 0.

**Criteria.**
1. `shoreSmoothing` 0: Seed 1729 fingerprint and 16 core tests unchanged; `shoreRadius` 0 at `shoreSmoothing` > 0: result byte-identical as in iteration 19.
2. `shoreSmoothing` 2, `shoreRadius` 5 versus `shoreRadius` 0 (radius 3): median `corners` at least 25 % smaller in both profiles; `boundary` (median) no larger; `sand` (median) within ±15 % of value at `shoreSmoothing` 0.
3. Hand-counted tests (new): 1×5 protrusion in straight boundary stays at r = 3 and disappears at r = 5; straight boundary without disturbance does not change at r = 5; determinism; r = 0 as before.
4. Seeds with sealed buildings at `fordLength` 0 and 48 I report before and after; 16 core tests and both G1 probes PASS; two runs byte-identical.
5. User appearance assessment has priority.

**Upfront caveats.** Window 11×11 also rounds fine, natural beach features (coves smaller than ~5 cells disappear); 1:1 steps on diagonal segments remain (tile grid); grass / dirt and rock / grass boundary is not smoothed; radius 5 is my choice; may change beach composition in profiles (measure `sand`).

### Iteration 21 — “Generate Terrain” window layout (recorded 2026-11-09 before code change; user decision: “first need to work on GUI”, GUI screenshot)

**Problem (from screenshot).** Window has one column ~35 fields (area, weights, brushes, profile, trails, fords, beaches, buildings, walls, doors, vegetation) that does not fit on 1080p screen: “Bush brush” row is clipped, status text overlaps lower fields, and “Stone brush” and further fields are invisible.

**Change (layout only, no behavior change).** Fields go into `wxNotebook` tabs in previous order: **Area** (area, X, Y, Z, width, height, seed), **Terrain** (weights, grass / dirt / water brushes, style, profile, rock and shore brushes, “Smooth beaches”), **Paths** (“Path length”, “Ford length”, “Rock passes”, “Path brush”), **Buildings** (buildings, floor, walls, doors), **Vegetation** (trees, bushes, stones). Minimap preview, help text, status and buttons stay outside tabs. Control names (`SetName`), default values, field enable logic and Preview, Validate, Place, Undo behavior unchanged.

**Criteria.**
1. `rme` builds; 16 core tests and both G1 probes PASS (probes do not open window, so do not check layout).
2. Window opens with five tabs and has height fitting 1080p screen without field clipping and without status overlapping fields (**user assessment**, I do not perform it in GUI).
3. Each previous field is on exactly one tab, none disappeared (check in code: number of `SetName("terrain-...")` controls before and after is the same).
4. Behavior (default values, field enable logic, Preview result for same settings) unchanged: **I will not verify in GUI**, user does.

**Upfront caveats.** Layout assessment is user's alone; native `wxNotebook` may not match application's dark theme; mnemonics (&) are unique within window so far, and after split may repeat across tabs without harm to behavior; I do not add scrolling (tabs have 3–11 fields each).

### Iteration 22 — POI points and fixed start in reachability validator (PHASE 5, step 1 of plan; recorded 2026-11-09 before implementation and measurement; user decision: “let's start per this plan”)

**Scope.** G5 discusses reachability “from fixed start”: required POI, entrances and exits Z. Generator does not yet have start or POI. This iteration adds to core **pure counting functions** (no generator change, no GUI, no map write): `poiReachabilityOf(SurfaceGrid, walls)` in `reachability.*`. Scope is **one floor surface**; exits Z and dungeons are separate plan stage (default decision, since user did not answer scope question; to change on their command).

**Definitions (walkability model as in iteration 13, walls enabled).**
- **Start:** walkable cell nearest to area center (Euclidean² distance to (width/2, height/2), tie: smallest row index). Start **does not need** to lie in largest component (unlike in 13), so reachability from it is a real test.
- **Entrances (Entrance):** door cells from `SurfaceGrid::doors` (one per building); reached when belong to start component.
- **Exits (Exit):** for each of four area sides, cell on edge nearest to side center and walkable (tie: smaller index); no walkable cell on edge = side has no exit (not counted). Exit reached when belongs to start component.
- **Result:** entrance count, reached entrances, exit count, reached exits, whether start exists, start component share in walkable cells (‰).

**Criteria.**
1. Hand-counted tests: lake dividing land (start on one side: entrance and exit on other unreached); building with doors reachable, without doors has no entrance; side without land has no exit; start selects nearest walkable cell to center (tie: smaller index); empty grid = no start; determinism; cross-check with independent implementation (BFS from start) on ≥ 50 random grids.
2. Measurement (seeds 1001–1032, side 512, profile v10 m06 and m19, `buildingDensity` 30 / 40, `pathLength` from profile; `fordLength` 0, 48 and 256): seeds with **all** entrances and exits reached from start and median start component share. **Criterion pass (G5 state for surface): ≥ 29 of 32 seeds with all POI reached at `fordLength` 256 in both profiles.** At 0 and 48 I report result without band.
3. 16 core tests and both G1 probes PASS (generator core unchanged: Seed 1729 fingerprint unchanged); two runs byte-identical.

**Upfront caveats.** Start “nearest center” is my choice (in real map start is town or temple, which generator does not create); exits on sides do not know what lies in adjacent areas; POI are today only doors and edge exits (no ruins, camps, listed points); no repair or seed rejection yet (separate plan step); results concern D22 walkability model.

### Iteration 23 — reachability repair and seed rejection (PHASE 5, step 2 of plan; recorded 2026-11-09 before implementation and measurement; user decision: “Yes, step 2”)

**Scope.** Iteration 22 measures whether all entrances and exits are reachable from fixed start; at `fordLength` 48 in 3–4 of 32 seeds they are not. Two mechanisms:
- **Repair (option `SurfaceTuning::repairReach`, 0/1, appended at end, not part of style profile; 0 = byte-identical as before):** after regular fords (iteration 14) another pass of `placeFords` in repair mode: while there exists a component **containing entrance or exit** (definitions from iteration 22) and different from start component, it gets ford (same shape, mouth and natural axis as in 14d) through water (River, Lake) up to **256** cells wide to nearest other component; start is determined once, at start. Components without POI do not get fords in this pass (regardless of size, even 1 cell).
- **Seed rejection (`generateReachableSurface`, core function):** tries seeds `seed`, `seed + 1`, … up to `maxAttempts`, returns first grid where all entrances and exits are reached from start, with used seed and attempt count; when none pass, returns first attempt (original seed) with `allReached` = false. No GUI change this iteration (window gets repair only).

**Generator window:** checkbox “Repair unreachable places” in Paths tab (default checked, sets `repairReach` 1 at Natural surface mode).

**Criteria.**
1. `repairReach` 0: Seed 1729 fingerprint and 16 core tests unchanged, `style_render` result as before.
2. Hand-counted tests: two lands divided by 6-width river, `fordLength` 3 does not connect them, but repair with doors on other side does (one component); on other side land alone without POI: repair adds nothing; 1-cell component with doors gets ford; determinism; `generateReachableSurface`: returns first passing attempt, attempt count, deterministically, and at `maxAttempts` 1 first grid with `allReached` = false when it does not pass.
3. Measurement (seeds 1001–1032, side 512, profile v10 m06 and m19, `buildingDensity` 30 / 40, `pathLength` 40 / 80, `fordLength` 48): seeds with all POI reached with `repairReach` 1: **criterion ≥ 31 of 32 in both profiles** (state without repair: 28 and 29 of 32); cells added by fords (median, max) no larger than at `fordLength` 256 alone (repair is to build less); seeds without repair with `repairReach` 1 and `fordLength` 0 I report without band. Rejection: percent of seeds where first attempt does not pass, and average attempt count (with `maxAttempts` 8) without and with repair.
4. 16 core tests and both G1 probes PASS; two runs byte-identical.

**Upfront caveats.** Repair by ford up to 256 width yields very long fords (appearance assessed only to reach 48); POI are only doors and edge exits; start “nearest center” is my choice; repair does not change state when water is wider than 256 (then rejection works); rejection changes seed from what user entered (not used in window); masks and protection may break ford (Preserve).
