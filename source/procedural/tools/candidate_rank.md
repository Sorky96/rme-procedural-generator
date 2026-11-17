# I51 — frozen ranking and holdout evaluation

Policy: the binding supplement in [G6_METRICS.md](../../../docs/procedural/G6_METRICS.md),
commit `396e057`. I51/G6 acceptance remains the integrator's decision.
Generator, GUI, persistence, `mountain` default and item definitions are unchanged.

## Core and policy

`candidate_score.{h,cpp}` measures a read-only surface through existing G4 functions.
The first 26 `candidateMetricNames` follow the binding table order; the remaining
seven (E4, rivers T/S/W, paths B/rockadj/rocknear) are diagnostics only.
The explicit policies are [m06.bands](../../../docs/procedural/g6-bands-v1/m06.bands)
and [m19.bands](../../../docs/procedural/g6-bands-v1/m19.bands).
Bounds use exact integer hundredths, inclusive. The CLI requires exactly the
26 positions, without duplicates or diagnostic votes. The pure scoring helper
also accepts subsets for isolated mathematical tests, never dropping an N/A
position from its supplied policy.

Ordering: hardFail ascending, inBand descending, distance ascending, index ascending.
The hard conditions remain validation, full level reachability including walkable
Z edges, unrepaired results and removed places (POI/caps/storeys/dungeons).
An all-hard-failed group still has a winner with that flag.

With C=ln(64), definitional N/A contributes C and zero band votes. Nonnegative
values use min(C,abs(ln(value/ref))); zero with ref>0 contributes C. Reference
zero uses min(C,ln(1+value)), including zero distance for value=ref=0. Signed
excess uses min(C,abs(asinh(value)-asinh(ref))) in native permille. Raw values
are checked against raw bands. Distance is rounded once after summing in table
order: `round(1000 * sum(contributions))`. Negative nonnegative metrics,
malformed input, missing columns and measurement failures are technical errors.

Absent channels/paths/buildings give zero length/density; absent widths, sides
and outline give N/A. G4 patch/contact denominators remain unchanged. A lag
without natural pairs retains G4's defined excess of zero. Reference qualification remains at least five
components/buildings; generator measurements retain G4's per-seed rules.
Outline retains the G4 upper-middle integer measurement; the holdout median
of 16 candidates separately averages positions 8 and 9.

`candidateSeed(userSeed,0)==userSeed`; further indices use the unchanged SplitMix64
finalizer and unsigned arithmetic. N=1 retains the original surface, including
1729, 100×100, fingerprint `17159159863505677800`.

## Commands and reports

```text
candidate_rank PROFILE SIDE FIRST_USER_SEED LAST_USER_SEED
  [--count 1..16] [--bands FILE]
  [--path-length 0..400] [--building-density 0..100]
  [--rock-blocks 0|1] [--g5-levels]
```

Default N=4. Without bands: explicit measure-only mode and no winner. Band format:
`g6-ranking-bands/1`, then `metric reference lowerHundredths upperHundredths`.
Shape references remain separate from tuning in `g4-profile/6`. TSV includes
column names, actual policy, original pre-level surface fingerprint and
post-build/repair surface metrics. Exit 2 means input/pipeline error, 4 scoring
error. A partial report with nonzero exit is not a measurement result.

`--g5-levels` selects existing I47 geometry without seed retries. It uses the
offline conservative blocking model of `style_render`, not the final adapter mask.

```text
rtk proxy py -I docs/procedural/smoke/g6_measure_candidates.py EXE PROFILE_DIR scratchpad/i51-matrix --g5-levels
rtk proxy py -I docs/procedural/smoke/g6_evaluate_holdout.py PRIVATE_ROOT PROFILE_DIR scratchpad/i51-matrix scratchpad/i51-holdout.json
```

The matrix uses frozen policies by default: m06/m19, seeds 1001–1032, N=16,
side 512, twice; path length/building density 40/30 and 80/40, rockBlocks 0/1.
Repeats must match bytes. Evaluation independently recomputes scores from TSV
and verifies all rows, winners, settings and frozen bands before holdout.

Private inputs are explicit paths under PRIVATE_ROOT: g4-channels-20261108a,
g4-scale-20261108a, g4-trans-20261108a, g4-build-20261108a, each M01–M19.
Audit requires successful verification receipts, source identity across bundles,
client 10.98, identical DAT/SPR hashes and keymaps, consistent repeated
channel/scale/path summaries where present, and complete required summaries.
Profiles must match client, surface floors, DAT, keymap and frozen M06/M19 shape
references. No private map, profile or image is copied into the repository.
Receipts do not expose an OTB checksum: this audit does not independently prove
the OTB binary's identity.

JSON records reference metrics, N/A reasons, differentiating-position counts,
and per-seed results for every map/family, including failures. Every map distance
averages exactly 26 contributions; reference N/A gives constant C. Zero
differentiating positions blocks evaluation. M01/M05/winter each weigh one third;
winter averages M11/M18. M12–M17 are reported separately as the related M19 family.
Both profiles use all these maps. Success requires hardFail=false and winner
distance strictly below the median of all 16 candidates. Ties, hard failures
and N/A keep their denominators. Random baseline is the eligible fraction per
seed averaged exactly with Fraction, reported as a rational and decimal.
Threshold: at least 23/32 separately for each profile. Evaluator exits:
0 threshold passed, 1 valid result below threshold, 2 blocked measurement.

## Validation and handoff

CTest registers the core test and, when Windows `py` exists, CLI/holdout tests.

```text
rtk proxy py -I tests/test_candidate_rank.py EXE scratchpad/cli-tests
rtk proxy py -I tests/test_g6_holdout.py
```

Hand-counted fixtures cover ordering, exact boundaries, zero references, signed
excess across zero, caps, fixed N/A penalty, one final rounding, all 26
contributions, family weighting, even medians, strict ties, hard failures and
exact baseline. Synthetic malformed TSVs are refused; definitional N/A remains
measurable. No fixture fits bands to generator output.

Results: [g6_i51_report.md](../../../docs/procedural/smoke/g6_i51_report.md).
The parent independently reviews/integrates, runs build_ninja.bat, G5 suite with
probes and G5 --i47, updates the gate report and cleans its build directory.
GUI, final adapter masks, game/original-RME compatibility and cross-compiler
libm determinism are outside isolated-core validation. G6 remains open.
