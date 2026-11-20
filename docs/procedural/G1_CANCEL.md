# G1 — Cancelling Preview preparation

## Retest after moving preflight checks ahead of allocation

The `g1-cancel-post-preflight-rme-20261104` run on the canonical executable `15c25557…` returned **PASS_G1_GUI_IN_FLIGHT_CANCEL_ONLY**. Cancel arrived 188.9 ms into the 500×500 Preview. The observer confirmed cancellation 50.7 ms later. Validate and Place remained disabled. Retrying published seed 1728 and passed Validate. The map, sentinel tile, and history stayed unchanged throughout.

| Full 500×500 Preview | Time to READY [ms] | Max. GUI gap [ms] | Peak WS [MiB] | WS increase [MiB] |
|---|---:|---:|---:|---:|
| Retry, seed 1728 | 1243.0 | 167.0 | 799.1 | 80.9 |
| Seed 1729 | 1279.5 | 168.1 | 800.1 | 80.4 |
| Seed 1729 | 897.4 | 157.2 | 800.5 | 79.6 |

The longest GUI gap was 168.1 ms, versus 180.9 ms in the first staged run and 546.1 ms in the synchronous baseline. Working-set growth was similar to both earlier runs. Three cycles establish the order of magnitude. The observed difference between staged builds is insufficient to establish a speedup from the preflight change.

The run returned exit code 0 with empty stderr. The verifier confirmed 421 objects, 421 compilation inputs, unchanged assets, and 14 CSV records. Place timing and the save/reload round trip were outside the scope of this run.

[Raw CSV](benchmarks/g1-ui-cancel-post-preflight-20261104.csv), SHA-256 `12c8b07cf3fa8e6b8f6d604c4a1fae4817d1c2b0e014f1f10e2f6b87dac38681`. [Screenshot of the successful retry](screenshots/g1-cancel-post-preflight-20261104.jpg).

## First staged run

2026-11-04. **PASS_G1_GUI_IN_FLIGHT_CANCEL_ONLY**. Overall status is **G1 IN PROGRESS**.

## Native 500×500 test

The generator opened a synthetic map with one sentinel outside the region using the private 1098-BlackTek profile. The observer confirmed that Cancel arrived in the “Preparing preview” state 214.8 ms after Preview started. After another 52.5 ms, the timer observed “Preview cancelled” with Validate and Place disabled. The map identity, revision, single sentinel, clean state, and empty history were preserved throughout.

Retrying in the same dialog showed 250,000 changes for seed 1728 and passed Validate. The cancelled seed 1729 never appeared as the result of that request. The next two full cycles used the baseline seed 1729.

![State after cancelling during a 500×500 Preview](screenshots/g1-cancel-interrupted-20261104.jpg)

[Screenshot of the retry with seed 1728](screenshots/g1-cancel-retry-20261104.jpg)

## Measurements

| 500×500 cycle | Preview to READY [ms] | Max. GUI gap during Preview [ms] | Initial WS [MiB] | Peak WS [MiB] | WS increase [MiB] | WS one second after Cancel [MiB] |
|---|---:|---:|---:|---:|---:|---:|
| Retry, seed 1728 | 989.4 | 170.5 | 716.1 | 797.4 | 81.3 | 717.4 |
| Seed 1729 | 1426.5 | 154.0 | 718.3 | 798.3 | 80.0 | 718.0 |
| Seed 1729 | 1566.4 | 180.9 | 719.0 | 798.8 | 79.9 | 718.7 |
| Baseline, three cycles, seed 1729 | 523.5–528.4 | 536.5–546.1 | 724.2–726.2 | 804.6–805.6 | 79.3–80.5 | 724.5–728.8 |

The longest GUI gap during a full Preview fell from 546.1 to 180.9 ms. Time to READY increased. Working-set growth stayed near the baseline. The process started with a lower working set and reached a lower absolute peak. The interrupted Preview peaked at 774.3 MiB, 60.7 MiB above its starting point. The longest timer intervals were 66.5 ms before Cancel and 86.9 ms while it was handled.

After a full Preview, the observer saw Validate after 89.2–94.6 ms and the window close after Cancel in 111.1–113.8 ms. The largest timer gaps for those intervals were 129.8 and 134.7 ms respectively. Times include waiting for the timer to read the state. The memory probe had a maximum sampling interval of 6.8 ms. The comparison applies to this run on one synthetic source map with two seeds.

[Raw CSV with 14 records, including private commit](benchmarks/g1-ui-cancel-20261104.csv). SHA-256 `64e351f6f08074c8f3ae42c701a0883e55492f232a9a005f37871a949b715cb1`.

## Verification scope

`build_ninja.bat` completed a Windows x64 Release build. The `g1-cancel-final-rme-20261104` run returned exit code 0 with empty stderr and unchanged input assets. The independent verifier checked 421 objects, 421 compilation inputs, both executables, and the CSV hash. The earlier `g1-cancel-rme-20261103` run timed out before the scenario and was excluded.

This run's coverage excludes closing the window or changing inputs during work, refusal after changing the target, Place/Undo/Redo with this binary, dense maps, leaks, and OTBM round trips with companion files. Historical core and G0 tests retain their earlier coverage.

## Implementation

The dialog owns one preparation continuation and a one-shot timer. The live-map snapshot, semantic core, resolver, and sandbox operations run on the main thread. The GUI event loop regains control between stages and between sandbox loop chunks. The 8 ms threshold applies at sandbox checkpoints. Snapshot, preflight checks, final validation, and cleanup have separate costs.

Cancel, input changes, and window close discard the task and its partial result. Window close releases the task before closing. The timer resumes only the current continuation and carries no result, so a discarded task cannot publish a preview later. Each resume checks the current target. The sandbox checks the asset revision, and publication still requires validation of the target and fingerprint.

The synchronous variant uses the same stages. Preparation tracks a single budget counter. Place uses the existing atomic diff and history publication path.

## Reproduction

```text
rtk proxy <PY> docs/procedural/smoke/g1_run_dialog_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --cancel-preview
rtk proxy <PY> docs/procedural/smoke/g1_verify_dialog_probe.py local-data/procedural-smoke/NEW_RUN --cancel-preview
```

`<PY>` is `C:/Users/Sorky/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -X utf8`. After creating the synthetic map, open Map → Generate terrain and set both dimensions to 500. Start Preview and press Cancel during preparation. When the cancellation message appears, set seed 1728 and run Preview → Validate → Cancel. In two subsequent dialogs, run full cycles with default seed 1729 and a 500×500 area. The observer records source and history state, GUI timer intervals, and the process working set and private commit. The comparison uses the [baseline measurement](G1_RESOURCES.md).
