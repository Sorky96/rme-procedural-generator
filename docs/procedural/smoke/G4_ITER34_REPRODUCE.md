# D21: reproduce G4 tables from iterations 3 and 4

One command reproduces the style_styles (iteration 3) and water_eval (iteration 4) tables from [G4_GATE_STATUS.md](../G4_GATE_STATUS.md). Criteria were recorded before implementation in [G6_METRICS.md](../G6_METRICS.md) (“D21”, commit e3e6f8b).

```text
py -I docs/procedural/smoke/g4_reproduce_iter34.py <NEW_SCRATCHPAD_DIRECTORY> [--local-data DIR] [--current-bin DIR] [--vcvars FILE]
```

The final output line is D21_PASS or D21_FAIL. The output directory must be new or empty. The script writes nothing to the repository or local-data.

## What it does

1. Checks that all inputs and historical reports exist. If something is missing, it prints missing paths and exits before building.
2. Builds the tools from the commits that produced the tables: style_styles from 269da1b and water_eval from 99f617c. It obtains source/procedural/core and *_main.cpp through git archive, then compiles them with one cl invocation using release flags from build_ninja.bat and /Brepro (without a PE timestamp). It does not use CMake or vcpkg; the core at those commits uses only the standard library. MSVC is found through vswhere, and --vcvars can specify it.
3. Runs each tool twice on the same inputs and compares reports byte by byte, both between runs and with the historical report.
4. Compares numbers quoted in G4_GATE_STATUS.md tables and text with the reproduced report: 15 checks for iteration 3 and 10 for row 4b.
5. Writes params.json (SHA-256 for every input, historical report and EXE, commit, source, flags, compiler version, command and repository HEAD) and summary.json.

--current-bin DIR additionally runs newer binaries (for example, build-ninja/source/procedural from the main checkout) on the same inputs. This run is informational only and does not affect the verdict.

## Inputs (explicit, in <local-data>/procedural-smoke)

| Input | Path | Evidence |
| --- | --- | --- |
| manifest | g4-split/manifest.tsv | only G4 manifest |
| fingerprints | g4-split/fp-c/<alias>.fp | last set before styles-1.json (fp-c 09:57, report 10:42) |
| style statistics | g4-dood-20261108b/<alias>/run/analysis/ | only directory with decor-context-surface.txt before 10:42 |
| water shapes | g4-water-20261108a/<alias>/run/analysis/water-shapes-surface.txt | only shape directory before water-1.json (12:22) |
| historical reports | g4-fit/styles-1.json, g4-fit/water-4b.json, g4-fit/water-4a.json | recorded during the iterations |

Directory selection is based on file timestamps because historical commands were not recorded. Byte-identical output to the historical report confirms the selection. g4-dood-20261108b lacks ground-keys-surface.csv. style_styles at 269da1b does not read that file, and params.json records it as null.

## Result (2026-11-19)

Five script invocations, each with two tool runs, in this session's scratchpad directories all ended in D21_PASS. The last two used the final script version. Compiler: MSVC 19.51.36257 x64 (Visual Studio 18 Community). water_eval EXE hash is stable between builds. style_styles EXE hash differs between builds in different directories despite /Brepro, and the cause is unknown. Reports are nevertheless identical, and the criterion concerns reports.

- style_styles 269da1b: both runs are identical and produce styles-1.json byte for byte (SHA-256 de465dde…); 15/15 values of the iteration-3 table match.
- water_eval 99f617c: both runs are identical and produce water-4b.json byte for byte (SHA-256 c179f0e5…); 10/10 values of row 4b and its text match.
- Row 4a: **cannot be rerun**. The model before 99f617c (first meander variant, 12:22–12:23) was not committed to git. The script checks only the saved water-4a.json.
- **Deviation (table unchanged):** row 4a gives P1 99.997% in the table, while the saved report gives 999 669 / 999 730 ppm = 99.994%. Other row-4a values (meander 8, T 1398, S 70, 18/9, 15/6, 0 islands) match. The check passes as “documented deviation” only when the report still has exactly 99.994%.
- Current binaries in the main checkout (build-ninja, built 2026-11-09 13:41, build commit unknown): water_eval produces water-4b.json byte-identically, while style_styles produces a different report because the generator and tool changed after iteration 3. Current binaries therefore cannot reproduce iteration 3. A build from 269da1b is required.

One invocation with --current-bin takes 3.5–7 min depending on machine load. It consists of two builds of about 20 s and three style_styles runs of about 47 s.

## What this does not check

- The statement “0 failures in 1200/1200 seeds” from iteration 4 is not in the report (river_invariant_failures = 0 for 31 seeds in each fold), so the script does not verify it.
- Run times from the tables (“44 s”) are not compared.
- Inputs from local-data come from older import-probe runs. The script does not regenerate them, only records their hashes.
- Compatibility was checked with MSVC 18 Community on this machine. Byte compatibility with another compiler was not tested.
