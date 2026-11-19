# G3-A — read-only import and item/asset compatibility, 2026-11-07

**PARTIAL (G3-A).** Implemented: header, SHA-256, client compatibility, loading a map copy and counting unknown values. Not implemented at this stage: semantic normalizer (G3-B), regions/biomes/topology (G3-C), feature extraction and final report (G3-D). The G3 gate is not passed.

## Included

- Core: source/procedural/core/reference_header.* (SHA-256, ReferenceHeader, checkCompatibility). Test: test_procedural_reference_header (NIST vectors at six chunk sizes, seven compatibility cases) PASS.
- Adapter: source/procedural/adapter/reference_import.*. Reads the header through IOMapOTBM::peekStartupInfo, hashes the file with SHA-256, loads the map through IOMapOTBM::loadMap into Map alone (without Editor and without writing; Map::open is protected and sets the save path, so it is not used), and counts tiles, items and unknown values (an item for which isInvalidOTBMItem() or !typeExists()).
- Probe: docs/procedural/smoke/g3_reference_import_probe.cpp, runner g3_run_reference_import.py. The runner copies the map to git-ignored local-data/, hashes the source and companion *-*.xml before and after, and refuses an output directory outside local-data/.

## Results (1098-BlackTek client)

| Map | Compatibility | Tiles | Items | unknown (ground/items) | Loader warnings |
| --- | --- | --- | --- | --- | --- |
| synthetic g0-metadata.otbm | compatible | 402 (matches known fixture) | 402 | 0/0 | 0 |
| forgotten.otbm (3 424 937 B, OTBM 3, 2048×2048, items 3.57) | compatible | 340 595 | 429 037 | 0/0 | 0 |

forgotten.otbm tiles on floors 0–15: 8, 52, 373, 1828, 4724, 11199, 23397, 118595, 28193, 35280, 28225, 24652, 25550, 18649, 14131, 5739. SHA-256 45031779…2304.

- The source map and forgotten-house.xml have identical hashes before and after (runner). Two independent runs produced byte-identical reference-report.json (SHA-256 0D538B97…77A0).
- Complete runner time (profile copy, EXE startup, client loading, import, hashing): 4.4–4.5 s wall for forgotten.otbm in three runs after the fix (one measurement is not a benchmark; no breakdown). FastOTBM tile loading alone: about 51 ms (loader log). Memory was not measured.

## Not verified

- Only .otbm was copied for import, so companion XML (house/spawn/zones) was not loaded and its effect was not checked. The forgotten-zones directory is not hashed by the runner (only *-*.xml files).
- Items inside containers are not counted or checked for unknown values.
- The compatibility rule (“older minor is OK, newer is reported”) is a project assumption, checked only for equal versions and unit tests. There is no fixture with a mismatched version on a real file.
- Cancellation and memory limit: added on 2026-11-07 (D8), described and measured in G3_REFERENCES.md#memory-limit-and-cancellation-d8; loadMap remains non-interruptible.
- This step has no GUI test because the interface was not changed.

## Correction: blocking wxWidgets dialog (user report, 2026-11-07)

The first three runs (synthetic, forgotten ×2) **were not unattended**: the private EXE displayed a modal “wxWidgets Warning: This application doesn't use a correct manifest… Common Controls Library v6” window, and continued only after manually clicking OK. Cause: g0_build_rme_snapshot.py links the probe without the rme.exe manifest. The same missing manifest produced 19× SendMessage(BCM_SETIMAGELIST) failed lines on stderr. The 9.3 s time from the first version must not be used because it included waiting for the click. Logs had no gap after startup because the window appeared before the first line was logged.

Fix: the runner embeds the manifest from build-ninja/rme.exe into the probe copy (mt.exe from the Windows SDK; missing SDK is an error, not silently skipped), and the probe redirects wx logs to wx-log.txt so no message opens a window. After the fix, four runs (synthetic, forgotten ×3) finish unattended, stderr and wx-log.txt are 0 bytes, and the forgotten report has the same SHA-256 as before the fix. Treat the first three runs as invalid evidence. The valid runs are …-d, …-e and …-f. It was not checked whether the dialog can return with another GUI configuration.
