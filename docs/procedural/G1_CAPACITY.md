# G1: generation capacity up to 2000 x 2000

Windows x64, 2026-11-11. Scope: increase supported generation dimensions and joint multi-floor resource limits. Terrain geometry, client item resolution, autoborder, map format and action ordering are unchanged. The user approved 2000 x 2000 as the next target.

## Limits and counting

The dialog policy is defined in `source/ui/dialogs/terrain_generator_limits.h` and used by the dialog and the opt-in capacity probe.

| Limit | Value |
|---|---:|
| Maximum width and height | 2000 each |
| Maximum cells in the input rectangle | 4,000,000 |
| Maximum snapshot context including the two-tile halo | 4,016,016 |
| Joint changed-tile limit | 20,000,000 |
| Preparation estimate budget | 20 GiB |
| Commit estimate budget | 24 GiB |
| Maximum configurable Undo memory | 8192 MiB |

The changed-tile bound covers surface, cave and up to three mountain caps jointly. It counts changed tiles, not individual ground/border/decor items. Item stacks are still limited by memory accounting and client data. Selection uses the same maximum input area and a mask budget of the cell count plus 64 KiB. Preview status reports all generated floors instead of only the surface.

Preparation and commit budgets constrain conservative cumulative estimates; they are not reservations or caps on total process RSS. Loaded assets, existing source maps, unrelated history and renderer allocations remain outside those estimates. Existing memory and history admission checks remain active. The default Undo preference is unchanged. Large jobs may require increasing **File > Preferences > General > Undo memory limit (MB)**; the 2000 x 2000 fixture required 8192 rather than 4096.

The requested rectangle must still fit the map. On a default 2048 x 2048 map, an origin of (100,100) with a 2000-square request exceeds the bounds: move the origin within the map or enlarge the map using its properties. The generator does not silently resize or relocate the map.

## Measurement before choosing the budgets

Private empty maps, seed 1729, loaded 10.98 fixture, natural surface, rock share 300 permille, vegetation, four cave entrances per 65536 cells, three requested mountain caps, minimum first component 64 and cap area limit 10%. These pilot jobs used a deliberately loose 64 GiB estimator budget to measure requirements; they did not bypass Undo admission. All passed exact payload comparison, Place, full Undo to an empty source and Redo. The 500 case retained four floors; 1000 and 2000 retained five.

| Side | Changed tiles | Preparation estimate (GiB) | Commit estimate (GiB) | Peak sampled private bytes (GiB) |
|---|---:|---:|---:|---:|
| 500 | 292,563 | 0.777 | 1.001 | 0.760 |
| 1000 | 1,263,025 | 3.986 | 4.991 | 1.649 |
| 2000 | 5,461,231 | 15.930 | 20.218 | 5.373 |

These are capacity measurements, not claims of a speed improvement or guarantees for every preset/source map.

## Production-policy verification

The final opt-in probe uses the same policy constants as the production dialog. It prepares a real loaded-client 2000 x 2000 result on five floors, compares every committed changed tile with its preview, verifies that a 4096 MiB history refusal leaves no map tiles or history entries and preserves the prepared preview, retries that same preview with 8192 MiB, undoes every floor to the empty source, and redoes every floor with full payload comparison.

```text
CAPACITY_START	side=2000
CAPACITY_FLOOR	z=7	changes=3992212
CAPACITY_FLOOR	z=8	changes=1034202
CAPACITY_FLOOR	z=6	changes=400552
CAPACITY_FLOOR	z=5	changes=31819
CAPACITY_FLOOR	z=4	changes=2446
CAPACITY_PREPARED	side=2000	floors=5	changes=5461231	estimate=17105101046	ms=53045
CAPACITY_HISTORY_REFUSAL_PASS	undoMiB=4096
CAPACITY_COMMITTED	estimate=21708631158	transition=4603530112	undoMiB=8192	ms=12391
CAPACITY_PASS

```

Final process memory: peak working set 5.326 GiB; peak sampled private memory 5.403 GiB. This includes the failed-history attempt and retry in the same process. The private test changes only its copied configuration. Source client assets are unchanged, process exit is zero, and only known hidden-window wx SetFocus warnings are tolerated.

The production dialog control probe verifies the 2000 x 2000 input range. The full procedural gate also passes: **18/18 core tests, --commit PASS, --surface PASS, SUITE_PASS**. The pinned disabled-mountain seed fingerprint remains asserted by the unchanged surface test. No local-data file is committed.

Reproduce after rebuilding `rme` and the core targets:

```powershell
py -I docs/procedural/smoke/g1_run_resolver_probe.py local-data/procedural-smoke/capacity-2000-new-run PROFILE_DATA CONFIG_TOML --capacity 2000
```

Use a new output directory under local-data for each run. The capacity probe is opt-in because of its time and memory requirements; the standard core suite does not silently run a four-million-cell job.

## Not verified

The capacity probe drives preparation and DrawOperations directly in a private app, rather than clicking the native dialog. Native 2000-square Preview/Cancel responsiveness, render performance on the generated map, overwrite of an existing populated region, every style/client combination, and saving/reopening this large map were not tested. Existing empty-only protection and map bounds remain in force. No OTBM serialization code changed.
