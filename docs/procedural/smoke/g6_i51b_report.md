# I51b — executor report, 2026-11-18

Prerecorded at `9ab1ca0`, G6_METRICS I51b. Metrics policy: `396e057`.
**Result: I51B_FAIL. G6 remains open.**

Only the explicit ordering experiment was implemented: A = hardFail, descending inBand,
distance, index; B = hardFail, distance, descending inBand, index.
Default ranking A, generator, GUI, map serialization, g6-bands-v1 bands,
26 metrics, units, N/A and criteria were unchanged.
Default mountain and the lack of item-ID dependencies remain unchanged.

## Matrix and decision

New seeds 2001–2032, N=16, side 512, --g5-levels; m06 length/density
40/30, rockBlocks=0; m19 80/40, rockBlocks=1. A/B use shared candidate rows,
two complete runs and 2048 generations in total. Each profile run was split into
four independent blocks of eight seeds, merged in canonical order. There were no
retries, seed removal or additional variants. The historical private 1001–1032
matrix was not evaluated.

| Profile | A | B | Exact baseline A=B | B >=23 and B>A | Winner changes |
| --- | --- | --- | --- | --- | --- |
| m06 | 29/32 | 28/32 | 125/256 | FAIL | 26/32 |
| m19 | 27/32 | 21/32 | 1/2 | FAIL | 23/32 |

B does not outperform A for either profile. In addition, m19 B=21/32 does not
reach the 23/32 threshold. This is a meaningful quality measurement below the
acceptance conditions (evaluator exit code 1), not a data-integrity failure.

Acceptance conditions are conjunctive and separate for both profiles. The result does
not tune bands, weights, parameters or threshold. B remains an explicit experimental
mode and does not replace production A. I51 retains the historical 18/32 FAIL for both
profiles. I51b does not close G6. The holdout was disclosed earlier, and new seeds do
not restore blindness. No statistical significance or quality claim is made for new maps.

Success requires hardFail=0 and distance strictly below the median of all
16 candidates (the average of positions 8 and 9). Ties lose. The primary score is
(M01 + M05 + (M11+M18)/2)/3. M12–M17 are supplementary only. Baseline is the exact
mean share of candidates meeting the same condition in each seed. hardFail and all
seeds remain in the denominator.

## Results by map and family

Improvement/regression/tie mean a smaller/larger/equal holdout distance for B versus A,
without rounding. JSON records success/failure transitions separately, including cases
where the winner changed but distance did not.

### m06

| Map / family | A successes | B successes | Exact baseline | Improvement / regression / tie |
| --- | --- | --- | --- | --- |
| M01 | 11/32 | 10/32 | 213/512 | 10 / 14 / 8 |
| M05 | 19/32 | 22/32 | 63/128 | 17 / 9 / 6 |
| M11 | 26/32 | 25/32 | 249/512 | 16 / 10 / 6 |
| M12 | 26/32 | 25/32 | 249/512 | 9 / 17 / 6 |
| M13 | 7/32 | 4/32 | 63/128 | 10 / 16 / 6 |
| M14 | 22/32 | 27/32 | 249/512 | 13 / 13 / 6 |
| M15 | 23/32 | 27/32 | 125/256 | 18 / 8 / 6 |
| M16 | 0/32 | 0/32 | 3/512 | 0 / 0 / 32 |
| M17 | 0/32 | 0/32 | 3/512 | 0 / 0 / 32 |
| M18 | 20/32 | 23/32 | 31/64 | 13 / 12 / 7 |
| family.M01 | 11/32 | 10/32 | 213/512 | 10 / 14 / 8 |
| family.M05 | 19/32 | 22/32 | 63/128 | 17 / 9 / 6 |
| family.M19-related | 26/32 | 28/32 | 249/512 | 8 / 18 / 6 |
| family.main | 29/32 | 28/32 | 125/256 | 14 / 12 / 6 |
| family.winter | 25/32 | 26/32 | 249/512 | 17 / 9 / 6 |

### m19

| Map / family | A successes | B successes | Exact baseline | Improvement / regression / tie |
| --- | --- | --- | --- | --- |
| M01 | 19/32 | 17/32 | 253/512 | 9 / 14 / 9 |
| M05 | 25/32 | 24/32 | 1/2 | 9 / 14 / 9 |
| M11 | 16/32 | 12/32 | 255/512 | 8 / 15 / 9 |
| M12 | 16/32 | 27/32 | 1/2 | 16 / 7 / 9 |
| M13 | 6/32 | 7/32 | 255/512 | 12 / 11 / 9 |
| M14 | 15/32 | 21/32 | 255/512 | 16 / 7 / 9 |
| M15 | 27/32 | 23/32 | 1/2 | 12 / 11 / 9 |
| M16 | 11/32 | 13/32 | 249/512 | 11 / 11 / 10 |
| M17 | 11/32 | 13/32 | 249/512 | 11 / 11 / 10 |
| M18 | 13/32 | 14/32 | 255/512 | 10 / 13 / 9 |
| family.M01 | 19/32 | 17/32 | 253/512 | 9 / 14 / 9 |
| family.M05 | 25/32 | 24/32 | 1/2 | 9 / 14 / 9 |
| family.M19-related | 14/32 | 21/32 | 1/2 | 15 / 8 / 9 |
| family.main | 27/32 | 21/32 | 1/2 | 10 / 13 / 9 |
| family.winter | 13/32 | 16/32 | 255/512 | 11 / 12 / 9 |

Full A/B results per seed, map and family, winner scores (index,
hardFail, inBand, distance), median, baseline and pair transitions:
[g6_i51b_holdout.json](g6_i51b_holdout.json). Raw reports and empty stderr remain
in this worktree's `scratchpad/i51b-matrix/`.

## HardFail and coverage

| Profile | Shared hardFail candidates | A hardFail winners | B hardFail winners |
| --- | --- | --- | --- |
| m06 | 10/512 | 0/32 | 0/32 |
| m19 | 1/512 | 0/32 | 0/32 |

| Map | Known reference /26 | Distinct m06 contributions | Distinct m19 contributions |
| --- | --- | --- | --- |
| M01 | 8 | 6 | 5 |
| M05 | 8 | 8 | 7 |
| M11 | 7 | 6 | 5 |
| M12 | 10 | 9 | 8 |
| M13 | 7 | 7 | 6 |
| M14 | 10 | 10 | 8 |
| M15 | 11 | 11 | 10 |
| M16 | 5 | 3 | 2 |
| M17 | 5 | 3 | 2 |
| M18 | 5 | 4 | 3 |

Low coverage and the cap limit holdout resolution. Reference N/A gives all candidates
the constant ln(64), without removing positions. JSON gives the reason for each N/A
(count <5 or G4 thresholds for patches/boundaries) and the metrics that actually
differentiate candidates. An input error blocks evaluation. It is not N/A.

Private-input audit: PASS for M01–M19, client 10.98, matching map versions,
PASS confirmations, import/semantic/verification sources, DAT/SPR, keymap
and repeated summaries. Four explicit series: g4-channels-20261108a,
g4-scale-20261108a, g4-trans-20261108a, g4-build-20261108a; profile
g4-profiles/10.98-v10. Maps, profiles and images were not copied to Git.
The complete referenceAudit, including profile hashes, is identical to the I51 report.

## Repeatability and freeze

Two reports for each profile are byte-identical. All matrix processes exited with code 0 and
empty stderr. The evaluator independently reproduced 1024 core scores and both selections
before reading the holdout. An additional audit reproduced all 1024 candidate seeds
(SplitMix64), checked pair sums and confirmed that success changes matched A/B transitions: PASS.

| Profile | SHA-256 report for each run |
| --- | --- |
| m06 | `224733843d77dbf81109029df6337dc783ce3690dfa36b22dd652509d01e6389` |
| m19 | `9c5ddb61a3bccb195ea5119dd6e6807a01765a5566d67e1a42d67a25dff05f6f` |

SHA-256 EXE: `272d634ea438979c67fc09c03359d5e510dc970f339e4e9b39fecb3483dd4a21`.
Measurement/evaluation code and the EXE did not change after the matrix run;
lokalny manifest hashy: `scratchpad/i51b-code-freeze.json`.

Polecenia (od katalogu worktree):

```text
rtk proxy py -I docs/procedural/smoke/g6_measure_candidates.py scratchpad/i51b-build/procedural/candidate_rank.exe C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10 scratchpad/i51b-matrix --g5-levels --i51b
rtk proxy py -I docs/procedural/smoke/g6_evaluate_i51b.py C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10 scratchpad/i51b-matrix scratchpad/i51b-holdout.json
```

## Tests and limitations

- Isolated build of the real source/procedural/CMakeLists.txt, MSVC
  19.51.36257 x64 Release, C++23, Ninja 1.13.2: PASS.
- CTest 22/22 PASS. After adding the aggregation test, I51b was rerun:
  3/3 PASS. Remaining I51 holdout tests: 8/8, CLI: PASS.
- Manual 26 bands: two times ln(1.5) gives 811; ln(577/384) gives 407
  przy 25 pasmach. A wybiera pierwszego, B drugiego; kandydat z hardFail
  przegrywa mimo 26 pasm i distance=0. Remisy pasm i indeksu: PASS.
- Manual aggregation of 32 pairs: 16 improvements, 8 regressions, 8 ties;
  A=16/32, B=24/32, baseline 5/64. Test granic B=22 (FAIL),
  A=B=23 (FAIL), A=22/B=23 (PASS), odmowy niekompletnej macierzy,
  invalid score/winner and experiment marker: PASS.
- N=1, candidate 0, fingerprint 1729 100×100 =
  17159159863505677800, repeatability and no mutation: PASS.

The canonical build_ninja.bat, g5_run_suite with probes, G5 --i47, manual GUI/Place/Undo,
the game, original RME and final adapter mask were not run. The parent performs these
regressions independently after review and ff-merge. libm determinism across compilers was
not confirmed. Maps were not re-imported. Historical confirmations prove the inputs but do
not contain an OTB hash, so binary OTB identity was not proven.

G6_GATE_STATUS was left unchanged. No build-ninja was created in this worktree.
The build and all working files are only in scratchpad. No dependencies were installed,
C:/repo/vcpkg/buildtrees was not touched and nothing was pushed. Scratchpad and local-data
are excluded from the commit.
