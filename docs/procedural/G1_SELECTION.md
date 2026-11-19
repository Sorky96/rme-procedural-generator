# G1: selection isolation during preview

Newer section: [G1_BUDGET.md](G1_BUDGET.md) adds and verifies joint conservative preparation budget. Status below historical in scope of this entry; whole-app RSS and future Undo memory remain open.

Status 2026-11-02: **PASS_G1_PREVIEW_SELECTION_ISOLATION_ONLY; full G1 IN PROGRESS**. This section replaces temporary `SelectedContext` refusal described in earlier status [G1_PREVIEW.md](G1_PREVIEW.md). Does not implement generator commit or GUI mask selection.

## Change

`TileOperations::markSelectionChanged` now checks position and identity validity: notifies current editor's selection only when tile read from its map is exactly passed object. Copy with same coordinates, copy using map's TileLocation, and tile from different map do not notify active editor. Map read is const and creates no structures or allocations. No global silencing, TLS, new operation options, or test hooks added.

In sandbox removed `selectedCell`, `SelectedContext` check, that enum value, and unnecessary includes. Placement and bordering still use shared implementations. Selected ground in unchanged halo can be safely copied and checked. Source flags and selection cache of both editors remain unchanged.

Full diff protection stays active. Selected border reproduced by shared calculator becomes unselected item; even with identical IDs this is real change to protected data, so whole candidate gets `OutOfMask`. Flags not restored artificially and diff not trimmed. Control trial with same unselected border passes.

Empty writable field with only `TILESTATE_SELECTED` bit still allowed input for `empty_only`. In buffer state recalculated by shared operations: new ground unselected. Source bit and editor selection remain unchanged. Not yet decision on result selection after commit.

## Real editor verification

`tests/test_procedural_preview.cpp` run as test wxModule with normal Application creates two private empty tabs. Does not open maps. Both maps have one control tile in selection, and generation works also on active and inactive map.

Cache invalidation checked via public `minPosition`/`maxPosition`: after priming cache test momentarily changes position of only control TileLocation, reads bounds, and immediately restores position, also after exception. No yield, map operations, or GUI between change and restore. This oracle for dirtiness flag in private fixture, not way to edit maps. Positive checks for real tile (`markSelectionChanged`, selection ground change, and `Tile::modify`) confirmed oracle sensitivity. No access to production private fields added.

Real listener `app.events:on('selectionChange', ...)` confirmed no unwanted events for copy, different map, and preview. Positive check uses explicit notification during empty normal selection session. This test does not prove event emission for non-empty selection actions: nested INTERNAL sessions may clear `selectionChanged`; existing behavior not changed.

Passed:

- copy with own TileLocation, copy action using live map location, tile from different map with same coordinates, nullptr, and invalid position;
- safe selected context on active and inactive map, no source/cache/event change; selected border with UID/AID rejected, identical unselected border accepted;
- normal selection removal, Undo/Redo and final Undo, bounds and pointer identity of selected tile after swap;
- real Lua setter `app.map:getTile(...).ground.actionId`, Undo/Redo and final Undo with selected tile and bounds kept;
- full earlier core → resolver → bordered sandbox 100×100/500×500, repeats, limits, global RNG, Cancel, stale epoch, and 79 allocation faults; fault fixture now contains selected empty field, and each case also checks cache of both editors.

Private histories and dirty flags of fixture cleaned after regression, tabs closed, listener unregistered, and normal app shutdown. No OTBM saved. Informational asset warning windows handled via Computer Use only in private process.

## Evidence and delegation

- `local-data/procedural-smoke/g1-selection-canonical-20261102.log`: canonical Windows x64 build, exit 0. Normal EXE `7048a947d419bd57dc8d1757548b9708172e5d813348d1c898765066ec17dc79`.
- `g1-selection-lua-rme-20261102/verification.json`: PASS, exit 0, empty stderr, normal shutdown, and unchanged source assets.
- `g1-selection-lua-rme-20261102/current-verification.json`: **PASS_G1_PREVIEW_SELECTION_ISOLATION_ONLY**, 415 current objects/sources/probe/normal EXE; 877 G0 database sources has only ten expected G1 differences. New difference in existing sources of this section is only `source/map/tile_operations.cpp`; shared G0 changes kept.
- Claude read-only review `1cc3531a-550e-430b-b56a-29ba93fe2d44`: completed with no refusals and no checkout change. Checked Action/Selection/draw/Lua operation order and const lookup, found no counterexample to guard. Identified selected-border/empty-tile/event limits, which addressed. Report/stdout/stderr and Git before/after checked. Claude did not compile or test; Codex ran tests independently.

Reproduction on new `OUTPUT`, after canonical build, with existing private assets profile:

```text
rtk proxy python docs/procedural/smoke/g1_run_resolver_probe.py OUTPUT PRIVATE_PROFILE_DATA PRIVATE_CONFIG --preview
rtk proxy python docs/procedural/smoke/g1_verify_preview_probe.py OUTPUT --selection
```

Kept failed trials: `g1-selection-rme-20261102` — missing ActionQueue include in harness; `g1-selection-observers-rme-20261102` — FAIL of final script with nonexistent `app.getMap()`, despite earlier passed cases. Test fixed to existing `app.map` and `actionId`, then new full run. Earlier trials not renamed to PASS.

## Remaining gate

GUI capture of exact selection, its staleness, and empty tile membership to selection at future commit not verified. `Selection::add` may place empty field in collection without selected bit; commit boundary must account for membership and identity, not only `Tile::isSelected`. Such transaction not implemented now nor generator Undo/Redo passed based on normal action regression.

Open: full work budget/RSS, GUI Cancel and schedule, revalidation of relevant assets/map/mask/selection, atomic commit of exact diff and its faults/history versus manual drawing, minimal dialog/rendered preview, full GUI smoke and OTBM round-trip 100×100/500×500, and portability of equal-order equality. G2 not started. User maps/companions, Docker, database, and world untouched; no commits and push.
