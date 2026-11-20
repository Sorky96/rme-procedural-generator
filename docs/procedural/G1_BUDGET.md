# G1: joint terrain preparation budget

Newer section: [G1_MASK.md](G1_MASK.md) verifies retrieval and revalidation of the exact mask of the actual Selection. Integration with dialog and control at commit remains open; the budget results below are kept for history and repeated in the new full probe.

Status 2026-11-02: **PASS_G1_JOINT_PREPARATION_BUDGET_ONLY; full G1 IN PROGRESS**. This section complements [selection isolation](G1_SELECTION.md) and [sandbox](G1_PREVIEW.md) with one preparation limit. It does not introduce commit handling or generator interface.

## Contract

`prepareTerrain` in `source/procedural/adapter/terrain_preparation.{h,cpp}` combines snapshot capture, semantic core, resolver, and `DrawOperations::prepareTerrainPreview`. It returns its own, movable `PreparedTerrain`: snapshot, concrete plan ID, sandbox/diff, and `PreparationMemory` report. The semantic grid is destroyed before building the sandbox. The result does not borrow the mask or profile of the caller.

`PreparationBudget` specifies `maxCells`, `maxContextCells`, `maxChanges`, and `maxBytes`. Default zeros refuse execution. Joint preparation works only on the GUI thread, with no yield and no mutation of the source map, global RNG, brushes, or selection. It handles `stop_token`, including refusal before start and cancellation during work; this is not yet Cancel button handling or responsive GUI schedule.

A single counter of remaining bytes decreases after checked reservations. First, infrastructure is reserved (`sizeof(PreparedTerrain)` and 64 KiB allowance), core and plan, then snapshot, and the sandbox receives only the remaining limit. The report sums `infrastructureBytes`, `snapshotBytes`, `semanticBytes`, `planBytes`, and `previewBytes`. Released stages are not credited back: it is a conservative sum of stage costs, not a measurement of momentary live object memory.

Preflight checks geometry and overflows, two-tile halo consistent with capture, coordinate bounds, and cell limits. After snapshot it checks full mask sizes and the number of actually writable fields before generation/resolver. A sparse mask does not consume the change limit with the whole rectangle. The names of three brushes and the client identifier have length 1–256 bytes. The plan includes the ID vector, three profile strings, the retained client identifier, and its local copy of the resolver with overhead. New `VersionManager::GetCurrentVersionIDView()` lets you check length without prior allocation; the view becomes invalid when the client changes/unloads, the existing getter remains unchanged.

Stage errors are kept in the `PreparationError` variant. Input errors and external `bad_alloc` have separate `PreparationInputError`. Refusal does not publish a partial result. Final stop test precedes return of owners.

## Budget limits

64 KiB and allocation overhead are explicit conservative estimator assumptions, not measured guarantees of stack or system RAM. The budget does not cover the source map, borrowed inputs, already-loaded assets, the entire application/UI/renderer, runtime allocation, allocator overhead beyond assumed assumptions, other results held concurrently, or future Undo. There is no hard process heap/RSS limit here.

The test counter captures requested bytes of plain `new/new[]` on the calling thread. It uses a fixed array with no allocations during measurement; it does not change malloc pointer layout passed between RME and wxWidgets DLL. It does not cover fully aligned-new, C/DLL/GPU allocation, or RSS. Warmup uses existing profile and independent stage paths. The measurement array, fixture, source, and assets remain outside the task report. Zero result after destruction pertains only to tracked requests, it does not prove absence of all process leaks.

## Implementation and independent review

Claude Code completed two adapter files in a separate checkout via local bridge. Runs `231dc92b-7e46-4d8c-844b-725c2d5fd266` and fix `817abf8b-36b6-46f5-8d75-a9c290090c11` ended with code 0, no tool refusals, and empty stderr. Report, stdout, stderr, full new files, and diff were reviewed; the worker's existing tracked diff did not change between these runs. Claude did not build or test.

Codex reviewed the implementation and commissioned fixes: use allocation-free view instead of copy-getting client, count three profile strings instead of nine, and explicitly check mask size agreement instead of silent minimum. Codex added the getter, CMake integration, tests, and verifier, and copied accepted files byte-for-byte. Integration hashes are in `local-data/procedural-smoke/g1-preparation-integration-20261102.json` and are checked by verifier. No commit or push; shared uncommitted changes kept.

## Results

Canonical Windows x64 `build_ninja.bat`: exit 0, 181 steps, log `local-data/procedural-smoke/g1-preparation-canonical-20261102.log`. Current normal EXE SHA-256: `a8ce2a3d23f8dfd41abc9a5d0963729b801de8d7069a349fc0c1fedfa56dd439`.

Full trial of normal Application via test wxModule: `local-data/procedural-smoke/g1-preparation-visible-rme-20261102`. Exit 0, normal shutdown, empty stderr, source assets unchanged. Used private referenced assets and two synthetic maps in memory; no user map files opened. Verifier `g1_verify_preview_probe.py --preparation` confirmed 416 current RME objects, 877 G0 database sources, and ten expected G1 differences. Evidence: `verification.json` and `current-verification.json` in this directory. Probe EXE SHA-256: `72c9b31c5e213836df106dbcb4c1385d6a6ab44097b0e1f5027d8918b281db6b`.

| Size | Joint estimate (B) | Peak tracked new requests (B) | Tracked bytes after destruction | Fingerprint |
|---|---:|---:|---:|---|
| 100×100 | 7 234 198 | 2 923 649 | 0 | 18297947229083653543 |
| 500×500 | 168 808 358 | 69 507 073 | 0 | 5645408562528222048 |

Both sizes were compared against separately composed core → resolver → sandbox path: concrete ID and fingerprint are identical. Exact joint limit passes; limit one byte smaller refuses as `PreviewError::Limit`. Limit sufficient for each individual stage but insufficient for their joint preparation also refuses. Component sum, ownership after move, rejection of moved-from preview, and independence from later mask/caller profile change were checked.

**106 single-allocation-failure cases** in preparation passed: controlled `AllocationFailed` or full deterministic result, no partial result and no source/cache changes; retry remains deterministic. Zeros and short cell/context/change limits, snapshot limit preserving its error type, sparse mask, overflowed/negative geometry, no writable fields, oversized profile rejected before allocation, missing brush, wrong thread, no client, pre-cancel, and stop during 500×500 also passed. Protected UID at water/grass boundary propagates `OutOfMask` with no map change.

The entire existing probe was repeated: four sandbox variants 100×100/500×500, 79 of its allocation faults, full diff and guards, base-ground, mask/temple, epoch assets, cache and two-editor events, selected-context and selected-border refusal, plus normal Lua setter and selection with Undo/Redo. This regression of normal actions does not cover future generator commit. Older snapshot/border-order test programs remain historical evidence at unchanged proper sources.

First run `g1-preparation-budget-rme-20261102` is kept: despite correct markers and code 0 it contains wxWidgets `SetFocus` error (0x00000057) in stderr and **is not a clean pass**. Original runner printed `stderr_empty=false` but did not account for it in `passed`; now empty stderr is a success condition. Test option `--visible` was added and full trial repeated with visible private window; new run is clean. The cause of the error or whether visibility fixes it generally was not proved. Production UI code or wxWidgets was not changed and stderr was not filtered.

## Remaining G1 requirements

Open: actual RSS/whole application and result coexistence, future Undo memory, GUI Cancel and schedule, exact GUI mask, and staleness validation of assets/map/mask/selection before commit. Future commit must include membership of empty tiles in selection collection, also without selected bit. Missing: atomic commit of exact diff with failure/Undo/Redo and independent manual drawing history, minimal dialog/rendered preview, GUI/OTBM round-trip 100×100 and 500×500, and confirmation of order equal to other platforms.

G2 not started. forgotten.otbm and companions not used, Docker, database, or user world not touched.
