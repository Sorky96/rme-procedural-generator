# G1 — atomic commit of concrete preview and history

Update 2026-11-03: separate [G1_DIALOG.md](G1_DIALOG.md) confirms current GUI 100×100 with minimap and Place/Cancel/Undo/Redo. The report below preserves historical core scope; full G1 remains IN PROGRESS.

Status 2026-11-03: **PASS_G1_ATOMIC_TERRAIN_HISTORY_CORE_ONLY**. Full **G1 IN PROGRESS**; pass covers commit/history core, without generator GUI workflow.

## Contract

`DrawOperations::commitTerrainPreview` and `commitSelectedTerrainPreview` recheck current editor/offline, map, snapshot payload, assets, configuration, and respectively exact membership mask. They copy concrete tiles of their own preview: ID, subtype, item order, flags, and minimapColor remain identical. Commit/Undo/Redo do not invoke brush, RNG, regeneration, or autoborder.

The result is confirmation of action acceptance to history. Lua callback run after publication may perform next edit or Undo; receipt is not permanent confirmation of current map content. All receipt data arises before callbacks.

`ACTION_GENERATE_TERRAIN` creates a separate entry even with `GROUP_ACTIONS=1`, between manual actions and subsequent generations. The normal `addAction/addBatch` path does not accept this type. The generator uses existing `Action`, `Change`, `BatchAction`, and `ActionQueue`, with a separate path for prepared swap; it does not add a second history.

## Atomicity boundary

Before the first tile change, result copies, Change/batch, map infrastructure reservation, minimap cache entry, possible Selection capacity, and place in history deque are created. Only then come allocation-free swaps of tile owners and membership pointers and history publication. Old tiles go directly to Change, so Undo preserves original identity and full data. Redo is not removed nor old history before the last possible refusal/allocation.

`BaseMap::TileReservation` on refusal removes only cells/nodes/floors it created and restores chunk masks. It preserves existing TileLocation and metadata. Unused grid vector capacity may remain; grid content, tile count, revision, dirty flag, selection, and history must remain unchanged. `swapReservedTile` does not update entity indices: the path refuses tiles with house, spawn, creature, mapflags/extra, and location counts/exits. It is not a general transaction of arbitrary entities.

Undo/Redo reserve needed resources before changing history index. Minimap allocation or selection collection error returns refusal without partial change. Live, active Lua transaction, and busy/deferred selection also refuse. During history traverse, normal Selection sessions and Lua transactions cannot start. Dirty tracker/minimap/GUI observers are notified after publication; observer exception does not roll back accepted action. Lua `actionChange` and membership changes emit after history lock release.

## Selection and budget

Membership is independent of tile flags and items. Change exchanges actual membership state in both directions, also for empty tiles without selected bit and for restoration after null-source Undo. This allows restoring selection after later change without history entry. The general Action path removes swapped pointers without relying on selected bit and remembers missing flag restoration on normal Undo; this includes later Selection action after generation.

`TerrainCommitBudget` is fail-closed. Single uint64 counter covers still-held preparation and conservative commit/Undo cost: new and swapped tiles, infrastructure, Change/Action/batch, history, scratch, and Selection reserve. Public numbers from `PreparedTerrain::memory` do not reduce the fee: re-estimation of containers/payload is performed and private peak preview is used; larger fee remains, no refund of earlier stages. New batch refuses over UINT32_MAX because old BatchAction cache is uint32. The generator also respects Undo count and memory settings without changing normal action policy.

Selection reserve is `3 * sizeof(Tile*) * (S0 + change count) + 32`. Before future Undo/Redo, reservation may refuse if current collection would require larger new allocation; with existing sufficient capacity swap does not allocate. Refusal preserves map and history; after reducing selection, retry is possible. These are policy estimates, not hard allocator limit or RSS. Other history, source map/assets, other tasks, GUI/render/observers, and C/DLL/GPU allocation remain outside the limit.

## Claude and independent verification

Codex designed and integrated commit path and tests. Claude implemented only `source/map/basemap.h/.cpp` in dedicated worktree; accepted checked diff and byte-identical integration manifest. Implementation run: `f1a70830-50b7-4919-b699-fc2e39353beb`. Reviews: `f9c68d5b-ef2d-4fed-b323-3bf49f91ca77`, `46601207-d478-4391-aaa3-1a175876b5ee`, `1c06e930-0bca-4366-a11c-c2ebfd40000b`, `f9d6ea26-ebe7-4a57-8243-ce26b04b041e`. Reports completed/exit 0, no permission denials, empty stderr; read-only runs did not change checkout. Notes about callbacks, minimap preflight, Redo membership, and equal number of changed members were addressed. Codex performed and independently checked the last minor change of notification on equal swap and general history refusal message after last review; these are not declared as byte-identical Claude review. Code confirms 32 B allowance agrees with kSnapshotAllocationOverhead and brush names are inline std::array, not separate heap vector. SelectionThread is started after main-selection.start and joined before finish, with no yield; no arbitrary concurrent queue use forced. Review does not replace running tests.

Final Windows x64 build exit 0: `local-data/procedural-smoke/g1-commit-complete-canonical-20261103.log`.

Full run `g1-commit-complete-rme-20261103`: PASS/exit 0/normal shutdown/empty stderr/input assets unchanged. `current-verification.json` confirms 421 current RME objects and 877 base G0 sources with exactly 22 expected G1 differences. Current sources and EXE hashes also compared; existing unrelated whitespace in shared G0 was not cleaned.

Normal EXE SHA256: `8f6cb69762f2df0641a3f0e150329cb85197648dd8b1fe3f4677a4dad7b6abc1`. Probe SHA256: `0cdbe48cfc71479ac8416434790e919fc5ef30c3f247efd41998ddc6695fccd2`.

| Size | Transition/Undo estimate B | Peak tracked new B | Allocations before publication | Concrete result fingerprint |
|---|---:|---:|---:|---:|
| 100×100 | 8881480 | 3965894 | 80195 | 6608263949644166501 |
| 500×500 | 218763816 | 98785798 | 1987447 | 173161415496114986 |

**112 forced commit ordinary new/new[] errors** passed, 15 reservation attempts, exact limit and one byte too few, mask with hole, two separate generations and manual drawing before/after, active Lua transaction, busy/live/thread/stop, changed membership, spoofed public accounting, and overflow. Actual minimap cache flush before Undo/Redo fault, actual Selection expansion fault, its budget refusal and retry, flagged/unflagged null-source Redo, later normal Selection Undo, equal-count member swap with Lua selectionChange and callback actionChange creating separate action were checked. Existing full probe 100/500, 79 sandbox faults, 106 preparation, 29 prepared refusals, and 39 asset catalog mutations also PASS.

New measurements include test scalar/array C++ allocator and are not RSS, C/DLL/aligned/GPU, or performance guarantee. Zero allocation shown in prepared swap/publication itself to first post-publication observer with available cache entry; needed preflight allocation is separately tested and may refuse. Post-acceptance observers do not fall in forced rollback scope.

Number 15 means reservation attempts on successive ordinal allocations, not count of confirmed distinct `bad_alloc`: retained grid capacity may reduce allocation count on retry. Number 112 commit concerns actually observed forced refusals.

Regression of normal queue: `g1-commit-complete-queue-20261103`, fresh link with same normal EXE, result `PASS`, cases `35`. Comparisons of private OTBM/XML snapshots and existing Redo/Undo performed by unchanged G0 probe. GUI warnings: `35`; eight deliberately injected GUI notification faults at full, rollback-safe commit also saved. This historical harness is not pure visual gate or full G0 rerun. Its limitations were not moved to clean stderr of main G1 trial.

Informational startup warning of curated assets was closed manually via computer-use in same instance. Assets not edited. Replay command: `rtk proxy <PY> docs/procedural/smoke/g1_run_resolver_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --commit --visible`, then `rtk proxy <PY> docs/procedural/smoke/g1_verify_preview_probe.py local-data/procedural-smoke/NEW_RUN --commit`. `<PY>` = `C:/Users/Sorky/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -X utf8`.

Earlier runs `g1-commit-rme-20261103`, `g1-commit-final-rme-20261103`, and `g1-commit-membership-rme-20261103` kept for history; last runner exceeded 180 s waiting for startup warning, and its later PREVIEW_PASS does not receive current/full pass. This process was not restarted; same instance allowed to finish normally. These runs do not confirm current EXE after latest fixes. First compile error of new module also kept (`g1-commit-canonical-20261103.log`); removed const/mutable Tile* list deduction problem is not skipped test.

## Still outside the pass

Missing: GUI Commit/Cancel, rendered preview, responsive schedule, and final GUI/OTBM round-trip of generator. Sustained/system OOM, arbitrary concurrent mutations, or asset owner corruption, and all-platform agreement were not forced. Normal queue receives no general atomicity guarantee of arbitrary existing Action. Handling informational startup warning of private assets is not visual acceptance of generator. User maps, forbidden map/companions, database, or Docker not opened; no commit/push. G2 not started.
