# G1 — Place → Undo → Redo → Save repeatability for 500×500

2026-11-05. Two additional native RME runs extend the [first measurement](G1_ROUNDTRIP.md#place-undo-redo-and-save-measurement-for-500500). Every process ended with code 0. Independent verifier `--roundtrip-timing` checked four records from each run against the GUI log and returned **PARTIAL_G1_GUI_ROUNDTRIP_TIMING_FOCUS_WARNING**. Each stderr contained one known `SetFocus` warning with code `0x00000057`.

## Ranges from three runs

| Operation | Event-to-observation [ms] | Maximum GUI timer gap [ms] | Peak working set [MiB] | Working-set increase [MiB] | Peak private commit [MiB] | Private-commit increase [MiB] |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Place | 403,4–419,4 | 428,1–544,4 | 912,32–912,63 | 110,23–110,56 | 760,05–760,72 | 114,82–115,25 |
| Undo | 77,0–78,1 | 89,3–119,2 | 839,60–841,54 | 0,26–2,44 | 687,86–691,78 | 0,30–2,70 |
| Redo | 72,1–76,8 | 85,6–94,9 | 839,82–841,16 | 0,34–0,65 | 688,02–691,96 | 0,34–0,70 |
| Save | 188,0–190,0 | 198,4–213,3 | 839,41–841,30 | 0,04–0,04 | 687,02–691,50 | 0,03–0,10 |

Timing ends on the first 20 ms observer tick that sees a history or save-flag change. Maximum timer gap is a separate measure because it may begin before the command, for example while handling a menu. The memory sampler requests a 5 ms interval. The largest recorded gap was 6.61 ms, so shorter peaks may fall between samples. Increase means the sampled peak minus that operation's baseline. Host load and first-run cache state were not recorded.

## Build and conditions

`build_ninja.bat` ended with code 0 after adding the existing Ninja to the session PATH. Ninja reported `no work to do`. Production EXE SHA‑256 in every run was `15c25557d955a13831e665b27db793f4086478036fd52025a13cef490d592451`.

Manifests have identical hashes for 421 compilation inputs and 421 production objects, plus identical hashes for the probe source, runner scripts, input assets, and profile configuration. Each run compiled `input_probe.obj` separately and linked a private EXE. Byte comparison of private EXEs showed only a difference in two recordings of the same link timestamp.

Each run used synthetic OTBM 5 with five fixture tiles, a private 1098-BlackTek profile, and a 512 MiB history limit. Generator settings were empty 500×500 rectangle from (100, 100, 7), seed 1729, weights 3/2/1, and grass, dirt, and clear-water brushes. After Preview and Validate, Place on map and Edit → Undo, Edit → Redo, and File → Save were used. Reproduction instructions are in the [round-trip report](G1_ROUNDTRIP.md#place-undo-redo-and-save-measurement-for-500500).

## Evidence and verification scope

| Katalog w `local-data/procedural-smoke/` | SHA‑256 surowego CSV | SHA‑256 prywatnego EXE |
| --- | --- | --- |
| `g1-place-timing-20261105` | `cf1e3db8a72893a9d11c481f3288b530fb5e91404527afb83dcca8671dca8e17` | `f1c30a1db3c1e508da0cf47d2c04c3041a4632ad225d9ee907d0fdc1d1427e22` |
| `g1-place-timing-repeat-2-20261105` | `6cf0555766c0317901ca5f7bb40e4ce1890ebaa7b3d4f35f337c611ff36f6c1e` | `9a088da7639260ecc6771901f35c5cc82ee3dd84a2e9693a54a7851b2f98038d` |
| `g1-place-timing-repeat-3-20261105` | `8c5eed709dc31ca8abb98c13e34ca53bcab15c8e25b1eeca7572a1ddc8b4d2ce` | `0bb25f3ce7aae1864d59c9404e6f1e2923c9cd5fc0f7a842b929148801435332` |

Each directory contains `current-verification.json` and `run/roundtrip-timing.csv`. [Aggregate CSV](benchmarks/g1-place-timing-repeatability-20261105.csv) contains all 12 rows with run IDs and memory in bytes. Aggregate CSV SHA‑256 is `de481061ee3efeaff7e33f63383909f857da222474d12a8cf16c868be8a344bb`.

![Validated preview of 250,000 tiles from the second run](screenshots/g1-place-timing-repeat-2-20261105.jpg)

The measurement mode checks history, tile count, and five fixture tiles. Save observes the cleared dirty flag and presence of a file already created by the baseline save. Therefore, outside the timing measurement, SHA‑256 of saved synthetic OTBM files was compared with the preserved full-run result `g1-roundtrip-500-post-preflight-rme-20261104`. All four files have SHA‑256 `b02588b46490c486b4eacc94cff1c41d5da8e805f51aec47b136774b004f5d56`. In the third run, the pre-Place hash was `d1fa55bffb9a422e0a5e4109d1fef15f5052287a289fa8a6293925d1ce616671`.

Post-save comparison also confirmed identical sizes and SHA‑256 for all five non-empty XML companions across the three timing runs and the preserved full 500×500 round-trip. House, monster, NPC, waypoint, and zone files were compared. Reopen evidence comes from earlier OTBM 5 runs. `SetFocus` diagnosis is deferred. **G1 IN PROGRESS**. Resource-gate assessment needs agreed thresholds.
