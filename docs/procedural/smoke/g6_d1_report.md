# D1 — SetFocus warning while closing tabs and the application

2026-11-19. Criteria: [G6_METRICS.md](../G6_METRICS.md), “D1 and D4” section
(commit `04b03de`). Result: **case reproduced, fixed and checked in 2/2 runs**.

## Cause

`EditorManager::CloseCurrentEditor` removed the tab inside `ScopedInfoBar("Closing map...")`.
The “Closing” window creates a `wxWindowDisabler`, so the frame is disabled. In wxWidgets 3.3.1,
`wxAuiNotebook::DoRemovePage` calls `SetSelectionToWindow` after removing a page, then
`DoModifySelection` focuses the remaining page (`auibook.cpp:4220`). Win32 `SetFocus`
on a window with a disabled ancestor returns error 87 (0x57). This affected every close of
the current tab when another tab remained: File → Exit, File → Close and the tab-close button.
It was not limited to application exit.

## Fix

`MapTabbook::DetachTab` removes a page from the notebook without destroying it, while
`MapTabbook::DestroyDetachedTab` destroys it. `CloseCurrentEditor` detaches the page before
showing the “Closing” window, while the frame is still enabled. Map and GL resources (the
expensive part) are still released under that window. `DeleteTab` composes both operations,
so `CloseAllEditors` and `CloseLiveEditors` keep their existing behavior without a “Closing” window.
Palette order and page-change events remain the same as in `DeletePage`. The difference is that
`DestroyDetachedTab` also removes the `conv` map entry that previously remained.

## Probe

- `g6_d1_close_probe.cpp`: private wx module with the normal application entry, built with
  `g1_build_input_probe.py --application-entry --symbols`, z `ProceduralFocusTrace`.
- `g6_d1_run_close_probe.py OUTPUT PROFILE_DATA CONFIG [--case …]`: each case runs in a new
  process and profile copy. PASS requires exit code 0, ordered markers, unchanged assets and empty stderr.

Cases (the startup tab is the first tab):

| Case | Run |
|---|---|
| `exit` | 2 clean tabs, File → Exit |
| `cancel` | first tab dirty (1 tile), current tab clean; File → Exit closes the clean tab, then Cancel on “Save changes”; check that the dirty tab with 1 tile remains; discard changes and Exit again |
| `tab-close` | 3 tabs; File → Close; `PAGE_CLOSE` for the current tab (same handler as the button); File → Exit |

Client 10.98 (`1098-BlackTek`) from profile `g0-reference-profile-20261030`. Startup loads
the client and shows a modal list of 22 tileset warnings (`missing_id`, `inverted_range`).
In older runs an operator closed it. The probe records its contents and closes it with `EndModal(wxID_OK)`.

## Results

| Przebieg | EXE `rme.exe` | exit | cancel | tab-close |
|---|---|---|---|---|
| before fix (`04b03de`) | `80cba5b9…` | FOCUS_WARNING, 1 error | FOCUS_WARNING, 1 error | FOCUS_WARNING, 2 errors |
| po poprawce, przebieg 1 | `fb12d483…` | PASS | PASS | PASS |
| po poprawce, przebieg 2 | `fb12d483…` | PASS | PASS | PASS |

Before the fix, every error had a stack from `MainFrame::OnExit` or a menu/tab handler →
`DoQuerySave` → `CloseCurrentEditor` → `MapTabbook::DeleteTab` → `wxAuiNotebook::DoRemovePage`
→ `DoModifySelection` → `SetFocus`. Window state at the error: frame `wx_enabled=0`, with the
“Closing” window visible. This is the same stack as in [G1_FOCUS_TRACE.md](../G1_FOCUS_TRACE.md).
Each wx error was written twice to stderr. All runs exited with code 0 and complete markers.
After the fix, stderr and `focus-trace.log` are empty. Cancellation preserves the process,
dirty tab, tile and `ShouldSave` state.

Rejected setup attempts were retained privately. In the first, the probe waited for the startup
warning dialog and the cases timed out. In the second, the probe looped in that dialog before
creating the startup tab. This produced 3 or 4 tabs instead of 2 or 3, but reproduced the same warning.

## Limits

- Commands are `wxEVT_MENU` and `PAGE_CLOSE` events queued through production handlers,
  not mouse clicks. Cancel is `WM_CLOSE` on the native “Save changes” window, returning
  IDCANCEL rather than simulating a button click.
- Maps are empty and synthetic. Large-map close time was not measured, and the “Closing”
  window was not visually inspected. The focused control after closing was not checked visually.
- Other SetFocus sources were not investigated, including D2 (generator activation stack) and
  `LoadVersion` warnings with a disabled frame in the refusal probe.
- CTest, the G5 suite and Lua tests were not run. The coordinator runs the regression.
