# I53 implementation and measurements (2026-11-18)

Binding preregistration: `87e2246`, I53 in `G6_METRICS.md`. Preregistration,
production ranking A, generation algorithms, mountain geometry and OTBM format
are unchanged. This report does not close G6. The coordinator owns the independent
review, fast-forward integration, regression, `G6_GATE_STATUS` update and removal
of the agent's build directory.

## Implementation

`copy_guard` reuses G4 `similarity` (16x16, minimum 50% present, content modulo 4,
uniform windows excluded). Canonical names are the existing G4 `StyleKind` names:
Grass, Dirt, Rock, Shore, Water. Generated Lake/River become Water through
`styleKindOf`. Generated Preserve, Path, Floor and Bridge are absent. Reference
Other, Unmapped and NoGround are absent; semantic unknown-content pixels are also
absent. Thus the check covers five natural ground classes, without items,
building interiors, path paving or underground geometry. Equal class hashes are
not proof of copying by the generator.

The offline builder reads only verified M06/M19 directories in the explicit G4
bundle. It reconstructs class rasters from `kind-map-zNN.png`, masks unknown
content using `semantic/floor-NN.png`, and verifies the exact approved `keymap.tsv`
against both supplied profiles. It never reads old ground-name `fingerprint.fp`,
holdout maps or their hashes. Every eligible surface floor 0..7 is required;
missing/unsupported rasters fail rather than silently reducing coverage. PNG CRC,
dimensions, exact decompression size and the G4 exporter encoding are checked.
All raster inputs and their digests are recorded in the private audit, including
the complete approved key mapping.
The retained G4 client's `items.otb` digest is checked against the profile too;
DAT identity comes from the unchanged-source verification receipt.

`i53-index/1 g4-natural-v1` stores client, OTB/DAT SHA-256, the frozen fingerprint
options/counters, M06/M19 source map, semantic input and keymap SHA-256 manifests,
and a SHA-256 checksum of the complete payload. Reading is bounded (64 MiB file,
256 MiB default offline parsing allowance), canonical and fail closed. Exactly
M06 then M19 are allowed; mismatched keymaps/assets, duplicate source identity,
bad checksums, malformed/trailing/truncated data, wrong options and fewer than
16 distinctive windows in either reference are errors. This is a trusted local
index, not an authenticity signature or automatic reference discovery.

The generic filter is the intersection of the two reference sets only. Candidate
data never defines it. Candidate and reference counts before/after the filter,
shared unique hashes and candidate containment per reference are reported.
Copy means shared >=16 AND candidate containment >=100 per mille for either
individual reference. Fewer than 16 candidate windows after filtering explicitly
means not evaluated, never a claim of no copy. No index is also not evaluated.

`candidate_rank --copy-index FILE` checks before ranking A. An explicitly bad
index aborts the series. GUI Surface has an optional explicit local index path;
empty means no evaluation, invalid means the entire series is refused. It loads
an immutable index snapshot per series and evaluates on the existing worker,
then retains only the compact assessment in the gallery. Selection rebuilds must
match that assessment. A detected copy sets hardFail and blocks the button and
forced Place handler. Map/parameter invalidation and worker cancellation remain
I52's existing mechanism; changing the index path invalidates the series too.
No assessment mutates the map; accepted placement still uses DrawOperations.

The entire retained index/parsing estimate is subtracted once from I52's remaining
series allowance. Each live preparation reserves another `64*cells+65536` bytes
for the class raster, semantic model, fingerprint/filter storage, rolling buffers
and allocation allowances. Estimates saturate and share the existing subtract-only
counter. Gallery metadata fits the existing 4 KiB per-row metadata allowance.
There are no N copies of the index or prepared map. These are estimates, not RSS
limits; OS/thread/allocator exclusions remain those documented by I52.

## Private measurement

Inputs, read only:
`C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/`
`g4-kindmap-20261108a/{M06,M19}` and `g4-profiles/10.98-v10`.

Two independent index builds were byte-identical, 6,427,267 bytes; SHA-256:
`ce39e905317ce1891a008fcb2f4e03c2d12c329247ba4cfed65138361fc874b8`.
Generic unique hashes: 1544. M06 unique windows: 235522 before, 233978 after;
M19: 142499 before, 140955 after.

Frozen matrix: profiles m06/m19; user seeds 1001..1032; N=1; side=512;
pathLength/buildingDensity 40/30 and 80/40; rockBlocks 0/1; `--g5-levels`;
frozen `g6-bands-v1` A. Two complete runs per profile were byte-identical.
All 64 candidates had sufficient post-filter samples. Copy hits: 0/32 m06,
0/32 m19. No seeds or hits were removed and no thresholds were adjusted.
Shared hashes exceed 16, but containment stays below 100 per mille.

| Profile | Candidate windows before | After | M06 shared / containment | M19 shared / containment |
| --- | --- | --- | --- | --- |
| m06 | 27627..37550 | 27395..37305 | 339..536 / 9..16 per mille | 99..171 / 3..5 per mille |
| m19 | 27538..38069 | 27342..37883 | 237..447 / 6..14 per mille | 103..208 / 2..6 per mille |

Existing geometry/reachability/repair hardFail remains for m06 seed 1032 (I53
itself is below thresholds); all other matrix rows have hardFail=0.
Final TSV SHA-256:

- m06: `615c4614142a39a1ed2f7b7d94f3861656f920ed8f3eb44535b5e5184d8c0f63`
- m19: `4201efabfc048224199cb1db91e23ed6093eb5a375923cc5ed5f84fad3e31abf`

Private raw evidence stays in the agent worktree's `scratchpad/i53-index-final`,
`i53-index-repeat`, `i53-index-asset-audit` and `i53-matrix-verified`. The final
matrix was rerun twice per profile after the last production change and has the
same TSV digests as the earlier complete measurement. No map, raster, index or profile is
included in this commit.

## Validation evidence

- Final canonical `build_ninja.bat`: PASS using the requested Ninja path and
  existing main-checkout `build-ninja/vcpkg_installed`, with manifest install OFF.
  Final core-only rebuild/run: 21/21 and `SUITE_PASS`. Final CTest: 25/25, zero
  failures (15.78 seconds). Logs: `scratchpad/canonical-verified.log`,
  `suite-core-verified.log`, `ctest-verified.log`. No dependency installation or
  vcpkg buildtree changes were requested or performed.
- Full `g5_run_suite.py --probes`: 21/21 core tests, commit probe PASS, Surface
  probe PASS, `SUITE_PASS`. Private logs: `scratchpad/suite-final.log` and
  `suite-{commit,surface}-20261118-142017`. Commit stderr was empty. Surface had
  157 known hidden-window SetFocus warnings, no other stderr; source assets were
  unchanged. All nine required Surface markers passed, including mountain,
  storeys, dungeons, rock passability and occupied lower-floor coverage.
- `g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1`: `G5_GATE_PASS`.
  All eight rows (two profiles, dungeons off/on, retries 0/8) were deterministic,
  each with 32 seeds and zero unrepaired seeds. Raw logs and `summary.json`:
  `scratchpad/i53-i47`.
- CLI contract test: PASS, including unchanged seed-1729 100x100 fingerprint
  `17159159863505677800`, N=1/4/16, deterministic A/B experiments, argument/band
  refusals, no-index status and bad/missing-index errors. No changes were made
  to either ranking order.
- Python decoder/provenance tests: 3/3 PASS. Syntax parsing of all four new or
  modified Python files: PASS. Index reconstruction was repeated independently,
  then repeated with the explicit OTB-asset audit; all three indices have the
  SHA-256 recorded above.
- Final real-client series probe: PASS, exit 0, assets unchanged. Evidence:
  `scratchpad/i53-series-budget-corrected/verification.json` and
  `run/resolver-probe.log`. All six I53 markers passed: charged shared-budget
  refusal (PreviewError::Limit, variant stage 4), detected copy blocking forced
  Place, missing/corrupt/other-client index refusal, cancellation, small sample
  explicitly not evaluated, and clear candidate Place/one Undo/Redo matching the
  preview. Existing N=1/4/16, both profile rankings, nonrecommended selection,
  hardFail and stale/cancel/close scenarios passed too. Stderr contained exactly
  9 known hidden-window SetFocus warnings and no other lines. This is automated
  production-control/handler coverage, not manual visual acceptance.

Early development invocations exposed a scratch batch quoting error and a Python
builder syntax error; both were corrected before these measurements. One series
probe build was correctly refused by the harness because a linked source changed
during its build. That run is incomplete and is not counted as probe evidence.
The final source is frozen for the subsequent canonical build and fresh series
probe. The full suite above preceded the final declared-allocation guard in the
index parser and the omission of unmeasured optional metrics; those final changes
do not alter generation, ranking or successful fingerprint arithmetic.
The first completed I53 series probe stopped at an overly specific budget-test
assertion: it required the wrapper's MemoryLimit enum, whereas the shared budget
can be refused by a later stage's limit. That incomplete run is not counted.
The corrected probe also verifies a successful indexed preparation's charged
bytes before checking that the smaller shared budget refuses it.

## Reproduction and GUI procedure

With the existing Ninja directory on PATH, existing vcpkg installation selected
and manifest install OFF in the worktree build cache:

```powershell
rtk proxy cmd /c build_ninja.bat
rtk proxy py -I docs/procedural/smoke/g6_build_copy_index.py build-ninja/source/procedural/copy_index.exe C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-kindmap-20261108a C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10 scratchpad/i53-index
rtk proxy py -I docs/procedural/smoke/g6_measure_copy.py build-ninja/source/procedural/candidate_rank.exe C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10 scratchpad/i53-index/references.i53 scratchpad/i53-matrix
rtk proxy ctest --test-dir build-ninja -R 'test_procedural_|test_g6_|test_candidate_rank_cli' --output-on-failure
rtk proxy py -I docs/procedural/smoke/g5_run_suite.py --probes C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/suite-commit-20261118-090105/run/data C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/suite-commit-20261118-090105/run/config.toml --output-root scratchpad
rtk proxy py -I docs/procedural/smoke/g1_run_resolver_probe.py scratchpad/i53-series C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/suite-commit-20261118-090105/run/data C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/suite-commit-20261118-090105/run/config.toml --series
rtk proxy py -I docs/procedural/smoke/g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1 --output scratchpad/i53-i47
```

1. Load the matching client and open Surface generator on a new empty map. Leave
   the local index path empty, Preview, select a candidate: similarity must say
   not evaluated. Place remains subject to all existing hard conditions.
2. Enter the absolute matching `references.i53` path. Preview: inspect the
   per-reference shared hashes, per-mille containment and before/after counts.
   A small sample must say not evaluated; it must not say no copy.
3. Enter a missing, damaged or other-client index: Preview must refuse the entire
   series, leave the map/history unchanged and disable Place. Changing this path,
   seed, other parameters or the target map invalidates earlier candidates.
4. For the deliberate synthetic copy fixture produced by the series probe
   (`scratchpad/i53-series-budget-corrected/run` in this worktree),
   use its private `copy-fixture.i53` with built-in tuning, seed 1729, 100x100,
   N=1, mountains 0, repair off, paths/buildings/camps/caves 0 and all six ground
   brushes set to grass. The semantic candidate is the same surface used to
   build that fixture: hardFail must block Place. The automated probe also calls
   the disabled handler directly and checks that the map/history remain empty.
5. Repeat with the private `clean-fixture.i53`: choose an admissible result,
   Place, one Undo, Redo. The result must match the preview. These two files are
   explicitly synthetic test indices bound to the loaded test assets; they are
   not production reference evidence.
6. During a multi-candidate run, Cancel or change input/close the dialog: no late
   result or map change may appear. Open a fresh dialog to resume.

Manual visual/keyboard/DPI acceptance, actual RSS exhaustion, original-RME
load/save compatibility and cross-compiler determinism are not claimed. Existing
holdout quality failures and the need for I54/I55 remain; this iteration does not
measure style improvement or close G6.
