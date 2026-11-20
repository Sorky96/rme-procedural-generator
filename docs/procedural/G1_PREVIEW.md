# G1: sandbox of placement and bordered diff

Status 2026-11-02: **G1 IN PROGRESS**. This stage does not write changes to edited map and does not cover commit, Undo/Redo, or entire G1 workflow.

**Newer section 2026-11-02:** [G1_SELECTION.md](G1_SELECTION.md) closes selection cache isolation and removes temporary `SelectedContext`. Current sandbox allows safe selected context; protected real changes still refused. The proofs and description of earlier refusal are history of previous stage. Current build and full two-editor regression result in new report.

## Executed contract

`DrawOperations::prepareTerrainPreview` takes indicated map, own snapshot with two-tile halo, concrete `GroundPlan`, explicit limits on change count/memory, and `stop_token`. Works synchronously on GUI thread, no yield, no active brush switch. Checks client, asset epoch, map generation/revision, full snapshot content and location metadata, halo geometry, masks, `empty_only` protection, profile, and membership of placement IDs to current brush.

Own `BaseMap` receives deep data copies. Placement uses existing `TerrainPlacement`, including `BorderBaseGroundId` interpretation. Bordering touches only placements and their eight neighbors, via actual `GroundBorderCalculator`. Generator includes special rule deduplication preserving first occurrence in neighbor order. Default normal drawing path preserves address sorting.

Full diff compares ground, stack, attributes, retained unknown data, entities, and tile metadata. Classification skips only known derivative flags/minimap cache; does not skip unknown bits, selection, modified, or optional border. Each real change outside mask or on protected field rejects entire candidate. Unchanged copies removed from buffer only after bordering done. Source remains read-only.

`TerrainPreview` holds buffer and ordered row-major diff positions. Fingerprint includes actual ID/subtype/selected of generated plain items, stack order, positions, and tile state; rejects unexpected classes/item attributes/entities/metadata instead of hiding them behind ID hash. Recount checks epoch before reading definitions. Buffer access obligates for non-moved object; after move `currentFingerprint` refuses.

## Safety and memory limits

Shared `TileOperations::update` notifies current editor's selection also when updating detached copies. So **each selected tile/ground/item/creature/spawn in affected context causes `SelectedContext` before sandbox cloning and mutation**. This is conscious refusal, not complete G1 selection workflow solution. No change to global `TileOperations` behavior and no notification silencing added.

Budget covers sandbox estimate: BaseMap/grid/cells/nodes/floors structure, payload copies, vectors, changes, and temporary item placement. Before each border maximum 128 new items reserved, stack buffer growth, and scratch. Border removal does not credit resolved limit, vector may retain capacity. Arithmetic checks limit before multiply. This conservative estimate based on standard container layout, **not hard RSS limit or entire work budget**. Snapshot/core/plan/Undo must be billed jointly in next integration.

Token cancellation checked before work and between rows/stages. No Cancel click handling or async sandbox schedule yet. Current synchronous GUI preparation does not prove no UI freeze. Equal-cluster sorting remains shared `ranges::sort`; identity between different standard libraries/platforms not verified.

## Delegation and review

Claude implemented only `ground_border_calculator.{h,cpp}` and separate test of real overlapping rules, in dedicated worktree. Run `91c89455-6c4e-4bcf-88b2-8472a6c83f7f` completed, no refusals, no tests on worker side. Codex reviewed full diff/new file, moved only these files, and independently built and ran test.

Read-only Claude review `4a58a62d-c51c-48d2-b85f-056a069dcc3f` detected selection notification via copies and resolver/preview mismatch for base-ground variants. Added selected context refusal and handling consistent with TerrainPlacement. Report, stdout, empty stderr, and unchanged worker checkout checked; worker did not run commands or tests. CLI output not treated as implementation pass.

## Earlier proofs

Harnesses run from main checkout, after canonical `build_ninja.bat`, with `rtk proxy` and local Python 3. New `OUTPUT` must be under `local-data/procedural-smoke` and not exist yet:

```text
rtk proxy python docs/procedural/smoke/g1_run_input_probe.py OUTPUT --border-order
rtk proxy python docs/procedural/smoke/g1_run_input_probe.py OUTPUT --snapshot
rtk proxy python docs/procedural/smoke/g1_run_resolver_probe.py OUTPUT PRIVATE_PROFILE_DATA PRIVATE_CONFIG --preview
rtk proxy python docs/procedural/smoke/g1_verify_preview_probe.py OUTPUT
```

Verifier assigned to final 2026-11-02 proof set, not arbitrary profile/test. Source profile used: `local-data/procedural-smoke/g0-reference-profile-20261030/data` and adjacent `config.toml`; only pre-prepared private 1098 assets. Builder links current objects of real RME; preview test uses normal Application input and test wxModule. Informational asset warning window closed in private instance via Computer Use. No map opened and nothing saved via OTBM.

- **Final current run:** `g1-preview-reviewed-rme-20261102/verification.json` PASS/exit 0/empty stderr/normal shutdown/unchanged source assets. Also includes Cancel during 500×500 (token from separate thread, not GUI button), exact mask with temple protection (42 changes), epoch reload, refusal of selected empty field and selected ground in halo, and actual `BorderBaseGroundId` variant via factory and TerrainPlacement.
- `g1-preview-reviewed-rme-20261102/current-verification.json`: **PASS_G1_OWNED_BORDERED_PREVIEW_ONLY**, 415 current objects/sources/probe/normal EXE; 877 G0 database sources has only nine intended G1 differences. Canonical `g1-preview-reviewed-canonical-20261102.log` PASS 0; normal EXE `fdfc4632a0d2aef94c4ea1bf3fe50d4bd16c6931f82e91954e827891b560c6e9`. Additional CTest core PASS 1/1 Release. G0 audits not resumed.
- `g1-border-order-current-rme-20261102/verification.json`: PASS, exit 0, actual engine and controlled lower/upper address of rule X, repeat and single duplicate execution; default order preserved.
- `g1-preview-snapshot-regression-20261102/verification.json`: PASS, exit 0, regression of unchanged snapshot/estimator/full compare functions, including 24 capture allocation faults. Later sandbox fixes not covered by this historical binary.
- `g1-preview-protected-rme-20261102/verification.json`: PASS, exit 0, normal shutdown, empty stderr, source assets unchanged. Actual core → resolver → sandbox for 100×100/500×500, uniform water and grass/dirt/water mix without semantic grid modification; repeatable full diff, global RNG, exact/too-small budget, 79 allocation faults, UID/AID/PZ protection at border. This run before final test extension and fixes after review.

Final numbers for mix: 100×100 — 9,983 border items, 10.8362 ms, estimate 6,684,400 B, fingerprint `18297947229083653543`; 500×500 — 247,236 border items, 332.764 ms, estimate 157,330,560 B, fingerprint `5645408562528222048`. These are measurements of sandbox preparation alone in this private trial, not performance comparison, peak RSS, or entire workflow time. Full concrete tile repeats compared besides fingerprints.

Kept failed trials: first `g1-border-order-rme-20261102` rejected by stale object check, first preview ended FAIL wrong empty-tile protection scenario, `g1-preview-real-mixed-rme-20261102` missing include error in harness. `g1-preview-final-rme-20261102` exceeded runner timeout waiting for informational asset dialog; same process unblocked but trial not passed as controlled PASS. All private test windows exited. Failed/incomplete trials not renamed to PASS. Builder now also checks whether probe or source of linked object did not change during build. First two final verifier calls rejected too-strict tag-number parser and unaccounted `.rc` resource file; after harness fix rechecked same artifacts, without re-running runtime.

## Remaining G1 gate

Full work acceptance still requires integration of complete budget and stale mask/selection/relevant assets, resolution of selected context, atomic commit of exactly approved diff via Action/Change/ActionQueue, independent history versus manual drawing and its faults/Undo/Redo, minimal dialog/preview and GUI Cancel, real GUI smoke, and OTBM round-trip 100×100/500×500. G2 not started. User maps and companions not opened, Docker, database, or world not touched.
