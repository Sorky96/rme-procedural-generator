# Procedural Generator — test plan and G0 evidence

## 2026-11-03 — G1: baseline RSS and GUI blocking measurement

**PASS_G1_GUI_RESOURCE_BASELINE_ONLY**, full **G1 IN PROGRESS**. [Report, CSV and screenshots](G1_RESOURCES.md). Three actual Preview/Validate/Cancel cycles for 100×100 and three for 500×500, without a second large result in the observer. Preview 500×500 to observation READY: 523.5–528.4 ms; GUI timer breaks: 536.5–546.1 ms; peak WS of entire process 805.6 MiB, growth 79.3–80.5 MiB. After large Cancel WS 724.5/724.9/728.8 MiB; three cycles do not determine absence of leaks. All six cycles preserved the map, sentinel, revision and empty history. Normal shutdown/exit 0/empty stderr/assets unchanged; 421 objects and CSV hash verified. Production and normal EXE remained unchanged; Codex without new Claude.

**Cancel during generation not implemented or passed.** Measurement covers Cancel after completed Preview and Validate. Estimator budget 512 MiB is not the process RSS limit. Next point: staging/cancellation and rejecting delayed results; then GUI refusals and final OTBM/companion round-trip. G2 not started, older audits preserved, no access to user maps/world/DB/Docker.


## 2026-11-03 — G1: Undo hints and readable small selection

UX fix completed: refusal message provides current limits and path to Preferences, selection mode hides rectangle fields and shows captured range/field count, minimap is centered through explicit `StretchBlit` rectangle. [Report, screenshots and hashes](G1_DIALOG_UX.md). Without new Claude run. Final Windows build exit 0; `g1-dialog-centering-rme-20261103` **PASS_G1_NATIVE_DIALOG_SELECTION_UX_ONLY**, normal shutdown/empty stderr/assets unchanged and 421 objects verified. Checked description, hidden fields, centering/resize and stable refusal/Place/Undo/Redo for five fields with holes.

Full 500×500 + selection matrix and 40 MiB refusal/512 MiB retry passed earlier in this step (`g1-dialog-polish-retry-rme-20261103`, **PASS_G1_NATIVE_DIALOG_UX_MATRIX_ONLY**), before final change of paint alone. **This matrix was not rerun on final EXE**; first failed wrapped message comparison test preserved as FAIL. Full **G1 IN PROGRESS**. Next item: responsive cancellation/scheduling and RSS; then remaining GUI refusals and final generator OTBM/companion round-trip. G2 not started; older audits not restarted, shared changes and user data preserved.


## 2026-11-03 — G1: GUI 500×500 and exact selection

**PASS_G1_NATIVE_DIALOG_MATRIX_ONLY**, full **G1 IN PROGRESS**. [Report and screenshots](G1_DIALOG_MATRIX.md). Actual Preview/Validate/Place/Undo/Redo for 250,000 fields and five unflagged members in 4×4 mask with holes; compared full context and sentinel, Undo restored tile identity and membership. Refusals of empty selection, brush, zero weights, bounds and old membership preserved source/history. With 40 MiB Undo Place 500×500 correctly refused; in the same private instance 512 MiB was set through Preferences and retry succeeded. Normal source/EXE unchanged; current build exit 0, probe exit 0/normal shutdown/empty stderr/assets unchanged, 421 RME objects verified. Codex executed step without new Claude run.

Open: remaining GUI refusals/failures, refine Undo message and small mask presentation, responsive cancellation/RSS and final generator GUI/OTBM round-trip. G2 not started; shared changes and user data preserved. Next 3 tasks listed in report.


## 2026-11-03 — G1: native dialog and actual GUI 100×100

**PASS_G1_NATIVE_DIALOG_100_ONLY**, full **G1 IN PROGRESS**. [Report and screenshot](G1_DIALOG.md). Map → Generate terrain…: Preview/Validate/Cancel and Place as one Undo action; preview is minimap of specific tiles. Canonical build exit 0; `g1-dialog-final-rme-20261103` PASS/exit 0/normal shutdown/empty stderr/assets unchanged. Independent observer checked no mutation before Place, wrong seed and invalidation, Cancel and actual Place/Undo/Redo with 10,000 field comparison. Verifier confirmed 421 current RME objects. Claude implemented dialog and performed read-only review; Codex fixed layout/labels and performed tests. First incomplete inspection preserved as FAIL, without pass.

Open: GUI selection/refusals/failures and 500×500, sprite preview, responsive cancellation/scheduling, RSS/other results and generator GUI/OTBM round-trip. G2 not started; shared changes and user data preserved, no commit/push.


## G1 — concrete preview in atomic history, 2026-11-03

**PASS_G1_ATOMIC_TERRAIN_HISTORY_CORE_ONLY**, full **G1 IN PROGRESS**. [G1_COMMIT.md](G1_COMMIT.md). New canonical Windows x64 build exit 0; `g1-commit-complete-rme-20261103`: exit 0, normal shutdown, empty stderr and unchanged curated assets. Verifier confirmed 421 current objects, 877 base sources and 22 intended differences. Claude performed map reservation and review, Codex integration and independent tests. Shared G0/Claude changes preserved; no commit/push and no access to user maps/world/DB/Docker.

Commit copies specific diff without RNG/autoborder to separate Action/Change/ActionQueue; all allocations before swap and publication. Undo/Redo exchange full state and actual selection membership regardless of flags, also null-source Redo and subsequent normal Selection Undo. Passed 112 commit faults, 15 reservations, sizes 100/500, exact/short budget, stale/busy/live/Lua/thread/cancel, minimap and Selection preflight faults/retry, Lua callback and equal membership changes. Current full previous probe also PASS. Separate fresh G0 queue regression: PASS functional; old limitations/GUI warnings described in report.

Open: GUI dialog/rendered preview and Commit/Cancel, responsive scheduling/RSS/other jobs and generator GUI/OTBM round-trip. Do not equate history core with full gate. Next 3 tasks: (1) minimal G1 dialog with current request/mask/preview, (2) GUI Commit/Cancel and limit parallel results, (3) full GUI/OTBM round-trip and close G1 matrix. G2 not started.

## G1 — current offline target, 2026-11-03

**PASS_G1_CURRENT_OFFLINE_TARGET_ONLY**, not full gate. [G1_TARGET.md](G1_TARGET.md). Canonical build exit 0; `g1-target-rme-20261103/verification.json` and `current-verification.json`: PASS/exit 0/empty stderr/normal shutdown/assets unchanged. EXE `8282a1c7…`, 420 current objects, 877 base sources and ten expected G1 differences.

Zero allocation of current target for 100×100/500×500 and selected membership 500×500/4×4. Passed 22 refusals and positive controls: different map, both busy/cancel/thread, configuration/revision, mask/region/mapGeneration/membership, no map after close and new generation after restore, unconnected client and unbound server. Shared view and closing first view do not invalidate still-live map. Test does not force address reuse and does not network. Full previous probe (79/106 faults, 29 prepared refusals, 39 asset mutations, selection/Lua/UndoRedo) also PASS; startup warnings closed manually in same instance.

Did not pass atomic commit/Undo of generator, exclusion of active Lua transaction/history, publication of empty selection membership, GUI Commit/Cancel, rendered preview, full RSS/Undo or GUI/OTBM round-trip. Historical focused EXE preserve current proper sources/objects but do not contain new target adapter.

## G1 — current asset catalog, 2026-11-03

**PASS_G1_GENERATION_ASSET_CATALOG_ONLY; full G1 IN PROGRESS.** [G1_ASSETS.md](G1_ASSETS.md). Windows x64 build exit 0, EXE `e17b2f82…`. Full probe `g1-assets-rme-20261103`: PASS/exit 0/empty stderr/normal shutdown/input assets unchanged, 419 current objects and 877 G0 base sources with ten expected G1 differences. 39 catalog mutations without epoch bump refused validation; after value restore result passed again. Scan had zero allocations. Separate freshly linked border-order and snapshot tests also PASS, including 24 snapshot faults. Reran full probe 100×100/500×500, 79 sandbox faults, 106 preparations, masks, validation and selection/Lua/UndoRedo.

Checked among others persistence through retained editorData, flags/group/type/attribute/name, variants/chances/friends/Z, specific/optional/global border, border_types array, minimap/light/geometry metadata/sprite IDs, null case and hanging cached/raw links. Palette visibility and geometry cache refresh did not change fingerprint. Pre-cancel/wrong-thread/no-client refused. One scan sample: 13.4346 ms, without claim of speed or time guarantee. Full trial has manually handled startup warning of private assets, no restart.

Open: final editor/offline guard, atomic commit/failures/UndoRedo/empty field membership, separate manual edit history, GUI dialog/rendered preview/Cancel/scheduling, RAM/RSS/Undo/other results and GUI/OTBM round-trip. Raw pixels/textures/GPU/cache/runtime animation and internals of other brushes outside fingerprint of this slice. G2 not started; user data and shared changes preserved, no commit/push. Did not force parallel writes, persistent OOM, damage to owners/store tables or mid-call stop of this scan.

## G1 — prepared result validation, 2026-11-02

**PASS_G1_PREPARED_VALIDATION_ONLY**, not full gate. [G1_VALIDATION.md](G1_VALIDATION.md) describes control and limitations. Windows x64 build exit 0; `g1-validation-rme-20261102/verification.json` and `current-verification.json`: PASS/exit 0/empty stderr/normal shutdown/assets unchanged. EXE `47ab14b0…`, 418 current objects, 877 base sources, ten expected G1 differences. Informational startup dialog of private assets closed through computer-use; test executed in same instance.

100×100/500×500: zero validation allocations, no fingerprint changes; moved-from refused. 29 cases include foreign map, every field config, profile/masks, stop/thread, changes and mixing public plans/inputs/preview, payload, UID copy, live AID in halo without revision, location count and revision. Separately unload -> NoClient and reload -> StaleAssets. Full previous probe with 79 sandbox faults, 106 preparations, exact masks and normal selection/Lua/UndoRedo also passed. Joint estimates 7,234,214 / 168,808,374 B include additional sizeof; peaks of tracked new preparation did not change.

Did not pass independent asset changes without epoch, editor/offline lifetime and final validation on actual commit, generator UndoRedo and empty field membership, mid-call Cancel of validator, GUI Cancel/scheduling, full RSS, dialog/renderer-preview or GUI/OTBM round-trip. Older limited and unclean results preserved; did not restart completed audits.

## G1 — exact Selection mask, 2026-11-02

**PASS_G1_EXACT_SELECTION_MASK_ONLY**, not full G1 gate. [G1_MASK.md](G1_MASK.md) contains contract, Claude review and limitations. Windows x64 build exit 0; `g1-exact-selection-rme-20261102/verification.json` and `current-verification.json`: PASS/exit 0/empty stderr/normal shutdown/assets unchanged. EXE `9b4c2334…`, 417 current objects, 877 base sources, ten expected G1 differences.

Masks 100×100/500×500 of actual two-card collection preserve one hole and 9,999/249,999 empty members without selected bit. Estimates 10,104/250,104 B, exact limit passes, capture has one allocation, validate zero. Passing mask to prepareTerrain gives specific diff only inside mask without source change. Passed same-bounds membership swap and return, object exchange at fixed coordinates, item flags independent of membership, ownership, padding, malformed mask/count/geometry, busy, empty, many floors, wrong thread, pre-cancel, limit and single mask fault, detached/foreign/outside-header entry. New empty field in hole invalidates snapshot despite current mask. Reran full previous probe with 79 sandbox faults and 106 preparations and selection/Lua/UndoRedo.

Did not pass mask integration with dialog/commit, future membership after commit/Undo, mid-call Cancel of mask alone, persistent OOM/RSS, generator commit/failures/separate history, GUI Cancel/scheduling, rendered preview and GUI/OTBM round-trip. Did not force null/duplicate/order corruption inside Selection; checks read in code. Older FAIL/limited results preserved.

## G1 — shared preparation budget, 2026-11-02

**PASS_G1_JOINT_PREPARATION_BUDGET_ONLY**, not full G1 gate. [G1_BUDGET.md](G1_BUDGET.md) describes contract, estimator, Claude and measurement limitations. Windows x64 build exit 0; `g1-preparation-visible-rme-20261102/verification.json` and `current-verification.json`: PASS/exit 0/empty stderr/normal shutdown/assets unchanged, 416 current objects, 877 base sources and ten expected G1 differences. EXE `a8ce2a3d…`.

100×100 and 500×500 compared with separate stages: identical ID/fingerprint, exact joint limit passes, limit minus byte refuses; independently sufficient stage limits disregard joint limit. Passed ownership/move, cost sum, rare mask and cells/context/changes limits, geometry, too-long profile before allocation, no-client/wrong-thread/pre-cancel/mid-call stop, error type handling and protected UID. 106 preparation allocation faults, 79 sandbox faults and full regression of two editors/selection/Lua/UndoRedo passed without source change and global RNG. Estimates / tracked peak new: 7,234,198 / 2,923,649 B and 168,808,358 / 69,507,073 B; zero tracked bytes after destruction. New counter is not measurement of RSS or all process allocations.

Preserved unclean first run `g1-preparation-budget-rme-20261102` with SetFocus error in stderr. Runner tightened: empty stderr is success condition. Full new visible run passed; cause of first error remains unproven. Did not pass GUI Cancel/scheduling, mask/staleness/selection on commit, generator commit/UndoRedo, full RSS/Undo/other tasks, dialog/renderer-preview, GUI/OTBM round-trip or order of equals on other platforms.

## G1 — selection isolation, 2026-11-02

**PASS_G1_PREVIEW_SELECTION_ISOLATION_ONLY**, not full G1 gate. [G1_SELECTION.md](G1_SELECTION.md) describes contract, oracle, Claude review and proof boundaries. Canonical build exit 0; `g1-selection-lua-rme-20261102/verification.json` and `current-verification.json` PASS/exit 0/empty stderr/normal shutdown/unchanged source assets, 415 current objects, 877 base sources and ten expected G1 differences. EXE `7048a947…`.

Checked actual two-card cache through public bounds with positive controls, isolated notifications of copy/action/other map/invalid/null, no unwanted `selectionChange` in controlled empty session, safe selected context of both maps and refusal of selected-border with identical unselected positive control. Normal selection deletion and Lua setter `app.map…ground.actionId` passed Undo/Redo/final Undo with proper pointers and boundaries. Full sandbox regression 100×100/500×500 and 79 allocation faults also check cache. Selection no longer globally locked as `SelectedContext`.

Did not pass GUI capture mask, empty membership/stale selection on commit, Lua emission for non-empty selection actions, generator commit/Undo/Redo, full RAM/RSS, GUI Cancel/scheduling, dialog/renderer-preview, GUI/OTBM round-trip or portability of equals. Preserved FAIL: missing ActionQueue include and nonexistent API in harness script; corrected new run passed in full.

## G1 — sandbox and bordered diff, 2026-11-02

**PASS_G1_OWNED_BORDERED_PREVIEW_ONLY; full gate IN PROGRESS.** [G1_PREVIEW.md](G1_PREVIEW.md) contains scope, commands/harnesses, Claude delegation, limitations and preserved FAIL/timeout. `g1-preview-reviewed-canonical-20261102.log` build 0; `g1-preview-reviewed-rme-20261102/verification.json` and `current-verification.json` PASS 0, 415 current objects, 877 base sources and nine intended differences. Current normal EXE `fdfc4632…`.

Actual core/resolver/DrawOperations placement/autoborder, exact tiles and fingerprint of repeats 100×100/500×500, global RNG, exact/short budget, 79 allocation faults, pre-Cancel and Cancel during work, wrong thread/no client, malformed plan/context/item/profile, raw source change, full UID/AID/PZ at out-of-mask border, irregular mask and temple, refusal of selected context, compatibility of base-ground variant, stale preview after force reload. Normal shutdown, empty stderr, source assets unchanged. Separately actual overlapping specific rules at both address orders/dedup/default path and snapshot regression with 24 capture faults PASS. CTest core 1/1 Release.

This is not test of GUI Cancel click, GUI responsiveness, current selection cache of other editor, full work memory/RSS, atomic commit/Undo/Redo, dialog/renderer, OTBM round-trip or equals compatibility between standard libraries. Selected context is rejected, not handled. Not proceeded to G2.

## G1-B — snapshot and copy accounting, 2026-11-02

**Snapshot stage PASS, full G1 IN PROGRESS.** [G1_SNAPSHOT.md](G1_SNAPSHOT.md). Canonical `g1-snapshot-recover-canonical-20261102.log` PASS 0 after correction of own VCPKG_ROOT wrapper; first interrupted build preserved without PASS. New normal EXE `f6185abf…`.

Actual probe `g1-snapshot-rme-20261102/verification.json` PASS/exit 0/empty stderr: absence of estimator allocation, payload/budgets, depth 64/65/null item, halo ownership and recursive data, changes UID/AID/opaque/Creature/Spawn/location metadata/map generation/revision, lost copy without matching definitions and 24 one-time capture allocation errors without source change. Snapshots 100×100/500×500 with halo have 10,816/254,016 fields and estimates 453,760/10,661,760 B at 64 MiB snapshot limit. Test covers rare maps without client, not full generation and not peak RSS.

Verifier `g1-snapshot-verification-20261102.json` checked 414 probe objects, current sources/EXE and 877 base sources: only six intended changes. Result `PASS_G1_OWNED_SNAPSHOT_ONLY` does not pass DrawOperations sandbox, bordered diff, full work budget, GUI Cancel/stale selection, atomic commit/Undo/Redo or GUI/OTBM round-trip G1. Full workflow and final manual smoke remain unexecuted.

## G1-B — actual resolver and asset revision, 2026-11-02

**Placement plan PASS, full G1 gate still IN PROGRESS.** [G1_RESOLVER.md](G1_RESOLVER.md) describes executed contract, Claude delegation and limitations. Canonical build `g1-resolver-revision-canonical-20261102.log` exit 0. Actual `g1-resolver-revision-rme-20261102/verification.json` exit 0, normal shutdown, empty stderr, unchanged source assets.

Test covered full placement ID vectors 100×100/500×500 on active 1098-BlackTek, proper brushes/items, exact repeats, independence from mask/global RNG, UID/AID/PZ and temple protection, budget boundary, refusals of client/brush/item meta/tamper mask/worker thread and pre-Cancel. Separately actual no-op, force reload of same ID, failed reload with private data directory, explicit/empty unload and refusal of old input; revision 3→9. Did not test commit, bordering, full RAM or GUI Cancel during work.

Preserved first FAIL exit 9 in `g1-resolver-rme-20261102`; corrected zero-weight variant refusal. Independent verifier checked 411 objects and sources/EXE and 877 base sources: only four intended GroundBrush/VersionManager changes. Result `PASS_G1_PLACEMENT_AND_ASSET_REVISION_ONLY` in `g1-resolver-verification-20261102.json` does not replace full E2E G1.

## G1-B — input and ground variants, 2026-11-02

**Partial G1 stage PASS, full gate IN PROGRESS.** [Contract, execution and boundaries](G1_INPUT.md). Canonical build `g1-input-canonical-20261102.log` exit 0; CTest `g1-core-faults-tests-20261102.log` 1/1 Release. New cases: default parameters fail closed, malformed SemanticGrid, pre-Cancel validation, five one-time core allocation errors and exact retries. Fingerprints 100×100/500×500 unchanged; core only, no E2E.

Actual RME probe `g1-input-rme-entry-20261102/verification.json`: exit 0, empty stderr, 12 synthetic map positions and nine protection variants, masks without aliasing/mutation, budget/pre-Cancel/worker refusal, legacy ground boundaries and 10,000 explicit rolls without RNG shift. No client or GUI. First harness had double main and FAIL link preserved in `g1-input-rme-20261102`; corrected builder swaps only private input object.

Independent `g1-input-verification-20261102.json` checked 410 probe objects, current sources/EXE and 877 base sources; differ only in two intended GroundBrush files. Did not test full tile/halo/assets snapshot, resolver/DrawOperations, full RAM, cancellation during run, atomic commit/Undo/Redo, stale preview, dialog or G1 round-trip. Nothing from this list is passed by current attempts.

## G1-A — started core, 2026-11-02

`test_procedural_core` registered in CTest, build and execution Release **PASS 1/1**. Actual library `source/procedural/core/generation.cpp`, no GUI/Python model. Checked: seed/config and stream independence, bounded RNG, weights (also sum wider than uint32), irregular mask/protection without input change and remaining RNG, boundaries/Z, zero and extreme sizes, cell/output limits, cancellation before start, fingerprint and full vectors 100×100 and 500×500. Log `local-data/procedural-smoke/g1-core-tests-20261102.log`; details [G1_CORE.md](G1_CORE.md).

This **does not pass G1**. Resolver, snapshot/halo RME, full budgets, stale preview, commit, Undo/Redo, live and GUI/E2E remain unexecuted. Core test does not prove border or serialization correctness, does not contain allocation injection and cancellation during work.

## Current assessment 2026-11-02: G0 VERIFIED

Final matrix of six phase 0 requirements and G0-01…09 contained in [G0_FINAL_20261102.md](G0_FINAL_20261102.md). Audit gate of build + actual integration points + synthetic map round-trip fulfilled. G1 not started. Dated entries below, including old 2026-11-01 matrix, are history and do not replace this assessment.

New `g0-draw-lua-clean-20261102` closed absence of current manual Draw/Lua/Draw: normal EXE `e8c3041e…`, two dried grass strokes, Lua transaction, exactly three entries, three Undo/Redo and final Undo, normal exit 0, stderr empty, no fixture save. `group_actions=true`, `use_automagic=false`; previous automagic=true remains separate test. First run preserved with accessibility assertions during shutdown; not considered clean GUI. Cause of diagnostics remains unresolved.

`g0-final-gate-complete-20261102/verification.json` has `PASS_G0_AUDIT_EVIDENCE`: hashes of current implementations, 35+67+1 full snapshot comparisons, rerun full verifier of four snapshots/two Save As and four negative controls, nested paths and zone. Rechecked current 180 sources/76 objects and 6 receiver results, no restart of its runtime. `g0-final-gate-canonical-20261102.log`: build 0, `ninja: no work to do`, unchanged normal EXE. User data/Docker untouched.

G0-09 is executed receiver audit with result `PASS_BOUNDED_CURRENT_RECEIVER`, **not** pass of full spawn behavior: current converter changes radius and omits direction. RME preserves source XML in round-trip. Justification of gate boundary, full evidence, preserved diagnostics and explicitly unexecuted tests in final report. No historical PARTIAL was renamed to full PASS.

## Handoff history and earlier assessments

## Handoff status 2026-11-02: current receiver 6/6, G0 for final assessment

**Current point completed; full gate not marked as completed.** [Handoff](HANDOFF_G0_20261102.md) contains exact hashes, commands, failed attempts and next steps. Historical descriptions below remain dated; newer results replace their current status, do not remove FAIL/PARTIAL.

`g0-blacktek-current-sources-20261102/build-hostx64-result.json`: **build 0**, 77 current CPP without unity, host MSVC 14.44.35207 x64, unchanged sources. Server binary `28d24ff43b6f1186d4d5adfa44f66b547b1eb405dba25646b823721db97d54fe` was not executed. Separate probe `b5bc1af23d3d60c318eaa9d027426c41c3ffce607c425146a3fa861677b0e0ce`, source `53feb0a23a4104c72461f4233bbf290ae9193255b3fbf850b033a63628387248`, links 76 current engine objects and renamed otserv/main, without /FORCE:MULTIPLE.

`runtime-current/iomap-cases.json` **6/6**, runner exit 0: valid 0, missing-house 5, missing-spawn 6, wrong-rent 9, empty-spawns 9, truncated-map 4. In positive test actual IOMap/loadHouses read house/exit/town/rent and selected floor 6/7 UID/AID fields and nested bags/coins. SmartConvertLegacyZoneFlags and runtime cache preserved PZ; ConvertLegacySpawns read XML and created Rat/position/time/region. Generated TOML read production ParseZoneEntry and rechecked Zone objects. Did not initialize or activate spawns or normal server. With no spawn XML function void skips conversion, and code 6 comes from harness assertion of missing zone; no error return of function itself is declared.

`runtime-current/verification-current.json`: **PASS_BOUNDED_CURRENT_RECEIVER** after independent check of 180 sources, 76 objects, HEAD/server project, EXE/sources/runner, unchanged inputs/DAT and hashes of all conversions. Python tomllib confirmed exact range 158:158:7…162:162:7, fixed/passive/forced, Rat 2:2:0 and 90 s and three protection positions 104:102:7 / 160:160:6 / 160:160:7. All six private cases and process.json preserved, instead of deleting after success/failure. Local maps destroyed before `_Exit`; global server shutdown not tested.

**Semantic limitation:** current converter applies padding 2 instead of radius 3 and does not represent legacy direction. These fields not considered preserved. Did not check normal server, DB/Docker, monster instantiation or reopening converted OTBM as complete world. This replaces historical G0-09 in scope of current receiver, does not prove full game compatibility. Full G0 requires final matrix assessment per request for continuation in new chat. Current clean GUI PASS is in `g0-gui-single-tab-20261102/verification.json`.

## Update 2026-11-02: final GUI PASS and receiver currency check

`g0-gui-local-observation-20261102`: full actual 0/1/0/1/0, user confirmed physical ID 1 row, normal exit 0 from Win32 handle; one SetFocus 0x57 with two cards. Original launcher exceeded 300 s during wait, without killing/restarting child; final existing process checked through `--verify-existing-house-gui`, verifier exit 0. This warning not removed. `g0-gui-single-tab-20261102/verification.json` **PASS**, normal EXE `e8c3041e3e7d6b06599a2c391fcc0599cfd945ebab2583e98cd958c0b4a45cfd`, one startup empty card, all five observed states, actual Redo menu, normal exit 0, empty stderr, runner 0 and unchanged inputs. This closes current normal GUI/start/shutdown in this scope. Cause of earlier empty images remains unresolved, preserved PARTIAL not rewritten.

`g0-final-canonical-20261102.log`: `build_ninja.bat` exit 0, MSVC x64 configuration and `ninja: no work to do`, canonical EXE unchanged. Log contains non-critical CMake CMP0167 warning (PowerShell described stderr as NativeCommandError); not hidden and not named compilation error.

**Gate still open through G0-09 currency.** Final hashing showed 186/187 differences from `blacktek-fresh/probe_build_manifest.json`; 85 old objects still agree with old manifest. Current server HEAD `4f82d1b1375f994106cc012f3058b3a248bd9a43`, clean `src`, 77 CPP and 103 H, new Zones/legacy spawn conversion. Historical 6/6 retains value for described snapshot but must not be attributed to current sources.

`g0-blacktek-current-20261102` **FAIL of new build preparation**: preserved vcxproj points to deleted files, including actions.cpp/bed.cpp/cylinder.cpp, C1083. Stopped only own MSBuild/tree after checking full command line with private OutDir. Build 1, no binary, manifest before/after confirms unchanged sources. Did not overwrite old objects or earlier attempt. `g0-blacktek-current-sources-20261102`: new directory, private override of `src/**.cpp` list according to current Premake, without unity, own obj/bin; **build in progress, tests not executed**. Current API has no old IOMap::loadSpawns/Spawns; probe adaptation must use actual Zones::ZoneManager::ConvertLegacySpawns and preserve proof boundaries and all inputs. Do not execute normal otserv/main or any operations on running Docker.

## Resume 2026-11-02: current normal GUI, observation still in progress

`g0-gui-local-observation-20261102`: normal canonical EXE `e8c3041e3e7d6b06599a2c391fcc0599cfd945ebab2583e98cd958c0b4a45cfd`, new private profile/run. GUI manifest consistent with previous build, without rendering observer module. CUA actually showed House header and transition of empty table to one blue ID 1 row. Earlier empty image currently not reproduced; cause of change not determined. After additional Ctrl+N second empty card is active, no save. PID 31092 / window 7279474 and full path verified through Win32; `live-observation.json` saves only current 0/1 and wait for user physical observation. User did not previously see previous closed window, which does not resolve its image during test.

**This is not full GUI PASS or G0 completion.** Left state with one house for response, without Undo or close in meantime. Separate `observe_exit.py` waits for actual exit of verified private PID; main launcher timeout does not mean process end and does not allow repetition. To pass remain further actual 0/1/0, normal shutdown, stderr assessment and final verifier.

## Update 2026-11-02: independent WGL controls and further verification boundary

`local-data/procedural-smoke/wgl-control-comparison-20261102.json` has result **DIAGNOSTIC_REPRODUCED_OUTSIDE_RME**, not full G0/GUI PASS. Local verifier `verify_wgl_controls_20261102.py` compared manifests from source and EXE, actual paint/swap/driver logs, front/back samples and observed process outputs. Each process ended with normal Alt+F4, code 0, empty stderr; screenshots actually assessed by CUA in this chat. Did not run server/map or change system settings.

| Control | Identity | Observed result |
|---|---|---|
| `g0-wgl-control-20261102` | Source `97a759e746a6375f29fad428de3722b69a30d407025af5cfc8c20a1ac9066562`, EXE `06c95141dfe2eb8bac2e4f4e2c48eb5b3a7a8b0ebd90a541522993ded79ed59e`; 0 RME objects, wxWidgets/OpenGL only | Intel UHD 4.5, swap 1 / Win32 0 / GL 0; front three times 0,0,0, back exact 255,0,0 / 0,255,0 / 0,0,255. GDI label visible, GL area gray, no stripes |
| `g0-native-wgl-20261102` | Source `408970303e8b32da26af5b139e347f277f9f32d5e75544c14d85a8c927d02201`, EXE `8959a27f56ba826801222722bf52bc03a9711cb922d91241d42011d1d0065a0c`; Win32/WGL only, no wxWidgets/RME/NanoVG | Intel UHD 4.6, same correct back, black front and successful swap without errors. Actual screenshot: white client, no stripes |
| `g0-native-wgl-generic-20261102` | Source `1bd273c1b39ee60b12ece740a3e8d31cf51f143b68793fe1cfc53a81d0ac099c`, EXE `b4260bf611a4c294b28d2dcbb612c8db18008ffee9ada9673debd3ccb16e96fd`; new process and separate software pixel format 69, flags 33893 | Microsoft GDI Generic 1.1.0, front and back exact RGB, swap 1 / Win32 0 / GL 0; screenshot actually shows three expected stripes |

Windows `query session` showed console 1 Active, CIM Intel UHD Graphics 32.0.101.7088 status OK, 1920×1080. RDP disconnect not confirmed. Software format chosen only for new test window based on [PIXELFORMATDESCRIPTOR](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-pixelformatdescriptor) and [DescribePixelFormat](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-describepixelformat); without system change. Context7 did not provide accurate documentation of these fields, so direct original Microsoft pages used. OpenGL 1.1 does not meet production RME requirements and does not replace final test.

**Remains unverified:** actual physical image of hardware path, distinction driver/Windows/capture and final normal GUI PASS. No user response about physical screen image. Symptom reproduced also without RME and wxWidgets, so no speculative workaround introduced in renderer. For further closure of same open issue requires local observation of physical screen or external graphics state change. All control windows closed. Existing matrix and historical PASS/FAIL/PARTIAL not rewritten to full pass.

## Update 2026-11-02: buffer observer, GUI still partial

`g0-render-buffers-20261102`: module `g0_rme_render_observer.cpp` source `8e9b881d75e13ee32e87f7acf9c55c68e56a2e5416cbca23e03849a0e79158f0`, separate EXE `7e442da4d59b0d27eeaca08b3024abf2a459afa5c1e16547d0b0663f79f9e3d7`, 408 current objects from canonical `e8c3041e…`. Source copy in OUTPUT has same hash. Actual buttons/shortcuts/menu executed Add/Undo/Redo/Undo. Read-only observer checked house and row count 0/1/0/1/0, ID 1, name `Unnamed House #1`, rent/size 0, five paints, rendering enabled and no prior GL error. Strict control of own WGL DC passed. Observer does not modify production renderer or map. Additionally created second empty card through Ctrl+N; no save.

**PARTIAL result, not full GUI PASS.** Front buffer for observed 0/1 stages is black, back shows header and blue ID 1 row; window image still has empty palette and old map canvas pixels. Back buffer after SwapBuffers has only diagnostic significance. `partial-verification.json` preserves all ten PNG hashes, GL state and Intel UHD 4.6 / 32.0.101.7088 driver. Child 0 after normal Alt+F4, one wx SetFocus 0x57; runner 1 because of lack of positive visual observation not manufactured. `verify_render_buffers_20261102.py` is local verifier of this limited result.

`g0-gui-visible-startup-20261102`: copy of normal canonical EXE with SW_SHOWNORMAL still showed empty palette after Add. Change reverted, normal close and independent Win32 handle confirmed exit 0. `g0-gui-screenclip-20261102` did not provide completed region capture; own Snipping Tool cancelled. `g0-render-observer-20261102` confirmed only initial 0/1 transitions and black GL_FRONT, exit 0. All preserved as PARTIAL. Configuration and selected XML/TOML/font/OTB/DAT/GLSL profile and assets do not differ from earlier successful GUI except private data_directory path (`gui-comparison-20261102.json`); this does not establish cause.

Context7 confirmed [wxGLCanvas 3.3](https://docs.wxwidgets.org/3.3/classwx_g_l_canvas.html) and [WGL swap method](https://github.com/KhronosGroup/OpenGL-Registry/blob/main/extensions/ARB/WGL_ARB_pixel_format.txt) documentation. Local wxWidgets 3.3.1 `src/msw/glcanvas.cpp:776` calls native SwapBuffers and reports its refusal. Not accepted that absence of GL error or correct back buffer proves screen visibility.

`g0-render-swap-20261102/partial-verification.json`: separate presentation experiment with additional native swap, **PARTIAL**, not normal GUI. Source `90d002eaf361019874f5e660aeb0b656eff0115c9f425f138cd817409a64e30e`, EXE `1e352ce1b6adb19f4e80335e25c7f9251632741f316af8ff792b432e4e6100bf`, manifest 408 objects, matching archived source copy. Transitions 0/1/0; GetLastError after additional SwapBuffers 0, return 1, GL error 0. Five own canvas/parent windows without WS_EX_COMPOSITED/WS_EX_LAYERED. Front after experiment still black, back shows header/row, CUA image empty. All eight image hashes preserved. House reverted; normal Alt+F4, child 0, empty stderr, runner 1 because of intentionally unfinished five stages. `verify_render_swap_20261102.py` checked manifest/images/states/exit. No user response about physical screen image before closing private window. Cause not determined, no speculative renderer/driver or system settings change introduced.

Final audit summary, still in verification: 2026-11-01. Base: `5a01a8fe1c34465559701d50681cc2210ef32c67`. Criteria from [project plan](../PROJECT_RME_PROCEDURAL_GENERATOR.md); integration points described in [ARCHITECTURE.md](ARCHITECTURE.md). Current status defined by matrix below; dated entries preserve earlier FAIL/PARTIAL/IN PROGRESS, are not current task list.

## Historical G0 gate matrix — 2026-11-01

**G0 still IN PROGRESS; G1 not started.** Below compiled evidence for Windows x64, client `1098-BlackTek`, synthetic OTBM 3 fixture and separate OTBM 5 zone. Data/history/I/O have described final regressions but latest normal GUI repeat did not allow confirming GL rows and has SetFocus diagnostics. Not changed to full PASS; requires determining whether image capture or actual rendering state. Does not indicate production readiness of future generator or full atomicity of existing editor.

| Protocol requirement | Evidence and pass scope |
|---|---|
| G0-01 — build/start | Canonical `build_ninja.bat`, final `g0-companion-header-canonical-20261101.log`, code 0; normal EXE `e8c3041e3e7d6b06599a2c391fcc0599cfd945ebab2583e98cd958c0b4a45cfd`. Actual normal GUI/start/exit 0 in `g0-house-gui-after-queue-20261101` on previous base `07d87869…`; later changed only path resolution/backup and OTBM link, checked with current integration processes |
| G0-02 — client and fixture | Isolated single profile `1098-BlackTek`, `dat_only`, items 3/57; signatures/hashes and standard OTB discrepancy described below. Used IDs verified; 22 material warnings also concern IDs outside fixture, do not declare completeness of all client materials |
| G0-03 — S0/mask/halo | `g0_smoke.lua`, `g0_verify_saved.lua`, original GUI and current regression `g0-lua-allocation-after-queue-20261101`: exact 20×20 X/Y 100…119, Z=7, checked halo and no additional positions |
| G0-04 — actual editing | 400 ground through actual Lua/RME, one action. Last current Lua test with base `07d87869…` saves/reads result and compares with independent reader; later I/O changes have final full regression below |
| G0-05 — Undo/Redo | Test 400 fields, 67 Lua commit cases, 35 non-empty history/Redo/merge/limits cases, full snapshots; Add/Edit/Delete house chain and restore entire fixture. `g0-queue-allocation-final-20261101`, `g0-lua-allocation-after-queue-20261101`, `g0-house-history-after-queue-20261101` |
| G0-06 — Draw/Lua/Draw | Actual GUI test 27.10 with `group_actions=true`, eraser and dried grass; three separate Undo/Redo and area/halo restore. This specific saved run of older build; did not repeat manual strokes after queue fix. New queue has separate merge and Redo test |
| G0-07 — callback error | Three Sol2 errors and rollback of six mutations; later 17 setters, rotation/podium, tile/item references after Undo/Redo/delete/move/switch and reopen, Lua allocation/non-empty queue refusals and restored full snapshots. Detailed scopes and older reproductions remain below |
| G0-08 — standard save and read | Final `g0-persistence-after-paths-20261101` **PASS** on EXE `e8c3041e…`: actual snapshot/Save As/load, four snapshots, two Save As rel/abs with BOTH waypoints and sentinels, full comparison 402 positions, 401 remaining nodes, nested bag/coins semantics, house/spawn XML, UID/AID/text/description, four negative controls. Includes floors 6/7, PZ, house/town/exit/rent and Rat. OTBM 5 zone has separate actual save/reopen, independent ID/position/XML and negative controls described below; not BlackTek fixture |
| G0-09 — receiver | `blacktek-fresh/iomap-cases.json`: **6/6**, actual `IOMap::loadMap`, `Houses::loadHousesXML`, `Spawns::loadFromXml`, current 85 objects per-source, without unity and `/FORCE:MULTIPLE`. House, two floors UID/AID/PZ, nested containers and spawn checked in server objects. Original RME and running Linux/Docker not receiver of test; do not declare compatibility with them |

Six phase 0 positions: Git/license/toolchain state — documented in baseline audit; integration points — symbol table in ARCHITECTURE; OTBM — G0-08/09 above; build/20×20/Undo — G0-01…06; noise/algo/geo/demo/transaction assessment and language choice — formal C++ core/snapshot/resolver/commit decision, Lua optional presets/demo; documentation and small PR — existing plan in PROGRESS/ARCHITECTURE. Shared and uncommitted changes preserved, worked on dedicated branch; no new commits.

Discovered data blockers reproduced, fixed and checked: callback error capture, item/lifetime tracking, Save As not overwriting source XML, undo/dirty and house conflicts, wrong close card/borrowed palette references, history publication after allocation refusal and companion paths. Additional save protocols have concrete PASS: backup/archive collisions, Unicode in tested range, writer/restore/remove errors, first-save, Cancel through actual wx button, V1/legacy/invalid startup, interruption before target creation and interruption with existing new targets. This does not extend evidence beyond executed cases.

**Explicitly unverified outside audit gate:** every possible serialization/recovery interruption phase, power failure and fsync, concurrent writers/name reservation for all, full write atomicity and all mutators, VM allocator/persistent or concurrent OOM, all client versions and name/Unicode combinations, full GUI/Lua/CTest suite, two live peers and shared views, all house dialog fields, non-empty NPC in OTBM 5, normal server harness/recovery shutdown, original RME and deployed Docker. Cancel currently has described PASS below for synthetic real button event after target creation/shortening; manual handling and all phases not verified. Earlier word “requires” on further full atomicity describes audited API limitation, not ready G1 contract. G1 must implement and check controlled commit/Cancel/budgets described in its protocol. No claims of speedup or determinism of entire generator.

## Results executed in audit

### Update 2026-11-01: Cancel of save and last GUI check

`g0-save-cancel-final-20261101` **PASS**, child/runner 0, empty stderr. [Separate module](smoke/g0_rme_save_cancel_probe.cpp) source `885fae8e52ab2cf26f306b71db7273ed96fab787c990cb708ba08c5054e6b728`, EXE `cb1d3cbeeb6aed0012ae519644283ee46b427b44a5f829c2b33213a9317b5b7d`, 408 current objects and normal EXE `e8c3041e…`. Test `wxEventFilter` waits for paint of visible `wxGenericProgressDialog` titled `Saving`, existence of private target and size up to 4 bytes (new creation or actual shortening of existing file). Sends single `wxEVT_BUTTON(wxID_CANCEL)` through real button event handler and requires handling event. Does not change manager flags manually, does not add hook to production. For new Save As requires false, original metadata/dirty/unnamed and absence of three new targets, marker, backups, dialog and remaining Cancel state; next save and RME read must succeed. For existing set requires identical OTBM/both XML bytes and editor state after cancellation, cleanup and retry success/read. Runner compared final all 402 fields/remaining nodes/nested items/XML and six input hashes. This synthetic wx event, **not manual click, latency measurement or test of all cancellation phases**. First directory `g0-save-cancel-20261101` is only build of unexecuted variant; before first run narrowed filter to shown/repainted dialog and shortened file, final variant built separately.

`g0-gui-after-paths-20261101` remains **PARTIAL of full House GUI protocol**, runner 1; `observed-exit.json` confirms normal exit 0. Normal final EXE launched empty map, native House/Add and Ctrl+Z (`Undo action`), without assertion and without save. In captured image GL table was empty after Add, canvas contained old pixels; **did not save fabricated 0/1/0/1/0**. Stderr has one `SetFocus` 0x57 warning. `startup-shutdown-verification.json` contains only limited `PASS_WITH_DIAGNOSTICS` start/handle/normal close/hash/no new file; `partial-verification.json` preserves incomplete visual assessment. Did not weaken existing GUI verifier.

Control `g0-gui-baseline-control-20261101` used preserved normal EXE `07d87869…`, verified by earlier manifest, on next new profile, without rebuild and without replacing canonical EXE. Showed **same symptom** of empty GL table after Add and old pixels also in small, non-maximized window. Ctrl+Z and normal Alt+F4 gave Undo status and exit 0; one identical SetFocus warning. Full runner also refused PASS of rows. Observations of both processes saved before closing. Result does not establish cause, does not prove tool only, and does not invalidate historical earlier correct GUI. **To close G0 remain explanation of this discrepancy and final matrix acceptance after it.** User data and running Docker not used.

Further control `g0-gui-compositor-20261101`: another fresh copy of current normal EXE. After Add executed Alt+Print of active private window, pasted bitmap to confirmed new empty Paint and saved only `windows-active-rme.png` in this output. File includes private RME and white Paint canvas background; independently read image still has empty House area/old pixels. Not considered resolution of cause (ShareX also works on computer, so system shortcut may use similar capture mechanism). Saved observation/hash before Ctrl+Z and normal Alt+F4; child 0, runner rejected full GUI PASS, one SetFocus warning. Paint closed after saving own new image. Did not change renderer code based on screenshot alone.

### Update 2026-11-01: refusal of new target deletion and rollback interruption

`g0-removal-denied-final-20261101` **PASS, process/runner 0**, empty stderr. Separate first-save module with 408 current objects (normal EXE `07d87869…`, test `44578b23ab7e4822177cb445a79175f5e4dab108e1dc2320fd9b11256bda2c7a`, source `7b824105de74a28cc6b2c506aef8573c5682bc7a3a587fc0dcafe05613ae4299`) first executes current first save protocol. Then saves private new map, deletes only three own exits, leaves editor of named map and creates house sentinel. This test of **new targets of normal save**, not new first-save proof.

Only `run/rollback-denied` directory gets explicit ACE deny DELETE for inherited files and DELETE_CHILD for directory. Existing house XML open for read with share READ/DELETE, without WRITE. Actual coordinator copies house backup, writer creates OTBM and spawn XML, house save fails, rollback cannot restore house or delete two new files. Read `Save failed` dialog mentions backup and both new targets. Test checks metadata/dirty/unnamed, house and backup sentinel, full V1 with 9 records (one backup and two new targets) and refusal of next save without marker/OTBM change. Guard releases handle and deletes only own ACL refusal; marker and files stay as proof. Runner compares six input hashes and independently reads earlier successful empty set. Do not name new files correctly finished map set.

First run `g0-removal-denied-20261101` is **FAIL of scenario preparation**, process 9: subdirectory in `HouseFilename` triggered actual wx `Assign` assertion, continued save did not fail as intended. Not overwritten or passed. Further analysis showed actual path handling problem, described below.

`g0-partial-rollback-crash-20261101` **PASS of limited interruption and restart**. On new fixture and identical EXE read actual incomplete rollback dialog, then Win32 observer verified full handle path PID 39916 and executed `TerminateProcess(0x4701)`. Code 18177 also read by runner (its FAIL is expected injection result, not full module PASS). Marker SHA-256 `0ffffed62ba852bb6e5b792fedc2b185febdab192ec6710caa0d6ef001437f14`; four existing file hashes saved in `interruption.json`. After process death deleted only test ACL refusal because guard could not execute it. Normal canonical EXE run on same private profile showed `Incomplete save` with two actually existing targets. New empty map and Ctrl+S to `blocked-after-crash.otbm` gave `Save failed` with full marker path. Verifier confirmed identical marker, OTBM, spawn XML, house XML and house backup bytes, unchanged six inputs and absence of rejected target. `restart-verification.json` PASS; launcher ended only its child after verification. **This is not normal shutdown or interruption in middle of serialization/recovery; not power loss test.**

### Update 2026-11-01: companion file subdirectories — fix in verification

Above reproduction and CodeGraph indicated `wxFileName(path, configuredFilename)` in `MapXMLIO::normalizeMapFilePaths` and `SetFullName(configuredFilename)` in backup: both APIs expect basename and assert for path. Moreover backup/marker/rollback assembled path differently than XML writer. Fix parses full path with one-parameter constructor, preserves existing normalization and map directory restriction; coordinator uses same resolver for backup, marker and rollback. Companion backup has actual path + `~`, without subdirectory flattening. Does not change OTBM format or existing path restriction policy. Context7 wxWidgets 3.3: [constructors and Normalize](https://docs.wxwidgets.org/3.3/classwx_file_name.html).

First canonical `build_ninja.bat` ended code 0 (`g0-companion-paths-canonical-20261101.log`, two sources and link). Did not pass build itself as I/O regression; runtime results and further build below.

`g0-nested-companions-20261101` ended **PARTIAL, process 9, runner 1**: previous first-save, relative nested save/readback and actual nested rollback passed but `Absolute readback failed`. Module source `2598bac8…`, normal EXE `8babcdff…`, test `add14530…`. `partial-verification.json` preserves manifest and actual empty stderr. Cause: `IOMapOTBM::saveMap` for absolute companion wrote only basename in header, XML writer preserved subdirectory. Further fix writes relative link to absolute file inside map directory; external target still limited to basename. Does not change type or order of OTBM attributes. New build/regression must confirm proper header/XML pair; previous PARTIAL is not PASS.

Final `g0-nested-companions-final-20261101` **PASS**, child 0, empty stderr. Source `526f40aa…`, EXE `720ff58018cd62b47b4e4947a81e6af1fc2182b3e1dd8064ca2c59e490b2c8ba`, normal `e8c3041e…`, 408 objects. Canonical build `g0-companion-header-canonical-20261101.log` ended 0. All assertions of relative write/read, actual writer/restore error, exact seven backup list, identical bytes of three originals and copies, absolute read with preserved `nested/houses.xml`, private external sentinel and final cleanup passed. First runner read rejected correct nested spawn because old `canonical` intentionally allowed only basename. Did not restart ended process: [separate verifier](smoke/g0_verify_nested_companions.py) checked existing `observed-exit.json`, manifest, all markers, six inputs, empty both map/XML sets and exact header names `{11: nested/spawns.xml, 13: outside-house.xml}` with directory containment check. Saved `verification-final.json`; default basename policy of existing snapshots not weakened. `reader-negative-controls.json`: rejection of nested by default policy, wrong exact name and header with matching expectation `../outside-house.xml` before opening external XML — **3/3**. This distinction of initial helper FAIL and final independent verification should be preserved in report.

`g0-persistence-after-paths-20261101` **PASS**, child/runner 0, on same final objects; module `43b1174c…` unchanged, EXE `711f805db4c43f99b53b704928bf7a1c8b9e7e05fbc57a6ab8d41ce1b8bba05d`. Four snapshots, actual collision in same second, snapshot house write error, two Save As rel/abs, waypoint BOTH and sentinel, re-read RME, full 402 positions/401 remaining nodes/nested item semantics/XML and four independent negative controls; six input hashes unchanged. This final regression of full fixture after serializer fix. Did not rerun server test on running Docker.

```powershell
rtk proxy python docs/procedural/smoke/g0_build_rme_snapshot.py OUTPUT --source docs/procedural/smoke/g0_rme_first_save_probe.cpp
rtk proxy python docs/procedural/smoke/g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --first-save --nested-companions
# Run only after child 0 exits, without restarting, if the older helper rejected the subdirectory.
rtk proxy python docs/procedural/smoke/g0_verify_nested_companions.py OUTPUT FIXTURE
```

### Update 2026-11-01: non-empty history, Redo, merge and limits

[History publication module](smoke/g0_rme_queue_allocation_probe.cpp) creates active editor copy of 402-field fixture, replaces empty queue with test subclass of existing factory and builds real `app.transaction` entries. Overridden `BatchAction::commit` executes base implementation, then arms one-time scalar `operator new` refusal. Production receives no hook. Test has access to protected memory counter, object IDs and labels through subclass, not changed public API.

| Scenario | Preparation | Final cases / rejected publication allocations |
|---|---|---|
| `grow` | 14 real entries; next insert needs deque memory in used MSVC | 9 / 2 |
| `count-limit` | 14 entries, then set limit 1; preserve existing one-trim policy | 9 / 2 |
| `merge-redo` | P1, P2, Undo P2; forced P1 merge window through subclass timestamp (no clock change) | 8 / 1 |
| `memory-limit` | 14 entries, then memory limit 0; successful commit should remove prior history | 9 / 2 |

On refusal requires identical identity and labels of all entries, index and `memory_size`, restored AID/dirty, working old Redo and full Undo chain. Success requires full target and expected append/merge/trim, then Undo/Redo and retry Undo. Eight GUI observer failures leave complete action and have matching stderr diagnostics. After intentional eviction cannot undo deleted prefix: test explicitly restores **full field snapshot through regular action**, before independent full map comparison. Each `clear()` must return index/count/memory counter to zero and preserve field AID; full snapshot executed before this final cleanup.

| Run | Authoritative result |
|---|---|
| `g0-queue-allocation-before-20261101` | **REPRODUCED FAILURE, process 9**. Baseline snapshot PASS. First deque refusal after 14 entries: 14/14 but queue fingerprint changed. Old code added new batch memory before `push_back`; read after refusal 5850. `reproduction.json`, no full PASS |
| `g0-queue-allocation-after-20261101` | **PARTIAL**. 35 C++ assertions completed but reader rejected 14 count/memory limit snapshots. Setting AID to 0 in test restore did not reproduce absent attribute. `partial-verification.json`; oracle not unlocked |
| `g0-queue-allocation-final-20261101` | **PASS, process/runner 0**. 35 cases, 35 full OTBM/XML comparisons and 35 `clear` checks; 7 actual publication refusals preserving old entries, 8 reported GUI failures with complete reversible commit |

Queue fix plans Redo rejection/eviction without mutation, reserves predecessor vector before merge or prepares empty deque slot before old entry removal; updates counter on success. `clear()` also zeros memory. Previous one-trim policy of entry count preserved: this is **not strict RAM budget or immediate descent to lower limit test**. Thus provides no strong guarantee for all exceptions in `Action::commit` and all callers. Context7: [deque::push_back guarantee](https://github.com/microsoftdocs/cpp-docs/blob/main/docs/standard-library/deque-class.md), [exception guarantees](https://github.com/microsoftdocs/cpp-docs/blob/main/docs/cpp/how-to-design-for-exception-safety.md); also checked `_Emplace_back_internal` in installed MSVC 14.51.36231. Reservation before merge checked via ast-grep.

Final normal EXE `07d8786909395c813adcba3afc09ed8da0f468f7d12eb623f46da1350ae630af`; build 0, log `local-data/procedural-smoke/g0-queue-allocation-canonical-20261101.log`. Queue source `7efc3a44fe3d5e96c3c0e18ff8e66143efa01d3a9589b3ce945166a08e97ff63`. Final test source `b011cebe54a6c0e38132c67935a2d9029a140075226d3d885688d1c6d4998c1f`, EXE `fc61f91a1963b47cb26d9c9549f01786767e0d35ab80105cc84eb445b980238a`, 408 fresh objects. Manifest adds queue source and object. Verifier checked full range 0…baseline+4 for each scenario, all snapshots/clear/restore, actual refusals and unchanged inputs and no new OTBM/marker. Settings changes concerned only private profile process, restored in guard.

```powershell
rtk proxy python docs/procedural/smoke/g0_build_rme_snapshot.py OUTPUT --source docs/procedural/smoke/g0_rme_queue_allocation_probe.cpp
rtk proxy python docs/procedural/smoke/g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --queue-allocation
```

Regressions from same current object set **PASS**, code 0:

- `g0-house-history-after-queue-20261101`: eight markers of existing house protocol, restored snapshot and full independent comparison. Module source `f1408cf5…`, new EXE `af625038bd61c92d61f731e1f3172b7b4369e8974b0475733f8d540e66713d37`.
- `g0-lua-allocation-after-queue-20261101`: unchanged module `4056f8f8…`, new EXE `eb11440879aa235f9864a44293449d3cc5ebc2f9f11ef6ae9d99b9477eeea13e`. 67 cases/comparisons, actual 400 fields/Undo/Redo, first private save and re-read and independent mask, ground and XML check. Lua series limitations below still apply.
- `g0-house-gui-after-queue-20261101`: copy of normal EXE without module, actual 0/1/0/1/0 houses after Add/Undo/Redo with menu/Undo. ID 1 row, rent 0, size 0. Normal Alt+F4, **exit 0, empty stderr**, no alert and no save. Observations saved before close, manifest/exit/hashes checked. Title asterisk remained after Undo; not used as data oracle. This result confirms limited normal GUI/shutdown on current EXE, not all views and live.

Queue module ends via `_Exit` after test editor destruction, stderr contains 36 wx SetFocus warnings; does not declare visual GUI PASS. Limitations: scalar one-time refusal, test factory and forced merge window, no persistent/concurrent OOM and live peers. User data, forbidden map and running Docker not used. RME has no route in skill selector (review-only); executed described build and behavior tests. G0 still IN PROGRESS.

### Update 2026-11-01: Lua commit allocation series and 400-field regression

[Separate module](smoke/g0_rme_lua_allocation_probe.cpp) replaces scalar `operator new` only in its EXE. After five actual AID setters in `app.transaction` arms one-time refusal of next/n-th allocation before callback return. Each attempt loads new copy of same 402-field fixture to active card. Points: 100/100/7, 104/102/7 (house), 110/110/7, 160/160/6 and 160/160/7. Checks complete setters, AID, house ID and unique house entry, tile count, dirty state, history index/size, ability to start next transaction; creates snapshot even with damaged result. Runner compares full OTBM/XML with independent reader.

| Run | Result and evidence scope |
|---|---|
| `g0-lua-allocation-before-20261101` | **FAIL/REPRODUCED, 9**. Normal commit/Undo lost house ID, independent reader rejected field 104/102/7. Injection 1 gave 401 fields, history 0/0. `reproduction.json` preserves both rejections, no PASS |
| `g0-lua-allocation-after-20261101` | **PARTIAL, 9**. After fix of copy, house and dirty 58 snapshots compared fully. Case 57: callback returned error, map restored but history 1/1 remained. `partial-verification.json`; incomplete series not PASS |
| `g0-lua-allocation-final-20261101` | **PASS of described scope**, process 0 and launcher, 67 cases and full comparisons. 58 actual refusals: 54 rollbacks without history/dirty; 4 reported GUI observer failures with complete reversible commit. Baseline 0 and eight attempts beyond its 58 allocations without injection also checked |

Map operation refusal must restore entire fixture and history 0/0. GUI notification refusal noted by production `Editor::notifyStateChange`, not propagated as operation error: test requires all five changes, exactly one action, then full Undo and matching snapshot. Verifier requires matching stderr diagnostics for each such case. Does not pass partial result or silent failure. Did not change test to accept failed commit with preserved history and reverted map — previous case 57 remains FAIL.

After series used unchanged [g0_smoke.lua](smoke/g0_smoke.lua) and [g0_verify_saved.lua](smoke/g0_verify_saved.lua) on actual new editor: 20×20, one action, empty halo, Undo to zero, Redo, private first save `g0-smoke.otbm`, re-read by RME. Reader independently requires exactly 400 nodes without additional fields, ground 4526 fetched from loaded brush, empty stacks/metadata and both empty XML. Script hashes in `run/smoke-script-inputs.json`; sources and executed copies match. Original fixture and three copies preserved hashes. Allowed only new private `g0-smoke.otbm`/companions and snapshot directories; no final marker.

Final canonical EXE `f274e2ca51ca78c4a871b22a86acc9d5ac65f707305079e41018d65ce34caa3b`, build 0 in `local-data/procedural-smoke/g0-lua-notification-canonical-20261101.log`. Module source `4056f8f861a5736347429c4b9f3dc1581f66b84efb5236778423b5db1961e1be`, test EXE `4554b08e33dc1e1a52e845a3701b11275cb255988c1cd86505be2b2ca1dda9dc`, 408 fresh objects. Manifest covers Lua, editor and house sources/objects. After test checked normal EXE and changed sources compatibility with manifest. First module compilation error (`Position` not constexpr) fixed before first execution. Intermediate build of one fix part not test of complete fix.

```powershell
rtk proxy python docs/procedural/smoke/g0_build_rme_snapshot.py OUTPUT --source docs/procedural/smoke/g0_rme_lua_allocation_probe.cpp
rtk proxy python docs/procedural/smoke/g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --lua-allocation
```

Used interpreter is full Codex runtime path described above; OUTPUT/run must be new. Runner checks EXE hash against manifest, full number range 0…baseline+8, all snapshots/markers, sources, 400 fields and XML save. Timeout leaves existing process for observation, not automatic restart.

**Unverified:** history with existing Undo/Redo and limits, other mutators, Lua VM allocator, actual/persistent or concurrent OOM, active live and normal global shutdown. Additional copies of existing fields in commit increase memory cost; no time/memory measurement. Besides four expected `Editor state notification failed: out of memory.` reports stderr contains 69 wx `SetFocus` (0x57) warnings; cause not determined. `post-verification-diagnostics.json` is separate read of these logs, without overwriting original `verification.json`. **Functional data/history PASS is not clean visual GUI PASS**. Module closes test editors and ends via `_Exit`. Running Docker and user maps not used. G0 still IN PROGRESS.

### Update 2026-11-01: actual refusal of house insertion allocation

[Module](smoke/g0_rme_house_allocation_probe.cpp) contains test replacement for scalar `operator new`, absent from normal RME. One-time thread-local flag refuses next allocation only during call to `Houses::addHouse` on local new map. Does not simulate map or exhaust system memory.

| Test | Result |
|---|---|
| `g0-house-allocation-before-20261101` | **REPRODUCED FAILURE**, code 9; baseline `ed8c2b92…`, separate EXE `102a637f6977c5c3314731058f86e22b98eb368b5ec88d3a80d6da4f75d71744`. Actual `bad_alloc`, then no house owner. Launcher rejected attempt. Did not reach retry/duplicate |
| Canonical build after `Houses::addHouse` fix | **PASS**, 155/155, code 0; EXE `0677c3a73e5f52a23e498f78cd37020e59cf09341bf37cbd6e297b9f6a31be3d` |
| `g0-house-allocation-after-20261101` | **PASS**, code 0 process and launcher, same source module `e1f8b42d8f3ea16d418a11046c4112c00d01cde08ed4e761f09758f263c623e4`. Test EXE `1cb5303dfc9822b18744f93be926d69c54ad13dfb5db8fa0f72031d7fb16e799`, 408 live objects |

After fix, exception leaves the same `unique_ptr`, generation, name and rent, zero houses in map and unchanged ID allocation (next 1). Retry inserts same ID 99 object. Second ID 99 attempt is rejected without destroying caller object or changing map entry. Required five runtime markers, manifest of source/house object/EXE, observed exit code and `verification.json` saved. Local map destroyed before `_Exit`. Disk map not opened or saved by test function; runner checks six hashes of copied fixture set and no new map/marker. Closed read normal material warnings dialog from private startup.

**Guarantee scope:** `Houses::addHouse` now takes `unique_ptr&&`, allocates empty node with `try_emplace` before any object/count change and takes ownership only after success. Duplicate rejection preserves object. Loader/import/action calls passed compilation. This is not proof of strong exception guarantee for entire Undo/Redo, exit-code reconstruction, dirty trackers, GUI notifications or save; post-insertion allocations remain separate scope. Context7: [conditional move try_emplace](https://github.com/microsoftdocs/cpp-docs/blob/main/docs/code-quality/c26800.md), [operator new and bad_alloc](https://github.com/microsoftdocs/cpp-docs/blob/main/docs/cpp/new-and-delete-operators.md).

```powershell
rtk proxy python docs/procedural/smoke/g0_build_rme_snapshot.py OUTPUT --source docs/procedural/smoke/g0_rme_house_allocation_probe.cpp
rtk proxy python docs/procedural/smoke/g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --house-allocation
```

Do not use this allocator module for normal work or add it to the `rme` target. RME has no route in `sorky-validate-change` selector (review-only); canonical build and above behavior test executed.

Separate regression `g0-house-history-allocation-fix-20261101` **PASS** on current baseline `0677c3a7…`: same set of eight required markers as in history protocol below, snapshot of recreated house, RME read and independent full OTBM/XML comparison, unchanged inputs. Separate EXE `720c55bfe6a91978a88f8abfea00199e49504b8ddea92b8c2d4db96c459ac3fc`, 408 objects; test source `f1408cf54b8fc0c0118a31f96a04157e00e01e36bd40cf0f4c7f19619144db38`. Build and full runner code 0. `g0_rme_house_history_probe.cpp` now waits for `IsMainLoopRunning` and enabled root; checks this also in `run`, so test happens after `OnInit`. Corrected historical comment about missing palette Undo; test assertions unchanged. No allocator substitution in this module. Own local `LiveServer` serves only state guard: no `bind`, listen and peers; running Docker not used.

### Update 2026-11-01: positive rotation and podium Lua methods — PASS

New [module](smoke/g0_rme_item_methods_probe.cpp) launched [Lua test](../../lua_tests/test_item_methods.lua) via production `LuaEngine::executeFile` on active tab of approved 402-field fixture. Separate EXE, 408 live objects, baseline normal EXE `ed8c2b92…`, test EXE `fd35b2522005c85611fb071cb3889d1eb1aa79316624d961bd7cd588375b07f5`. `g0-item-methods-20261101`: build 0, process 0, full runner 0, six required markers and `verification.json` **PASS**.

| Case | Verified scope |
|---|---|
| `rotate` | Loaded definition 1442→1476; pre-transaction reference, rollback on error, explicit and implicit history entry with Undo/Redo |
| `setPodiumOutfit(128,3)` | Actual `Podium`, exactly one entry; rollback and explicit/implicit Undo/Redo. Oracle C++: all 12 outfit fields, direction, three raw flags |
| `setPodiumOutfit(129)` | Same attempts with nonzero baseline direction 3; missing direction argument preserves 3 |
| Invalid calls | LookType -1, direction -1/256, plain item: rejection, state and history size/index unchanged |
| Fixture recovery | Two final Undo remove base podium change and test items; index 0, empty stack 110/110/7, field count 402; no save |

Profile 1098 does not contain podium. Runner changed **only a new private copy** of single XML definition 1443 to `type=podium`, then normal loader/factory created the object. `run/method-inputs.json` gives hashes of original and changed XML and script. Verifier checked them again and six hashes of input OTBM/house/spawn (original and copy), no new map/marker. Podium serialization, its handling by standard profile/BlackTek, full in-memory map comparison, allocation errors and global GUI close not tested. Module closes test editor before `_Exit`. Read and closed only normal material warnings from private instance.

```powershell
rtk proxy python docs/procedural/smoke/g0_build_rme_snapshot.py OUTPUT --source docs/procedural/smoke/g0_rme_item_methods_probe.cpp
rtk proxy python docs/procedural/smoke/g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --item-methods
```

Each run requires new OUTPUT/run. Used local interpreter: `C:/Users/Sorky/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`; `python` alias not available. Do not retry completed test only to change report. G0 remains IN PROGRESS; this closes missing positive methods of G0-C in described scope.

### Update 2026-11-01: final map close regression — PASS

Current canonical build `build_ninja.bat` finished with code 0. Normal EXE `ed8c2b926d8c9c676d0b6e607d010708b9657cbfd2764fe0272a7aff10cf0b8d`, source `editor_tabs.cpp` `06ae6fe77fabc02f32f9b9833578a26d4d8e6da663b282fc4c9a31c793e9f231`. After two fixes new private test **PASS**, `local-data/procedural-smoke/g0-house-close-final-20261101`, no test module and no map saves.

1. Empty map → House palette empty. Opened only `run/house-roundtrip.otbm`; loaded 402 fields and house ID 1/rent 4321/size 1. Empty clean startup tab was replaced on open.
2. Ctrl+N created second clean tab `Untitled-2`. Clicking x of **second** closed it, leaving fixture and its proper house in palette. This is positive regression of wrong `GetInt()`.
3. Ctrl+N → second tab `Untitled-3` → Add House. Clicking x of second gave `Save changes` with name **Untitled-3.otbm**. Escape/Cancel kept both tabs and new map house. Reclose and No closed only second tab. Remained fixture and house ID 1/rent 4321/size 1; no new map saved.
4. Closing last fixture emptied house list and town selection. Reopening same copy restored house, reclosing cleared palettes again. Edit without map did not open House Properties nor crash.
5. Observations saved before normal Alt+F4. `Popen.wait` returned **0**; full launcher too 0. Verifier checked five main states 0/1/0/1/0, explicit confirmations of correct tab/rebind/Edit, empty diagnostics list, EXE copy hash and six file hashes (original and copy of each of three inputs), no new OTBM and marker. `verification.json` PASS.

Artifacts: `gui-build-manifest.json`, `run/house-ui-observation.json`, `observed-exit.json`, stdout/stderr and `verification.json`. Older PARTIAL remains unchanged. Scope does not cover shared views of same map, active live network, process interruption or allocation failure. `sorky-validate-change` returned review-only for RME paths; used repository-proper validation: canonical build and GUI. All G0 still IN PROGRESS.

```powershell
rtk proxy python docs/procedural/smoke/g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --house-gui --house-gui-case close-fixture
```

### Update 2026-11-01: two-tab close regression

First palette unpins build: code 0, EXE `d7cb77055bdeb26fa41ac19bb552670c58985c4b03dfe34d03d45eb3978f0203`. `g0-house-close-fixed-20261101` showed empty palette/town selection after closing last tab, after rereading private fixture house ID 1/rent 4321/size 1 and again empty palette after close. Edit without map opened nothing, normal exit 0. **PARTIAL**: with two clean tabs (`house-roundtrip`, `Untitled-2`) clicking x of second closed first. No maps saved. `run/house-ui-observation.json` explicitly contains error; runner rejected it instead of generating PASS. Separately saved `verification.json` has PARTIAL, manifest and `observed-exit.json` preserved.

Handler used `GetInt()` instead of `wxBookCtrlEvent::GetSelection()`. Verified inheritance and separate index field in installed wxWidgets 3.3.1 headers; Context7 points to notebook events and page removal documentation. Fixed index and its range. Final build regression must additionally confirm **which tab remained**, proper house of remaining map and no active house after closing last. Runner mode `--house-gui --house-gui-case close-fixture` requires in observation `case`, `closed_requested_tab`, `rebound_remaining_house`, `post_close_edit_noop`, chain 0/1/0/1/0 and empty diagnostics list. Do not give bare line numbers as proof of correct map selection.

### Update 2026-11-01: Remove and house properties via GUI

`g0-house-edit-gui-20261101`, copy of normal RME `29691821…`, **PARTIAL**. After base history cycle created empty house ID 2. Attempt rent 4321 without town rejected by actual `Invalid town selected` dialog; Cancel left rent 0. Remove → Yes confirmation → Undo → Redo showed house counts 1/0/1/0 and restored ID 2. Then opened only private `run/house-roundtrip.otbm` (402 fields). G0 House had G0 Town; just rent in House Properties changed 4321 → 4322, Undo → 4321, Redo → 4322. After closing earlier empty tab and rejecting its changes, closing only fixture tab showed `Save changes` with exact name `house-roundtrip.otbm`. Selected No; no files saved. This confirms warning after just rent change, not other dialog fields or save of changed rent.

**Bug found:** after closing last tab House palette still showed ID 1/rent 4322/size 1. Did not click Edit on that remaining row. Code `CloseCurrentEditor` refreshed palettes before removing page, re-assigning references to its map; final removal may not generate page-changed. `HousePalette::GetSelectedHouse` can return borrowed row pointer. Central `MapTabbook::DeleteTab` fix unpins palettes before `DeletePage` and refreshes after; its runtime remains to execute.

After closing tabs normal Alt+F4 gave code 0 without assertions. Launcher limit 300 s elapsed earlier: process not restarted; Win32 observer opened handle PID 17936, confirmed path `g0-house-edit-gui-20261101/house_gui.exe` and read final code. `observed-exit.json`, `run/house-ui-observation.json` and `extra-gui-verification.json` hold evidence. Independently verified hash invariance of both sets of three files. Second attempt report remains PARTIAL with explicit palette error, despite successful Remove/rent/prompt/shutdown stages.

### Update 2026-11-01: normal GUI Add House and close

**PASS of limited GUI scenario**: copy of normal EXE `29691821bfcbd40da449162ec20015adee4471f4731e4c03b730d08d63907b8a`, no test module, new empty editor and fresh private profile. House palette showed sequentially **0 → 1 → 0 → 1 → 0** rows after Add, Undo, Redo and final Undo. Row had ID 1, rent 0, size 0. Add launched by button; Undo via Ctrl+Z; actual Redo via Edit/Redo. Ctrl+Y did not launch Redo; visible menu shortcut is Ctrl+Shift+Z, but its action not confirmed. No grounds for earlier suspicion of palette refresh error. After final Undo Alt+F4 closed program without save prompt and without diagnostic alert. Process code **0**, no `_Exit` module and no forced termination. Normal material warnings at startup read and closed before test.

Artifacts in `local-data/procedural-smoke/g0-house-gui-20261101`: EXE copy manifest, `run/house-ui-observation.json`, `observed-exit.json`, `verification.json`, stdout/stderr. Launcher finished with error reading missing observation JSON after normal RME exit; code 0 comes from observed transition via `process.wait` and check `code != 0`, explicitly saved as evidence source in `observed-exit.json`. First observation save did not execute due to unavailable `python` alias; supplemented with full interpreter path. Later verifier **PASS** checked completed set without process rerun: PID, exit code, observation chain, EXE hash, original fixture and three copies, no new map and marker. Runner now saves `Popen.wait` result before observation verification in future attempts.

Repetition: new OUTPUT directory; no module build required. During GUI save observed states to `run/house-ui-observation.json` **before** closing application. `--verify-existing-house-gui` performs only check of existing artifacts and requires `observed-exit.json`; not for appending expected results. Default GUI limit 300 s leaves live PID after timeout.

```powershell
rtk proxy python docs/procedural/smoke/g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --house-gui
rtk proxy python docs/procedural/smoke/g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --house-gui --verify-existing-house-gui
```

Edit/Remove, save warning after rent edit, peer synchronization, allocation errors and shutdown after recovering existing map not checked here. Map not saved. All actions concerned private `house_gui.exe`; user map and running BlackTek Docker not used.

### Update 2026-11-01: actual first-save interruption and GUI restart

**PASS of limited interruption scenario**, not full crash recovery. Used approved module/EXE from previous recovery section (`3a421d9feccf0e9b8074fa009a0cadb4264d1255a7754b2446fb09656d5710f8`) on new `g0-firstsave-crash-watch-20261101`. New [observer](smoke/g0_interrupt_first_save.py) operates only in private `local-data/procedural-smoke`: requires guard fixture, current PID and matching full EXE path. `--watch` armed before launcher. Waited for actual `FIRST_SAVE_FAILURE_BEGIN` and exact complete V1 with seven empty backups and three expected targets in nonexistent private directory, then terminated only that process. After opening handle re-checked marker.

PID 12416 exited with expected code **18177 (`0x4701`)**, observed by Win32 and parent process. Standard first-save runner correctly reported incompletion instead of PASS: `Probe exited with 18177` exception is expected consequence of this injection, not production assertion error. `interruption.json` describes phase as “after reading complete V1, before removing marker”; exact instruction and whether serializer returned false not established. No error dialog observed in this run and should not be claimed. First `g0-firstsave-interrupted-20261101` finished normally and constitutes only another first-save PASS, **not** interruption test.

[Restart launcher](smoke/g0_restart_interrupted.py) checked hash and launched in same `run` private copy of normal RME `29691821bfcbd40da449162ec20015adee4471f4731e4c03b730d08d63907b8a` as `restart/recovery_gui.exe`, no test module. Does not launch on user world. Changes only this test's configuration: separate data directory, empty recent files, no welcome/single-instance forwarding/updater. Actual startup showed `Incomplete save` with three full paths from interrupted V1 and info about marker retention. After Escape and closing known material warnings new empty tab `Untitled-1.otbm` was created. Ctrl+S opened Save; selected new `blocked-after-crash.otbm` in same private `run`. Program showed `Save failed` with full path of existing `.saving.txt`. After closing message tab kept name without modification mark.

[Verifier](smoke/g0_verify_interrupted_restart.py) confirmed actual UI observations, normal EXE hash, unchanged binary marker, absence of three original targets and entire `blocked-after-crash*` set, hashes of three input copies and original set. Result `restart-verification.json` **PASS**, marker SHA-256 `b730eda50009ae9e36598bdb93c9fe481ee82aaa015ca61465093f8a577a7994`. After positive check launcher terminated only its private child and saved observed code in `restart-exit.json`; this is not normal shutdown test. Preserved artifacts include `marker-at-interruption.bin`, `interruption.json`, `restart-ui-observation.json` and logs of both runs.

Repetition requires new output with EXE/module manifest as before. Observer and standard runner must operate in separate processes; first arm observer, then run first-save. Its nonzero code 18177 should match `interruption.json`. Then run restart, read its actual windows and perform private save test; verifier creates report only after these observations. Never terminate process selected by name alone `rme.exe`.

```powershell
rtk proxy python docs/procedural/smoke/g0_interrupt_first_save.py OUTPUT --watch
rtk proxy python docs/procedural/smoke/g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --first-save
rtk proxy python docs/procedural/smoke/g0_restart_interrupted.py OUTPUT
rtk proxy python docs/procedural/smoke/g0_verify_interrupted_restart.py OUTPUT FIXTURE
```

**Unverified:** other write/recovery interruption phases, partial target files, save of existing map with backups on process kill, power loss and normal close. Scenario contains actual new empty map and real writer/coordinator but target directory did not exist. Do not generalize to data durability or entire set atomicity. Did not change production C++; did not use `forgotten.otbm`, its companions or BlackTek DB/world/running Docker.

### Update 2026-11-01: marker protocol and startup

Private `.saving.txt` now has V1 format with `RME_SAVE_RECOVERY_V1`, record count and final `RME_SAVE_RECOVERY_END`. First seven entries preserve old backup order; rest are new targets. Each record ends LF; reader accepts CRLF. Map and companion format unchanged. [Shared data parser/writer](../../source/editor/persistence/recovery_marker.h) limits input to 1 MiB / 1024 records, checks count and end completeness, backup suffix and forbidden CR/LF/NUL in paths. Existing marker file still not overwritten. Corrupted/unknown marker remains, no file restore or deletion. Legacy recognized, but after recovering known backups marker and copies stay because old format does not prove new targets list completeness.

**CTest PASS 1/1**: [test_recovery_marker](../../tests/test_recovery_marker.cpp), Windows x64, registered in CMake and built separately. Test rejects every truncated correct V1 prefix, changed count, extra record, unknown version, incomplete legacy entries, missing paths, bad suffix, NUL/newline and over-limit stream; confirms first-save, backups, UTF-8 and legacy CRLF. Command: `rtk proxy ctest --test-dir build-ninja -R '^test_recovery_marker$' --output-on-failure`. Log and hashes in `local-data/procedural-smoke/g0-recovery-marker-20261101`. Documentation of target/test selection checked in Context7 for CMake. Do not equate parser result with full recovery.

RME build after protocol hookup **PASS**; EXE SHA-256 `1f60b352a87c9815a165f8d62422f57a01cdab0c4bac7754e469538a42510ba0`. Extended [first-save/startup module](smoke/g0_rme_first_save_probe.cpp) linked with these 408 objects; fixed test EXE `be0c933c4138d798bae7f6ab8df8a8e5c77f8dcc11249b5991f81f600b2b4b39`, source `b4ef856729e1730b56e08fee3295c40f838dbd11e5efc663d85070b16ff967c8`. Runner `--marker-startup invalid|legacy|versioned` prepares only new private directory with three target sentinels and three synthetic fixture backups. Requires read actual dialogs saved in `run/startup-ui-observation.json`, final C++ marker, byte check of all targets/backups/marker and hashes of source set and its copy.

| Run | Actual result |
|---|---|
| `g0-versioned-marker-runtime-20261101` | **UNVERIFIED**, first PASS invalidated: idle callback may have run in startup dialog loop before `OnInit` finished. Raw artifacts and `verification-premature.json` preserved |
| `g0-marker-startup-final-20261101`, invalid | **PASS rejection**, code 0. Truncated V1 without footer, actually read and closed `Incomplete recovery marker`; three target sentinels, three backups and marker byte-preserved. Fixed module requires working main loop (`IsMainLoopRunning`, verified in Context7 wxWidgets 3.3) |
| `g0-marker-legacy-final-20261101`, legacy | **PASS data, not clean GUI startup**. Read `Legacy recovery marker` and prompt with three private backups; selected recovery. Three targets restored, three copies and marker preserved, read 402 fields. `wxGenericProgressDialog` destructor assertion occurred: active loop differed from temporary during map open in `OnInit`. Continue and Escape closed only diagnostics/warnings of private app. Limitation recorded in report |
| `g0-recovery-deferred-runtime-20261101`, full V1 after fix | **PASS**, code 0. After actual prompt and recovery confirmation three targets byte-equal to input OTBM/XML; marker and three finished backups deleted. Deferred open in main loop read 402 fields and house. Normal material warnings closed via Escape; earlier wxWidgets alert did not occur |
| `g0-recovery-legacy-deferred-20261101`, legacy after fix | **PASS**, code 0. Read legacy warning and prompt, recovered three targets, opened 402 fields/house; three backups and marker byte-preserved. Earlier wxWidgets alert not observed. Report contains no old run diagnostic limitation |

Legacy run exceeded runner wait limit 180 s. Process not restarted nor fixture overwritten. Win32 observer opened handle to still-running PID 18676, confirmed full private EXE path and after completion saved `observed-exit.json` with code 0. `--verify-existing-startup` reads only existing evidence, checks PID/code/input hashes and executes same verifier. Normal cases have 45 s limit, startup 180 s; timeout leaves PID to investigate.

Build of fix deferring recovered map open to `OnEventLoopEnter` **PASS**; module also waits for enabled main window, so does not finish test in startup dialog. Final normal EXE SHA-256 `29691821bfcbd40da449162ec20015adee4471f4731e4c03b730d08d63907b8a`, test EXE `3a421d9feccf0e9b8074fa009a0cadb4264d1255a7754b2446fb09656d5710f8`, module source `ed66061311da43a76c762cf332a599278ce721e8920fbabb77cff75b1997f9a3`. Checked their origin and final match of current source/normal EXE to manifest. Two runs after fix used identical test EXE with separate new fixtures. Verifier confirmed all three targets, marker/backup retention/deletion policy, source hashes and actually observed prompts. Do not equate normal 22 material data warnings with full client asset validation.

**First-save V1 regression PASS**, code 0, same final EXE, separate `g0-firstsave-v1-20261101`. Module used correct seven entries and target to verify actual `noreplace` of existing marker (rejection not from input validation alone). New writer and reader kept full list; additional marker `FIRST_SAVE_VERSIONED_MARKER_PASS` required by runner. Passed previous write blocks, I/O error, rollback and successful retry with RME read and independent check of empty OTBM and both XML. During actual error message read `.saving.txt`: exactly 13 lines, V1 header, count 10, seven empty backup entries, three expected new targets and footer. Read saved in `run/data/user/data/observed-marker-lines.json`; final `verification.json` confirms unchanged three fixture files and their copies.

**Unverified:** actual process interruption in write/recovery phases, power loss and remaining open error cases. Full V1/legacy above prepared synthetically before start; first-save regression covers actual writer and I/O error but without process kill. `forgotten.otbm`, its companion files and running BlackTek Docker remain outside tests.

### Update 2026-11-01: first save of new map and I/O rollback

[New test module](smoke/g0_rme_first_save_probe.cpp) **PASS, code 0**, with current 408 objects and normal EXE `f058557fda7acc87ba8635ea02bf164c408ef751eadc788d5c1100063df9f26f`. Creates new `Editor` (not loaded map with changed flag), 0 fields, unnamed=true, explicit dirty. Version from approved synthetic fixture `1098-BlackTek`/OTBM 3. Goal is first-save protocol, not retest of 20×20 or full metadata.

1. Test marker `KEEP_MARKER`: actual `MapBackupService::WriteCrashMarker` returns false and preserves bytes. `saveMap(..., false)` also returns false before write, keeping entire name tuple, unnamed/dirty and field count. Empty existing marker blocks write the same way. Module removes only its two markers via actual `ClearCrashMarker`.
2. Nonexistent private target directory: actual serializer reports error. When `Error` message open, read actual `.saving.txt`: exactly 10 lines, first 7 empty, next paths `first.otbm`, `first-house.xml`, `first-spawn.xml` in that directory. Read preserved in `run/data/user/data/observed-marker-lines.json`; target directory absence confirmed at same time. Message read and closed by Computer Use tool only in private EXE window. After function returns false map state matches input, marker deleted, target and its directory still not exist.
3. Retry to new existing directory: success, new relative house/spawn names, unnamed and dirty cleared, no `~` backups and marker. Actual RME reader read 0 fields and 0 houses. Independent OTBM parser checked no fields and presence of both correct companions; separate XML parser confirmed empty `houses` and `spawns` roots.

Artifacts: `local-data/procedural-smoke/g0-first-save-runtime-20261101/{build-manifest.json,verification.json,run/snapshot-probe.log}`. EXE SHA-256 `31310090481d9f355defb8aa6cb53e335c6436deb3f6b601e673363d0763cbec`, module `f7ad5d0ccdea63511f03ba23e243fa1f7f0981a18be08f8b510e5236cf1b996f`. Runner requires all three final markers, one success path, no FAILED, source hash comparison and independent map/XML check. Three source fixture files and their copies kept hashes from loader table.

```powershell
rtk proxy python docs/procedural/smoke/g0_build_rme_snapshot.py local-data/procedural-smoke/g0-first-save-new --source docs/procedural/smoke/g0_rme_first_save_probe.cpp
rtk proxy python docs/procedural/smoke/g0_run_rme_snapshot.py local-data/procedural-smoke/g0-first-save-new local-data/procedural-smoke/g0-container-20261030 local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --first-save
```

In this scenario production shows error despite `showdialog=false`. Close only message of new private `snapshot_probe.exe`; runner after 45 s leaves process and saved PID to investigate, does not kill it or retry. Executed run finished within limit. Runner clears `recent_files`/`recent_edited_map_path` from config template and rejects config mentioning `forgotten.otbm`. Do not handle open user RME.

Scope does not cover process interruption, power loss, marker write failure, startup recovery of this set or first save of map with content. Error test had nonexistent target directory, so does not check partially saved files or their failed deletion. Module skips global close via `_Exit` after local editor destruction. Normal build not rerun: production sources unchanged after previous verified build. `forgotten.otbm`, its companions and running Docker/world/BlackTek database remained untouched.

### Update 2026-11-01: full house history chain and live watchdog

Production changes: `HousePresenceChangeData` moves `unique_ptr<House>` between map and history, preserving object identity for prior actions. Palette directs add/remove to `ActionQueue`; removal disconnects fields and exit index without changing remaining content. Undo restores both links. Properties update exit index also on ID change. Add/remove/edit of house metadata are blocked in live (current protocol does not sync them). Control of initial `addAction` rejects an impossible action before creating history. `ast-grep` confirmed both new calls to `make_unique<Change>` in the palette.

**Build Windows x64 PASS**: regular `build_ninja.bat`, existing Ninja added to PATH and `VCPKG_ROOT=C:/repo/vcpkg` in process only. First pass compiled 196 objects, but link stopped on open `rme.exe` (`LNK1104`). Process and state preserved by moving previous file to `build-ninja/rme.pre-house-lifecycle-20261030.exe`. Next pass compiled changed `action.cpp` and finished linking with code 0. New `rme.exe` SHA-256 `f058557fda7acc87ba8635ea02bf164c408ef751eadc788d5c1100063df9f26f`.

**Extended integration test PASS**, code 0, executed 01.11; snapshot carries time 08:43:42. Used [the same module](smoke/g0_rme_house_history_probe.cpp) and runner with `--house-history`, rebuilt with new 408 objects. Directory preserved preparation date: `local-data/procedural-smoke/g0-house-lifecycle-runtime-20261030`. EXE SHA-256 `6705c7a31d3c2401e446f4ae737febcfa447e904c851cf2cec5749cc92befb66`, module `702d8bae78972953ecb14524fb5ae32a2417811fa0c56e2a113074ef75e7699e`. Before execution, their conformance to manifest and hash of regular EXE were confirmed.

| Scenario | Assertions of real objects |
|---|---|
| Add/edit/remove | House ID 2, change to 3, name/rent/guildhall, removal. Three separate actions; each Undo and Redo, properties and same pointer/generation after restoration. Addition and its Undo mark dirty |
| Remove existing house | ID 1, one field (104,102,7), exit (105,102,7). After removal no house/house ID/exit index, field flags preserved. ID conflict rejects Undo without shifting history; after conflict removal, restoration of same properties and object |
| Repeat and clean | Redo/Undo does not duplicate exit record. Cleaning history after house removal ends without error and without restoring object |
| House ID and exit | After change 1 → 2 index contains exactly once 2 and does not contain 1. Undo restores index 1; prior conflict and generation tests still pass |
| Live state | `LiveManager::StartServer` creates real `LiveServer` and `NetworkedActionQueue`; no `bind`, listener, or connection thread. Attempts at edit, add, and remove leave house, field, exit, dirty, and history unchanged. After `CloseServer` offline edit and Undo work again |
| Save after restoration | Real snapshot and re-read by RME: house, field, and exit index present. Independent `g0_verify_snapshot.compare` confirms full 402 tiles, 401 exact field nodes and container semantics, header/remaining nodes with basename normalization and both XML |

`verification.json` requires all eight runtime markers and exactly one restored snapshot for independent comparison. SHA-256 of input OTBM and house/spawn XML in source and copy remained consistent with loader table. Normal GUI after this change, two live peers, allocation exceptions, and global application shutdown were not checked; `_Exit` follows after destruction of local editors. Do not treat metadata refusal in live state as a test of network synchronization. `forgotten.otbm`, its companions, world/database, and running Docker BlackTek were not used.

Repeat commands as in section below, with new output directory; runner now also requires lifecycle/live tests and independent comparison of restored snapshot. Earlier report from 30.10 preserves its narrower scope and does not prove these new cases.

### Update 2026-10-30: house Undo/Redo reprise and generation

[Test module](smoke/g0_rme_house_history_probe.cpp) **PASS**, separate build and runner with code 0, same 408 objects of current RME. Without mocks runs `Action` with `HousePropertiesChangeData`, `Editor::addAction` and real `ActionQueue::undo/redo` in private editor on `g0-container-20261030`. Fixture: house ID 1, rent 4321, one field (104,102,7). Change ID 1 → 2, name, rent 6789 and guildhall creates one history entry, marks dirty, preserves generation/exit/size and changes ID of related field.

- Add private ID 1 conflict: Undo false, both objects and their properties unchanged, history index/size 1. Remove conflict: Undo true, index 0, full house property tuple matching input, field house ID 1. After prior `clearChanges` Undo sets dirty.
- ID 2 conflict before Redo: false, no mutation of both houses/field and index. Remove conflict: Redo true, index 1, full property tuple from first commit and field house ID 2.
- Remove edited house via real `Houses::removeHouse`: field loses house ID; old Undo false and index 1. Create new house with ID 2: different generation; old action still returns false, no mutation of new house, field, and history.

Does not save map. Direct add/remove prepares only conflict in one-off editor; does not prove Undo of those operations. Does not test GUI, all dialog fields, live, or automatic restoration of deleted house. Ends `_Exit` after destruction of local editor, skipping global shutdown. Artifacts in `local-data/procedural-smoke/g0-house-history-runtime-20261030`: `build-manifest.json`, `run/snapshot-probe.log`, `verification.json`. EXE SHA-256 `85fd8b952f006f5fb729499dac3c2c153a9f0af5ea93f212d7e0a4ed80a03c27`, module source `0feace41d7ecea951d88da8bce4dd397983c43b13c211db237694167b19b3dc0`. Runner confirmed unchanged three source files and their copies. Report concerns runtime assertions and hashes, without independent save analysis, because nothing saves.

```powershell
rtk proxy python docs/procedural/smoke/g0_build_rme_snapshot.py local-data/procedural-smoke/g0-house-history-new --source docs/procedural/smoke/g0_rme_house_history_probe.cpp
rtk proxy python docs/procedural/smoke/g0_run_rme_snapshot.py local-data/procedural-smoke/g0-house-history-new local-data/procedural-smoke/g0-container-20261030 local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --house-history
```

### Update 2026-10-30: snapshot and Save As BOTH on real functions

**PASS in described scope**, G0 still **IN PROGRESS**. [Test module](smoke/g0_rme_snapshot_probe.cpp) is linked by [builder](smoke/g0_build_rme_snapshot.py) into separate EXE with 408 objects of existing `build-ninja`. Does not replace serializer or reader: calls `MapBackupService::CreateSnapshot`, `EditorPersistence::saveMap(..., false)` and `EditorFactory::LoadFromFile`. Standard `rme.exe` is not overwritten. Module runs after real application initialization; does not call `UnreachableCleaner`, which removes fields. Ends via `_Exit` after destruction of local editors, so normal application/global objects shutdown is not tested.

[Runner](smoke/g0_run_rme_snapshot.py) creates only new `run` directory in private output, copies `g0-container-20261030`, isolated profile `1098-BlackTek`, config and assets. `indirectory_installation = 1` also isolates recovery marker. Rejects directory reuse and missing EXE/sidecar manifest. Does not end process automatically after timeout — prints its PID for investigation. First runner attempt revealed missing sidecar on manual EXE copy; wxWidgets warning closed after read, process completed attempt. Second attempt, with manifest and corrected pre-check, ended without interaction with code 0 (18:01:46). Artifacts: `local-data/procedural-smoke/g0-persistence-runner2-20261030`.

| Attempt | Result and checked contract |
|---|---|
| Four snapshots | Different directories; real basename repeats in seconds 18:01:45 and 18:01:46. Three clean states and one dirty. Name/filename, six companion names, unnamed, dirty and field count preserved; each result read by RME |
| Invalid directory | Tag `g0:invalid` rejected on Windows, message present, map state preserved |
| House XML write error | Test thread creates new XML via `CREATE_NEW` and holds handle with `FILE_SHARE_READ`. First attempt truly blocked serializer write; result false, partial OTBM left, message with directory, dirty state and names restored. No ACL changes |
| Save As relative/absolute input | Two fresh editors. Existing sentinels `copy-house.xml`/`copy-spawn.xml` preserved; new names `copy-1-house.xml`/`copy-1-spawn.xml`. Success cleans dirty. Re-read 402 fields, waypoint, text/description and UID/AID |
| Waypoint BOTH | Simultaneously OTBM binary node and `copy-waypoint.xml`, name `G0 Both`, position (160,160,7). Sentinel `blocked-waypoint.xml` causes false before creating `blocked.otbm`, without changing metadata/dirty |

[Independent verifier](smoke/g0_verify_snapshot.py) requires both final PASS markers and no FAILED. For snapshots compares header, remaining nodes after normalization by house/spawn basename only, full 402 tiles, 401 exactly equal field nodes, container semantics at (100,100,7) and both XML trees. For Save As compares 401 remaining fields, ID and original ground attributes and exactly expected text/description, both waypoint formats, house/spawn XML and sentinels. Four negative controls must reject changed rent, map width, waypoint XML and waypoint binary. Report `verification.json`: four snapshots, two Save As, one observed write error, four negative controls. Runner confirms SHA-256 of three input files in original and copy before/after; consistent with loader table below.

EXE SHA-256 `8dc5e943d334328f551a80b5d8a68fcf38ba5b29a102d8cbe845ae190d2530fa`; source module `43b1174c462e07e6343378d0bea4d38d4f270f173d0328f0f690e662474242f7`. `build-manifest.json` also contains hash of normal EXE and source/object of snapshot service. Builder rejects CPP newer than objects; does not constitute independent full control of all headers. Used unchanged, previously built RME from 12:31:05; production C++ was not changed in this attempt.

Repeat (output must be new; builder also generates required `snapshot_probe.exe.manifest`):

```powershell
rtk proxy python docs/procedural/smoke/g0_build_rme_snapshot.py local-data/procedural-smoke/g0-persistence-new
rtk proxy python docs/procedural/smoke/g0_run_rme_snapshot.py local-data/procedural-smoke/g0-persistence-new local-data/procedural-smoke/g0-container-20261030 local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml
```

In executed attempt `python` is the runtime indicated in environment section. When copying finished build must preserve EXE, sidecar manifest and `build-manifest.json`. Original combined runtime (17:55:56–57) is in `g0-snapshot-runtime-20261030`. Earlier correct snapshot from that directory was also read by isolated, fresh BlackTek loader (code 0 and data assertions); partial result was rejected on house XML (5). This is not proof of full BlackTek start/stop or Docker image conformance.

Process/power failure, allocation exceptions, concurrent writes to already-reserved directory, all companion types, and full first-write/recovery protocol were not checked. Save As collisions are tested with existing files, not with concurrent writer. Text/description was changed directly only in private prepared test map; this is not additional proof of Undo of Lua setters. World, database, and running user Docker were not modified.

### Update 2026-10-30: reopen and setter matrix

Result on Windows x64, `build-ninja/rme.exe` from 12:31:05, SHA-256 `2cd4a8ad93f6d0c7410ca2f200df56f2b6f467f9f4d11b5f39fbc6301d331f67`. Prepared `local-data/procedural-smoke/g0-reference-profile-20261030`: separate copy of OTBM/XML and separate `data` directory, containing copy of `data/1098`, shared data files and `clients.toml` with only `1098-BlackTek` record. Local `config.toml` sets `[version].data_directory` to absolute path of this directory with trailing separator and `use_custom_data_directory = 1`. Client technical record remains same (`dat_only`, items 3/57); assets point to current set. This configuration removes ambiguity of File → Open only in test instance. Normal data directory and OTBM header were not modified.

[`test_reference_map_reopen.lua`](../../lua_tests/test_reference_map_reopen.lua) **PASS**: 17:35:44 tile and ground references preserved at (160,160,7); tab closed (`Map closed.`), then same file opened via File → Open. Re-read loaded 402 fields. Without Lua reload second stage at 17:36:17 reported `REFERENCE REOPEN PASS: stale tile/item rejected; fresh references resolve`. This does not prove forced address reuse or correctness of all methods on stale references.

[`test_item_setter_matrix.lua`](../../lua_tests/test_item_setter_matrix.lua) **PASS**, 17:38:10. Requires freshly opened copy of `g0-metadata.otbm`, empty history and compatible item types. Creates via transaction only temporary stack 2148 and container 1987 at (100,100,7), then:

| Object | Fields checked |
|---|---|
| Ground (160,160,7) | actionId, uniqueId, tier, text, description |
| Stack item 2148 | actionId, uniqueId, tier, text, description, count, subtype |
| Container 1987 root on field | actionId, uniqueId, tier, text, description |

For each of 17 combinations read original value, took reference before transaction, changed field, forced `error()`, confirmed error message, value restoration and no history index/size change. Then checked explicit commit with one index increment and Undo/Redo and standalone setter with Undo/Redo. After each attempt restored tested value. For five numeric fields values -1 and 65536 were rejected without field/history change (10 attempts); calling outfit on non-podium also returned error without new action. Final Undo removed temporary stack and container, restored index 0, empty field and UID/AID of ground fixture. This is not full canonical snapshot of all map fields or test of container content setters; correct rotation/podium and allocation errors were not checked. Changed tier/text/description were not saved nor tested for serialization.

Actual console is in `lua-tests.log` in profile directory, EXE/script/fixture hashes and copy comparisons in `verification.json`. All five byte comparisons passed: three fixture files and two scripts run from `build-ninja/scripts` against sources in `lua_tests`. OTBM SHA-256 remains `2fe46ab222606e1f8d54e30b88a58e63e0b7b48c41888ec3b19339d97c8778c4`. Maps were not saved; BlackTek and Docker were not used in these attempts.

### Update 2026-10-30: real IOMap BlackTek on fresh objects

**PASS for indicated fixture and loader scope**, not for full server. `blacktek-fresh/build.log` ends with creation of `bin/Black-Tek-Server.exe`. New probe links 85 objects per-source from that directory; does not link any `unity_*.obj` or original `otserv.obj`. Current `otserv.cpp` is compiled separately with `/Dmain=blacktek_unused_server_main`, and its standard startup is not called. No `/FORCE:MULTIPLE`. Builder checks CPP and header dates against objects and writes their SHA-256 in `probe_build_manifest.json`. This is stronger confirmation of origin than mere presence of EXE; earlier attempt on old unity remains invalid.

Sources: [C++ harness](smoke/g0_blacktek_iomap_probe.cpp), [builder](smoke/g0_build_blacktek_iomap.py), [case runner](smoke/g0_blacktek_iomap_cases.py). Local BlackTek MSVC build with per-source objects and indicated synthetic set saved earlier by RME is required. Example of executed command (Python runtime per environment section):

```powershell
rtk proxy C:\Users\Sorky\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe docs/procedural/smoke/g0_build_blacktek_iomap.py C:/repo/ProjectSorky/BlackTek-Server local-data/procedural-smoke/g0-1098-20261026/blacktek-fresh "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvarsall.bat"
rtk proxy C:\Users\Sorky\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe docs/procedural/smoke/g0_blacktek_iomap_cases.py local-data/procedural-smoke/g0-1098-20261026/blacktek-fresh/iomap_probe.exe C:/repo/ProjectSorky/BlackTek-Server/vcpkg_installed/x64-windows/x64-windows/bin C:/repo/ProjectSorky/otclient/data/things/1098/Tibia.dat local-data/procedural-smoke/g0-container-20261030 --report local-data/procedural-smoke/g0-1098-20261026/blacktek-fresh/iomap-cases.json
```

Builder and runner ended with code **0**. **6/6 cases matching expectation**: correct set (0), no house XML (5), no spawn XML (6), changed rent (9, data assertion), empty spawns (9, count assertion), truncated map (4, loader error). XML parser may return success without loading proper content, so test includes result objects. Assertions in real server classes:

- House ID 1, `G0 House`, rent 4321, town ID 1, entry (105,102,7), one field and house binding to (104,102,7).
- Ground at (160,160,6/7), AID 12006/12007, UID 12106/12107 and PZ.
- Field (100,100,7): container 1987 → container 1987 → item 2148, count 7, AID 12042.
- One spawn (160,160,7), radius 3, one block, Rat, 90000 ms, south direction. Reading private spawn records is test-only (`private` exposed only in harness); server code and class layout are not changed.

Runner assumes separate temporary directory for each process, creates only minimal XML of Rat definition and memory pool config directory. Does not load server config, does not call thread startup, services, database or spawns. Checks for no input byte changes. Hashes of source fixture before/after:

| File | SHA-256 |
|---|---|
| `house-roundtrip.otbm` | `b0f4c1465695c841b4488ff35abe4ef2a1b125997467e33659954c5036418564` |
| `house-roundtrip-house.xml` | `050b6e04e63b3d79da20e45bef6ad81d4bac74c42fef6acb65dc65ec0d93a141` |
| `house-roundtrip-spawn.xml` | `dc85b6835770ba8665e534339324e655d5c23693616b9ca726d2a49bfa6ad8b6` |

**Explicit harness limitation:** first execution returned from test function after successful assertions and local map destruction with result 0, then ended with `0xC0000005` in final phase of global destruction (stack contained `execute_onexit_table`). Cause undetermined. Final harness executes `std::_Exit(result)` after function return, so tests local map/buffer destruction too, but skips destructors of global server objects that did not pass standard startup. Do not declare PASS of BlackTek start/stop, full world, user monster definitions, original RME, or deployed Linux/Docker. Running Docker BlackTek remained untouched. G0-09 now has proof of real read of this OTBM 3 and XML by current Windows sources; G0-08 and full gate still require closure of remaining described scopes.

**Map reopen — attempt incomplete, 30.10 17:31:** new [`test_reference_map_reopen.lua`](../../lua_tests/test_reference_map_reopen.lua) run on copy of `g0-reference-reopen-20261030/g0-metadata.otbm` in RME EXE from 12:31:05. Welcome loaded 402 fields with explicit `1098-BlackTek`; script preserved ground and tile references and reported `REFERENCE REOPEN ARMED` at 17:31:08. Tab close confirmed status `Map closed.`. Repeat File → Open of same file picked standard `10.98`, displayed no assets, and after cancel status `Failed to load map.`. Second stage of script **not executed**; this is neither FAIL nor PASS of lifetime assertions. Fixture files were not saved, header not modified. For repeat need unambiguous profile selection in same session. Test should not reload Lua between stages, as it would lose stored references.

| Check | Result | What actually confirms |
|---|---|---|
| `rtk git status --short --branch` before changes | Clean `master`; created `docs/procedural-phase0-audit` | Baseline of local code; does not establish upstream SHA |
| `rtk proxy codegraph status .` | Current index, 3328 files | Index availability; not test coverage |
| MCP CodeGraph and `ast-grep` for `g_gui.GetCurrentBrush()` | Confirmed dependencies of three `DrawOperations::draw` overloads | Real drawing dependency on GUI |
| First `rtk proxy cmd /c build_ninja.bat` | FAIL: Ninja not in `PATH` | Environment issue, no source compilation |
| Repeat canonical build with local Ninja and `VCPKG_ROOT` | Dependencies and config OK; RME compilation FAIL, `C3859/C1076`, Windows 1455 | Virtual memory exhausted at default parallelism; not a passed build |
| Resume same target/build tree with `--parallel 2` | **PASS**, code 0, `[382/382] Linking CXX executable rme.exe` | 2026-10-26: `build-ninja/rme.exe`, 8026112 bytes, launched in GUI as OTAcademy Map Editor 4.1.2; no source or pagefile change |
| `tools/test_tibia_data.py -v` via Codex Python runtime | 40 tests: 33 PASS, 7 SKIP | Parsers and CLI on synthetic data. `Tests1098` integration skipped due to no client |
| `tools/tibia_data.py --otb data/1098/items.otb info` | PASS: 26282 items, OTB 3.57.62-10.98 | Read existing OTB, no DAT/SPR confirmation and render consistency |
| File inventory in checkout | No DAT, SPR, OTBM or ready EXE found; OTB present in `data/` | No complete set for GUI test in this directory; private maps outside project not searched |
| Assets indicated later by user | `C:\repo\ProjectSorky\otclient\data\things\1098\Tibia.dat` and `Tibia.spr` | User also indicated `otclient_gl_x64.exe`; not RME build and does not replace smoke editor |
| Inspection of indicated DAT/SPR + local OTB 10.98 | PASS of parsing alone, code 0, no warnings: DAT `0x42A3`, SPR `0x57BBD603`, 26282 items DAT and OTB, 338944 sprites | Parser recognized 10.98 format, extended, improved animations, frame groups, RGB (24 SPR validation samples). Signatures do not prove semantic consistency of OTB mapping — further test found discrepancies |
| Same tool tests with copies of indicated assets in isolated harness | **34 PASS / 6 FAIL**, code 1, no SKIP | Indicated DAT differs from fixture expected by original tests. Errors concern item count, properties ID 930/1162, sprites and OTB hash; test expectations not changed |
| Sol2 callback probe with project config | REPRODUCED: `func();` does not throw despite `error()` after side effect; diagnostic code 0 | Confirms Lua/Sol2 behavior used by build, does not test map, RME transaction or Undo |
| Smoke Lua 20x20 in GUI, `1098-BlackTek` | **PASS** placement of 400 ground, single Undo and single Redo, 2026-10-26 | Region X/Y 100–119, Z=7; halo 99–120 remained absent. Empty map test without protected metadata; does not check bordering or history separation from manual edits |
| Save via standard workflow | **PASS of save**, `g0-smoke.otbm` 3351 bytes and house/spawn XML 33 bytes each | Save does not yet mean correct re-read |
| Regular File → Open of saved map | Picked standard profile `10.98`, requiring different assets | Header does not distinguish profiles with same client/OTB number. Cancelled asset choice for wrong profile; required explicit `1098-BlackTek` choice |
| Resume GUI 2026-10-27 | Process gone after OK in Preferences while setting Client Path | Confirmed no window/process; no message and no Application Error 1000 found. Cause undetermined; later attempt with isolated config below |
| Re-read with isolated config and explicit profile | **PASS for fixture**, 2026-10-27 18:36:36 | 400 exact ground ID from brush, empty stacks/creature/spawn/house ID/mapFlags, halo absent. Grass visible in renderer. Empty house/spawn files do not prove complex metadata handling |
| Callback error on map, G0-07 | **FAIL of rollback**, 2026-10-27 18:37:31 | `propagated=false; tile_present=true; tile_count=401`. `error()` was ignored by wrapper and change entered history. Error visible in log comes only from final `assert` of diagnostics |
| Cleanup after error attempt | **PASS of manual Undo and check**, 18:38:26 | One Undo, then re-run verifier confirmed original 400 fields. Diagnostic change not saved to disk |

Note on CLI: tool global arguments precede subcommand. Attempt `info --otb ...` was rejected by parser; correct command given in table. Do not treat failed attempt as OTB read error.

SHA-256 of smoke test input (read-only, without copying assets to Git):

| File | SHA-256 |
|---|---|
| DAT 10.98 from indicated directory | `14bc3d04b8167744e3207a09da1ae1fecb89d4222e50ed93850defee032458cb` |
| SPR 10.98 from indicated directory | `041f55f13662517e57cf93fb0c3f743ad8d2bd8aec14d408842f5bf57eeb6653` |
| `data/1098/items.otb` | `9f73f8bf0d5799cd7caef1b37ec979a14fcaa52405127c7838a13b32ab1a67ee` |

### Reference OTB and provided client discrepancy

Harness: `local-data/procedural-smoke/g0-1098-20261026/`. Copied without modification `tools/tibia_data.py` and `tools/test_tibia_data.py`, and copies of DAT/SPR and local OTB placed in `Tests1098/` required by tests. Log: `tibia-data-tests.log` in harness directory. Data is local and ignored by Git.

Reference test expects maximum DAT ID 23725 and 23626 items; provided DAT has 26381 and 26282 respectively. For client ID 930 test expects sprites 5255/5256 and gets 9833/9834. Oracle hash of sprites does not match OTB for ID 930. This is result on different data set, not proof of parser regression; at same time blocks recognition of standard OTB as consistent with this client.

In `data/clients.toml` exists `1098-BlackTek`, `configType='dat_only'`, `dataDirectory='1098'`. `ItemDefinitionResolver::resolveDatOnly` starts from server ID = client ID, then applies XML. Profile loaded in RME with signature validation; confirmed smoke scope `grass`, without binding standard OTB. Additional read: DAT ID 4526 is a passable ground with minimap color 24 and sprites 6592/6593/6705/6709; standard OTB binds server ID 4526 to client ID 4515. Brush `grass` in `data/1098/brushes/brushes.xml` uses server look ID 4526. Loader reported reversed ranges `Walls` 1205..1202, `Borders` 4514..4225 and 3394..3360 and missing IDs 30001 and beyond in `Others`. Full materials not approved.

### Reproduction of Sol2 callback error handling

Local files in same ignored harness: `sol_callback_probe.cpp` and `run_sol_callback_probe.cmd`. Command from repo directory: `rtk proxy cmd /c local-data\procedural-smoke\g0-1098-20261026\run_sol_callback_probe.cmd`. Used MSVC x64, `/std:c++latest /EHsc /MD`, real `source/lua/lua_sol_config.h`, headers and `lua.lib` from `build-ninja/vcpkg_installed/x64-windows` and corresponding `lua.dll`.

Probe contains `static_assert` of equivalence of `sol::function` and `sol::protected_function`; runs callback incrementing counter before `error()`, ignores its result in `try/catch`, then executes second call with `valid()` check. Result: `returned_without_throw=true`, `counter_after_ignored_result=1`, `checked_call_failed=true`. This is code 0 **diagnostics reproducing risk**, not PASS of RME rollback. Link to source: `LuaAPI::transaction` calls callback without result inspection then proceeds to commit. Full G0-07 must check actual tiles and history.

Command of executed tests:

```powershell
rtk proxy C:\Users\Sorky\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe tools/test_tibia_data.py -v
```

Repeat canonical build with settings for current PowerShell session only:

```powershell
$env:PATH = 'C:\repo\vcpkg\downloads\tools\ninja-1.13.2-windows;' + $env:PATH
$env:VCPKG_ROOT = 'C:\repo\vcpkg'
rtk proxy cmd /c build_ninja.bat
```

Verify Ninja path on given machine. Do not use `clean`/`clean-all` options without need. `PATH` change serves to run existing script, does not replace it with alternative build.

After memory error resumed target `rme` generated by this script in same `build-ninja`, after `vcvarsall.bat x64`, with command `cmake --build build-ninja --target rme --parallel 2`. Local wrapper: `local-data/procedural-smoke/g0-1098-20261026/build_rme_limited.cmd`. Did not change `build_ninja.bat`, compilation flags or dependencies. First inline attempt via `cmd /c` failed on path quoting of `vcvarsall`; replaced with readable CMD file. Final result: code 0, linking `[382/382]`, EXE from 2026-10-26 18:45:53.

### Manual smoke repeat

Scripts from actually executed attempts: [place/Undo/Redo](smoke/g0_smoke.lua), [read content](smoke/g0_verify_saved.lua), [callback error](smoke/g0_error_probe.lua). Copy them to `scripts/` next to test EXE, reload scripts and run manually with Run button in Script Manager. Do not set auto-run. Test executes existing Lua bindings, not full brush/autoborder pipeline.

1. New empty map with client `1098-BlackTek`: run `g0_smoke.lua`, perform one Undo and run it again, then one Redo and run a third time. Results 2026-10-26: PLACE 18:53:46, UNDO 18:54:18, REDO 18:54:33, all PASS.
2. Save as `g0-smoke.otbm` in an isolated directory. Close and open from Welcome screen with explicit profile `1098-BlackTek`, then run `g0_verify_saved.lua`. Do not assume that regular File → Open recognizes the client variant.
3. Only after READ PASS run `g0_error_probe.lua`. It adds ground in empty (140,140,7), then deliberately reports an error. Current build ends diagnostics as FAIL. Undo once and repeat verifier; do not save the result of the failed transaction.

Resumption 2026-10-27 used the harness working directory and local `config.toml` (RME reads it relative to cwd):

```toml
[[clients]]
name = '1098-BlackTek'
clientPath = 'C:\repo\ProjectSorky\otclient\data\things\1098\'
default = true
```

OTBM in Welcome: version 3, size 2048x2048, items major/minor 3/57, relative files `g0-smoke-house.xml` and `g0-smoke-spawn.xml`. `otbmVersions=[3]` in TOML denotes version 3; loader subtracts 1, obtaining `MAP_OTBM_3=2`. Do not confuse format number with enum value.

## G0-06 and partial G0-08 — 2026-10-27

**G0-06 PASS for the tested sequence**, 19:38:31. Profile `1098-BlackTek`, OTBM 3, fixture 400 ground; in configuration `group_actions=true`, `use_automagic=true`, `undo_size=400`, `undo_mem_size=40`. First manual `Draw` at (101,101,7) used 1x1 eraser and triggered automatic borders. Script [`g0_manual_history.lua`](smoke/g0_manual_history.lua) saved the state of a 24x24 area and created ground via Lua transaction at (150,150,7). Second manual `Draw` with `dried grass` brush at (106,105,7). History: `Draw`, `G0 mixed history Lua`, `Draw`. Test checked each state after three Undo, three Redo, and three cleanup Undo; log: `G0 MIXED HISTORY PASS: Draw / Lua / Draw remained separate across Undo and Redo; fixture restored.`

First attempt of second Draw with eraser created an entry, but did not change the data snapshot; script rejected it with message `Second manual stroke missing`. After reverting that attempt, `dried grass` was used and control was repeated with result PASS. Snapshot includes tile presence, ground ID, item stack order, house ID, flags and creature/spawn presence, not complete all attributes and objects on the map.

**G0-08 partially PASS**, 19:44:43. [`g0_metadata_roundtrip.lua`](smoke/g0_metadata_roundtrip.lua) created tiles (160,160,6) and (160,160,7): ground with AID 12006/12007 and UID 12106/12107, mapFlags 1, and on Z=7 spawn radius 3 with `Rat`, spawntime 90 s. After Save As and reopening in RME with explicit profile `1098-BlackTek` these values were checked, 402 tiles total, base area ground and halo. Log: `G0 METADATA ROUND-TRIP PASS: floors 6/7, UID/AID, flags, spawn, Rat and base halo preserved.` Spawn XML was separately parsed and position, radius and time were confirmed. Fixture does not include houses, zones, containers, full attributes or external consumers.

**House and town — partial round-trip, 28.10:** on a separate copy `house-roundtrip.otbm` town `G0 Town` (ID 1) and house `G0 House` (ID 1, town ID 1) were created, one house field and exit (105,102,7) were assigned. Saved XML contains name, ID, town ID, rent 1234, size 1 and exit coordinates. After RME restart and reading with profile `1098-BlackTek` the label `G0 House` was visible, house palette showed ID 1, rent 1234 and size 1, and properties dialog showed name and town `G0 Town`. Separately, only the rent was changed to 4321 and Ctrl+S was used: XML contained the new value, though no asterisk appeared in the title after the change. This is not a complete automatic check of all tile attributes or test of external consumer; remaining metadata requires separate verification.

**House editing — work in progress results, 30.10:** on a copy `local-data/procedural-smoke/g0-house-undo-20261030`, in a build with uncommitted changes, rent was changed from 4321 to 6789. Palette showed successively 6789, after Undo 4321 and after Redo 6789; saved house XML contained 6789. Undo after save restored 4321 and again marked the map as changed. House ID was changed 1 → 2, Undo/Redo and save were performed: XML and field node (104,102,7) showed ID 2; after Undo and save both showed ID 1. Separate collision attempt **FAIL**: after resetting ID to 2, a second house with ID 1 was added; Undo did not overwrite the new house, but left ID 2 and shifted history. Collision changes were not saved. Live cooperation and all dialog fields were not tested; implementation is not ready.

**History conflict check — partial PASS, 30.10 07:18:** after next change and build 197/197, on this copy the change ID 1 → 2, adding house with ID 1 and Undo were repeated. Both houses remained unchanged, and GUI reported inability to undo due to missing house or occupied ID. Script [`test_house_history_conflict.lua`](../../lua_tests/test_house_history_conflict.lua) confirmed error for `editor:undo()` and `editor:goToHistory()`, in both cases without changing history index (`HOUSE HISTORY CONFLICT PASS`). Retry after conflict removal, deletion of original house, Redo at conflict, live cooperation and Undo of house add/remove were not checked. After test the state of two houses was saved in an isolated copy.

**Retry on current build, 30.10 12:10:** on a separate copy `g0-house-conflict-final-20261030` the change ID 1 → 2, adding house ID 1 and Undo in GUI were repeated. The message refused to undo, both houses remained in palette, and the same Lua script again reported `HOUSE HISTORY CONFLICT PASS`. The copy was saved after test. The above limitations remain current.

**Loss of unsaved house change — FAIL, 28.10:** after saving rent 4321 on this synthetic map, it was changed in dialog to 5678. Palette showed 5678, but title had no asterisk. Card closure occurred without asking for save; after closing XML still contained 4321. The change in memory was thus lost without warning. Code `HousePalette::OnEditHouse` refreshes palette, but does not add action or mark map as changed; other house and town operations were not verified by this test. Merely marking state would not ensure required Undo, so fix requires separate scope covering metadata history.

**Save As — confirmed data problem:** in directory `g0-1098-20261026` saving new `g0-metadata.otbm` preserved in header `g0-smoke-house.xml` and `g0-smoke-spawn.xml`, and existing spawn of original synthetic `g0-smoke.otbm` changed from empty 33 B file to 177 B XML with Rat. `EditorPersistence::saveMap` assigns new XML names only for `map.unnamed`, and `IOMapOTBM::saveMapToDisk` uses existing names. Set of new map and both XML was copied to `local-data/procedural-smoke/g0-metadata-20261027/`; original empty synthetic spawn was restored, and orphaned OTBM duplicate was deleted after checking identical SHA-256 `2fe46ab222606e1f8d54e30b88a58e63e0b7b48c41888ec3b19339d97c8778c4`. Read test PASS was performed from isolated set. File `g0-smoke.otbm` was not overwritten.

**G0-07 FAIL for existing Item attribute**, 19:49–19:50. On isolated, saved `g0-metadata.otbm`, [`g0_item_attribute_rollback.lua`](smoke/g0_item_attribute_rollback.lua) changed `tile.ground.actionId` from 12007 to 0 inside callback and called `error()`. Result: `propagated=true; action_restored=false; history_unchanged=true`. Moreover, separate successful transaction changing same AID gave `success=true; action_changed=true; history_changed=false`. Script restored AID before assertion; map not saved. In `source/lua/lua_api_item.cpp` setter `actionId` calls `Item::setActionID` without marking tile in `LuaTransaction`. Callback fix from `a71fe4b` resolves error catching, but does not make all Item mutations tracked. Before G1 separate fix and tests of tile owner tracking required also for other setters; do not assume their behavior without tests.

**Reference obtained before transaction — FAIL, 28.10 07:30:28:** extended diagnostics obtained `tile.ground` before entering `app.transaction`, changed AID through this reference and reported error. Error reached `pcall`, but AID was not restored, and history remained unchanged. Script restored AID value before final assertion; map not saved. Fix must cover also this case, not only references obtained inside callback.

**AID and UID regression test — FAIL, 28.10 09:57:51:** [`test_transaction_item_undo.lua`](../../lua_tests/test_transaction_item_undo.lua) was run in script manager on isolated `g0-metadata.otbm` with profile `1098-BlackTek`. After changing two attributes through reference obtained before transaction and reporting exception, test stopped at assertion `Item attributes were not rolled back without a history entry`. Script restored values on fixture before assertion; map not saved. Subsequent assertions of successful transaction and Undo/Redo were not performed and require repetition after fix.

**Transaction nesting — partial PASS, 28.10 07:27:53:** rejected `app.transaction` call inside active transaction previously went to common `catch`, which also rolled back outer transaction. Check before this block was added. [`test_transaction_nested.lua`](../../lua_tests/test_transaction_nested.lua) in newly built RME, on reloaded `g0-smoke.otbm` and empty history, performed change before rejected call and after it. Log `NESTED TRANSACTION PASS` confirmed one history action, Undo and Redo of both fields and final Undo to 400 fields. Test does not save map and does not fix separate Item attribute problem.

**Save As — partial regression, 27.10:** for previously saved OTBM 3 map, copy name selection skips existing auxiliary files. In isolated directory Save As was performed from `g0-metadata.otbm` to `g0-saveas-target.otbm`, after placing sentinel files under names `g0-saveas-target-house.xml` and `g0-saveas-target-spawn.xml`. Byte comparison confirmed unchanged source OTBM, both its XML and both sentinels; new OTBM contains references to `g0-saveas-target-1-house.xml` and `g0-saveas-target-1-spawn.xml`, and parsed new spawn XML contains Rat. Fixture did not introduce changes before save. Save was performed in RME with profile `1098-BlackTek`.

**Save As and OTBM 5 read — partial PASS, 28.10 16:22:** separate copy of metadata fixture was converted in properties to OTBM 5 and saved. In same isolated directory existing files `g0-otbm5-copy-monster.xml` and `g0-otbm5-copy-npc.xml` with sentinel content were prepared, then Save As was used to `g0-otbm5-copy.otbm`. Both sentinels preserved content, save created separate files `g0-otbm5-copy-1-house.xml`, `-monster.xml`, `-npc.xml`, and monster XML contained Rat. Welcome after restart showed OTBM 5 and these three names; with explicit profile `1098-BlackTek` local copy `g0_metadata_roundtrip.lua` with matched filename reported PASS for 402 fields, floors 6/7, UID/AID, flags, spawn, Rat, base fields and halo. This does not check non-empty NPC, zones, changes before Save As or external consumer; OTBM 5 client profile not approved for other use.

**Zone OTBM 5 — partial round-trip, 28.10:** in copy of above fixture zone `G0 Zone` (ID 1) was added and field (104,104,7) was assigned with zone brush. Save As to `g0-otbm5-zone.otbm` skipped existing sentinel `g0-otbm5-zone-zones.xml`; its content remained unchanged, and new `g0-otbm5-zone-1-zones.xml` contains zone name and ID. Welcome indicated this file in OTBM 5 header. After RME restart and opening with profile `1098-BlackTek` 402 fields were loaded, and zone label was again visible on map. This is manual check of one zone and one field; does not confirm full G0-08 compatibility or reading by external program.

**Zone fixture byte verification, 29.10:** independent of RME read classes, script [`g0_verify_zone_fixture.py`](smoke/g0_verify_zone_fixture.py) read node tree of saved copy and XML: OTBM 5 format, 402 saved fields and exactly one zone node with ID 1 at (104,104,7); XML contains name `G0 Zone` and ID 1. Result PASS, code 0. Script checks only this fixture and structure needed for this assertion; not complete OTBM validator or reading by external editor or server.

**Negative validator check, 29.10:** on copies in temporary directory it was verified that validator accepts unchanged fixture, rejects XML with zone ID changed from 1 to 2, invalid zone XML name in OTBM header and OTBM truncated by last byte. Probe `local-data/procedural-smoke/g0-1098-20261026/verify_zone_negative.py` ended with code 0. This is not proof of detecting all damage types.

**BlackTek as consumer — source analysis, 29.10:** in local `C:/repo/ProjectSorky/BlackTek-Server` on HEAD `c620e7957` `src/iomap.cpp` rejects raw header version 0 or greater than 2. Verified above OTBM 5 copy has raw version 4, so this loader implementation will reject it on version check; do not use this fixture as proof of BlackTek compatibility. This is conclusion from code and read header, not executed server test or evaluation of other build. For G0-09 must separately check in isolated consumer fixture OTBM 3 (raw version 2) along with auxiliary files and not run server on existing database or user world.

**BlackTek — partial node parser test, 29.10:** separate probe was compiled MSVC x64 from local `BlackTek-Server/src/fileloader.cpp` and Boost library of this checkout. On `g0-smoke.otbm` probe called actual `OTB::Loader::parseTree` and `getProps`, read raw version 2 and counted 400 tile nodes; result PASS, code 0. Probe is in ignored `local-data/procedural-smoke/g0-1098-20261026`. Does not call `IOMap::loadMap`, does not create map objects and does not read XML: does not pass G0-09 nor proves semantic compatibility with server.

**Isolated IOMap attempt — invalid, 30.10:** auxiliary program with `IOMap::loadMap` was linked with available server unity objects, using `/FORCE:MULTIPLE` due to second `main`. Running from test directory returned code 1 after `ObjectPools` initialization warning, without confirming entry to test `main`; map and XML were not verified. Later check showed that object containing `iomap.cpp` is from 10.04.2026, and source has date 25.07.2026: cannot therefore attribute this program to current source. Standard server startup connects to database and runs updates before loading map, therefore not run on existing user configuration. Further test requires current isolated build and explicit initialization check and dependencies; server data were not changed.

**OTBM 5 header test, 29.10:** corrected `testScenario9_OTBM5_CrystalServer` checks description and filenames of monsters, NPCs, zones and houses, which previously it was writing without reading them in assertions. Scenario was run separately from temporary harness in `local-data/procedural-smoke/g0-1098-20261026`, compiled MSVC x64 with libraries of existing `build-ninja`; result PASS, code 0. Test uses project classes for writing and reading nodes and does not check actual map, XML content, zone fields or external consumer. Other scenarios of this suite were not run.

**OTBM suite limitations:** current `build-ninja` registers in CTest only `test_universal_rotator`; does not contain `test_otbm_gzip_cpp` target. First three scenarios of OTBM suite require specific `tests/gzip_world.otbm` and `tests/none_world.otbm`, which were not found in this checkout. Scenario 8 checks local client version selection model, not production selection mechanism; scenario 10 reproduces fragment of serialization logic in test, so its result does not prove working production serializer. Do not treat these tests as proof of complete G0-08 or deciding client profile.

**Waypoint XML — partial PASS:** in isolated directory `g0-waypoint-20261027` `waypoint_storage_format = 1` was set and waypoint `fixture` (160,160,7) was added to source XML. Save As to `blocked.otbm` refused save due to existing `blocked-waypoint.xml`: sentinel file remained unchanged, OTBM was not created, and source OTBM and house/spawn XML remained identical byte-wise. Save As to `waypoint-copy.otbm` created `waypoint-copy-waypoint.xml` with this waypoint. After restart and opening copy with profile `1098-BlackTek` Waypoint palette showed `fixture`, 160,160,7. XML has no reference in OTBM header and must have name derived from map name, so collision of this name interrupts save. After this test this check was extended also to OTBM mode, because reading empty waypoint list can fetch existing XML; result of this test described below.

**First save and OTBM waypoint mode — partial PASS, 28.10:** new, empty map `1098-BlackTek`, `waypoint_storage_format = 0`. Attempt to save to `blocked.otbm` was stopped by existing `blocked-waypoint.xml`; OTBM was not created, sentinel remained unchanged. Next first save to `newmap.otbm` skipped existing `newmap-house.xml` and `newmap-spawn.xml`; their sentinel remained unchanged, and OTBM points to newly created files `newmap-1-house.xml` and `newmap-1-spawn.xml`. Fixture in `local-data/procedural-smoke/g0-firstsave-20261028`.

**Further action:** check modified content, OTBM 5, waypoint mode BOTH, absolute/relative paths, cancellation, rollback and external read compatibility. Existing backup and save rollback mechanism requires separate evaluation; mere check of name availability is not atomic file reservation. Until regression completion test only on isolated copies.

**Backup — partial regression, 28.10:** in `g0-backup-20261028` synthetic map was modified with eraser. Existing sentinel `g0-metadata.otbm~` caused save refusal and remained untouched; OTBM and house/spawn XML were identical to source copy. Then OTBM collision was removed by moving sentinel and sentinel `g0-smoke-house.xml~` was created: editor moved OTBM to temporary backup, encountered XML collision and restored OTBM byte by byte. Separate attempt opened only synthetic OTBM with sharing disabled (`CreateFileW`, `share=0`); after save attempt and lock release original OTBM and XML remained unchanged. After removing collision, correct save of modified map passed. Tests concern backup preparation, not failure after save start or crash recovery.

Previously existing code deleted `*.otbm~` before OTBM move and only logged `rename` error. In prepared fix backup is not deleted, error stops save, and already moved files are restored. Further check remains necessary: crash between backup move and marker creation, restore error and save on nested paths and race with another process were not proved. Do not treat fix as complete save atomicity.

**Unicode paths in save rollback, 28.10:** source check showed that backup preparation used Windows paths in wide form, and restore after serialization error passed narrow UTF-8 strings to `std::filesystem`. Deletion of partial file and backup restore were unified to wide paths. Compilation and linking of `rme.exe` passed after closing test process; serialization failure on Unicode path not yet triggered, so runtime behavior remains to be checked. First linking failed (`LNK1104`), because test process blocked EXE after stopping on wxWidgets assertion at close; build script despite Ninja error returned code 0, so result was evaluated also after Ninja message, not only exit code.

**Unicode — runtime regression, 28.10:** copy of `g0-metadata.otbm` with two XML was placed in isolated directory `g0-unicode-żółć-20261028`. Before fix Ctrl+S stopped already at backup, even after XML lock release; path in message was empty. In save function encoding-dependent path name conversions were replaced with UTF-8 conversions and wide paths were used when deleting temporary copies. Incremental build and linking `[2/2]` passed. After reopening fixture `house.xml` lock by `FileShare.Read` led to serialization error and incomplete rollback; original OTBM, house and spawn and all three backups had hashes matching state before attempt. After lock release and restart recovery was selected: originals kept same hashes, and three backups disappeared. This is test of restore error on this one path, not confirmation of successful save, persistence after power failure or handling all filenames and backup settings.

**Unicode — normal save and re-read, 28.10 15:54–15:57:** after recovery and reopening same synthetic copy with explicit profile `1098-BlackTek` Ctrl+S reported `Map saved successfully`. Directory contained OTBM and both XML, without `~` files; spawn XML still contained `Rat`. After closing and reopening with same profile verifier `g0_metadata_roundtrip.lua` reported PASS: 402 fields, floors 6/7, UID/AID, flags, spawn, Rat, 400 base fields and halo. Test does not cover map change before save, filename containing Unicode or permanent backups.

**Unicode — persistent backup, first result FAIL, 28.10:** in isolated `config.toml` `always_make_backup = true` was set. Ctrl+S reported success, but instead of date-stamped archives three temporary copies `~` remained; before further test they were saved in subdirectory `prior-temporary-backups`. Cause in code was use of narrow paths for `std::rename` without checking result. Change was prepared using wide paths, copy without overwriting existing archive and warning with locations of preserved copies if archiving fails. Requires rebuild and runtime test.

**Unicode — persistent backup after fix, partial PASS, 28.10 16:07:46:** incremental build ended with linking `[2/2]`. On same fixture and with `always_make_backup = true` Ctrl+S reported success. Three date-stamped archives were created: OTBM and house and spawn XML; their SHA-256 matched files before save. No `~` files remained in main directory. Attempt did not cover archive collision, copy or delete error, filename containing Unicode or content change between map versions.

**Unicode — archive collision, PASS for this scenario, 28.10 16:11:01:** in fixture directory 120 `G0_COLLISION_SENTINEL` markers were created under successive archive names for OTBM with timestamp. Ctrl+S with `always_make_backup = true` reported warning that map was saved, but OTBM copy remained in `g0-metadata.otbm~`; copies of both XML were archived. All 120 markers preserved content, and SHA-256 of OTBM file and preserved `~` matched state before attempt. Attempt does not cover XML collision, failure during copy or failed `~` deletion; diagnostic files remain in ignored fixture directory.

**Save As + existing backup — partial PASS:** in `g0-saveas-backup-20261028` `g0-metadata.otbm` was loaded, then existing `copy-target.otbm` was selected and synthetic copy replacement was confirmed. Existing `copy-target.otbm~` stopped save; message indicated backup name, source map remained in window. Byte comparison confirmed unchanged source and target OTBM, source XML and backup sentinel; new XML were not created. After moving sentinel and retrying save, target OTBM was replaced, which points to new `copy-target-house.xml` and `copy-target-spawn.xml`; spawn contains Rat, and source files and preserved sentinel remained unchanged. Test does not cover failure during serialization.

When closing session after save `wxWidgets Debug Alert` appeared with assertion `GetWindow() != nullptr` in `GetDescription()`; after closing map Stop was selected. Cause and relation to fixture undetermined.

As part of existing CTest verification `test_universal_rotator` target was built in same build tree via `cmake --build build-ninja --target test_universal_rotator --parallel 2`, after loading MSVC x64. Then `ctest --test-dir build-ninja -R "^test_universal_rotator$" --output-on-failure`: **PASS 1/1**, 0.20 s, code 0. Session helper script: `local-data/procedural-smoke/g0-1098-20261026/build_rotator.cmd` (local, ignored). This test does not prove Lua correctness, Save As or round-trip.

## Warunki testu integracyjnego G0

<a id="g0-b--naprawa-callbacka-i-regresja-2026-10-27"></a>

### G0-B — callback fix and regression, 2026-10-27

Branch `fix/lua-transaction-callback`, audit base `b2bc871`. Change in
`source/lua/lua_api_app.cpp`: explicit `sol::protected_function`, check callback
result and pass error to existing rollback path. Sol2 documentation
checked via Context7 `/thephd/sol2`; syntax `result.valid()` and
`sol::error` extraction confirmed by compilation with installed version.

- Build: `rtk proxy cmd /c local-data\procedural-smoke\g0-1098-20261026\build_rme_limited.cmd`, code **0**, `[2/2] Linking CXX executable rme.exe`. After fixing formatting, final source build was repeated, also code 0. EXE 8026112 bytes, 2026-10-27 18:52:13; this EXE was run in GUI.
- Regression: [`lua_tests/test_transaction_rollback.lua`](../../lua_tests/test_transaction_rollback.lua), run manually on freshly opened fixture with explicit `1098-BlackTek` and empty history. **PASS**, 18:53:46. Three callbacks create two tiles each and report different errors. Messages were checked, deletion of six mutations, unchanged history size/index, two successful transactions, rejection of callback return values, two Undo, two Redo and restoration of 400 fields. In history Redo remain two correct test actions.
- Original `g0_error_probe.lua`: **PASS**, 18:54:08 — `propagated=true`, `tile_present=false`, `tile_count=400`, message `G0 ROLLBACK CONTENT PASS`. This is same reproduction that on audit base left 401 fields.
- `g0_verify_saved.lua` after both tests: **PASS**, 18:54:30 — 400 exact ground IDs, empty stacks/entities/house IDs/flags and halo.
- OTBM file was not saved during regression. SHA-256 before and after: `2e0728b71bc5506db21a22be8e29920c17d418f599a7fcbf627536eb45f728ce`.

Odczytany log Script Manager:

```text
[18:53:46] ROLLBACK PASS: 3 original errors propagated, 6 tile mutations restored, history unchanged.
[18:53:46] TRANSACTION REGRESSION PASS: successful commits, separate Undo/Redo, no failed entries; fixture restored to 400 tiles.
[18:54:08] G0 ROLLBACK CONTENT PASS; history still requires a separate check.
[18:54:30] G0 SAVED CONTENT PASS: 400 exact ground IDs; empty stacks, entities, house IDs, flags and halo.
```

Test is deliberately outside general Lua runners, requires dedicated map and
manual run. Full Lua/CTest suite not performed nor tests of allocation failure, editor
change or close in callback, nested transactions or rollback of complex
metadata. G0-06 with actual manual editing via `DrawOperations` still requires
separate proof; two Lua transactions do not replace this case.

Required: working `build-ninja/rme.exe`, explicitly selected client with complete assets, compatible ground brush and synthetic map. Absence of any condition means test not performed, not PASS. User maps and reference data not to be modified.

Tests to be performed in `local-data/procedural-smoke/<run-id>/`, with separate output files for each attempt. OTBM and all indicated auxiliary files must be in test copy; especially check absolute paths to XML so save does not touch originals.

Record: SHA and diff, compiler/dependency versions, client and asset signatures/hashes, OTBM version, brush names, automagic/grouping/Undo settings, coordinates, log and result of each step. For performance tests also hardware, time and memory; G0 does not yet make speedup claim.

Performed attempt: synthetic map, profile `1098-BlackTek`, OTBM 3, Z=7, mask X/Y 100–119 and halo 99–120. Brush `grass` selected from current materials; ID came from `lookId` of loaded brush. Fixture contains only ground, without additional floors and complex metadata.

## Protocol G0 — minimal smoke test

**Noise — partial PASS, 28.10 07:19:18:** in Script Manager of actual `build-ninja/rme.exe` run without open map [`g0_noise_probe.lua`](smoke/g0_noise_probe.lua). For 400 points simplex grid with seed 1729 and frequency 0.037 matched exactly with single calls and repeated grid; value range was checked and fact that seed 1730 changed at least one sample. Log showed `G0 NOISE PASS`. This is proof only for this build and these parameters, without comparison between platforms, benchmark, brush RNG or determinism of generator as whole.

**Algo/geo — partial PASS, 28.10 07:20:54:** [`g0_algo_geo_probe.lua`](smoke/g0_algo_geo_probe.lua) was run in same RME without map. For 20x20 cave and two iterations, seed 1729 repetition was checked, difference from 1730, values 0/1 and edge walls; Bresenham line (2,3)–(6,5) had both ends and adjacent successive points. Log showed `G0 ALGO GEO PASS`. Test does not evaluate cave quality, passability or all algorithms. In code `algo.generateCave` initializes default seed via `random_device` before reading explicit seed; `geo.bezierCurve` and `algo.voronoi` collect points by iterating Lua table, without explicit key ordering. Do not treat these paths as stable order contract or complete determinism without further tests.

| ID | Action | PASS Criterion |
|---|---|---|
| G0-01 | Run Windows x64 build and then program | Exit code 0, new EXE from this build exists; GUI opens without error |
| G0-02 | Load selected client and create synthetic map | No unresolved ID/missing assets in tested range, saved format and client version |
| G0-03 | Save state S0 and prepare empty 20x20 area on Z=7 with halo | Exact 400 field mask; known state of surroundings and metadata |
| G0-04 | Run minimal Lua edit in `app.transaction` on copy, using ID verified for client | 400 intended changes, no changes outside declared range; record actual call path |
| G0-05 | Undo once, then Redo once | Undo restores S0, Redo restores S1 with item stacks and metadata, does not restart randomization |
| G0-06 | Before and after transaction perform separate manual edit via `DrawOperations` | History separates manual edits and transaction; single Undo does not remove adjacent edit |
| G0-07 | In separate attempt call `error()` after several transaction callback mutations | Full rollback, no partial result and history entry, error reported; test particularly important for Sol2 with safety enabled |
| G0-08 | Save S1 with standard workflow, close and reopen copy | Compatibility of canonical map content and complete auxiliary file set, no loss of UID/AID, houses/spawns/zones covered by fixture |
| G0-09 | Open result in declared format consumer, if available | Result saved separately for original RME/OTS; absence of consumer means unverified compatibility with it |

Minimal Lua edit is test of existing API, not consent to skip `DrawOperations` in production generator handler. Do not apply without checking `terrain_generator_demo.lua` demonstrator, because it has rigid IDs and does not control all RNG sources.

Canonical comparison includes coordinates and tile presence (absent vs empty), ground, ordered item stack, attributes, container content, creature/spawn, flags, house ID, zones and related metadata. Comparison of only minimap, `Tile::isContentEqual` or file size is not complete oracle. Do not declare PASS round-trip until such range was compared and described.

<a id="testy-do-dodania-wraz-z-g1"></a>

## Tests to add with G1

| Layer | Required cases |
|---|---|
| Core without GUI | Fixed seeds, stable order, incorrect sizes/Z, overflows, memory budget, irregular mask, protection |
| Resolver/sandbox | Correct ID, specific diff determinism, border at mask edge, read-only halo, footprint crossing protection, no original mutation |
| Commit/history | Input change after Preview, map close, card/asset switch, application error, no history merge, full Undo/Redo, live lock |
| Background tasks | Cancel without map/history changes, reject old result, close event receiver, limit variant count and retry attempts |
| E2E | 100x100 and 500x500, Preview matches Commit, complete round-trip files, metadata and surroundings preserved |

Do not count Python simulation as test of actual C++ bordering or renderer. `tests/test_ground_border_ordering.py` contains model/simulation; for G1 proof in RME code is required.

Existing CMake registers `test_universal_rotator` in CTest; `build_ninja.bat` script builds only `rme`. After configuration completion required target must be separately built and run. Presence of remaining `tests/*.cpp` on disk does not prove their CTest registration. `lua_tests/` scripts require RME context, their README does not replace execution log.

## Gate status

G0 remains **IN PROGRESS**. Callback error catching was fixed; later test of direct ground attribute change passed within range described below, but remaining G0-C/G0-07 cases require verification. G0-06 passed for Draw/Lua/Draw sequence. Partially passed round-trip of two floors, UID/AID, flags, spawn with monster, separate house fixture and one OTBM 5 zone; additionally checked position of sole zone node in saved file. Save As fix passed limited collision attempts described above, but does not prove complete atomicity. Full attributes and external consumer G0-09 not verified. Before G1 complete protocol and adapter decisions.

**Rollback of newly created files — analysis and fix in progress, 29.10:** originally `restoreBackup` ignored delete error of file without prior backup and could delete recovery marker despite remains after failed save. Code was changed to report paths of undeleted files and preserve marker and backups. Incremental build and RME link passed; delete error in runtime not yet reproduced. Marker still contains backup names, not newly created targets: insufficient for their automatic deletion after restart. If OTBM backup available, current recovery procedure may also delete marker after restoring backups, without checking these new targets. Complete first save and Save As protocol requires further fix and error and process interruption tests.

**File rollback operation test, 29.10:** production function `rollbackFile` is called by `EditorPersistence::saveMap`. CTest `test_rollback_file` on Windows x64 checked Unicode path, backup restore, new file deletion and no file, and target lock by `CreateFileW` without delete sharing: delete and restore errors were returned, after handle close both operations passed. CTest PASS, rebuild and RME link PASS. This is test of single file function, not proof of GUI message, marker behavior, complete Save As or recovery after failure.

**First save and marker — additional code analysis, 29.10:** at first save or Save As to new file `backup_otbm` is empty, but `WriteCrashMarker` still writes marker. After save interruption `Application::OnInit` shows recovery dialog only when first marker line is not empty. In such situation marker may remain without dialog, and `EditorPersistence::saveMap` blocks subsequent saves due to its existence. Not reproduced by process interruption. Plan of separate change: explicitly write in versioned protocol complete targets and their state before save, also recognize first save marker, and on failed deletion preserve target information and report error; handle old markers without guessing missing paths. Tests on isolated files should cover interruption before and after each phase, repeated recovery, delete error, marker access absence, incomplete marker and existing and new auxiliary files. Do not delete unknown files and do not treat forced process termination test as proof of persistence after power loss.

**Transaction initialization — partial verification, 28.10 09:05:** after changing initialization order, rme.exe was built and linked; on freshly opened g0-smoke.otbm map with profile 1098-BlackTek test lua_tests/test_transaction_rollback.lua ended with ROLLBACK PASS and TRANSACTION REGRESSION PASS messages. Map not saved. Allocation errors not simulated; nesting test was performed earlier on previous build. Problem of rolling back existing Item attributes remains open.

**Crash recovery gap — code analysis:** in EditorPersistence::saveMap existing files are moved to backups before calling MapBackupService::WriteCrashMarker. Process failure between these operations can leave files under backup names without recovery marker; moreover WriteCrashMarker does not check open or write errors. This is not failure test nor proof of file loss: safe write protocol and interruption test required before treating crash recovery as verified.

**Nested transactions — re-regression, 28.10 09:09:** on current build after initialization change, freshly opened g0-smoke.otbm map and profile 1098-BlackTek; lua_tests/test_transaction_nested.lua reported NESTED TRANSACTION PASS. Map not saved. Scope does not include Item attribute modification or allocation failure.

**Backup recovery — change in progress:** startup path was fixed to not delete target if backup file does not exist, and preserve marker on refusal or incomplete restore. rme.exe was built and linked after last change. Recovery runtime test and failure after partial restore not yet performed; code remains unverified. Marker is still created only after backup move, so crash recovery gap remains open.

**Recovery — isolated file test, 28.10:** after build with completeness check at start, synthetic marker was applied in separate data directory. Missing XML backup stopped recovery before changing existing OTBM and its available backup; XML and marker remained unchanged. After providing missing backup, rerun restored both files and deleted marker. OTBM was intentionally text file, so expected attempt to open it reported invalid format. Real process failure, error after move start, or marker for new map not tested. Changes remain unverified; in particular gap before marker creation still exists and risk of partial restore after later file operation error.

**Recovery with file lock, 28.10:** after build with backup copy, isolated fixture with two synthetic files was run, blocking target XML through FileShare.None. OTBM was copied, XML reported error, both backups and marker remained. After lock release and rerun, both files contained original data, and backups and marker were deleted. This is not test of real map or power interruption. Added lock of next write with existing marker not yet checked in GUI. Gap before marker creation remains open.

**Write lock at marker — PASS, 28.10:** in isolated configuration synthetic marker was left, recovery was rejected at start, opened g0-smoke.otbm fixture with profile 1098-BlackTek and selected Ctrl+S. Editor showed Save failed with marker path, whose content remained unchanged. SHA-256 of OTBM after test: 2e0728b71bc5506db21a22be8e29920c17d418f599a7fcbf627536eb45f728ce, matching earlier measurement. Concurrent instances, marker access errors or corrupted marker not checked.

**Control Save As without marker — PASS, 28.10:** after removing only synthetic test marker, opened map was saved as guard-control.otbm in isolated directory. GUI reported save, OTBM (3361 B), house XML and spawn XML (33 B each) were created; marker did not remain. This is not complete re-read test or test of existing target files.

**Guard-control re-read — PASS, 28.10 09:37:** copy from control Save As was opened via Welcome with explicit profile 1098-BlackTek. Header indicated guard-control-house.xml and guard-control-spawn.xml, GUI loaded 400 fields. Local variant of g0_verify_saved.lua differed in required file name and passed: 400 correct ground IDs, empty stacks, creatures, spawns, house IDs, flags and halo. First attempt did not load script saved with BOM; after UTF-8 save without BOM passed. XML files were empty, so test does not prove round-trip of complex metadata.

**Backup as copy before marker — partial verification, 28.10:** after changes build and link passed. Ctrl+S of existing guard-control.otbm in isolated directory succeeded without leaving marker or temporary files. Then sentinel guard-control-house.xml~ was placed; write reported second backup collision, original OTBM and both XML remained, sentinel preserved content, and earlier created OTBM copy was deleted. Copy failure, marker write, or serialization not simulated; persistence at power failure, complete new map handling or concurrent write safety not shown. Code remains in progress.

**Serialization error and recovery, 28.10:** in isolated guard-control.otbm house XML was opened with FileShare.Read, enabling copy but blocking write and XML restore. Ctrl+S reported incomplete rollback; marker and backups of OTBM, house and spawn remained, and SHA-256 of each of three backups and files matched state before attempt. After lock release and restart, recovery selection restored same hashes, deleted backups and marker. Auto map open failed, standard profile 10.98 chosen instead of 1098-BlackTek; test confirms byte recovery, not auto open. Power loss or marker write error not simulated.

**New files in marker — change in progress, 29.10:** after seven previous backup paths marker also writes potentially new write targets. On startup editor displays their list and does not delete marker automatically, even after recovering existing backups. Empty or incomplete marker without paths also triggers warning. GUI test of this variant and real failure test not yet performed; lack of atomic path reservation and persistence after power failure remain limitation.

**First save marker — partial PASS, 30.10:** in separate profile `g0-firstsave-marker-final-20261030` synthetic marker was prepared with seven empty backup lines and two new targets. On startup of current build `Incomplete save` dialog indicated both paths and marker preservation. After closing program marker text and content of both artificial files matched fixture. In first check byte comparison of marker with LF text did not pass due to CRLF write by Windows; comparison with line ending normalization passed. Visible paths in native dialog were abbreviated with ellipsis, though accessibility tree contained them fully. Real write interruption, next write lock check in this variant and new file deletion not simulated; preserved in isolated fixture.

**Path readability — PASS for fixture:** after changing `Incomplete save` to text field and rebuilding RME, rerun showed both full paths, wrapped without ellipsis. Ctrl+A selected content; clipboard copy not performed. Test covers two short targets and does not prove handling arbitrary path count or real recovery after failure.

**Next write lock — PASS for first save marker:** in same separate profile copy of `guard-control.otbm` was opened with profile `1098-BlackTek` and 400 fields. Ctrl+S displayed `Save failed`; SHA-256 of OTBM, house XML, spawn XML, marker and two synthetic targets remained unchanged. After changing lock message to text field, build and retry, full marker path was visible, and same six hashes still matched. Test does not cover concurrent instances or real write interruption.

**Nested container — partial PASS:** [`g0_container_fixture.py`](smoke/g0_container_fixture.py) created on isolated OTBM 3 copy field (100,100,7) with nodes bag 1987 → bag 1987 → 7 coins 2148 with AID 12042; DAT 10.98 identifies 1987 as container, test profile uses `dat_only`. RME with `1098-BlackTek` opened map (402 fields), saved, after restart reopened and saved again; independent script confirmed structure and values after both saves. First check version required identical attribute order and reported FAIL: RME save reordered count and AID. After comparing values independent of order check passed. Input file preserved as `before-save.otbm`. Not complete comparison of all fields, metadata, many contents or external consumer test.

**Fixture environment comparison — partial PASS, 30.10:** [`g0_compare_roundtrip.py`](smoke/g0_compare_roundtrip.py) compared input OTBM and result after second save: all 402 positions present, 401 remaining field nodes identical, test field header and container content semantics preserved. House XML and spawn XML trees equal after whitespace omission. Result does not close G0-08.

**Comparison extension — PASS for this fixture:** same script also compared file header and all remaining OTBM nodes except field area content; they are identical. [`test_g0_compare_roundtrip.py`](smoke/test_g0_compare_roundtrip.py) executed five trials on temporary copies: accepted correct result and separately rejected changed OTBM metadata, different field, container content and house XML. This is not proof of correctness of attributes absent in fixture or reading by server.

**References between maps — partial PASS, 29–30.10:** `lua_tests/test_reference_map_switch.lua` on isolated map `g0-metadata.otbm` with profile `1098-BlackTek` remembered field and item references. On second empty map both accesses rejected, after return to first map read original values (`REFERENCE SWITCH OTHER MAP PASS`, `REFERENCE SWITCH RETURN PASS`). Then repeated first stage, closed source map and on remaining map got `REFERENCE SWITCH OTHER MAP PASS`. Next run expected return to closed map and per script construction reported error; not test of reopen. Maps not saved. Containers and address reuse not checked.

**Lua field reference — partial PASS, 29.10 19:19:** Windows x64 build with `TileReference` binding run on isolated copy `g0-saveas-backup-20261028/g0-metadata.otbm` with profile `1098-BlackTek` (402 fields). Script `lua_tests/test_transaction_tile_reference.lua` changed field flag (160,160,7), forced callback error, checked restored flag and empty history and expected error of old reference use. Then checked commit, Undo, stale reference error and Redo; final Undo restored flag. Console: `TILE REFERENCE PASS: rollback, undo and redo`. Maps not saved. Result concerns this scenario; remaining reference attempts described separately, G0-07 still open.

**Lua ground attributes — partial PASS, 29.10 19:26–19:28:** after rewiring bindings to `ItemReference` and Windows x64 build run on same isolated copy `test_transaction_item_undo.lua`. Console reported `ITEM TRANSACTION PASS: rollback, one history entry, undo and redo`: test checked rollback AID, UID, tier, text and description after error, old reference, commit AID/UID, Undo/Redo and clone outside map. Then `test_item_implicit_undo.lua` changed AID without explicit transaction and checked Undo/Redo and final Undo; result `IMPLICIT ITEM UNDO PASS`. Maps not saved. Not yet checked other setters, stacked items, nested containers, moving, map switching, setter error or API completeness; not full PASS of G0-C/G0-07.

**Item on stack — partial PASS, 29.10 19:29–19:30:** on same copy `test_item_stack_lifetime.lua` created item 2148 on field (100,100,7), set AID, deleted it in transaction ended with error and checked rollback and invalidation of previous reference. Then moved it to (101,100,7), checked staleness of old reference and state after Undo/Redo, then reverted both actions. Retry also covered UID and stack element count. Console twice: `ITEM STACK LIFETIME PASS`; maps not saved. Profile 10.98 DAT identifies client ID 2148 as stackable and non-ground; script additionally checks ID properties in loaded profile. Containers, further setters and map switching still need tests.

**Iterators and selection — partial PASS, 29.10 19:34:** after build with item owner check `test_reference_api_compatibility.lua` on same map checked `nil` for empty position, 402 fields with `map.tiles`, position and AID of fixture field and at least one spawn with `map.spawns`. Also checked that adding and removing from selection item from another field rejected without selection change, and correct field/item pair can be added, read through `selection.tiles` and removed. Console twice: `REFERENCE API COMPATIBILITY PASS`. Not checked all methods or behavior after map switch; maps not saved.
