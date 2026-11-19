# G1 — SetFocus warning diagnostics

2026-11-04. In run `g1-focus-chain-rme-20261104` two call stacks recorded for Win32 error `0x00000057` on main thread.

| Event | Path to error call |
|---|---|
| Window activation with generator open | `MainMenuBar::OnProceduralTerrain` → `wxDialog::ShowModal` → event loop → `wxTopLevelWindowMSW::OnActivate` → `DoRestoreLastFocus` → `wxSetFocusToChild` → `wxWindow::SetFocus` |
| Application close with two tabs | `MainFrame::OnExit` → `DoQuerySave` → `EditorManager::CloseCurrentEditor` → `MapTabbook::DeleteTab` → `wxAuiNotebook::DoRemovePage` → `DoModifySelection` → `wxSetFocusToChild` → `wxWindow::SetFocus` |

First stack from focus restore after activation, second from tab selection when removing active. In both cases `SetFocus` call itself comes from wxWidgets. Stacks contain no Preview step handling. Do not identify faulty HWND nor cause of earlier round-trip warning.

Observer recorded all nine invalidation markers, including correct retry and new dialog. Runner exceeded wait limit before finishing GUI sequence. After resume process closed manually, and app log shows tab closing and resource cleanup. Runner did not record exit code or final execution manifest. Trial passed as stack diagnostics. Earlier functional results remain partial. Interposer records each stack once, while logging chain output each message twice to stderr.

Raw proof in `local-data/procedural-smoke/g1-focus-chain-rme-20261104/run/focus-trace.log`. Product code and vendored wxWidgets unchanged. First build attempt stopped at unavailable SDK declaration, and next private instance disabled after replacing with correct logger variant.

Trial uses private RME instance and synthetic map used in [Preview invalidation test](G1_INVALIDATION.md). Optional observer listens to wxWidgets log, records `SetFocus` warning call stack in `run/focus-trace.log`, and passes message on. Linker creates separate PDB for test binary. DbgHelp uses local symbols from that directory and installed wxWidgets libraries.

```text
rtk proxy <PY> docs/procedural/smoke/g1_run_dialog_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --invalidation --focus-trace
```

`<PY>` is `C:/Users/Sorky/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -X utf8`.

<a id="stan-okien-przy-błędzie-w-przebiegu-odmów-2026-11-06"></a>

## Window state at error in refusal run (2026-11-06)

Win32 `SetFocus` fails with error 87 (`0x57`) when target window or its ancestor is disabled. Checked via separate test: disabled top-level window, disabled parent panel, and disabled target window give 87, hidden panel or hidden window do not error, destroyed window gives 1400.

`ProceduralFocusTrace` now also records window state on error (foreground, active, focus and its ancestors, all top-level windows). Run `g1-focus-state-b-20261106` (`--refusals --focus-trace`, operator: menu and dialog) gave six `REFUSAL_CASE_PASS`, `REFUSAL_PASS` markers and one recorded `SetFocus` warning (stderr prints it twice via logging chain). At error moment:

| Window | State |
|---|---|
| main frame (`wxFrame`) | `wx_enabled=0`, visible |
| `Generate Terrain` | `modal=1`, enabled, foreground and active |
| focus | button in generator dialog |

Stack points to source: `LoadVersion` → `LayoutManager::LoadPerspective` → `PaletteManager::CreatePalette` → `GUI::SelectBrushInternal` → `wxWindow::SetFocus`, called from probe timer during `ShowModal` of generator dialog. This is step 4 of refusal probe (`loadClient()` after `UnloadVersion`), which loads client with frame disabled. Warning in this run thus comes from probe step, not Preview, Validate, or Place handling. In normal use client does not load while modal generator dialog open.

What this **does not** show: two earlier stacks (frame activation with generator open and tab removal on close) have no recorded window state, so for them disabled window is hypothesis consistent with Win32 test, not finding. Not checked whether without step 4 refusal run is free of warning, nor whether focus returns correctly after dialog close. Runner ended code 1 due to non-empty stderr, as in previous refusal runs.

```text
rtk proxy <PY> docs/procedural/smoke/g1_run_dialog_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --refusals --focus-trace
```

<a id="zamykanie-aplikacji-i-kod-wyjścia-próbników-2026-11-06"></a>

## Application exit and probe exit codes (2026-11-06)

Run `g1-focus-rt-20261106` (`--roundtrip --focus-trace`, 100×100) ended `ROUNDTRIP_PASS` and one `SetFocus` warning (stderr prints it twice via logging chain). Stack: File → Exit → `MainFrame::DoQuerySave` → `EditorManager::CloseCurrentEditor` → `MapTabbook::DeleteTab` → `wxAuiNotebook::DoRemovePage` → `DoModifySelection` → `wxWindow::SetFocus`. Window state at that moment: main frame disabled (`wx_enabled=0`), visible "Closing" window (enabled, foreground, non-modal in `IsModal` sense). By Win32 test from section above (target or ancestor disabled gives error 87) this is same mechanism: wx selects next tab after removing active and sets focus in frame that "Closing" window disabled during shutdown. Application exits so focus has no effect; not investigated whether outside shutdown it behaves differently.

Run repeated with added window state recording in `g1-focus-rt-c-20261106`: full `ROUNDTRIP_PASS`, same single warning with same stack.

**Probe exit codes with trace.** Two trace runs (`g1-focus-state-b-20261106` and `g1-focus-rt-20261106`) ended with code `0xC0000374` (heap corruption) after final marker, and non-trace runs with code 0. Cause after reading wx code: `ProceduralFocusTrace` is active logging target, and wx on shutdown does `delete wxLog::SetActiveTarget(nullptr)` (`init.cpp`), while probe deleted same object via `unique_ptr` (double delete). Probes now release ownership (`trace.release()`). Run `g1-focus-rt-c-20261106` after this change has full `ROUNDTRIP_PASS` and exit code 0. Conclusion rests on two runs with corruption and one without after change; this was diagnostic probe error, production EXE does not contain this code. Runs with code `0xC0000374` kept without pass.

Unexplained remains stack from 2026-11-04 (frame activation with open generator in invalidation run): no window state recorded for it, and today's runs did not reproduce it.
