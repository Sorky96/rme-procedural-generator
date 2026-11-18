# I56: local surface profile learning

Entry: **Terrain generator → Terrain → Learn from open map…**. This is an
in-process workflow; the application never launches the offline CLI.

1. Open the source map with its compatible client loaded. Choose **Learn from
   open map…**. Enter X, Y, width, height and surface floor 0–7 explicitly.
   The rectangle must fit inside the map and cannot exceed 512×512.
2. Confirm your right to use the map. **Read rectangle** reads at most one row
   per GUI timer tick. It retains only a bounded semantic sample, not the map.
3. Review the displayed `g4-keymap/1` text. Each row has four tab-separated
   columns: `key`, class, rule, exact brush name. Change the class column to
   correct a proposed assignment. Classes are case sensitive: `Grass`, `Dirt`,
   `Rock`, `Shore`, `Water`, `Other`, `Unmapped`, `NoGround`. Keep the empty
   brush name as `NoGround`. Unbrushed and invalid grounds are never guessed.
4. **Analyze** reports natural/recognized, skipped and present tile counts.
   Fewer than 1024 natural tiles or less than 50% coverage refuses fitting.
   Fit uses G4 `accumulateKindStats`, `targetFromStats` and `fitTuning` directly:
   fixed side 160, seeds 1–6, four coordinate-descent passes, default start.
   The same sample is both targets; this is fitting, **not held-out validation**.
   The progress indicator pulses while the owned worker runs. **Cancel analysis**
   discards its result; closing requests cancellation and joins the worker.
5. Inspect the result, enter a filename stem (1–64 ASCII letters, digits, `-`,
   `_`) and **Save profile** to confirm. No path or device names are allowed.
   A flushed temporary file is published atomically without replacing an
   existing destination. The normal local profile folder is used, shown in the
   generator's profile tooltip. Map names, map paths and coordinates are not
   serialized. Ground brush names are retained for the normal palette resolver.
6. Select **Natural surface**, choose the saved profile in the normal
   **Style profile** list, and Preview on a suitable target map. I54 feedback,
   Adjust and rollback remain available. The source is `user-local-surface`,
   never M06/M19; no M06/M19 ranking bands are inferred. Palette names still
   pass the existing generator resolver checks.
7. Verify Place creates one Undo entry; Undo restores the target and Redo
   reproduces its payload. Learning, cancellation, profile saving and feedback
   must leave the map and Undo unchanged.

Map identity/revision/dimensions, client revision and the existing generation
asset fingerprint invalidate the sample/result when they change. Rectangle,
mapping or permission changes cancel a fit and invalidate its result. Before
save, the handler rechecks the source, actual controls, OTB/DAT hashes and
profile compatibility, including forced handler calls.

Automated checks: `test_procedural_style_learning` and the actual-client probe
`g1_run_resolver_probe.py --learn OUTPUT PROFILE_DATA CONFIG`. Use a fresh
private OUTPUT under `scratchpad`. The latter operates synthetic source/target
maps through production GUI controls. It is not a manual visual/keyboard review.
No private source maps, profiles or test run outputs belong in Git.

This workflow fits surface shares/adjacency and its palette only. It does not
learn vegetation, buildings, underground geometry, similarity references, ML or
ranking bands, and does not satisfy I55 or close G6.

## Implementation verification (2026-11-18)

- Windows x64 Release: canonical `build_ninja.bat` passed. Final cache has
  `VCPKG_MANIFEST_INSTALL=OFF`, root `C:/repo/vcpkg`, and the main checkout's
  `build-ninja/vcpkg_installed`. No dependencies were requested by the final builds.
- `g5_run_suite.py --probes`: **SUITE_PASS**, 23/23 core executables,
  commit and surface probes PASS; source assets unchanged. Commit: no stderr.
  Surface: 157 known SetFocus warnings, no other stderr. The core surface test
  retains seed 1729, 100×100 fingerprint **17159159863505677800**.
- I56 actual-client probe: **LEARNING_DIALOG_PASS**. Permission refusal,
  cancelled read/fit, local fit/save and immediate normal offer,
  invalid filename, no learning map/Undo mutation, invalidation of a completed
  fit after a map revision change, I54 feedback/Adjust retaining user provenance,
  Preview and one Place/Undo/Redo. Assets unchanged; two known SetFocus warnings,
  no other stderr. Uses a synthetic source map generated with the real client,
  not private map files. Core tests separately cover exact coverage boundaries,
  unknown grounds, corrections, determinism, unsafe names, save conflicts/write
  failure and client/assets compatibility.
- `g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1`: **G5_GATE_PASS**,
  32/32 per profile, identical repeated reports (dungeons off/on, retry 0/8).
- I55 Python tests: 9/9. I55 criteria and its package implementation unchanged.
- `git diff --check` and Python smoke-runner compilation passed.

Private evidence remains in `scratchpad`: `build-i56-final-validation.log`,
`suite-i56-final.log`, `i56-suite-final/`, `learning-i56-verified.log`,
`i56-learning-verified/verification.json`, `i47-i56-final.log` and
`i56-i47/summary.json`. Only implementation, tests and these instructions are
committed. The implementation agent did not push, merge or clean up the worktree.

Two failed setup attempts are retained, not counted as passes. An initial
`vcvarsall` invocation changed `VCPKG_ROOT` before the build script captured it;
the script reset the cache and entered its vcpkg install step. It was interrupted:
the log records registry fetching and a dependency plan, with no completed
package installation recorded. The wrapper was corrected to reset root and
manifest-install settings after MSVC initialization and configure the cache
explicitly before `build_ninja.bat`. A concurrent validation attempt also hit
LNK1104 on the core library; the final build and suite ran sequentially.
MSVC probe links emit C4723 warnings from the unchanged wx dependency headers.

Not verified: manual visual/keyboard acceptance, other platforms, real private
world quality, or a new held-out quality evaluation. The coordinator still owns
review, fast-forward integration, independent regression and build/worktree
cleanup. I55 and G6 remain open.
