# G6: criteria recorded before implementation and measurement

Document G6 (PHASE 6: multiple candidates, ranking, feedback, versioning). Format as in [G5_METRICS.md](G5_METRICS.md): each iteration has criteria recorded and committed before implementation; change after measurement only explicit, with date.

## User decisions (2026-11-17)

1. **Lexicographic** ranking: hard conditions first, then style compatibility, finally tiebreaker.
2. Assessment on **existing maps**: families outside M06 and M19 according to folds [G4_SPLIT.md](G4_SPLIT.md), i.e., M01, M05, winter (M11 + M18), and M12–M17 family. User can provide excerpts of large worlds as additional holdout maps; M06/M19 excerpt is not a holdout (D17).
3. GUI feedback: Approve/Reject/Adjust and selection from candidate gallery, **local** save (JSON beside profile, outside repo).
4. ML: decision by executor. **Decision: withdrawal with justification** (“ML” section below).
5. Blind test on existing maps: 20 pairs, ranking wins in ≥ 65 % of pairs.
6. N candidates: default 4, maximum 16.

## ML — decision to withdraw (2026-11-17)

Style described by manual profiles m06/m19 (D20). Holdout data families 5, maps 19, from one client (10.98). User feedback does not yet exist. Model trained on such data cannot be fairly distinguished from overfitting to M06/M19, and each fold would have 1–7 maps. Per PROJECT, documented withdrawal does not block release. Experiment returns for consideration when at least 200 feedback assessments from at least 3 map families are collected. Then model must beat lexicographic ranking from I51 per blind test criteria from I55, recorded before experiment.

## Iteration 51 — candidate quality score (recorded 2026-11-17 before implementation and measurement)

**Scope.** Pure core function evaluating one generator result (surface grid, floor plan, reachability report) against style profile and offline tool ranking N seeds. Iteration **does not change generator, window, or map save**. Fingerprint Seed 1729 100×100 = 17159159863505677800 remains unchanged.

**Result definition (`CandidateScore`, lexicographic tuple comparison):**
1. **Hard (`hardFail`, 0 or 1):** 1 when validation fails, multi-floor reachability is not complete (`levelReachabilityOf`, D22), or fix removed locations. Candidate with 1 loses to any candidate with 0. Fragment copies addressed in I53.
2. **Bands (`inBand`, more = better):** number of shape metrics from `g5_shape_table` (paths, buildings, outline, transitions, rivers, scale; only metrics that profile has in band [0.67·ref; 1.5·ref] or absolute threshold from G4/G5) fitting within profile band.
3. **Distance (`distance`, less = better):** sum over same metrics |log(value / ref)|, with value 0 capped to log(1/64). Comparison in fixed point (×1000, rounding) so result is deterministic.
4. **Tie:** lower candidate index.

Candidate i has seed `mix(userSeed, i)` with fixed mix function in core. **Candidate 0 is exactly user seed**, so N = 1 gives byte-for-byte today's result.

**Criteria.**
1. **Manually calculated test:** 3–4 synthetic candidates with given metrics give order matching manual calculation, including cases “hardFail loses despite better bands,” tie after `inBand` resolved by `distance`, and complete tie resolved by index.
2. **Determinism:** two tool runs (profiles m06 and m19, seeds 1001–1032 as user seeds, N = 16, side 512) give byte-for-byte identical report. Candidate 0 = current result (verified by fingerprint).
3. **Out-of-sample test (unbiased).** Result computed against profile bands, i.e., in-sample M06/M19. Evaluation is distance of same metrics from holdout maps, computed on them with G4 tools (M01, M05, M11, M18, M12–M17, separately per map, against profile closest in style per [G4_COMPARE.md](G4_COMPARE.md)). For each pair (user seed, profile), ranking winner among 16 has holdout distance **less than median candidates** in **≥ 70 %** of pairs, for each of both profiles separately. Report also gives random selection result as reference (expected ~50 %).
4. **No regression:** `g5_run_suite.py` → SUITE_PASS with probes, gate `g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1` → 32/32 m06 and m19, Seed 1729 fingerprint unchanged.

**Caveats up front.** Shape metrics measure surface only (cave and floors enter only via hard condition). Holdout map assignment to profile is manual. Bands come from two maps (D17), so criterion 3 checks only whether result is not worse than random selection on unseen maps, not whether it is good style assessment. Blind test in I55 settles this.

## Further iteration plan (criteria will be recorded before each iteration)

### D23 — occupied floor and one Undo regression (2026-11-19)

First scope of orderly work after G5 includes evidence of editor operation.
Probe with real client checks cave and dungeons with occupied z+1
in cases with house tiles and spawn. Fixture must actually have
cave and at least one dungeon. Existing tiles, metadata, and indices
remain identical in Preview, Place, Undo, and Redo. Generated part
avoids protected area per G5 rules, and Place remains one history step for all floors. We report actual probe coverage.

We run existing capacity probe 500×500 with one step. Result
passes preview and Place consistency and full Undo/Redo without write outside
mask. Limit refusal leaves map and history unchanged and is reported
separately. Larger cases remain for separate assessment. Regression preserves
SUITE_PASS, G5 --i47 32/32 both profiles, fingerprint 1729, and default mountain.
Editor evidence does not close in-game movement or manual tab assessment.

### I56 — local profile learning from user's open map

User can open own compatible map in RME, then select
“Learn from open map” in editor. First scope includes one open
map and explicitly indicated surface rectangle on one floor from range
0–7. Standard path works without manual CLI run. User
sees area, count of recognized and skipped tiles, and analysis result.
Before analysis, confirms right to use map. Data remains local.

We use existing semantic statistics and deterministic
G4 matching. Class mapping is explicit and based on names and
current client brushes. Unknown ground remains unrecognized.
Sample with fewer than 1024 recognized natural ground tiles or with
coverage below 50% of present tiles refuses matching, giving numbers.
Rectangle has dimensions within map bounds, at most 512×512. User
sees applied mapping and can correct assignments before analysis.

Client compatibility tied to same OTB/DAT identifiers as
in existing profile offer. Profile has explicit user origin,
no map names/paths and no assignment to M06/M19 reference. Save after
confirmation, under chosen name, to local profile directory. Name
cannot escape that directory. Existing file protected against
overwrite, write is atomic. Profile after save appears in normal
generator selection and passes existing validation and I54 history.

Analysis uses map reading on GUI thread in limited portions,
and matching operates on own data outside it. Progress, error, and cancellation
visible. Map, client, or settings change invalidates result. Analysis,
cancellation, and profile save do not affect map or Undo queue.
Scope covers surface style matching. ML, fragment copying,
multi-map learning, and dungeon matching stay outside this step.

Tests: identical input gives identical profile, too small sample, unknown
grounds, bad name, no use right, name conflict, cancellation,
stale map, client compatibility. Real client probe: map →
profile → generator offer → preview → Place/Undo/Redo. Regression: build,
SUITE_PASS with probes, G5 --i47 32/32 both profiles and fingerprint 1729 unchanged.
I56 is workflow extension. Does not pass blind test I55 nor G6.

### I55 — preparation for accepting new families (2026-11-19)

Before measurement, tool verifies documented provenance, use right,
and declaration that maps did not participate in analysis or tuning. Rejects
known identities and related families per frozen G4 rules. Excerpts
of one world remain one family. Frozen manifest ties units,
client, mapping, kinship evidence, and metric inputs via SHA-256.
Comparison uses only accepted, verified packages.
Manifest created first, then L2 and L3 distances can be computed.
Input change after freezing blocks measurement. Missing real data
leaves test blocked. Synthetic cases verify tool,
and their results do not count toward gate. I55 threshold and aggregation unchanged.

### I55 — ranking assessment and user blind test (before implementation and measurement)

We compare same generator and same pool N=4 (default I52 count).
L2 is operational control: first acceptable result in index order
per existing rules, without style assessment. L3 is best acceptable per A.
Both sides have identical parameters, hard conditions, and I53 index M06/M19.
I54 feedback recorded in versioned format, without ranking update
during experiment. No acceptable result means no pair and blocks
full pass of that series. We do not replace seeds after viewing results.

**Blind test:** exactly 20 pairs, seeds 3001–3010 for m06 and m19, side 512,
I51 parameters with G5 geometry and N=4. Both sides have identical area,
view set, and scale. If same result selected, pair still in test.
Package for assessor reveals only anonymous id and sides A/B.
Side assignment frozen before assessment, with balance 5/5 each profile,
in separate local key. Assessor chooses A/B/tie per overall
preference, with optional note on naturalness, style, diversity, or playability. All 20 assessments required before key revealed.
L3 success is ≥13/20, ties are not wins. Report separately gives ties,
proportions per profile and count of identical pairs. Package of semantic views
assesses shape and terrain distribution. Playability confirmed by separate tests and probes.

**Quantitative comparison:** shared pool as above, seeds 3001–3032 per profile.
Same 26 metrics, N/A, distances, and equal family weights as I51. We report
L2 and L3 distances, count of strict L3 wins on 32 seeds per profile,
ties, median difference, and coverage. As comparison condition we take
≥23/32 strict L3 wins separately per profile on new families.
Beside we report in-sample M06/M19 results and diagnostic M01/M05/M11+M18
already seen in I51/I51b. M12–M17 at most separate addendum. Discrepancies
between samples described as possible overfitting, accounting for coverage.
I53 control reports rejections, missing assessments, and their denominators.

New families must be outside earlier analyses and tuning, with explicit
provenance and use right, and pass G4 kinship control before
L2/L3 results revealed. Their manifest and unit choice for assessment frozen
before measurement. Excerpts of same world remain one family. Missing
such data blocks quality claim on unseen maps, but blind preference test
and diagnostic comparison can be prepared. These do not substitute that proof. I51/I51b preserve historical results. I55 PASS requires both
assessments and regression. G6 remains open until evidence collected.

Tests: completeness/parity of sides, refusal of incomplete assessments, deterministic
choice and result, identical pairs, ties, boundaries 12/13 and 22/23, missing sources
and insufficient coverage. Materials, key, and assessments in scratchpad only.
Code, instructions, and result without private data to Git. Build, SUITE_PASS
with probes, G5 --i47 32/32 both profiles and fingerprint 1729 unchanged.

### I54 — local feedback and profile history (before implementation)

1. Surface: explicit Approve / Reject / Adjust for currently selected and
   prepared candidate. Approve means acceptable result, Reject can be
   recorded even for hardFail. These are single, explicit assessments, independent of
   Place. Gallery selection alone does not record assessments of other candidates. Optional
   note up to 1024 UTF-8 bytes with visible over-limit error.
2. Local, versioned JSON in user's profile directory. Entry ties
   assessment to client and resource identity, profile and version or built-in
   settings, index, initial and actual seed, floor fingerprints,
   and format/policy version. Entry has permanent unique identifier.
   No map paths, map names, or tile data. We preserve earlier assessments.
3. Adjust consciously records current supported style tuning as new
   local version based on selected, acceptable candidate. GUI
   shows range of recorded parameters. History preserves previous
   versions and their relationships. Base profiles read-only. Older
   format profiles still work. Local version preserves existing
   client compatibility limits and band origin. No base profile
   disables Adjust, but allows candidate assessment.
4. Return to chosen compatible version changes active profile version without
   deleting newer versions. Operation explicit and invalidates I52 series.
   Next Adjust after return creates new version with chosen version as
   parent. After restart, assessments, history, and version choice persist. Atomic write and limited loading. Validation, write, or write
   conflict error reported clearly, without overwriting previous
   data. Unknown format version not automatically reset.
5. Controls inherit Theme and keyboard support. Stale preview does not
   accept feedback. Feedback, Adjust, and return write nothing on map
   or in Undo. Place remains single Undo, with I52/I53 blocks.
   No automatic ranking change, remote sync, or ML.

Tests: read and re-open history, all three actions, profile and
client data compatibility, UTF-8, corrupted/alien format, write error,
conflict, rollback, and Adjust from earlier version. Client probe verifies
selection of another candidate, write its assessment, refusal of old result,
rollback, no map/Undo changes, and Place/Undo/Redo. Build, SUITE_PASS with
probes, G5 --i47 32/32 both profiles and fingerprint 1729 unchanged. Manual
GUI assessment and quality blind test reported separately. Private history outside Git.

### I53 — compatible-fragment detection (2026-11-18, before implementation)

Scope: semantic fingerprint of candidate surface and comparison with local
reference index used by profiles. We use `similarity` algorithm
from G4: 16×16 windows, at least 50% present cells, skip uniform
windows and sample by content modulo 4. Comparison ignores position and floor.
References and generator use identical surface class names and same
presence convention. We recompute references from semantic data and
approved G4 mapping, never from existing ground-name hashes. Unknown
and Preserve are absent. Report lists mappings and class limits.

Local index has format version, client, fingerprint parameters, and
source manifest. Only M06/M19 enter decision. M01/M05/M11/M18 nor
M12–M17 maps can feed index or filter. Windows present in both maps
removed from both references and candidate before comparison. Report contains
window counts before and after filter and common unique hashes per reference.
Hard condition: for any single reference ≥16 common unique
hashes and ≥100‰ containment against candidate fingerprint after filter. Thresholds
from G4 and remain frozen. Fewer than 16 windows after filter means
“too little data,” without declaring no copies. Index without sufficient
reference, with error, or incompatibility gives no assessment result.

In offline mode with given index and in Surface GUI with active compatible
local index, copying meeting both thresholds gives hardFail before
ranking A and blocks Place even after forced call. Without index
or too small sample user sees similarity not assessed.
If explicitly configured index corrupted/incompatible, series reports
error instead of skipping check. Check does not modify map. In GUI uses
existing cancellation, result actuality, and I52 shared budget.

Tests: manual arithmetic of both thresholds; copy shifted with different
key order; independent terrain; common, rare, uniform windows and too small sample;
incompatible/corrupted data and cancellation. Probe: detected copy blocks Place,
clean acceptable candidate still has one Place/Undo/Redo. If local inputs available,
report for m06/m19, seeds 1001–1032, N=1, side 512,
I51 settings with --g5-levels, two identical runs. We record all
hits without threshold correction. Class hash agreement does not prove
copying by generator. Check does not cover items or dungeon geometry.

Regression: canonical build, SUITE_PASS with probes, G5 --i47 32/32 both
profiles, fingerprint 1729 100×100 unchanged. I53 report and GUI steps in
G6_GATE_STATUS. Private indices and maps outside Git.

### I52 — candidates in GUI (2026-11-18, before implementation)

Scope: existing Surface generator window, N default 4, range 1–16,
gallery/list with thumbnail, index, actual seed, and validation state.
Ranking A remains unfulfilled baseline: UI does not promise quality and does not
use B. For recognized m06/m19 profile applies explicit g6-bands-v1 bands;
without assigned policy shows no ranking, does not guess profile.

1. Candidate i uses candidateSeed(userSeed,i); candidate 0 preserves seed.
   N=1 preserves current result and fingerprint 1729 100×100.
   Existing validation retries explicitly reported with used seed;
   may not present seed after retry as original. Two runs
   with same inputs give same results and recommendation choice.
2. For available ranking, best acceptable A selected by default;
   user may choose another acceptable candidate. Selected and
   recommended distinguished by text, not color alone. Without ranking
   default first acceptable. May not save hardFail candidate,
   unvalidated, incomplete, or from old parameter set.
3. Hard condition assessment considers actual adapter validation and
   reachability, not only offline approximation. Selection changes preview
   with existing mechanism; generation and switching do not mutate map.
   Place records exactly selected result, one Action/Undo; Redo reproduces
   it without re-randomization. Map/parameter change invalidates candidates.
4. Generation off GUI thread, progress and cancellation. Window close,
   cancellation, map change, and late result cannot touch stale
   window/map. Memory has explicit total budget for entire series, not N independent
   full budgets. No space causes readable error without partial Place;
   may not silently reduce N. We do not store N full map copies.
5. Existing layout and Theme; selection available via keyboard, visible focus,
   readable error and empty result state. No feedback approve/reject/adjust
   and profile history (I54), no generator change, mountain, or map format change.

Tests: N=1/4/16 and boundaries, both profiles, fallback without bands, A order,
choice of non-recommended, hardFail blocks Place, cancellation/stale results,
series budget and no mutation before Place. Real client probe:
candidate selection -> Place -> one Undo -> Redo and preview consistency.
Build; g5_run_suite with probes SUITE_PASS; G5 --i47 rockBlocks m06=0,m19=1
32/32; fingerprint unchanged. Separate manual GUI report (if not performed,
do not announce full I52 pass). No claims about quality improvement.

### I51b — ranking order experiment (2026-11-18, before implementation and measurement)

**User decision:** prepare separately registered ranking improvement experiment.
I51 remains FAIL (m06 18/32, m19 18/32). Below experiment
does not replace historical result or I51 criteria.

**Hypothesis, not settled:** discrete count of matching bands may mask
smaller, continuous errors of other metrics. We check one order change,
without weight learning, metric selection, or new variants after viewing results.
Proposed variant outcome not yet computed on I51 reports.

**A (control):** (hardFail ascending, inBand descending, distance ascending, index).
**B (single experiment):** (hardFail ascending, distance ascending, inBand descending, index).
Both are lexicographic ranking with hard condition first.
distance remains total sum from policy 396e057. g6-bands-v1 bands,
26 metrics, N/A, units, cap, and all generator parameters unchanged.
We do not change production default A order during experiment.

**Data and execution:** new user seed matrix 2001–2032, N=16,
side 512; m06/m19 profiles, pathLength/buildingDensity 40/30 and 80/40,
rockBlocks 0/1, --g5-levels. Same candidate set serves A and B,
without retrying or removing failed seeds. Two full runs must
give byte-for-byte identical reports. Candidate 0 preserves user seed.
Historical seeds 1001–1032 can be assessed only after code frozen,
solely as additional result, without participation in B acceptance decision.

**Main assessment:** same maps M01,M05,M11,M18 and average over three families,
as in I51. M12–M17 additional only. Success = winner hardFail 0
and holdout distance strictly below median of all 16 candidates.
Threshold for B: at least 23/32 separately for m06 and m19. Moreover B must have
strictly more successes than A on new matrix in each profile.
Conditions joint; failure of any means I51B_FAIL,
without B acceptance and no automatic criteria change. We do not declare
statistical significance based on threshold crossing alone.

Report includes A and B per seed, map, and family; improvement/degradation/
no change pairs, winner count changes, hardFail, inBand, and distance of winners,
random baseline computed exactly as in I51, coverage and N/A reasons.
We do not use holdout for winner selection; serves only evaluation.
Data error invalidates run, not quality failure or N/A.

**Proof limitation:** maps assessed already in I51; new seeds do not make
them unseen again. I51b is development experiment after holdout disclosure.
Even PASS does not close G6 or replace assessment on new maps
or blind test I55. We do not select new maps based on B results.

**Tests and regression:** manually calculated case where A and B select
different candidates; hardFail and complete ties; independent reproduction of both
orders in Python; pair aggregation and refusal of incomplete matrix.
Build, g5_run_suite with both probes → SUITE_PASS; G5 --i47 with rockBlocks
m06=0,m19=1 → 32/32 both profiles; fingerprint 1729 100×100 unchanged.
No GUI, Place/Undo, generator, or map format changes. Implementation by
Codex medium agent in worktree, after this commit; review, ff-merge,
independent regression, G6_GATE_STATUS report, and build-ninja worktree cleanup.

### I51 — binding supplementation before implementation and measurement (2026-11-17)

Below rules replace earlier unspecified metric choices,
distance, and holdout assignment. Record after partial implementation b828824,
but before ranking and holdout measurement; earlier matrix measured only
reproducibility, without bands. We do not change generator or pass threshold.

Fixed set of 26 metrics for both profiles. B(r)=[0.67r;1.50r],
A50(r)=[max(0,r−50);r+50], U20(r)=[0;r+20]; bounds inclusive,
stored exactly in hundredths of unit.

| Metrics in order | m06 references | m19 references | Band |
| --- | --- | --- | --- |
| channels.L,W,B,C | 384;2898;41134;614 | 302;4337;38913;865 | B |
| scale.E16,E64 | 394;233 | 425;197 | B |
| patch.D,F,P | 421;749;5638 | 451;625;5656 | B |
| patch.RB,RF,RS | 333;167;36 | 121;862;15 | B, poza m19 RS: U20 |
| trans.R0,R1,R2,R3 | 33;938;24;4 | 99;502;251;147 | A50 gdy r<50, inaczej B |
| trans.D0,D1,D2,D3 | 951;32;12;4 | 728;155;27;88 | A50 gdy r<50, inaczej B |
| paths.L,W | 70;3642 | 78;4606 | B |
| build.per64k,long,short | 31;9;7 | 37;9;7 | B |
| outline.median | 30 | 24 | B |

Sources: G4_GATE_STATUS, channel tables (129–134), scale (143), soil/beach
(162), paths (209–212), transitions (263), buildings (281–284);
G4_METRICS: scale (186), outline (295), band rules (107,123,142,148,178,214,243,285).
P denotes corrected P′. Transitions R0..R3=RD/RG/RS/RW, D0..D3=DG/DR/DS/DW.
E4 is control; paths.B disabled historically as artifact (I9d).
rockadj/rocknear have no reference bands for both profiles. rivers.T/S/W
did not receive such bands, later channel measurement covers networks wider.
All eight transitions remain active. Remaining metrics reported
diagnostically, no additional votes in ranking.

**N/A and distance.** Each candidate has 26 positions; we do not change denominator
or set depending on candidate. Missing column, wrong count, or measurement failure
invalidates run. Definitive N/A not technical error.
No channels/paths gives length 0, no buildings density 0; widths,
sides, and outline without objects remain N/A. We preserve G4 count thresholds.
N/A with known reference: 0 inBand points and contribution C=ln(64).
Negative excesses are correct measurements. We apply bands to raw values.
For non-negative metrics and ref>0: d=min(C,abs(ln(value/ref))), zero gives C.
For ref=0: d=min(C,ln(1+value/u)), u=1 native unit.
For signed excesses: d=min(C,abs(asinh(value/u)−asinh(ref/u))), u=1‰.
distance=round(1000·sum d), one rounding after sum in table order.
This explicit expansion of earlier definition, including penalty limit
and excess transformation; not declaration of current code behavior.
Order: hardFail ascending, inBand descending, distance ascending, index ascending.

**Holdout.** We compare each profile with all maps M01,M05,M11,M18,
without choice of closer style. Distance per map = average of 26 contributions.
Reference N/A contributes constant C for all candidates; report shows
causes and count of differentiating positions. Map with no such position
blocks assessment. Winter = average M11/M18; main result = average of three families
M01, M05, winter (at 1/3 each). M12–M17: separate M19 family report only.
Median of 16 candidates is average of positions 8 and 9. Pair success requires hardFail=0
and winner distance strictly below median. Tie loses; we do not remove
hardFail, N/A, or seeds from denominator. Threshold: at least 23/32 for each
profile. Random baseline computed exactly as share of candidates meeting
same condition for each seed, then average over seeds.
Report contains results per map, family, and main result, also failures.

Matrix: seeds 1001–1032, N=16, side 512, two identical runs;
m06 pathLength/buildingDensity=40/30, m19=80/40, rockBlocks=0/1,
geometry --g5-levels. Candidate 0 preserves user seed.
Tests cover new distance rules, N/A vs error, band boundaries,
family aggregation, even median, ties, and exact random baseline.
Holdout maps analyzed earlier in G4: assessment beyond selected
families, not completely untouched research set. We freeze list
accepted by user, do not select it again after results.

### Holdout correction — user decision 2026-11-17

Before ranking and holdout measurement, user accepted correction of division:
independent G6 holdout includes M01, M05, and winter family M11+M18.
M12–M17 reported separately as M19 related data per G4_SPLIT.md;
do not enter holdout threshold pass from criterion 3 I51. This correction
replaces earlier definition of M12–M17 as family outside M19.
Does not change ≥70% threshold or requirement for separate result for both profiles.

- **I52:** N candidates in generator window (default 4, max 16), selection of best, gallery; one Undo on Place.
- **I53:** memorization test: result fingerprint against reference (`similarity.*`) as hard condition.
- **I54:** feedback Approve/Reject/Adjust, local history, versioned profiles with rollback.
- **I55:** G6 assessment: holdout against L2/L3, overfitting, blind test 20 pairs (≥ 65 %).
# D25 — portable G5 measurement profile path

Measurement should find G4 profile directory in `local-data` of main
checkout also from worktree. Explicit `--local-data` parameter indicates
alternative source. Missing profile ends run before generation
with message containing missing path. Test includes directory
with spaces and I47 command behavior. I47 result should remain 32/32
in both profiles, with identical repetitions. Generator code outside scope.

# D21 — reproducing G4 measurements from iterations 3–4

One command runs `style_styles` and `water_eval` with explicit local data
and writes results and parameters to scratchpad. Two runs on
same inputs give identical measurement results. Agreement with
G4 tables shown on available historical inputs. Missing data and deviations
described explicitly, without changing historical values and without generator change.

# D10 — rotated and reflected copies

Check all eight square symmetries for fixed-beforehand
fragment each of M06 and M19. Criterion is detection of each copy at
I53 thresholds: at least 16 common windows and 100‰ containment.
Check on independent terrain without hits. Generator matrix for
seeds 1001–1032 should remain at 0/32 for each profile in two
identical runs. Fingerprint changes require rejection of old
index format with rebuild instructions. When criterion not met,
describe results without tuning thresholds and leave Place block unchanged.
Regression: build, SUITE_PASS, I47 32/32 per profile and preserved fingerprint 1729.

# D1 and D4 — window close and Preview memory

- D1. Reproduce described case File → Exit on client 10.98.
  If occurs, remove SetFocus warning during close.
  Check exit code, close cancellation, and normal tab close.
  If no reproduction, record conditions and result without blind fix.
- D4. Repeat series of 30 and 200 Preview cycles with same settings.
  Measure private memory at points after warm-up and after dialog close.
  Compare with heap data if available, and provide diagnosis boundaries.
  Private memory growth alone insufficient to claim leak.
  Eventual fix preceded by separate criteria.
