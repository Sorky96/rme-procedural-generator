# G0 manual scripts

The `--resources` dialog runner and verifier measure the baseline process and GUI pause behavior with three Preview/Validate/Cancel cycles at 100×100 and 500×500. Cancel is sent after a result is ready, so this is not cancellation during generation. See [G1_RESOURCES.md](../G1_RESOURCES.md). Do not combine it with `--matrix` or `--selection-only`.

`--selection-only` runs only the sparse-selection UI regression. `--matrix` runs `test_procedural_dialog_matrix.cpp` with 500×500 UI, sparse selection and refusal cases. It requires real UI input and a sufficient private Undo limit. See [G1_DIALOG_UX.md](../G1_DIALOG_UX.md) and [G1_DIALOG_MATRIX.md](../G1_DIALOG_MATRIX.md).

`g1_run_dialog_probe.py` and `test_procedural_dialog.cpp` observe the real native dialog. `g1_verify_dialog_probe.py` checks binary, object and source freshness plus curated assets. See [G1_DIALOG.md](../G1_DIALOG.md).

Normal GUI without a test module is run with `g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --house-gui`. It copies the canonical EXE to a private directory and waits for manually observed Add/Undo/Redo/Undo and normal close. `--house-gui-case close-fixture` checks fixture open/close/reopen, the correct tab and palette detachment. Record observations in `run/house-ui-observation.json`. A timeout leaves the PID for investigation. Results and evidence limits are in [TEST_PLAN.md](../TEST_PLAN.md).

These are diagnostic scripts for the existing RME Lua API, created during the 26–27 October 2026 audit. They are not a generator implementation or an automated CTest suite. Copy them into `scripts/` beside the test EXE and run them manually from Script Manager on a synthetic fixture. Do not enable autorun.

- `g0_smoke.lua` places 400 ground tiles and checks manual Undo and Redo.
- `g0_verify_saved.lua` performs a read-only check of `g0-smoke.otbm`.
- `g0_error_probe.lua` intentionally fails after mutating (140,140,7). The old build reproduces rollback FAIL. Run one Undo and the verifier after that failure. Do not save diagnostic changes.
- `g0_manual_history.lua` checks three separate Undo/Redo actions for manual Draw and Lua transactions.
- `g0_metadata_roundtrip.lua` checks UID/AID, flags and spawn after save/reopen. Copy the complete OTBM fixture and XML files to a separate directory first.
- `g0_item_attribute_rollback.lua` checks an AID transaction on isolated `g0-metadata.otbm`. The audited build reports rollback FAIL and the script restores the AID before its final assertion.
- `g0_noise_probe.lua` compares 400 simplex-noise samples without changing the map.
- `g0_algo_geo_probe.lua` checks cave, edge-wall and Bresenham repeatability without changing the map.
- `g0_verify_zone_fixture.py` checks 402 fields and zone ID 1 at (104,104,7) in the OTBM5 fixture. Run without `python -O`.
- `lua_tests/test_reference_map_reopen.lua` checks two calls before and after reopening an isolated map in the same Lua session. Do not reload scripts between stages.
- `lua_tests/test_item_setter_matrix.lua` checks 17 setter cases and 10 invalid values with rollback and Undo/Redo. Do not save.

Assets and binary maps are not in the repository. Fixture names and field counts reduce accidental execution but do not replace an operator check of the active map.

## Save and history probes

`g0_build_rme_snapshot.py`, `g0_rme_snapshot_probe.cpp`, `g0_run_rme_snapshot.py` and `g0_verify_snapshot.py` build a private EXE from existing RME objects, run real snapshots and Save As with BOTH waypoints, and compare the complete fixture. A timeout leaves the PID for investigation. Do not rerun on the same files. `_Exit` skips normal shutdown after local editors are destroyed.

`g0_rme_house_history_probe.cpp` is a separately linked `--house-history` module. It checks conflict/generation, the add/edit/remove Undo/Redo chain, history cleanup and metadata refusal in live state, then compares a restored-house snapshot with an independent parser. It does not test GUI buttons or peer connections.

`g0_rme_first_save_probe.cpp` is a `--first-save` module. It checks marker refusal, first-save failure to a missing directory and later successful OTBM/XML reading. The failure dialog must be closed manually. It does not simulate a process crash.

`--first-save --rollback-removal` checks removal failure and house-backup restoration. Only its own `run/rollback-denied` receives temporary DELETE/DELETE_CHILD refusals. `--first-save --nested-companions` checks nested XML and path containment. Both modes require a new OUTPUT/run and must not be combined.

`g0_verify_nested_companions.py OUTPUT FIXTURE` verifies a completed nested child without restart. It requires matching manifest/PID/output, runtime markers and unchanged inputs. Default `canonical` rejects paths other than the basename. Do not use it to turn a failed child into PASS.

`g0_rme_save_cancel_probe.cpp` and `--save-cancel` send `wxEVT_BUTTON(wxID_CANCEL)` through the native Saving dialog and check new/existing targets, map state, bytes, marker cleanup, cancellation reset and the next successful save. It is not a mouse-click or latency test.

`g0_interrupt_first_save.py OUTPUT --rollback-removal` operates only on a previously observed private PID and complete first-save marker. It records hashes and removes only its own ACL refusal. The standard runner should report 18177. Existing restart and verify scripts continue the check.

The `--marker-startup invalid|legacy|versioned` modes test startup with private targets and backups. Record observed dialogs in `startup-ui-observation.json`. A timeout must not be retried. `--verify-existing-startup` only verifies a completed case and requires `observed-exit.json`.

`g0_interrupt_first_save.py`, `g0_restart_interrupted.py` and `g0_verify_interrupted_restart.py` interrupt only a verified private PID, restart the normal EXE and check the next-save lock. They are not general process terminators and do not test shutdown.

## Lua item methods and allocation probes

`g0_rme_item_methods_probe.cpp` and `lua_tests/test_item_methods.lua` check rollback and explicit/implicit Undo/Redo for rotation and podium methods. Build with `g0_build_rme_snapshot.py OUTPUT --source docs/procedural/smoke/g0_rme_item_methods_probe.cpp`, then run `g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --item-methods` with a new run. The podium definition is synthetic and does not prove BlackTek compatibility.

`g0_rme_house_allocation_probe.cpp` uses a scalar `operator new` replacement in a private EXE. It checks one controlled refusal while inserting into `Houses`, then retry and duplicate behavior. It must not be added to the normal target, does not exhaust system memory and does not prove complete ActionQueue exception safety.

`g0_rme_lua_allocation_probe.cpp` arms a scalar `operator new` after five setters in a real Lua callback. It checks rollback or a fully undoable commit, OTBM/XML snapshots, the 400-tile smoke, Undo/Redo and first save. It does not prove every mutator, persistent OOM, visual GUI or global shutdown.

## Current BlackTek with Zones

`g0_blacktek_current_iomap_probe.cpp` targets the rebuilt server API (ItemContainer, Zones). It loads approved DAT and synthetic OTBM through IOMap and checks houses, floors, UID/AID, nested items, PZ conversion, legacy spawn XML and production TOML rereading. Global server shutdown is not tested.

Build with `g0_build_blacktek_iomap.py SERVER FRESH_BUILD VCVARS --source docs/procedural/smoke/g0_blacktek_current_iomap_probe.cpp --vcvars-version 14.44.35207`. The fresh non-unity build and matching toolchain must already exist. Building the probe is not a runtime PASS.

Run `g0_blacktek_iomap_cases.py PROBE DEPS_BIN DAT FIXTURE --current-zones --report NEW_REPORT_DIR/iomap-cases.json` with a new report directory. The runner pins fixture hashes, preserves its six directories and compares inputs before and after. The old test mode still uses `TemporaryDirectory` and rejects new files.

Current conversion semantics differ from legacy Spawns. Rat keeps position 160,160,7 and 90 seconds, while the converter creates fixed zone 158…162 with padding 2, without the old radius 3 or direction. Do not report those fields as preserved or equate conversion with monster execution or deployed Docker. Current results and failed builds are in `../HANDOFF_G0_20261102.md` and `../TEST_PLAN.md`.

## House-table presentation diagnostics

`g0_rme_render_observer.cpp` is a private module, never a normal target. Build with `g0_build_rme_snapshot.py OUTPUT --source docs/procedural/smoke/g0_rme_render_observer.cpp`, run `g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --render-observe`, choose House and perform Add → Undo → Redo from the menu → Undo while observing `RENDER_CAPTURE` in `run/render-observer.log`. The module checks its drawable, map and table data, and writes private front/back PNG and GL state. The back buffer after swap is diagnostic only. Positive verification requires viewed images and `render-ui-observation.json`. Close with Alt+F4, not `_Exit`.

For a separate presentation experiment only, create `run/.g0-diagnostic-extra-swap` before Add. This extra swap excludes the run from normal GUI PASS. Do not add it to production based on diagnostic output alone. Archive a source copy matching the manifest hash before changing the module.

## Publishing with existing history

`g0_rme_queue_allocation_probe.cpp` is run with `g0_run_rme_snapshot.py OUTPUT FIXTURE PROFILE_DATA CONFIG --queue-allocation` on a new private run. It checks deque growth, entry-count limit, merge with existing Redo and memory limit by arming allocation failures at batch publication. Refusal must preserve entries, labels, index and memory counter, and execute existing Undo/Redo. The test does not cover every `Action::commit` exception or visual GUI behavior. Preserve stderr and limitations.
