# G1-B — own snapshot and copy accounting

Status: snapshot/change detection, without DrawOperations sandbox and commit. Full G1 remains IN PROGRESS.

`generation_snapshot.*` takes on main thread own input, full copies of existing tiles and location metadata. Context is region with two-tile halo, trimmed to actual Position limits. One strip may be border target, another supplies it eight neighbors. Creates no locations or fields in live map. Locations without Tile still keep spawn/waypoint/town counts and house exits. Copies of existing tiles have own TileLocation.

First pass accounts for structure/vectors/masks, additional exits and copy estimates; only second pass copies tiles. `SnapshotBudget` defaults refuse work, has separate limits on context field count and estimated bytes. Completely excluded area, invalid bounds, limit, Cancel, or allocation error do not return partial snapshot. Existing deepCopy restores item type from current definitions; snapshot compares each copy to source and refuses if copy lost type/content/metadata.

`snapshot_memory.*` accounts 64-bit for Tile/Location, ground/stack, Item subtypes, attribute maps with keys and strings, containers with children, raw OTBM payloads/trees, creature with original name/alternatives, spawn, zones, opaque tile payloads, and exits. Each add/multiply checked before use; container and tree nesting has 64 limit. Estimator uses const attribute view instead of `getAttributes()` returning copy. Does not use overflowing or incomplete uint32 `memsize()`. Independent of client definition.

This conservative estimate with std::string/std::map layout assumptions and allocation overhead, not strict RSS limit or complete generator budget. Does not cover existing assets, BaseMap/grid sandbox, resolver result, history/Undo, and remaining stages overhead. Large empty snapshot trial does not cover full G1 E2E or 500×500 on dense existing map.

`snapshot_equal.*` compares actual values, not just ID/subtype: attributes (double bitwise), type and selection of items, containers, teleport/door/depot/podium subtypes, preserved invalid data, extra/zones, flags/house/minimap, full Creature and Spawn state. TileLocation metadata checked separately. Does not change existing narrower `Tile::isContentEqual()`. New `Creature::hasSameState` not comparison operator and does not auto-change Lua equality. Bindings read; existing public mutators unchanged.

`snapshotMatchesMap` refuses different map identifier/revision, assets reload, different dimensions/map version, and change to any captured tile or location metadata. Covers untracked direct data changes in context, e.g. UID/AID whose mere revision number won't detect. Not full check of all independent asset catalog changes or new dialog mask and does not replace re-validation before commit.

## Claude and independent verification

Estimator and test implementation: Claude in existing dedicated worktree, run `668dec3c-e82c-4ecf-92b0-f90d82ba92d9`; review fixes `0e66a728-b22d-4b33-ac36-582facdbbd05`. Codex read reports/stdout/stderr/status/full tracked diffs and new files. First version copied attribute map before accounting and contained test app stand-ins; both fixed before integration. Worker did not compile or run tests, no refusals and no commits. Codex added capture/equality and extended tests. No change to Tile/Creature layout or existing copy semantics.

Focused build of three new CPP passed. First canonical wrapper incorrectly set VCPKG_ROOT before vcvarsall, which substituted vcpkg from Visual Studio. build_ninja changed cache, removed own CMakeFiles, and started unneeded dependency rebuild. Own process CMD 23128 tree identified and killed; broken log `g1-snapshot-canonical-20261102.log` kept, no PASS. Correct wrapper set C:/repo/vcpkg before direct build_ninja, and full build **PASS 0**, 414 steps: `g1-snapshot-recover-canonical-20261102.log`. Production script, world, database, or Docker not edited.

New mode `g1_run_input_probe.py --snapshot` links test with real RME objects and private renamed entry point. No Application::OnInit, windows, client, input maps, or world files. **PASS, normal exit 0, empty stderr**: `g1-snapshot-rme-20261102/verification.json`. Executed payload growth, test without estimator allocation, budgets, depth 64/65, null item, halo ownership, UID/AID and opaque payload, Creature/Spawn and location metadata, independent maps/revision, lossy-copy rejection, and all **24** observed single allocation error points of capture. Each returned AllocationFailed without partial snapshot and without source map change.

100×100 snapshot had 10,816 context fields, estimate 453,760 B, and capture time 0.2661 ms; 500×500 — 254,016 fields, 10,661,760 B, and 8.6326 ms. In both cases real own vectors, halo, and map comparison passed at 64 MiB snapshot limit. Map was sparse, with single tile in halo. Not full generation, dense map, RSS measurement, or speedup claim. No overflow on impossible-allocate containers, sustained OOM, GUI Cancel during run, or full G1 E2E not executed.

Remain: shared DrawOperations placement/autoborder in sandbox, concrete diff and its scope check, full work budget and Undo/peak, GUI Cancel/stale selection, atomic commit and application faults, separate Undo/Redo, minimal dialog, real E2E and round-trip. G2 not started.
