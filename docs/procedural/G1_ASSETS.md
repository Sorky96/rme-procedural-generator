# G1: fingerprint of the current asset catalog for generation

Newer section: [G1_TARGET.md](G1_TARGET.md) composes the check of the current live offline target with result validation and optionally actual membership. Atomic commit, reentrancy/history, and GUI remain open. The catalog results below are reproduced in the new full probe.

The 2026-11-03 excerpt completes [validation of prepared result](G1_VALIDATION.md). It concerns changes to loaded definitions, ground brushes, and metadata used by generation, also without changing the VersionManager epoch. Full G1 remains **IN PROGRESS**.

## Decision and scope

Setter counters alone do not cover writes through retained `mutableEditorData` or public rule objects. `fingerprintGenerationAssets` in `source/procedural/adapter/generation_assets.{h,cpp}` reads current values on each call. It works only on the main thread, with no allocation, yield, directory/RNG/cache changes, rendering, decoding, or GL upload. Working data has fixed size. Stop is checked on entry, at most every 4096 units in unbounded loops, and before success.

The fingerprint includes:

- epoch and client ID, versions, order/ID and max ID store;
- identity/group/type visible through `ItemDefinitionView`, full flag mask, all current 16 `ItemAttributeKey`, texts/JSON, and editor brush association;
- existing `GameSprite` for each client ID: presence/identity, public dimensions/patterns/frames, minimap, light, draw height/offset, and `sprite_ids`;
- the full brush registry: keys, identities, ID and ground type; for each GroundBrush also name, friends/hate, look/Z, border flags, optional, randomize, chances/variants, and ordered rules/specific cases;
- global and own AutoBorder with ID/group/ground/13 tile IDs and global `GroundBrush::border_types`.

Weight is observed through the existing attribute API with its calculation to int64; it does not hash the hidden float representation. Internal store arrays must satisfy the equal-size contract maintained by append. It is not a sanitizer for arbitrarily corrupted tables. When `allIds()` contains duplicates, the hash includes their order and values resolved by current `get(id)`, just like the generation path; there is no access to shadowed rows by index.

## Ownership and identity

Stored raw `ItemEditorData::brush` links are not dereferenced. They are identity tokens. A non-empty AutoBorder link must point to its own `unique_ptr` object or an object in the current global registry; confirmation is done by pointer comparison. An unknown link, empty block/case, or invalid type of an existing sprite means `InvalidAssets`. Own objects and registry objects are read only by their proper owners.

The only change to the GroundBrush class is narrow reader friendship; no mutators or public views of private data were added. The class layout, randomization, draw, or bordering was not changed.

The fingerprint is conservative and **local to the process**: it contains pointer tokens and the order of the unordered registry. It is not saved to the map, does not serve as a persistent key, and is not included in the deterministic fingerprint of the result. A change to a covered part of the catalog unrelated to the current region also invalidates the preview; restoring identical values and identity can restore compliance. This is 64-bit consistency control, not cryptographic protection against memory corruption.

## Integration

DrawOperations checks and remembers the fingerprint before building the sandbox, refusing a malformed catalog before using raw border links. `TerrainPreview::matchesAssets` recalculates it on demand. `validatePreparedTerrain`, after checking client/epoch and before checks of source/preview that can use cached links, requires current catalog compliance. Non-compliance or `InvalidAssets` gives `StaleAssets`, stop gives `Cancelled`.

The scan is not part of the render hot path or the loop of each tile. The new scalar is covered by the existing `sizeof` estimator: common preparation grows by another 16 B with no new allocation per tile. Source association and the current concrete fingerprint of the result remain separate.

## What remains outside the scope

It does not cover raw pixels, images, textures, atlas/GPU, runtime animation, geometry caches, or the interior of brushes other than ground. It does not hash doodad/collection/RAW links of definitions that the G1 terrain path does not use. It reads loaded data; it does not monitor file changes on disk before they are loaded. Proper ownership of the registries/containers themselves remains a C++ requirement.

Full G1 still requires final lifetime guard/proper editor and offline mode, atomic commit/failure/Undo/Redo and empty tile membership, independent manual drawing history, GUI dialog/rendered preview/Cancel/schedule, Undo budget/RSS/other results, and GUI/OTBM round-trip 100×100/500×500. This section does not cover these requirements and does not start G2.

## Claude and review

Claude completed two new files via bridge, run `14ea4969-9641-4259-add4-6a0b0e291666`: completed/exit 0, no refusals, empty stderr, report/stdout aligned. Tracked worker diff remained identical to the previous run. Codex reviewed the full new files, report, and diff, including polling, pointer ownership, and scope of each field. Worker did not build or test. Files were copied byte-for-byte; manifest `local-data/procedural-smoke/g1-assets-integration-20261103.json`. Codex performed integration, testing, and independent verification.

forgotten.otbm/companions, user maps, Docker, database, or world were not used. Shared uncommitted changes were kept; no commit/push.

## Independent results

Status 2026-11-03: **PASS_G1_GENERATION_ASSET_CATALOG_ONLY; full G1 IN PROGRESS.**

Windows x64 build exit 0, EXE `e17b2f82…`. Full probe `g1-assets-rme-20261103`: PASS/exit 0/empty stderr/normal shutdown/input assets unchanged, 419 current objects and 877 G0 database sources with ten expected G1 differences. 39 catalog mutations without epoch bump refused validation; after restoring values, the result passed again. Scan had zero allocations. Separate freshly linked border-order and snapshot tests also PASS, including 24 snapshot faults. Full probe repeated 100×100/500×500, 79 sandbox faults, 106 preparation, mask, validation, and selection/Lua/UndoRedo.

Full EXE hashes: normal `e17b2f82ce8671737a5b38398d1783f0387574bb9a7aedd2fd40185eef98bd08`, probe `4a61cc1af577f18d93010751039ed1fb6d151923d2dcfd80881977dc7c7ae4f0`. Canonical log: `local-data/procedural-smoke/g1-assets-canonical-20261103.log` (33 steps). Evidence in `g1-assets-rme-20261103/verification.json`, `current-verification.json`, probe log, and build manifest. Current focused runs: `g1-assets-border-order-rme-20261103` and `g1-assets-snapshot-rme-20261103`, both exit 0/empty stderr; verifier checks their sources, objects, and EXE against the current build. Older evidence kept for history.

Cases include all categories mentioned in TEST_PLAN, including actual dangling non-owning links in controlled private fixtures. Scan has zero observed new/new[] and one sample time of 13.4346 ms on this loaded catalog. This is not a comparative benchmark, time guarantee, RSS, or cost per catalog. Finding the global owner of links is linear; cost may grow with the number of rules and borders. Public registries/unique_ptr owners must remain valid; arbitrary memory corruption, concurrent mutations, sustained OOM, or mid-call stop were not checked.

Previous 100×100/500×500 fingerprints remained identical; estimates and peak new were confirmed again in the report. During startup of a private instance, the informational Warnings dialog about referenced assets was handled via computer-use, without restart or asset editing. This is not an unattended test or visual acceptance of the generator's rendered preview. After full run, normal exit 0 and no dangling private windows. git diff --check PASS.
