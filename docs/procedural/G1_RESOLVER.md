# G1-B — ground placement plan and asset revision

Status: plan implementation, without sandbox/diff/commit. The entire G1 gate remains open.

`source/procedural/adapter/ground_resolver.*` maps Grass/Dirt/Water to three brush names for the current client. On the main thread it checks the loaded client, the revision captured in GenerationInput, region/grid size/enum, mask/protection, and plan-payload budget. It rejects missing brushes, invalid chances, missing/meta items, missing visual client IDs, or inconsistent ground/brush. It does not hide errors behind the first found brush or a hard-coded item ID.

The plan owns names, client, config, map/asset identity and revision, grid fingerprint, and a flat placement-ID vector. Zero means Preserve and never deletion. The plan fingerprint includes the semantic fingerprint and explicit ID bytes. It is not complete provenance or a hash of the concrete post-border diff. The plan alone does not authorize commit.

Variant selection uses existing `GroundBrush::getPlacementItemIdForRoll`, preserving strict `<` and fallback. Three independent v1 streams (grass/dirt/water) perform one roll for each terrain at every position, including excluded positions. Changing only the mask/protection does not shift variants for remaining fields. There is no global RNG or map/active-brush mutation. Zero-weight variants remain valid brush members. Repeated cumulative chances are accepted according to the RME loader.

`maxPlanBytes` accounts for the structure, ID vector, and copied name/client characters with terminators. It is not a limit for the whole job/RSS, allocator overhead, three RNG engines, snapshot/halo, concrete tiles, or Undo. The resolver is synchronous on the main thread and is not ready GUI Cancel handling. The token is checked on entry, during variant validation, every 4096 positions, and before return.

## Claude delegation

Claude implemented `VersionManager::GetAssetRevision()` in a clean separate worktree on branch `feature/procedural-g1-asset-revision`, run `e14767b5-9984-4e39-8bf7-ddd8b20b88ce`. Ownership was limited to two VersionManager files. Report, stdout/stderr, full diff, and status were checked. There were no refusals, other files, commits, shell commands, or worker tests. Codex integrated the change after independently reading load/unload paths and shortened comments.

The counter starts at 0 and increments before each UnloadVersion operation. Client change, forced reload of the same ID, unloading an empty state, and failed loading after teardown invalidate the old revision. No-op LoadVersion preserves it. The counter applies only to serialized main-thread operations. It does not cover independent definition/brush/creature edits outside VersionManager and does not prove that all assets are unchanged. These paths still need resolver checks and validation before commit.

## Tests and evidence status

The first canonical resolver build passed 0: `g1-resolver-canonical-20261102.log`. The first real probe with 1098-BlackTek ended **FAIL, exit 9**, `g1-resolver-rme-20261102/verification.json`. It found an incorrect refusal of valid zero-weight variants with repeated cumulative chances. The condition was fixed. The real startup warning dialog had to be closed before testing. No second copy was started after waiting. Original assets remained identical.

Current run after the fix and revision integration **PASS, normal exit 0, empty stderr**: `g1-resolver-revision-rme-20261102/verification.json`. `tests/test_procedural_resolver.cpp` exercised real RME/capture/core/brushes at 100×100/500×500: ID repeatability, membership in mapped brushes, mask/global-RNG independence, budget, missing client/brush/valid item, mask tamper, pre-Cancel, and worker refusal. It confirmed protected UID/AID/PZ and temple preservation without map mutation. No-op, successful forced reload, failed reload, and explicit/empty unload also passed. Revision 3→9, old input rejected. Load failure affected only a new private copy of the asset directory under the verified probe marker. The source profile remained identical.

Canonical post-integration build passed 0 with 178 steps: `g1-resolver-revision-canonical-20261102.log`. Resolver-only time was 0.2923 ms / 10,000 positions and 6.7105 ms / 250,000 positions. Fingerprints were 15173774556512220455 and 2039901658534265928. This is not a full generator runtime/memory result or a speed claim. The test started and normally closed RME. Only its startup warning dialog was closed through computer use. No visual review of generated terrain was performed because this package does not draw maps. Claude's separate worktree was retained. The application marked it primary and refused archival. The source checkout is not a managed worktree, so attach was also refused. It was not removed manually.

Still unimplemented: tile/halo snapshot and result binding, DrawOperations placement/border sandbox, complete concrete-diff control, full budget/peak, stale map/selection/relevant-asset validation before commit, atomic commit and its failures, separate Undo/Redo, dialog, and full E2E/round-trip. Do not count placement-ID-only tests as the G1 gate. Do not start G2.
