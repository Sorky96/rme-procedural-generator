# G1 — Preview preparation refusals

2026-11-04. **PARTIAL_G1_GUI_PREVIEW_REFUSALS_FOCUS_WARNING**. All functional cases passed. The process ended with code 0, but stderr contained one wxWidgets `SetFocus` message with code `0x00000057`. The triggering stage was not identified.

Native-dialog test on a synthetic map with a private 1098-BlackTek profile. Each Preview invokes the operator. The observer changes only fixture state in this RME instance.

| Attempt | Test state | Checked message |
|---|---|---|
| 1 | Current selection contains one tile on each of floors 7 and 8 | The selection spans several floors |
| 2 | Empty rectangle with an open internal selection session | Finish the current selection or edit first |
| 3 | Empty rectangle in live mode on a host that is not running | Terrain generation is not available during a live session |
| 4 | One-time halo-vector allocation failure for a 100×100 rectangle | Not enough memory for this preview |
| 5 | Client unloaded for the same editor | No client is loaded |
| 6 | Client reloaded and default rectangle | Preview ready for 10,000 tiles |

The observer checks both field contents, selection size, revision, no dirty flag, and empty history. After each refusal it requires Validate and Place to be disabled. The test host receives a live-mode object and queue without listening, connecting, or synchronizing. Allocation failure is injected only in the observer binary on the GUI thread.

![RME refuses preview preparation for a selection on two floors](screenshots/g1-refusals-multifloor-20261104.jpg)

## Evidence and scope

`g1-refusals-aligned-rme-20261104` contains six ordered `REFUSAL_CASE_PASS` markers and final `REFUSAL_PASS`. Input assets and sources retained their hashes. The verifier confirmed 421 objects, 421 compilation inputs, and both EXEs. The runner retained a negative result for the empty-stderr criterion. The verifier explicitly marked the documented run as partial.

Canonical `build_ninja.bat` passed. Ninja reported `no work to do`, and the production EXE SHA-256 matches the cancellation run. The first observer build rejected an invalid `createTile` signature. A later build was rejected after the source changed during linking while allocation size was aligned with MSVC vector overhead. Both runs were preserved without passing.

Scope covers Preview refusals and returning to a ready preview. Real network sessions, other allocation-failure sites, repeated Place/Undo/Redo, and generator OTBM save/reopen with companions remain separate. Full **G1 IN PROGRESS**.

## Odtworzenie

```text
rtk proxy <PY> docs/procedural/smoke/g1_run_dialog_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --refusals
rtk proxy <PY> docs/procedural/smoke/g1_verify_dialog_probe.py local-data/procedural-smoke/NEW_RUN --refusals
```

`<PY>` is `C:/Users/Sorky/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -X utf8`. Do not click the map area, which ends the probe with `Source changed`. Open Map → Generate terrain and select Current selection. Press Preview, wait for `REFUSAL_CASE_PASS` for case 0, then return to Empty rectangle. Run five more Previews in the same dialog, waiting for each case marker. Close the dialog with Cancel after the preview is ready.

## Repeat on the 2026-11-06 tree

Run `NEW_RUN_REFUSALS_2` on the current tree produced six ordered `REFUSAL_CASE_PASS` markers 0–5 and final `REFUSAL_PASS`, exit 0. The verifier returned `PARTIAL_G1_GUI_PREVIEW_REFUSALS_FOCUS_WARNING`: the same 421 objects and 421 sources, with only one wxWidgets `SetFocus` message with code `0x00000057` on stderr. The runner retained `passed: false` because stderr was non-empty, as in the 2026-11-04 run.

Two earlier runs that day failed with `FAILED Source changed: map generation` because the operator clicked the map. The observer requires unchanged map generation and exits through `_Exit(8)` without a window message. The post-click generation change was not checked in code. The conclusion is based on log agreement, two marked chunks, two fixture tiles, and the run without map clicks passing. The observer error now reports initial/current generation and phase. The operator clicks only menus and the dialog.

The `SetFocus` warning in this run is explained by the [2026-11-06 diagnosis](G1_FOCUS_TRACE.md#window-state-at-the-error-in-the-refusal-run): it comes from the probe step that loads the client while the frame is disabled by a modal dialog.
