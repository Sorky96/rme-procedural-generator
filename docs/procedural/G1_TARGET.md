# G1: current editor and offline target

## Newer stage: commit and history

Atomic commit/Undo/Redo core and actual membership publication subsequently implemented and verified in [G1_COMMIT.md](G1_COMMIT.md). Report below preserves historical scope of target guard alone; full G1 gate remains IN PROGRESS.

Status 2026-11-03: **PASS_G1_CURRENT_OFFLINE_TARGET_ONLY; full G1 IN PROGRESS**. Section complements [asset catalog](G1_ASSETS.md), [result validation](G1_VALIDATION.md), and [exact mask](G1_MASK.md). This stage composes checks needed for future commit entry.

## Contract

`validateTerrainTarget` and `validateSelectedTerrainTarget` in `source/procedural/adapter/terrain_target.{h,cpp}` get current editor via `g_gui.GetCurrentEditor()` on each call. Take neither store old Editor/Map/MapTab pointers. Identity is existing `snapshot.input.mapGeneration`, captured during preparation.

Shared preflight refuses in order wrong thread (before GUI access), stop, no editor, different map generation, any live session, and busy/deferred selection. Uses existing getters and does not change tab focus. Rectangle variant then calls `validatePreparedTerrain` with current config/profile/mask. GUI membership not by default from it; ongoing selection session blocked for both variants though.

Selection variant additionally checks `validateGenerationSelection` on this current editor, requires same region/Z in config, and passes exact mask to result validation. `TargetValidationError` keeps separate target, membership, and preparation error types; does not substitute them into generic false. Preflight shared so does not re-execute full asset/snapshot scan.

Calls synchronous on main thread, no yield, allocate results, map changes, RNG, cache, directories, or history. Normal path of correctly registered tab also does not allocate when finding current editor. Internal tabbook lookup uses existing `conv` entry; any notebook registry corruption remains outside contract.

## Lifetime and shared views

Success returns borrowed `Editor*` for immediate continuation in same GUI call. Grants no retained right for later mutation. After yield, new event, or result storage must fetch and check target again. Future commit must enter via this check directly before own atomic operation.

Identity unit is map/editor. Second view of same Editor may pass, also after first view closes. Switch to different map refuses; return to unchanged source may pass. After closing last view and creating new map generation is different, regardless of allocator address reuse. Old source pointer not read.

`IsLive` check covers both client and server, also before connection made. Adapter only reads this state. Does not create session or socket, and does not connect, bind, accept, start, or stop network.

## Remaining limits

Still need atomic apply/rollback/Undo/Redo, proper selection membership publication for empty fields, independent manual drawing history, Undo memory budget, and reentrancy/active Lua transaction or history check. Guard itself does not mutate map, so does not resolve these obligations. Remain minimal dialog/rendered preview, responsive GUI Cancel/schedule, and GUI/OTBM round-trip 100×100/500×500. Current RSS, raw pixels/GPU/cache, and out-of-scope catalog exclusions still apply.

## Claude and review

Claude implemented only two new files, run `1ab3e529-7bf2-4610-81b3-f3031eec2bb4`; documentation fix `ead98698-19d3-4c0e-929b-b2fe4ce034dc`. Both completed/exit 0, empty stderr, no refusals, aligned report/stdout, and unchanged tracked diff. Codex reviewed code, actual GUI lookup path, and live session construction/destruction. Fixed too-broad statement about skipping selection: rectangle membership skipped but busy/deferred always checked. Worker did not build or test.

Accepted files copied byte-for-byte, manifest `local-data/procedural-smoke/g1-target-integration-20261103.json`. Codex performed CMake integration, testing, and independent validation. Shared changes kept; no commit/push. forgotten.otbm/companions, user maps, Docker, database, or world not used; G2 not started.

## Independent results

Canonical Windows x64 `build_ninja.bat` exit 0: `local-data/procedural-smoke/g1-target-canonical-20261103.log`. Normal EXE SHA-256 `8282a1c75a88bd1d2553bccb2cb553bb5d763a24c599dc1e23f9d0d170ceeb62`. Full new normal Application with test wxModule in `g1-target-rme-20261103`: **PASS/exit 0/empty stderr/normal shutdown/input assets unchanged**. Probe EXE `e1a427c1cc42b3e209606263c6683fb465c7ebda8e15eff468bd31982acce85b`.

Verifier `g1_verify_preview_probe.py --target` confirmed 420 current RME objects, 877 G0 database sources, and ten expected G1 differences. Full evidence in `verification.json`, `current-verification.json`, build-manifest, and probe log. Earlier focused runs border-order/snapshot keep current proper sources/objects but are historical full EXE before target adapter added; not named new runs.

Current editor, rectangles from (400,100,7):

| Size | Guard allocations | Concrete preview fingerprint |
|---|---:|---|
| 100×100 | 0 | 10815617532789726571 |
| 500×500 | 0 | 14888103173838574161 |

Both calls returned exactly current Editor*, kept snapshot, revision, tile count, history size/position, RNG, and cache of observed editors. Call selected range 500×500 and separate case 4×4 with five actual empty members also executed zero allocations. Selected range 100×100 tested earlier as capture/inactive map preparation; that result not presented as active target guard success.

**22 refusals** include different current map, both variants in busy/deferred selection, pre-cancel and worker, changed config/revision propagation, mask mismatched with result, selection region/mapGeneration, member swap at constant bounds/count, and later stale snapshot despite membership restore. After source close both variants return NoEditor; after new map create both WrongMap. Old Editor pointer not stored. Specific allocator address reuse not forced.

Actual additional view of same map accepted, also after original view closes. Switch to different map refused, return to unchanged source passed. Separate unconnected LiveClient and unrelated-port LiveServer gave LiveSession in both variants. Construction of these fixtures does not invoke connect/resolve/bind/accept or NetworkConnection::start; cleanup closes only inactive objects of own process. Return of server object to local state again allows matching result. Not test of connected network session.

Full probe repeated: four 100×100/500×500 sandboxes, 79 sandbox faults, 106 preparation and exact limits, 29 validation refusals, 39 catalog mutations, masks with holes, epoch/unload/reload, selected-context/selected-border, cache/events of two editors, and normal selection/Lua/UndoRedo. Earlier fingerprints, estimates, and new peaks remain consistent. Run has manually handled informational startup Warnings of private assets via computer-use in same instance without restart; not unattended or visual renderer-preview acceptance. At exit no private windows. `git diff --check` PASS.

Not covered: generator commit, reentrancy/active Lua transaction or history exclusion, its Undo/Redo/membership, GUI Commit button, responsive Cancel, sustained OOM/RSS, or GUI/OTBM round-trip. Existing GUI/Editor/LiveManager classes and Lua behavior not changed.
