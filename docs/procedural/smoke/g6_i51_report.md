# I51 — executor report, 2026-11-17

Binding policy: `G6_METRICS.md`, commit `396e057`. **G6 remains open.**

Implemented fixed 26 metrics and explicit bands in hundredths, N/A without changing
the denominator, bounded log/log1p/asinh distances, and exact holdout and baseline
aggregation. The generator, GUI, map saving and criteria were unchanged. Default
mountain and the lack of item-ID dependencies remain unchanged.

## Matrix and threshold

Two full runs on the same final MSVC x64 Release, C++23 version:
m06/m19, 32 user seeds 1001–1032, N=16, side 512, `--g5-levels`,
pathLength/buildingDensity 40/30 and 80/40, rockBlocks 0/1. In total 2048
candidates were generated and evaluated. Failed seeds were not retried.
Each run used four independent blocks of eight seeds per profile, then merged
in canonical order. The CLI test confirms byte identity between the split report
and the single-process report.

Final measurement commands (from the worktree directory):

```text
rtk proxy py -I docs/procedural/smoke/g6_measure_candidates.py scratchpad/build/procedural/candidate_rank.exe C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10 scratchpad/i51-matrix-complete --g5-levels
rtk proxy py -I docs/procedural/smoke/g6_evaluate_holdout.py C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10 scratchpad/i51-matrix-complete scratchpad/i51-holdout.json
```

| Profile | Successes | Share | Exact random baseline | Baseline % | Threshold 23/32 |
| --- | --- | --- | --- | --- | --- |
| m06 | 18/32 | 56.250% | 127/256 | 49.609375% | FAIL |
| m19 | 18/32 | 56.250% | 1/2 | 50.000000% | FAIL |

**Result: I51_HOLDOUT_FAIL.**

The result does not change bands, model or threshold. It is handed to the parent
for a decision. This report does not close I51 or G6. Matrix hardFail occurred in
6/512 m06 candidates and 1/512 m19 candidates. Every winner has hardFail=0.
The threshold failure comes from holdout-distance comparison. Evaluator exit code
is 1 (valid measurement below threshold), not a data-integrity failure.

Success means winner hardFail=0 and distance strictly below the average of positions
8 and 9 among all 16 candidates. All seeds and hardFail remain in the denominator.
Baseline is the exact mean share of candidates meeting the same condition. Both
profiles were evaluated on all maps.

## Results by map and family

| Map / family | m06 successes | m06 baseline | m19 successes | m19 baseline |
| --- | --- | --- | --- | --- |
| M01 | 13/32 | 27/64 | 18/32 | 1/2 |
| M05 | 20/32 | 253/512 | 20/32 | 1/2 |
| M11 | 19/32 | 127/256 | 15/32 | 255/512 |
| M12 | 16/32 | 253/512 | 19/32 | 1/2 |
| M13 | 12/32 | 127/256 | 12/32 | 255/512 |
| M14 | 21/32 | 253/512 | 18/32 | 255/512 |
| M15 | 20/32 | 63/128 | 21/32 | 1/2 |
| M16 | 0/32 | 1/256 | 12/32 | 61/128 |
| M17 | 0/32 | 1/256 | 12/32 | 61/128 |
| M18 | 16/32 | 251/512 | 14/32 | 255/512 |
| family.M01 | 13/32 | 27/64 | 18/32 | 1/2 |
| family.M05 | 20/32 | 253/512 | 20/32 | 1/2 |
| family.M19-related | 22/32 | 63/128 | 21/32 | 1/2 |
| family.main | 18/32 | 127/256 | 18/32 | 1/2 |
| family.winter | 18/32 | 253/512 | 14/32 | 255/512 |

Primary result = (M01 + M05 + (M11+M18)/2)/3. M12–M17 are only a separate
report for the M19-related family and do not contribute to the threshold.

Full results for 64 pairs, winner/median distances, baseline, failures, references
and N/A reasons: [versioned JSON report](g6_i51_holdout.json). Raw candidate reports:
`scratchpad/i51-matrix-complete/` locally.

## Repeatability and provenance

Reports from both runs for each profile are byte-identical. All matrix processes
exited with code 0 and empty stderr.

| Profile | SHA-256 report for each run |
| --- | --- |
| m06 | `e9584f1e0a1b4369567f594045f9267466ed3562fbcf78901d1da294f18b9e99` |
| m19 | `59de9ee474f5bcbbd5785f976475e0bf85b3fbaef6e8634d5db121e611c886e3` |

SHA-256 EXE: `0393e799096c060c3da1e6991cb93a6254b6663dea56963b88bea4a3ac34b97c`.

Bands are versioned in `docs/procedural/g6-bands-v1/`. Their references were checked
against M06/M19 and were not derived from generator results. An independent Python
calculation confirmed all 1024 core results (inBand and distance) and winner selection.

## Private-reference audit

PASS: complete M01–M19 from four explicit series under
`C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke`:
`g4-channels-20261108a`, `g4-scale-20261108a`, `g4-trans-20261108a`,
`g4-build-20261108a`. Profile: `g4-profiles/10.98-v10/{m06,m19}.profile`.
Confirmed client 10.98, matching map versions across series, PASS confirmations,
matching import/semantic/verification sources, DAT/SPR, keymap and repeated
channel/scale/path summaries. Summary and profile hashes are in JSON. Maps,
profiles and images were not copied to versioned files.

| Map | Known reference / 26 | Actually distinct m06 contributions | Actually distinct m19 contributions |
| --- | --- | --- | --- |
| M01 | 8 | 5 | 5 |
| M05 | 8 | 7 | 7 |
| M11 | 7 | 5 | 5 |
| M18 | 5 | 3 | 3 |
| M12 | 10 | 8 | 8 |
| M13 | 7 | 6 | 6 |
| M14 | 10 | 9 | 8 |
| M15 | 11 | 10 | 10 |
| M16 | 5 | 2 | 2 |
| M17 | 5 | 2 | 2 |

Known reference means a potentially differentiating position. Penalty clipping can
make all measured candidates contribute equally. The last two columns count positions
that actually differ in the profile matrix. No evaluated map lacks such positions.

Reference N/A contributes constant ln(64) for every candidate. N/A comes from missing
objects, fewer than five components/buildings, and thresholds of 1000 for patches and
boundaries. Detailed reasons for each position are in JSON. Missing objects yield
length/density 0 and width/side/outline N/A. The existing G4 definition is preserved:
excess without pairs has value 0.

## Measurement-independent tests

- Isolated build of the real `source/procedural/CMakeLists.txt`,
  MSVC 19.51.36257 x64 Release, C++23, Ninja 1.13.2: PASS.
- CTest: 21/21 PASS (19 core tests, CLI and holdout). After script changes,
  the relevant CLI/holdout tests were rerun: PASS.
- Hand-counted orderings: hardFail loses to better bands, distances 875/788 and
  an index tie. ln(2)=0.69314718056, asinh(1)=0.88137358702,
  asinh(1)-asinh(-1)=1.76274717404.
  26*ln(64) daje 108131; 24*ln(2)+2*asinh(1) daje 18398.
- Boundary tests B/A50/U20 and hundredths, ref=0, signed excesses, N/A,
  penalty clipping and one rounding step: PASS.
- Manual aggregation (0+6+(4+8)/2)/3=4, even median 0..15=7.5,
  baseline 7/16 with one hardFail and mean 7/32 with a tied seed: PASS.
  The full 32-pair example gives 16/32 and exact baseline 5/32.
- Missing column, invalid number, wrong core result, incomplete seeds,
  removed reference and changed version: measurement refusal PASS.
- N=1, SplitMix64, odcisk 1729 100×100
  `17159159863505677800`, brak mutacji: PASS.

## Limitations and handoff

The canonical build_ninja.bat, g5_run_suite with probes, G5 --i47, GUI, final adapter
mask, game and original RME were not run. libm determinism across compilers/platforms
was not proven. Private summaries came from historical confirmations. Maps were not
re-imported. Confirmations do not contain an OTB hash, so binary OTB identity was not
independently proven.

The parent independently reviews, ff-merges, performs the canonical build, both G5
validations and the gate report. No build-ninja was created in this worktree. Build
and working files remained only in scratchpad. No dependencies were installed,
C:/repo/vcpkg/buildtrees was not touched and nothing was pushed.
