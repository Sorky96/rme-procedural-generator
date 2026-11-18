# I55 — execution report, 2026-11-18

Preregistration: `6251dbb`, [G6_METRICS.md](../G6_METRICS.md).
**I55 is OPEN; G6 is OPEN.** This report prepares the blind study and reports
known-data diagnostics. It is not a quality acceptance decision. The
coordinator owns review, integration, independent regression, G6_GATE_STATUS
and cleanup. [Offline instructions](g6_i55.md).

## Recovery and implementation

Reviewed the previous `t3-a2f0d9e6` checkout read only: its two uncommitted
changes parametrized the existing candidate reader and added post-repair
semantic PNG export. Its ignored/untracked-file inventory was empty and it
had no scratchpad. Reused those changes, then completed the offline workflow,
form, validation and documentation in this worktree. No delegation or push.
No `.codegraph` index exists in this checkout, so source discovery used text
search. The previous checkout was not modified.

The initial export palette omitted `Bridge`, which can appear during level
repair. The first matrix aborted with an encoder error; it is retained in
`scratchpad/i55-matrix` as **technically invalid**, not a quality result.
Added the missing palette entry and a compile-time count check, then reran
the entire unchanged matrix in a new directory. No seed or candidate was
substituted and no generator/ranking/threshold was changed.

The completed `scratchpad/i55-matrix-final` uses both profiles, seeds
3001–3032, N=4, side 512, frozen I51 tuning/G5 geometry and the explicit
local I53 M06/M19 index. Each full run contains 256 candidates; both runs
have byte-identical TSV reports and all semantic images. Python independently
reconstructed scores, winners, candidate seeds and I53 containment/state.
There were no seeds without an admissible candidate.

The input manifest was written before generation. Its SHA-256 is
`7947ac342062bb888e5856f864b2a525a9414c98dee045196651035ce4f43807`.
It binds executable, profiles, bands, index and offline source files; it
explicitly records no accepted new families. Report SHA-256 values:

- m06: `fee5d09dd129c013725d48b98298089071c0357d3211d4636b5589bca3384926`
- m19: `9d0aa8a5012d976316fb251e35a095789a4ef6bc1294c147a26f5afb054f673d`

## Known-data quantitative diagnostics

All rows below use **32 seeds**, not the 20-pair blind subset. Distances use
the frozen 26 contributions including capped N/A. M01, M05 and winter have
equal weight; winter averages M11/M18. Negative median difference favors L3.
Detailed per-seed/map/family results, N/A reasons and source audit remain in
`scratchpad/i55-diagnostics.json`, outside Git. No selection identities or
blind-subset summary are published here.

| Profile | Reference family | Strict L3 wins / 32 | Ties | L2 median | L3 median | Difference of medians |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| m06 | M01 | 12 | 8 | 3.617168 | 3.616442 | -0.000726 |
| m06 | M05 | 14 | 6 | 3.395198 | 3.393922 | -0.001276 |
| m06 | winter | 17 | 6 | 3.911136 | 3.876416 | -0.034720 |
| m06 | diagnostic equal-family mean | 17 | 6 | 3.641328 | 3.629529 | -0.011799 |
| m19 | M01 | 17 | 5 | 3.620282 | 3.618788 | -0.001494 |
| m19 | M05 | 16 | 5 | 3.411258 | 3.402493 | -0.008765 |
| m19 | winter | 8 | 5 | 3.908451 | 3.912258 | +0.003807 |
| m19 | diagnostic equal-family mean | 13 | 5 | 3.644627 | 3.643993 | -0.000634 |
| m06 | M06 (in-sample) | 18 | 6 | 1.017650 | 0.957310 | -0.060340 |
| m06 | M19 (other fitted reference) | 14 | 6 | 0.863288 | 0.858989 | -0.004299 |
| m19 | M06 (other fitted reference) | 12 | 5 | 1.012588 | 1.030366 | +0.017778 |
| m19 | M19 (in-sample) | 22 | 5 | 0.941119 | 0.884290 | -0.056829 |
| m06 | M12–M17 (related appendix only) | 21 | 6 | 3.642858 | 3.600769 | -0.042089 |
| m19 | M12–M17 (related appendix only) | 15 | 5 | 3.655014 | 3.655811 | +0.000798 |

The diagnostic main paired-difference medians are -0.003174 for m06 and
0 for m19. Neither main diagnostic count reaches 23/32. These already exposed
maps cannot satisfy the registered new-family condition regardless of counts.

Coverage is weak: M01/M05 have 8/26 known reference positions, M11 has 7/26,
M18 has 5/26; respectively only 5, 7, 5 and 3 contributions vary across each
profile's pool. M06/M19 have 26/26 known positions; M12–M17 have 5–11/26.
Larger improvements against each profile's own fitted reference than against
the diagnostic families are consistent with possible overfitting, but do not
establish its cause: coverage and missing metric classes differ materially.
No statistical significance or unseen-map quality improvement is claimed.

I53: m06 128/128 and m19 128/128 candidates were evaluated below both copy
thresholds; copy rejections 0/128 per profile; not-evaluated/too-small 0/128.
Other hard failures were 4/128 m06 and 0/128 m19. All 32/32 seeds per profile
retained at least one admissible candidate. Semantic surface matching does
not assess items or underground geometry and is not proof of no copying.
The pool uses Candidate Rank's existing conservative offline blocking model;
the loaded-client adapter was not exercised for all 256 measured candidates.

## Blind materials and source blocker

The real anonymous package is `scratchpad/i55-evaluator/`, also available as
`scratchpad/i55-evaluator.zip`. It contains 20 anonymous pairs, 40 metadata-free
512×512 PNGs, a local HTML form, public view hashes and brief instructions.
The assignment key is in a separate private scratchpad directory. It was
frozen before publishing views, with the registered per-profile balance.
The key has not been opened for evaluation; no real ratings exist yet.
Assignments, identical-pair counts and blind-subset results are withheld.
Share only the evaluator directory/archive with the rater.

Independent package audit verified PNG CRCs, dimensions, decompression,
nontrivial terrain pixels, hashes, anonymous filenames/content and archive
membership. The archive contains 43 evaluator files and no key or ratings.
JavaScript syntax passed in the available Node runtime. Actual interactive
browser behavior and human usability were not exercised. Draft export/restore
must be checked by the coordinator/rater; retain answers in scratchpad.

Local source inventory examined the supplied procedural-smoke tree's reference
reports and possible provenance/permission/manifest filenames. It found 23
distinct source hashes: the 19 G4 maps plus four identities already present
in historical G3 tests (including BlackTek and a 503-tile compressed fixture).
No eligible unseen-family manifest with documented provenance, permission and
G4 relatedness evidence was found. The private inventory is retained as
`scratchpad/i55-source-inventory.json`. This is a local inventory, not proof
that no suitable sources exist elsewhere.

Required next inputs: new verified semantic bundles/maps and client mapping;
documented provenance and permission; a statement of no prior analysis/tuning;
G4 relatedness evidence against prior families; and an accepted, hashed family
and evaluation-unit manifest frozen **before** new-family L2/L3 measurement.
Crops of one world remain one family. The current diagnostic-only loader does
not accept arbitrary JSON as new-family evidence. No new-family measurement
was performed, no thresholds changed. ≥23/32 per profile on new families and
≥13/20 blind wins, plus regression, remain jointly required for PASS I55.

## Validation and environment limitations

- Windows x64 Release: canonical `build_ninja.bat` **PASS**, after direct
  CMake/Ninja builds of RME and Candidate Rank; incremental rebuild reported no
  work. Ninja 1.13.2 used as supplied.
  Existing main-checkout dependencies were reused with manifest installation OFF.
- I55 synthetic tests: 9/9 PASS; complete/balanced pairs, strict L2/L3 ordering,
  no admissible result, identical selections, ties, 12/13 and 22/23 boundaries,
  missing source/coverage, corrupt matrix/I53, anonymous packaging, incomplete
  ratings rejected before key access, changed image rejection and scratch paths.
- CTest offline selection: 4/4 PASS (I55, I51b, holdout, Candidate Rank CLI).
- CLI regression: PASS, including PNG export neutrality, repeatability, refusals,
  complete palette and seed 1729 100×100 fingerprint `17159159863505677800`.
- Core G5 tests: 22/22 PASS. G5 `--i47 --rock-blocks m06=0 m19=1`:
  **G5_GATE_PASS**, 32/32 both profiles, repeated reports identical, both
  dungeon settings and retry budgets retained by the existing regression tool.
  Private evidence: `scratchpad/i55-i47/summary.json` and `i55-i47.log`.
  Full `g5_run_suite.py --probes`: **SUITE_PASS**, core 22/22, Commit PASS,
  Surface PASS, both exit 0 and input assets unchanged. Surface emitted 157
  known SetFocus warnings accepted by the existing runner, with no other
  stderr; Commit had none. The raw Surface receipt has `passed=false` because
  that runner rejects any stderr; the existing suite explicitly recognizes
  these SetFocus-only warnings and returns PASS. Evidence: `scratchpad/i55-suite-final.log` and
  `scratchpad/i55-suite/suite-{commit,surface}-20261118-182053/verification.json`.
- `git diff --check`: PASS. Private maps, profiles, images, keys, ratings and raw
  diagnostics are not staged. Scratchpad remains local and untracked.

Environment incident: an attempted canonical `build_ninja.bat` reset the cache
after vcvars changed the vcpkg root to Visual Studio's copy, then began an
unrequested dependency install. This was interrupted. The log records partial
packages/builds under this worktree's `build-ninja/vcpkg_installed` and AppData
caches, not `C:/repo/vcpkg/buildtrees`; those shared buildtrees were not used.
No attempt was made to delete or roll back shared caches. Subsequent builds
used explicit CMake arguments and installation OFF. A final canonical run set
the correct root immediately before the script captured it, checked the cache
root and installation-OFF preconditions, and completed with
`I55_CANONICAL_BUILD_PASS`, without an install step. Relevant logs:
`scratchpad/build-i55.log`, `build-i55-direct.log`, `build-i55-final2.log`,
`i55-canonical-final.log`; the guarded wrapper is `scratchpad/canonical-i55.cmd`.

The first probe-suite attempt failed before building probes because the direct
configuration lacked `compile_commands.json`; after enabling its export, the
suite was rerun. Initial evidence is retained. No production runner was changed.
No manual GUI preference test, real I54 feedback, new-family evaluation,
cross-compiler determinism or performance improvement was verified.
