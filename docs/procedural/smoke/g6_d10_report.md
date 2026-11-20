# D10: rotated and mirrored copies in I53 (2026-11-19)

Binding preregistration: `7bd6968`, D10 in `G6_METRICS.md`. I53 thresholds
(shared >= 16 unique windows AND containment >= 100 per mille for one reference),
the G4 `similarity` code, generation, ranking A, I55 files and the OTBM format are
unchanged. This report does not update `TECH_DEBT` or `G6_GATE_STATUS`; the
coordinator owns those and the full regression.

## Fragment rule, fixed before measurement

Committed as `18f3126` (`g6_measure_d10.py`) before the first run. In each
reference's z07 class raster, produced by `g6_build_copy_index.py` from the same
inputs as the index, take the 128x128 block on the 128-aligned grid with the most
horizontally or vertically adjacent present-cell pairs of different classes; ties
by smallest y, then x. The rule reads class content only.

| Reference | Raster z07 SHA-256 | Fragment (x, y, side) | Class-boundary pairs |
| --- | --- | --- | --- |
| M06 | `0869947c…c8a9d2` | 896, 512, 128 | 4733 |
| M19 | `e78eb6ac…4817b8` | 512, 128, 128 | 3843 |

Each fragment is transformed by all eight symmetries (identity, rotations by
90/180/270 degrees, mirror X, mirror Y, transpose, anti-transpose), fingerprinted
by `copy_index` as an independent candidate and assessed against the index with
the I53 arithmetic: generic windows (the M06/M19 intersection) removed from the
candidate, then shared unique hashes and candidate containment per reference. The
negative control is a synthetic 40-site Voronoi terrain (seed 1729, classes 0..4)
under the same symmetries. Each measurement runs twice and the reports must be
byte-identical.

## Old I53 (`i53-index/1`, before this change)

The rebuilt old index is byte-identical to the I53 report:
`ce39e905317ce1891a008fcb2f4e03c2d12c329247ba4cfed65138361fc874b8`, 1544 generic.
Both report runs had SHA-256 `231c5ee5…a4c378`.

| Source | identity | rot90 | rot180 | rot270 | mirrorX | mirrorY | transpose | antitranspose | Copies |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| M06 shared/‰ | 3097/1000 | 1/0 | 0/0 | 2/0 | 4/1 | 3/0 | 1/0 | 1/0 | 1/8 |
| M19 shared/‰ | 3050/1000 | 7/2 | 0/0 | 5/1 | 5/1 | 2/0 | 2/0 | 3/0 | 1/8 |
| Control, max over refs | 26/10 | 21/8 | 28/11 | 21/8 | 19/7 | 15/6 | 18/7 | 21/8 | 0/8 |

D10 criterion: **FAIL**. The window hash is a raster-order polynomial, so only
the untransformed copy matches. All candidates had sufficient samples
(2343..3220 windows after the filter).

## Change

Only `copy_guard` changed. `classFingerprint` keeps the frozen G4 parameters
(16x16 windows, at least 50% present, content sample modulo 4, uniform windows
dropped) and the same five natural classes. Each window's value is the minimum
of its eight orientation polynomials, then mixed. The eight orientations are the
choice of which axis carries which base and an ascending or descending exponent
on each axis. All eight are rolled in O(1) per window (ascending sums use the
modular inverse of the odd base), so a window and every rotation/mirror image
get the same hash. The content sample and uniform detection use this canonical
value, so they are symmetric too. The G4 `computeFingerprint` is no longer used
by I53 and is unchanged for G4.

Working memory per call is about `512*columns + 12*width*8` bytes plus the kept
hashes, within the unchanged `copyWorkingBytes` estimate (`64*cells+65536`).

Index versioning: fingerprints are incompatible with the old ones, so the
index header is `i53-index/2 g4-natural-v1 d4-canonical-v1` and `copy_index`
prints `i53-fingerprint/2 d4-canonical-v1` before the fingerprint. The builder
refuses a `copy_index` without that line. `i53-index/1` is refused even with a
valid checksum. `candidate_rank --copy-index` and the Surface dialog state that
the file predates rotation/mirror-invariant fingerprints and must be rebuilt
with `g6_build_copy_index.py`. The `candidate_rank` similarity line now reports
`i53-index/2 g4-natural-v1 d4-canonical-v1`.

## New I53 (`i53-index/2`)

Two independent index builds were byte-identical:
`cf00fe397b643172313941c49c424343e72a29d9373deaf9505f44cb0dacd98f`. Generic
unique hashes: 1244. M06: 227076 before, 225832 after; M19: 138200 before, 136956
after. Both D10 report runs had SHA-256 `96f0486f…a8d3f7`.

| Source | Windows before/after | Shared / ‰ (own reference) | Other reference | Copies |
| --- | --- | --- | --- | --- |
| M06, all 8 symmetries | 3063 / 3061 | 3061 / 1000 | 0 / 0 | 8/8 |
| M19, all 8 symmetries | 3164 / 3141 | 3141 / 1000 | 0 / 0 | 8/8 |
| Control, all 8 symmetries | 2427 / 2404 | M06 32 / 13 | M19 22 / 9 | 0/8 |

D10 criterion: **PASS**. All eight symmetries give the same values, as intended.

Generator matrix (`g6_measure_copy.py`, frozen I53 settings: seeds 1001..1032,
N=1, side 512, I51 settings with `--g5-levels`, `g6-bands-v1`), two runs per
profile, byte-identical:

| Profile | Hits | Not evaluated | Windows after | M06 shared / ‰ | M19 shared / ‰ |
| --- | --- | --- | --- | --- | --- |
| m06 | 0/32 | 0 | 24851..35010 | 272..422 / 8..13 | 77..138 / 2..4 |
| m19 | 0/32 | 0 | 25143..36365 | 205..347 / 5..12 | 79..135 / 2..4 |

Full digests: m06 `e7c8a08f377ad6ad401208a5e558b8463f4de421f86f89c2e2125f37778a204f`,
m19 `a6a1c1a8e048e33de5d83d2052577dba8c1b431b441a82ab3bd0fe85a0bf3953`.
The existing hardFail of m06 seed 1032 (geometry/reachability/repair, not I53)
remains; all other rows have hardFail=0. The old binary with the old index
reproduced the historical I53 TSVs (`615c4614…`, `4201efab…`). With the copy
columns and the similarity/reference lines removed, old and new TSVs are
identical for both profiles: scores, hardFail, winners and seeds did not change.

## Consequence for I55

The I55 scripts and frozen intake require `i53-index/1` and the old similarity
line. They were not changed. An I55 replay needs the pre-D10 binary and index,
or a coordinator decision. Indices kept under earlier scratchpads
(`i53-index-integrator`, synthetic `copy-fixture.i53`/`clean-fixture.i53`) are
`i53-index/1` and are now refused with the rebuild hint.

## Validation

- Canonical `build_ninja.bat` (existing main-checkout `vcpkg_installed`,
  manifest install OFF, no dependency installation): BUILD SUCCESSFUL.
- Building every target afterwards stopped only at `test_universal_rotator`:
  unresolved `TerrainGeneratorDialog::TerrainGeneratorDialog(wxWindow*)` from
  `main_menubar.cpp`. No CMake file changed here; that target's source list does
  not include the dialog. It was not investigated further.
- CTest without that target: 34/34 PASS, including `test_procedural_copy_guard`
  (all eight symmetries on a 48x80 raster give identical fingerprints and are
  detected; independent terrain clear; legacy header refused even with a valid
  checksum; rebuild hint only for `i53-index/1`), `test_candidate_rank_cli`
  (seed-1729 100x100 fingerprint `17159159863505677800` unchanged; legacy index
  exits 2 with the rebuild message), `test_g6_copy` (builder refuses a
  `copy_index` without the D10 version line) and the unchanged I55 tests.
- The final builder was rerun twice after its last change: same index SHA-256
  `cf00fe39…`, same D10 report SHA-256 `96f0486f…`. The pre-D10 `copy_index`
  is refused by the builder. `candidate_rank` with the real old index exits 2
  with the rebuild message.
- Two of the agent's intermediate runs failed and are not counted: the first
  `classFingerprint` had a uint32 wrap in the sliding present-cell sum (caught
  by the existing sparse-window test) and returned BadFormat before Cancelled;
  the first builder version check compared the Windows `\r\n` line literally.
  The D10 and matrix numbers above come from binaries built after both fixes.

## Reproduction

```powershell
rtk proxy py -I docs/procedural/smoke/g6_build_copy_index.py build-ninja/source/procedural/copy_index.exe C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-kindmap-20261108a C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10 scratchpad/d10-index
rtk proxy py -I docs/procedural/smoke/g6_measure_d10.py build-ninja/source/procedural/copy_index.exe scratchpad/d10-index scratchpad/d10
rtk proxy py -I docs/procedural/smoke/g6_measure_copy.py build-ninja/source/procedural/candidate_rank.exe C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10 scratchpad/d10-index/references.i53 scratchpad/d10-matrix
```

Not claimed: manual GUI acceptance of the rebuild message, the Surface series
probe with the new fixtures, G5 `--i47` and `SUITE_PASS` (coordinator regression),
detection of partial copies with altered content, or rotations by angles other
than multiples of 90 degrees.
