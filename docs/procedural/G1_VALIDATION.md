# G1: validation of prepared result

Newer section: [G1_ASSETS.md](G1_ASSETS.md) adds check of current catalog values of definition/ground rules/sprite metadata also without VersionManager epoch. Scope below from 2026-11-02 historical in this part. Raw pixels/GPU/cache exclusions, final editor guard, and atomic commit remain open.

Status 2026-11-02: **PASS_G1_PREPARED_VALIDATION_ONLY; full G1 IN PROGRESS**. Section complements [exact selection mask](G1_MASK.md) and [joint preparation budget](G1_BUDGET.md). Concerns checks of current result consistency; does not add map confirmation.

## Contract

`validatePreparedTerrain` in `source/procedural/adapter/terrain_validation.{h,cpp}` takes explicit map, `PreparedTerrain`, current config/profile/mask, and stop token. Works synchronously on main thread, no allocation, yield, random, regeneration, map changes, brush, RNG, or selection cache changes. Does not switch tabs and may check inactive editor's map. Caller ensures object lifetime.

Order of checks and first reported error:

1. `WrongThread` before map/assets read, then `Cancelled` and `NoClient`.
2. `StaleAssets`, when VersionManager epoch or client ID differ from captured. ID compared via allocation-free view.
3. `StaleRequest`, when region/Z, seed, any weight, brush names, or exact request mask changed. Empty passed mask means full rectangle and equals full ones mask. Bits of fields excluded by protection also compared.
4. `InvalidPrepared` for inconsistent sizes/masks/protection, invalid captured parameters, or preview link mismatch.
5. `StaleMap`, when `snapshotMatchesMap` refuses different instance, revision, header, tile copies, or location metadata. Full value check also detects direct data change in context without revision bump.
6. `InvalidPrepared` if current preview payload does not give same fingerprint as prepared result.

With multiple concurrent inconsistencies this order matters. Stop check between stages and every 4096 fields in mask loop. Source scan, fingerprint, and preview link synchronous, no internal poll stop; does not guarantee responsive Cancel during those passes.

## Preview source association

`TerrainPreview` stores additional private `sourceFingerprint_`, computed once during preparation. `matchesSource(snapshot,plan)` checks it along with existing private config/map/asset stamps and plan fingerprint. Includes explicit config fields, extent, map stamps, lengths, and every byte of mask/protection, snapshot context and header, cell count, full config/stamps/client/profile of plan, semantic fingerprint, and concrete ID. Does not use addresses, padding, or global RNG.

Publicly modifiable snapshot and plan cannot accidentally be paired with another kept preview. Change of both mask and profile request copies together does not rewrite private fingerprint. Moved empty preview rejected before buffer read. Source fingerprint does not cover original tile payload; those values independently compared against live map by snapshot. Actual diff buffer has separate `currentFingerprint()` compared to kept result fingerprint.

These 64-bit consistency checks, not cryptographic protection against deliberate C++ data falsification. Earlier concrete result fingerprints unchanged. Extra field fits in existing `sizeof`-based accounting: joint preparation estimate grows 16 B (preparation and preview stage aggregates), no new allocation per tile or per frame.

## Safety limits

Success describes state at call moment. Creates no kept right to commit: future commit must run final checks without yield before mutation and separately check lifetime/proper editor, offline mode, and transaction. For request based on live selection must additionally call `validateGenerationSelection`; passing old mask buffer does not auto-read GUI.

Not shown detection of all independent brush/item/material changes without VersionManager epoch bump. Existing `currentFingerprint` refuses some invalid current definitions but not complete assets catalog stamp. This point remains open. Similarly Undo memory, whole RSS, GUI Cancel/schedule, minimal dialog/rendered preview, atomic commit/faults/Undo/Redo and independent history of manual actions, and GUI/OTBM round-trip 100×100/500×500.

## Delegation and review

Claude implemented only two new validator files in separate checkout, run `b66c62c4-7051-40bd-a31e-d9d6509495d8`. Completed/exit 0, empty stderr, no refusals; report and stdout aligned. Reviewed new files and full tracked diff, which remains identical to previous worker run. Worker did not run build or tests. Codex added binding in existing preview/DrawOperations, CMake, independent tests, verifier, and documentation. Two accepted files copied byte-for-byte; manifest `local-data/procedural-smoke/g1-validation-integration-20261102.json`.

## Independent verification

Canonical Windows x64 `build_ninja.bat`: exit 0, six build steps, `local-data/procedural-smoke/g1-validation-canonical-20261102.log`. Normal EXE SHA-256: `47ab14b0cffa58270c3363f4d63610685246b47fe6413d1528e512b45041c300`.

Full normal Application trial with test wxModule: `local-data/procedural-smoke/g1-validation-rme-20261102`. **PASS, exit 0, normal shutdown, empty stderr, source assets unchanged**. Verifier `g1_verify_preview_probe.py --validation` confirmed 418 current RME objects, 877 G0 database sources, and ten expected G1 differences. Evidence: `verification.json`, `current-verification.json`, log, and build manifest. Probe EXE SHA-256: `e16105a46e615b69c74126b18c077957996da26208aaf25e1dcfce3f3a238871`.

| Size | Validation call allocations | Result fingerprint | Joint preparation estimate (B) |
|---|---:|---|---:|
| 100×100 | 0 | 18297947229083653543 | 7 234 214 |
| 500×500 | 0 | 5645408562528222048 | 168 808 374 |

Single first new error during validation not triggered: in both sizes counter remained zero and actual moved result passed. Moved-from result refused. Each fingerprint identical to stage path and earlier runs. Peak tracked new requests remained 2,923,649 and 69,507,073 B, zero tracked bytes after destruction. Estimates 16 B larger per `sizeof`; not RSS measurement or speedup claim.

**29 refusal cases** passed: different map instance; change of each of nine config fields; profile change; changed/non-binary/short mask; pre-cancel and worker; changed concrete ID, profile, and semantic fingerprint of plan; simultaneous change of captured and passed mask; changed protection/extent and truncated input buffer; modification of item-selection in generated payload; swapped preview of different seed; change of original UID copy; direct AID change of live halo tile at unchanged revision; town count change of location; and bumped revision. After restoring controlled values fresh result passes again, and source, global RNG, and cache of both observed editors remain correct.

Separately passed no client after unload and `StaleAssets` after reloading same client, and validation of results from exact masks of real active and inactive editor. Full earlier probe repeated: four 100×100/500×500 sandboxes, full diff/protection/epoch, 79 sandbox faults, 106 preparation faults and limit bounds, exact masks with holes, cache/events of two editors, selected-context/selected-border, and normal selection and Lua setter with Undo/Redo. These regressions do not cover generator commit. Historical snapshot and border-order trials have unchanged proper sources but older linked EXE; not named new full runs.

Startup waited for informational `Warnings` dialog of private profile: inverted tileset ranges and missing ID references. Via computer-use skill read actual window and closed this dialog with OK button. Continued same process without restart and without assets/product change. Only then test module started full trial. This run with manually-handled startup, not unattended test. Final window inspection confirmed no private instance after normal exit.

`git diff --check` passed. Not checked all independent asset catalog mutations, closure of source editor at later commit, staleness after real generator Undo/Redo, forced stop mid-validation, GUI Cancel, sustained OOM, or full RSS. G2 not started. forgotten.otbm/companions and user maps, Docker, database, or world not used. Shared uncommitted changes kept; no commit/push.
