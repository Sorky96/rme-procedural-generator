# G1: exact mask of the real selection

Later work: [G1_VALIDATION.md](G1_VALIDATION.md) combines parameter/mask, client, snapshot, and prepared-preview integrity checks. Live membership is still checked separately. Atomic commit integration and complete independent asset-mutation control remain open.

Status 2026-11-02: **PASS_G1_EXACT_SELECTION_MASK_ONLY; full G1 IN PROGRESS**. This work complements the [preparation budget](G1_BUDGET.md) and [selection isolation](G1_SELECTION.md). It covers capturing and rechecking selection membership.

## Contract

`captureGenerationSelection` in `source/procedural/adapter/generation_selection.{h,cpp}` reads `Selection::getTiles()` from the selected `Editor&`. `GenerationSelection` contains a minimal one-floor rectangle, exact row-major mask, map generation, member count, and memory estimate. Holes are zero. An empty tile in the selection collection is one, even without the selected bit. Item or out-of-collection tile flags do not expand the mask. It does not use `minPosition`/`maxPosition` cache, rectangle the selection, or change its state.

No selection means `Empty`, never the full rectangle or map. Multiple floors mean `MultipleFloors`; there is no silent Z projection. Every entry must identify the real tile in this map at its coordinates, within header and `Position` bounds. Detached copies and objects from another map are rejected. Strict `Position` order (z, y, x), matching the existing `Selection` collection, rejects duplicates and invalid order without extra containers.

`SelectionMaskBudget` defaults to zero and refuses work. `maxCells` limits the complete mask rectangle, including holes, while `maxBytes` accounts for `sizeof(GenerationSelection) + 32 + cells`. Allocation-free preflight checks identity, bounds, and limits. Only then is one owned vector created. `bad_alloc` returns `AllocationFailed` without a partial result. This estimates owned input and allowance, not RSS or the complete preparation budget. A caller retaining this mask beside `PreparedTerrain` must account for it separately because the existing preparation contract excludes borrowed inputs.

`validateGenerationSelection` is allocation-free. It checks map generation, geometry, binary mask, one count, strict unique current membership, and exact minimal bounds. Replacing a selected field with another at the same bounds/count means `StaleSelection`. Invalid current entry means `InvalidSelection`. A region with an added zero margin is also rejected.

Both APIs take a live `Editor&` and run synchronously on the main thread without yield. Before reading the editor they reject the wrong thread, then stop and busy/deferred selection. Loops check stop every 4096 entries/bytes and before success. They require no active client and do not switch tabs. The single GUI-thread model follows the [wxWidgets 3.3 documentation](https://docs.wxwidgets.org/3.3/overview_thread.html) checked with Context7.

## Freshness boundary

The validator compares the current coordinate set, not event history or old pointers. History can replace a tile while leaving a valid mask. Changing the selection and restoring the exact set also restores it. Changing item-selection flags with the same membership does not change the mask. It does not store mapRevision or pointers.

Map content, halo snapshot, revision, and assets need separate validation. Creating an empty tile in a hole does not change mask membership, but invalidates a snapshot that captured the missing tile. Mask-validator success does not authorize commit or replace source checks. The caller guarantees editor and pointer lifetime in the correct selection collection. The helper does not repair dangling pointers.

## Claude and review

Claude prepared only two new files in a separate checkout. Run `b57245cc-6c6f-4cd5-9ce7-67bff4ec9fb1` and fix `fdf3a1aa-14bf-407c-a0b0-967a19c46e31`: completed/exit 0, empty stderr, no refusals. Codex read report/stdout/stderr, reviewed new files and the full tracked diff. The worker's existing tracked diff was unchanged. The worker did not build, test, or commit.

Codex found that equal one-count and positions do not detect an added empty margin. Claude added minimal-rectangle comparison to the existing validation path, with a regression test. An unused include was removed. Accepted files were copied byte-for-byte. Hashes are in `local-data/procedural-smoke/g1-exact-selection-integration-20261102.json`. Codex owns CMake, independent tests, integration, and reporting.

## Independent verification

Kanoniczny Windows x64 `build_ninja.bat` exit 0: `local-data/procedural-smoke/g1-exact-selection-canonical-20261102.log`. Nowy normalny EXE SHA-256: `9b4c2334c00ea74ee2dc669e884fccc4f422306143a0b1b0c499a6fed7b3e44a`. Nie zmieniono produkcyjnego build scriptu ani dotychczasowego zachowania Selection/Editor/Map.

Full normal-Application probe with test wxModule: `local-data/procedural-smoke/g1-exact-selection-rme-20261102`. **PASS, exit 0, normal shutdown, empty stderr, source assets unchanged**. Two real private editor tabs and synthetic in-memory tiles, without opening map files. `g1_verify_preview_probe.py --exact-selection` confirmed 417 current RME objects and 877 G0-base sources with ten expected G1 differences. Evidence includes `verification.json`, `current-verification.json`, build manifest, and full log. Probe EXE SHA-256: `e22990baf715990208883b9b65103c7631650ac9048c61a90b453a20552f8c55`.

| Mask rectangle | Selected fields | Owned-input estimate (B) | Prepared-preview fingerprint |
|---|---:|---:|---|
| 100×100 | 9 999 | 10 104 | 12307128149775414366 |
| 500×500 | 249 999 | 250 104 | 6528612615874077346 |

Each rectangle has one hole. All selected tiles are empty and lack the selected flag. 100×100 ran for an inactive editor and 500×500 for an active editor. In both cases capture made exactly one observed allocation and revalidation made zero. The exact byte limit passes. Tests checked each mask and passed it to real `prepareTerrain`: exact change count, no plan in the hole, every diff inside the mask, source tiles still empty, and snapshot still matching.

Membership replacement with the same bounds/count is rejected. Restoring the exact set passes again. Creating an empty tile in a hole leaves the mask current but rejects the earlier snapshot. Changing/clearing the live selection leaves the captured result owned and unchanged. Active tab and global RNG do not change.

Passed: default cell limit, undersized byte limit, one mask allocation-failure point, pre-cancel of both APIs, worker refusal for both APIs, deferred/internal-session refusal, non-binary/short mask buffer, invalid count, negative/extreme geometry, padding, negative/out-of-header live entry, detached copy, other-map entry at same coordinates, multiple floors, and empty collection. Item-flag changes with fixed membership and controlled tile-object replacement with correct collection update do not invalidate the mask itself. This is not a generator commit or Undo test.

All earlier 100×100/500×500 sandbox checks were repeated, including 79 sandbox faults, 106 shared-preparation faults, exact budget boundaries, snapshot/protection, selected-context and selected-border refusal, two-editor cache/events, and normal selection/Lua setters with Undo/Redo. Estimates and observed normal-preparation peaks remained consistent with the previous slice. No speed or RSS guarantee is claimed. Older failing attempts remain historical. `git diff --check` passed.

Forced null/duplicate/order corruption of the internal collection, persistent OOM, cancellation during the mask run, and RSS measurement were not performed. The code rejects malformed entries through explicit checks. Pointers must remain live under the Selection contract.

## Otwarte wymagania

The helper adds no range-selection dialog, rendered preview, or validation connection to a Commit button. It does not solve publishing empty-tile membership after future commit/Undo/Redo. Remaining work includes atomic exact-diff commit, fault injection, separate manual-drawing history, complete commit staleness control, GUI Cancel/scheduling, Undo/RSS/other-result memory, and 100×100/500×500 GUI/OTBM round-trip. G2 was not started. No forgotten.otbm/companions, user maps, Docker, database, or world were used. Shared uncommitted changes were preserved. No commit/push.
