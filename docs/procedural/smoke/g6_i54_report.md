# I54 — local feedback and version history

Prerecorded at `e9ca540`, I54 section in `G6_METRICS.md`. Executor report;
it does not close G6 or replace the integrator's independent review and regression.

## Implementation

Surface provides Approve / Reject / Adjust for the selected prepared
candidate. Selecting a gallery item does not save ratings. Approve and Adjust require
an acceptable result; Reject also works for hardFail. The handler checks
map, assets, settings and prepared-candidate fingerprints, including forced calls.
Notes are limited to 1024 **UTF-8 bytes**, with a visible save error and the text kept for correction.

`procedural/styles/history.json` in the user's local profile directory
contains format `g6-style-history/1`, ratings, versions and the active version. An entry has
a random SHA-256 identifier, client and asset hashes, base-profile fingerprint, local version
or built-in settings, ranking policy, candidate index, user/initial/actual seed and five floor
fingerprints. It stores the complete Surface and decoration settings used for generation. It
does not collect map paths/names, coordinates or tile contents. A note is text explicitly entered
by the user, not automatic telemetry.

Adjust stores a new snapshot with its parent and **only** these style changes:
pathLength, buildingDensity, ruinPercent, campCount, fordLength, rockPasses,
shoreSmoothing (0 or 2) and repairReach. The scope is shown in the GUI. It does not
store edited brushes, mountain/cave geometry or the other controls. Base tuning, band
provenance, client constraints, palette, statistics and profile decorations remain in
the retained snapshot. Input `*.profile` files remain read-only. Their format and parser
are unchanged. Without a profile, Adjust is disabled but the result can still be rated.

The first Adjust also preserves the base version. Explicit Activate / rollback selects a
compatible version and invalidates the series. A later Adjust names that version as its parent,
preserving younger branches. Reopening the window/application loads ratings, history and the
last activated local version. The unmodified base version keeps the old profile behavior and
default GUI controls.

Reading is limited to 8 MiB, 4096 ratings, 256 versions and JSON depth 16. It checks
types, ranges, UTF-8, parents, snapshot compatibility and active version. Unknown format or
corruption blocks saving without resetting the file. Saving checks the byte snapshot of the
loaded file under a directory lock, flushes the temporary file and atomically replaces it with
Windows MoveFileExW. Conflicts and write errors preserve previous data. A lock left after a
failure is not removed automatically, so the user receives an error instead of an overwrite.
A history that reaches a limit is rejected without deleting earlier ratings.

Controls use wxWidgets, the Theme font and colors, and Theme::Grid. They have accessible
names, visible labels and native keyboard handling. Feedback and history do not call map
operations or the Undo queue. The generator, mountain, ranking A, I52/I53 gates, map format
and single Action for Place remain unchanged. No item IDs, synchronization, learning or
automatic ranking changes were added.

## Executor validation

- Windows x64 Release, `build_ninja.bat`: **PASS**. Used the existing
  `vcpkg_installed` in the main checkout with `VCPKG_MANIFEST_INSTALL=OFF` and
  Ninja 1.13.2 specified in the task. Log: `scratchpad/i54-validation.log`.
- `g5_run_suite.py` without `--probes`: **22/22, SUITE_PASS**. This is the core result,
  not the full gate with Commit/Surface probes. The Surface test confirms
  `1729`, `100×100 = 17159159863505677800`.
- CTest for the store, profile, offer, Surface, candidate ranking, series
  and copy guard: **7/7 PASS**.
- Private `g1_run_resolver_probe.py ... --series`: **PASS**, exit 0,
  `input_assets_unchanged=true`, `CANDIDATE_DIALOG_PASS`. Result and source manifest:
  `scratchpad/i54-client/verification.json` and `build-manifest.json`.
  I54 results carry markers `I54_BUILTIN_FEEDBACK_NO_BASE_ADJUST_DISABLED_PASS`
  and `I54_SELECTED_FEEDBACK_UTF8_ADJUST_ROLLBACK_BRANCH_REOPEN_STALE_NO_MUTATION_PASS`.
  The probe also checked Approve/Adjust refusal and Reject saving for hardFail.
  After history, the M06 result selected candidate #2 and preserved one
  Place/Undo/Redo. Existing N=1/4/16 cases, both profiles, cancellation, freshness
  and six I53 cases passed.
- `git diff --check`: **PASS**. No measurements or claims of improved performance
  or ranking quality were made.

The probe linker log contains known wxWidgets C4723 warnings in external
`gdiimage.h` and a vcvars environment message about `vswhere.exe`; compilation
and linking exited with code 0. The probe stderr contained only wx SetFocus
messages tolerated by the existing runner.

Test `test_procedural_style_history` covers all actions, exact uint64 and UTF-8
round trips, note limit, invalid UTF-8, missing base, hardFail, client/asset
compatibility, restart, rollback and branching, two-store conflict, external file
change, lock, temporary-file creation error, corrupt/foreign format, invalid numbers
and tree, and size limit. Profile tests cover existing profile reading and offers.

The private `--series` probe extension uses the real client, production controls/handlers
and ActionQueue on synthetic maps. It checks rating another candidate, notes, Adjust,
rollback, branching, window reopen, stale-result and hardFail refusal, and Place → one
Undo → Redo against an independently prepared result.

Private histories, test profiles, client configuration and logs are only in
`scratchpad/` and are not part of the commit. No dependencies were installed,
`C:/repo/vcpkg/buildtrees` was not changed and nothing was pushed.

## Manual GUI acceptance steps

1. In the private application profile, load a compatible client and base `*.profile`. Open a new map and the Natural surface generator, then choose the profile.
2. Generate four candidates. Use the arrows to choose one other than the recommendation. Check the Selected label, seed and validation state.
3. Enter a note containing non-ASCII characters, click Approve, then Reject. Check the messages and that the map and Undo remain unchanged. Exceed 1024 bytes. Saving must refuse and keep the text for correction.
4. Change a supported style parameter. The old preview must lose its rating ability. Generate a new acceptable result, check the displayed Adjust scope and click Adjust. A local version should be created and the series should disappear.
5. In history, select the base and click Activate / rollback. Generate a new result and Adjust it. Check the new branch parent and the previous branch.
6. Close and reopen the application with the same client. Check ratings, branches and the active local version. Another client or assets must not offer these local versions.
7. Open two windows sharing a history snapshot. Save in the first and then in the second. The second must report a conflict without losing the first save.
8. Generate an acceptable result and run Place, Undo and Redo. Place must have one Undo entry. Check hardFail state: Reject available, Approve/Adjust and Place unavailable. Check focus, tab order and light/dark Theme.

Manual visual/keyboard acceptance and a human rerun of the complete process remain outstanding. The automated probe reopens the store and window. The I55 blind quality test remains a separate task. The integrator runs its own build, full SUITE_PASS with probes and G5 `--i47` (m06=0, m19=1), updates `G6_GATE_STATUS` and removes this worktree's build-ninja.
