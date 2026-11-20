# G6 — gate status

## D4 — Preview memory measurement

Criteria `04b03de`, integration commits `9786851` and `ebe0f9d`.
Analysis of four series on a 500×500 map with a single candidate separates
memory occupied by CRT blocks from approved reserve. After 200 cycles,
private memory increased by approximately 70 MiB, of which approximately 53 MiB is CRT
reserve, and 17 MiB remained outside identified heaps. Occupied CRT
blocks increased by approximately 0.03 MiB. Growth slowed in the second half of the series.
Formal G1 criterion still does not pass. Production code unchanged.

Canonical build, `SUITE_PASS` (23/23 and both probes without warnings)
and I47 (`G5_GATE_PASS`, 32/32 for both profiles, identical repetitions)
passed. Fingerprint 1729 and `mountain` wall preserved. Independent analysis
reproduced results of recorded series. Abbreviated `--heap` probe of 5 cycles
recorded complete samples in scratchpad, with process code 0 and preserved
inputs. G1 trend threshold gave negative result in this probe as well.

Automatic execution control blocked removal of `build-ninja`
from executor worktree. [Report, reproduction, and diagnosis boundaries](G6_D4.md).

## D1 — focus while closing a tab

Criteria `04b03de`, fix `6ba5973`, probe `40a9d02`.
Notebook disconnects the tab with frame enabled. Map
and GL resource cleanup occurs under the “Closing” window. On client 10.98,
executor reproduced Win32 error 87 before the change and confirmed
empty stderr twice after the change.

Canonical build and `SUITE_PASS` passed. Core 23/23, Commit probe
and Surface probe completed without SetFocus warnings. Fingerprint
`17159159863505677800` and `mountain` wall are covered by the set.
Independent D1 probe gave PASS for Exit, Cancel, and tab closure,
each time with empty stderr. I47 gave `G5_GATE_PASS`, 32/32
for m06 and m19, with identical repetitions.

Manual test: Open two maps and close the current one via File → Close,
then repeat with the tab button. With two maps, start
File → Exit with an unsaved change and cancel saving. Verify that the map
and change remained. Manual assessment of focus and closing a large map
remain unverified. Automatic execution control rejected
removal of `build-ninja` from D1 worktree.
[Reproduction report](smoke/g6_d1_report.md).

## D21 — reproducing G4 tables

Criteria `e3e6f8b`, integration `a877308`. Independent run
of `D21_PASS` compared two runs of each tool with historical
reports byte for byte. Agrees on 15/15 values from iteration 3 and 10/10
values from row 4b. Model 4a did not reach Git. Preserved report
gives 99.994% P1 versus 99.997% in the table. Historical values
preserved. Three runner tests passed. Executor worktree did not
contain `build-ninja`. Change concerns CLI.
[Reproduction instructions](smoke/G4_ITER34_REPRODUCE.md).

## D10 — I53 fragment rotations and reflections

Criteria `7bd6968`, baseline measurement `18f3126`, implementation `fd36847`.
I53 now detects 8/8 symmetries of a fragment from each of M06 and M19 with
the existing thresholds. Previous version detected 1/8 per map.
Negative control has 0/8 hits. Two matrix measurements for m06 and m19
are identical and give 0/32 hits per profile. Change covers
ground surface in 16×16 windows.

Index `/2` requires rebuilding. In Surface GUI, select the old index
and run Preview to check the rebuild message. Then
select the new index and run Preview again. Frozen I55 uses
tools and index `/1` from before D10.
Control run with this set reported `I55_MATRIX_REPEATABILITY_PASS`.
Canonical build and `SUITE_PASS` passed with 23/23 core tests,
both probes, preserved fingerprint 1729, and default `mountain` wall.
I47 gave `G5_GATE_PASS`, 32/32
for m06 and m19, with identical repetitions. Regression also covers
branch state after D21 integration.
Automatic execution control blocked removal of `build-ninja`
from executor worktree.
[D10 measurement results](smoke/g6_d10_report.md).
[I55 reproduction](smoke/g6_i55_intake.md).

## D25 — portable G5 measurement inputs (2026-11-19)

Criteria `ce474b7`, change `8015f6a`, and test correction `807e6e0`.
Measurement selects profiles from main checkout also from worktree. Parameter
`--local-data` allows specifying another directory. Missing profile stops
measurement before generation and indicates the full path.

Two path tests passed. Canonical build and `SUITE_PASS` confirmed
23/23 core tests, both probes, and unchanged resources. Set also verifies
fingerprint 1729 (`17159159863505677800`) and default `mountain` wall.
Independent I47 measurement with `--local-data` in a directory with spaces gave
`G5_GATE_PASS`, 32/32 for both profiles and identical repetitions.
Executor worktree did not contain `build-ninja` directory. Change concerns
CLI tools and requires no new GUI steps.

## D23 — occupied floor and Undo capacity (2026-11-19)

Criteria `b59f590`, probes merged via fast-forward to `5bbe147`.
On client 10.98, protection of house, zone, spawn, and waypoint
at z + 1 during cave and dungeon was confirmed. Preview does not change the map. Place records
both floors in one step, and Undo and Redo restore the correct state.
Independent probe `--capacity 500` reported `CAPACITY_PASS` for 300,137
changes on four floors with Undo limit of 4096 MiB.

Canonical build passed with dependency installation disabled.
`SUITE_PASS` includes 23/23 core tests and both probes on resources
with unchanged checksums. Surface probe confirmed default
`mountain` wall. Fingerprint 1729 for 100×100 is `17159159863505677800`.
`G5_GATE_PASS` in I47 mode confirmed 32/32 for m06 and m19 and
identical repetitions.
Automatic execution control rejected removal of `build-ninja` directory
from executor worktree. Directory remained on disk.

Manual test in the editor involves preparing a floor with a house and
spawn one floor below, running Preview and Place, and verifying both
floors after one Undo and Redo. Assessment of in-game landing,
remembering the Underground tab, and capacities of 1000 and 2000 remain open.
[D23 scope and probe results](G6_D23.md).

### I55 — accepting new families (2026-11-19)

Criteria supplementation `bd89a30`, implementation `0ecae87`. Tool
verifies declarations and evidence, source identifiers, and G4 kinship.
After review, it freezes families, units, and data fingerprints before matrix
generation. Fix `37c6688` also checks consistency of accepted
review content with re-audit, including units and family division.

Build, 8/8 intake tests, and 6/6 CTest independently passed. Input template
verified in own private directory. Executor worktree did not contain
build-ninja directory. Full regression SUITE_PASS: 23/23 and both probes,
with unchanged input resources. G5_GATE_PASS: 32/32 for both profiles,
identical repetitions, and preserved fingerprint 1729 (`17159159863505677800`).
Acceptance of real new maps and measurement of new families still await
input. Complete set of 20 blind assessments is a separate requirement. Synthetic tests
are not proof of quality. G6 remains open.
[Map acceptance and preparation instructions](smoke/g6_i55_intake.md).

## I56 — local learning from an open map

Criteria `90ebf92`, implementation `3c8d45c`. **Learn from open
map…** command analyzes one surface rectangle up to 512×512. User
reviews ground classes, confirms usage rights, and saves a local profile
compatible with the client. Generator shows the profile in the normal list. Workflow
operates on G4 statistics, without changing the source map or its Undo history.

Integrator's canonical build passed with manifest installation disabled.
Independent SUITE_PASS includes 23/23 and Commit and Surface probes. I56
probe with client resources completed with LEARNING_DIALOG_PASS, with save,
offer, I54, Place/Undo/Redo, cancellation, and sample invalidation. Input resources
retained checksums. G5_GATE_PASS gives 32/32 for both
profiles and identical repetitions. Fingerprint 1729 is 17159159863505677800.
Initial dependency installation attempt by executor was interrupted before
noting completed package installation. Executor worktree's build-ninja directory was removed.

Manual assessment of appearance and keyboard remains open. I55 and G6 still
require assessments and independent map families. [I56 GUI steps and scope](I56_LOCAL_LEARNING.md).

## I55 — blind test ready to run

Criteria recorded in `6251dbb`, tools merged as `3bfe971`.
Anonymous local package includes 20 pairs and a form with JSON export.
Integrator confirmed archive completeness, fingerprints of all 40 views,
and absence of key in the package. Key remains locked until 20 assessments are received.
Blind test threshold is 13 L3 wins out of 20 pairs.

Comparison of earlier families is diagnostic. Audit of local
inputs found no new families with documented provenance, consent,
and G4 kinship verification. Data of those families must be frozen before
quantitative assessment. Threshold is 23/32 strict wins separately for both profiles.
Lack of 20 assessments and new families leaves I55 and G6 open.

Integrator build and 5/5 CTest passed with dependency installation disabled.
Full `SUITE_PASS` includes 22/22 core tests, Commit and Surface probes,
and unchanged input resources. `G5_GATE_PASS` (--i47) gives 32/32
for both profiles and identical repetitions. Fingerprint 1729 still equals
`17159159863505677800`. Independent repetition of I55 and comparison with executor
matrix confirmed identity of 532 PNG/TSV files. Diagnostics
reproduced on main build. Form verified for package structure
and export logic in code. Manual test in browser remained open.

After merge, executor's build-ninja worktree was removed. Reported
unintended agent installation was interrupted. Report notes partial
writes in its build and AppData cache.
[I55 instructions](smoke/g6_i55.md).

## I54 — local ratings and profile versions

Criteria recorded in `e9ca540`. Changes merged as `93426ed`. Approve,
Reject, and Adjust record the rating of the selected candidate in local history.
Adjust records eight supported style settings as a new version.
Returning to an earlier version preserves newer versions and allows creating
another branch from it. Atomic write detects conflict in loaded data.
After review `1d5010b` ties the rating to actual candidate settings
from the adapter, including rock rockiness established by client data.

Executor confirmed Windows x64 build, 22/22 core tests,
7/7 selected CTest tests, and client series probe. Executor worktree's
build-ninja directory was removed after merge.

Independent set `SUITE_PASS` (22/22, Commit, Surface) and measurement
`G5_GATE_PASS` (32/32 both profiles, identical repetitions) passed.
Fingerprint 1729 100×100: `17159159863505677800`. I54 probe detected
UTF-8 buffer lifetime error when saving note. Faulty write was rejected,
and earlier rating remained in file. Fix `68a1fc4` preserves
source text during conversion. After it, canonical build
and rebuilt series probe passed with `CANDIDATE_DIALOG_PASS`, with byte-for-byte
note comparison and rollback verification, history branching,
refusal of stale result, Place/Undo/Redo, and I53 blocks. Input
resources remained unchanged. Probe reported only nine known
SetFocus warnings. Commit/Surface performed before UTF-8 fix.

Manual visual and keyboard assessment remains unverified. Quality blind test
belongs to I55. G6 remains open.
[GUI steps and executor report](smoke/g6_i54_report.md).

## I53 — compatible-fragment check

Criteria recorded in `87e2246`. Implementation reviewed and merged as
`561ef2f`. Explicit local index M06/M19 activates check in CLI and Surface.
Candidate with at least 16 common unique windows and coverage of
at least 100‰ for one reference gets hardFail and blocks Place.
Missing index or small sample have separate message. Corrupted or
incompatible index rejects the series.

Integrator rebuilt index M06/M19 and ran full matrix twice.
Index and reports of both runs have the same SHA-256 as executor's.
I53 hits: m06 0/32, m19 0/32. All samples were sufficient.
m06 seed 1032 still has hardFail from earlier check.
Report describes index origin and scope of five surface classes.
[Results and GUI instructions](smoke/g6_i53_report.md).

Canonical Windows x64 build and 25/25 CTest PASS, including
`17159159863505677800`. G5 set SUITE_PASS: 21/21 core tests and both
probes, input resources unchanged. Fresh series probe confirmed six
I53 cases, including forced Place block and Place → one Undo →
Redo for result below thresholds. G5 --i47 with rockBlocks m06=0,m19=1:
G5_GATE_PASS, 32/32 both profiles in all configurations and identical
repetitions.

For manual test, use `scratchpad/i53-index-integrator/references.i53`
with matching client or synthetic indices `copy-fixture.i53` and
`clean-fixture.i53` from `scratchpad/i53-integrator/run` in probe profile.
Manual GUI assessment and actual RAM exhaustion remain unverified.
After merge, agent's build-ninja worktree was removed. G6 remains open.

## I52 — candidate gallery and selection (2026-11-18)

Criteria pre-registered in `0a25e6c`. Codex medium implementation in separate
worktree reviewed and merged ff as `f610cde`. Added N=1–16 (default 4),
thumbnails, explicit initial/actual seed and attempt count, result selection,
and recommendation A only for explicitly linked M06/M19 profiles.
Without policy, the first acceptable result is selected. B does not reach GUI.
Ranking still has unfulfilled quality gate I51.

Gallery stores metadata and thumbnails; full sandbox of selected result
is reproduced without retry and compared with saved floor fingerprints.
Place records validated sandbox via DrawOperations as one Undo.
Series has shared estimated budget, cancellation, and actuality control for map,
parameters, and client data. Real OOM/RSS not verified.

Executor report and detailed manual steps:
[g6_i52_report.md](smoke/g6_i52_report.md). Executor reported build PASS,
23/23 CTest, Commit/Surface/series probes PASS, and G5 32/32 both profiles.
Two final bug fixes were rebuilt at executor without re-linking probes, so
integrator runs fresh probes from final sources.

Independent validator on final sources:

- Canonical build: BUILD SUCCESSFUL.
- Set: SUITE_PASS, 20/20 core tests; freshly rebuilt
  --commit and --surface probes PASS, input resources unchanged. Surface: 157
  known SetFocus warnings, no other stderr.
- Freshly rebuilt --series probe: CANDIDATE_DIALOG_PASS, exit 0,
  input resources unchanged. N=1/4/16, both profiles and fallback, selection
  of another acceptable candidate, Place/Undo/Redo, cancellation, parameter change/close/stale map, and forced hardFail Place PASS.
  Seven known SetFocus warnings, no other stderr.
  This is automatic control of production controls, not manual assessment.
- Measurement G5 --i47 rockBlocks m06=0,m19=1: G5_GATE_PASS, 32/32 both profiles.

Private evidence: scratchpad/g6-i52-{build,suite,series,g5}.log and
scratchpad/i52-integrator/verification.json. Probes rebuilt after both
executor final fixes; real OOM and allocation failure injection in
these paths not re-tested.
After ff-merge, only build-ninja worktree t3-aaa1909a removed after
checking target path. Manual GUI/DPI/keyboard test not performed.
I52 remains partial for assessment; I51 and I51b FAIL, G6 still open.

Manual test: generate N=1,4,16; select another acceptable candidate
than Recommended, verify floors, Place -> one Undo -> Redo. Verify
Place block for hardFail, series cancellation, parameter change, and default
mountain wall. Result selection nor Preview can change the map.

## I51b — failed experiment (2026-11-18)

Pre-registration `9ab1ca0`; Codex medium implementation in separate worktree,
review, and ff-merge `9b79919`. Default ranking A retained; B available
only in explicit offline mode --i51b. No changes to generator, GUI, or bands.

**I51B_FAIL:** B does not exceed A in any profile. Seeds 2001–2032:
m06 A=29/32, B=28/32; m19 A=27/32, B=21/32. Required B≥23/32 and B>A
separately for both profiles. Criteria and model not tuned after result.
Historical I51 still FAIL 18/32 both profiles; we do not replace its result
with new seeds. G6 remains open; this holdout was previously disclosed.

Executor report: [g6_i51b_report.md](smoke/g6_i51b_report.md),
full results [g6_i51b_holdout.json](smoke/g6_i51b_holdout.json).
Integrator re-evaluated raw reports and confirmed result.
Integrator tests: I51b 3/3 PASS, I51 evaluator 8/8 PASS.
Executor: 22/22 CTest PASS and two identical full matrix runs.
Integrator did not re-generate full matrix; reproducibility assessed
on executor reports.

Independent integrator validation: canonical build PASS; SUITE_PASS,
19/19 core tests, --commit and --surface probes PASS, input resources
unchanged (surface: 157 known SetFocus warnings, no other stderr).
Measurement G5 --i47 with rockBlocks m06=0,m19=1: G5_GATE_PASS, 32/32 both
profiles and identical repetitions. CLI test on rebuilt tool:
CANDIDATE_RANK_CLI_PASS, including fingerprint 17159159863505677800 and --i51b.
Worktree did not contain build-ninja, no directory to remove.

After build completion, offline CLI --i51b can be checked: for each seed
winner (A) and winnerB (B) should appear, computed from same rows.
Command repetition must give same report. B does not reach GUI.
Manual GUI test: Place/Undo/Redo and mountain steps described below;
not performed by integrator. Original RME and determinism across
compilers not confirmed.

## I51b — prepared experiment (2026-11-18)

At user request, an experiment to swap inBand/distance priority was recorded
before implementation and measurement. Only variant B and control A will
be evaluated on shared new matrix of seeds 2001–2032. Criteria, threshold
23/32 for both profiles, and improvement requirement versus A: G6_METRICS, I51b section.
B implementation and measurement not performed. This is a development experiment on
previously disclosed holdout; possible PASS will not close G6.
I51 result remains FAIL. Latest regression results below concern I51.

## I51 — full policy, failed result (2026-11-17)

Criteria completed before implementation and evaluation in `396e057`.
Codex implementation (medium, separate worktree) reviewed and merged
fast-forward as `0a9c127`. Ranking includes 26 metrics and versioned
bands, distinguishes definitive N/A from data errors, applies frozen
distance and aggregation of three holdout families.

**I51_HOLDOUT_FAIL: m06 18/32, m19 18/32; required 23/32 separately.**
Random baseline: m06 127/256, m19 1/2. Threshold, bands, and
generator unchanged after result. I51 and G6 remain open.
Report: [g6_i51_report.md](smoke/g6_i51_report.md), details in
[g6_i51_holdout.json](smoke/g6_i51_holdout.json).

Integrator re-ran evaluation of agent's raw reports: confirmed
18/32 for both profiles. Independent evaluator tests: 8/8 PASS.
Executor reported 21/21 CTest PASS and two byte-for-byte identical full matrix
runs with --g5-levels. Integrator did not re-generate full matrix.
Holdout coverage is small: known 5–8 positions per map of 26; penalty
limit additionally reduces number of actually differentiating positions.

Independent integrator validation:

- Canonical build: BUILD SUCCESSFUL.
- Set with probes: SUITE_PASS, 19/19 core, --commit and --surface PASS,
  input resources unchanged; 157 known SetFocus warnings in surface.
- Measurement `g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1`:
  G5_GATE_PASS, 32/32 for both profiles, identical repetitions.
- CLI on rebuilt tool: CANDIDATE_RANK_CLI_PASS, fingerprint
  17159159863505677800 preserved.
- Holdout evaluator: 8/8 PASS; recomputation of actual reports
  confirms I51_HOLDOUT_FAIL. Full matrix re-generation not performed.

Executor worktree does not contain build-ninja, no directory to remove.
Manual GUI steps below remain current; manual test was not performed.

## I51, 2026-11-17 — partial implementation

Criteria recorded before implementation in `9149923`. Codex agent changes
(reasoning medium, separate worktree) reviewed and merged fast-forward as
`b828824` to `feature/procedural-g6-ranking`.

Added pure lexicographic ranking, deterministic seeds, shape measurements,
offline CLI, and tests. Generator, GUI, and map saving did not change behavior.
Executor report: `source/procedural/tools/candidate_rank.md`.

**I51 and G6 remain open.** No approved bands for m06/m19 nor
passed holdout comparison. Need to clarify metric list, N/A,
non-positive references, and result aggregation. `G4_SPLIT.md` assigns M12–M17
to M19 family: these maps cannot be considered independent holdout of that family.
Criteria unchanged after results.

Executor reported 19/19 core tests, CLI test, preserved
17159159863505677800 fingerprint, and measurement matrix reproducibility. Reproducibility
without bands does not pass ranking.

Integrator validation:

- Canonical `build_ninja.bat`: BUILD SUCCESSFUL (MSVC x64 Release).
- CLI test on built `candidate_rank.exe`: CANDIDATE_RANK_CLI_PASS,
  including preserved fingerprint 1729.
- `g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1`: G5_GATE_PASS,
  32/32 for both profiles, identical repetitions, dungeon variants and retry 0/8.
- Full set with probes: SUITE_PASS; 19/19 core tests,
  --commit and --surface probes PASS, input resources unchanged.
  Surface probe: 157 known SetFocus warnings, no other stderr.

First build did not find Ninja; retried with local Ninja vcpkg.
First regression set reported SUITE_FAIL: linker LNK1104, because parallel
measurement used style_render.exe. After measurement completed, set re-ran.

User accepted holdout correction (commit `70c1053`): M01, M05,
M11+M18 as independent data; M12–M17 separately as M19 family.

## Manual GUI test after the build

1. Open an empty map with loaded client and generator window.
2. Set seed 1729, 100×100 area, and default parameters; verify preview reproducibility.
3. Run Place, then one Undo: entire result should disappear; Redo should restore it.
4. In cave variant, verify default mountain wall.

I51 does not add gallery or feedback to GUI. Manual test, holdout, blind test,
compatibility with original RME, and determinism across compilers were not confirmed.
Agent worktree did not contain build-ninja, so no directory to remove.
