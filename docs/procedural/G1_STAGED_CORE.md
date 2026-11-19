# G1 — core regression after Preview staging

Run `g1-staged-core-rme-20261104` detected a regression. Synchronous preparation correctly rejected a budget below infrastructure cost, but had already allocated a coroutine frame. The existing test required rejection without allocation. The run ended with code 9 at `Joint infrastructure limit allocated or ran`. Before rejection, both size fingerprints matched the historical run.

Preflight checks now run before creating the continuation on both call paths. The continuation receives the remaining budget, reservation sizes, and asset revision. The first resume rejects a revision change before reading the map. The dialog handles rejection before scheduling its first timer.

## Result after the fix

`build_ninja.bat` succeeded. Run `g1-staged-core-final-rme-20261104` reproduced all core-test markers and ended with code 0. It passed 80 sandbox fault attempts, 108 preparation fault attempts, 112 commit fault attempts, and 15 reservation fault attempts. Checks also covered 29 validation refusals and 39 asset-catalog mutations. Preview, mask, and commit fingerprints for 100×100 and 500×500 match historical results. The verifier confirmed 421 objects, 421 compilation sources, and unchanged input assets.

Stderr contained one deferred `SetFocus` warning. The run status is **PARTIAL_G1_STAGED_CORE_FOCUS_WARNING**. A refusal for a rectangle outside the map was also checked in a normal newly built RME with a separate private profile. Validate and Place were disabled. After restoring X=100, Preview showed 10,000 tiles and Cancel closed the dialog. This short GUI test did not repeat the round-trip or process measurements.

## G1 evidence status

| Criterion | Evidence |
|---|---|
| Exact diff, determinism, and one Undo entry | Current core test for both sizes. Native Place/Undo/Redo was checked on the previous build. [Round-trip](G1_ROUNDTRIP.md) |
| Mask, protection, and target-change rejection | Current core test covers exact mask, asset catalog, and target refusals. Native refusal and invalidation paths have separate runs. [Refusals](G1_REFUSALS.md), [Invalidation](G1_INVALIDATION.md) |
| Preparation and commit budget | Current core test covers exact limits and refusals. Process-memory and GUI-gap measurements predate the preflight fix. [GUI measurement](G1_CANCEL.md) |
| Save and reopen | 100×100 and 500×500 on OTBM 5 in the previous build, with five XML files and post-read data comparison. [Round-trip](G1_ROUNDTRIP.md) |

Full **G1 IN PROGRESS**. The full GUI matrix and process measurements were not repeated on the new binary.

## Odtworzenie

```text
rtk proxy <PY> docs/procedural/smoke/g1_run_resolver_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --commit --visible
rtk proxy <PY> docs/procedural/smoke/g1_verify_staged_core.py local-data/procedural-smoke/NEW_RUN
```

`<PY>` is `C:/Users/Sorky/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -X utf8`. Close the private-client resource message after startup. The probe tests synthetic editors.
