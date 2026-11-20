# G2-A — surface core (v2 algorithm), state as of 2026-11-06

**PARTIAL.** The C++ core passes its own test. There is no adapter, resolver, border integration or RME result, so this does not yet pass any G2 gate criterion (“readable in RME”, “no critical border errors”).

## Scope

`source/procedural/core/surface.h/.cpp` is a separate generator alongside v1 (`generation.h`). Types, fingerprints and the v1 resolver were not changed. v2 has its own kinds (`Grass`, `Dirt`, `Rock`, `Shore`, `Lake`, `River`), algorithm version and fingerprint domain.

- Height and moisture fields use value noise with three octaves, integer arithmetic only, and a function of the seed and absolute coordinates.
- Lakes use the lowest cells. Four-connected bodies smaller than `minLakeCells` return to land.
- Rocks use the highest cells and remove small patches.
- Rivers use priority-flood from lakes and the rectangle edge. The route is 4-connected and loop-free. Rivers shorter than `minRiverLength` are rejected. A river ends in a lake or on the generated rectangle edge.
- Shore is a `Shore` ring, width 0–4, around lakes, and does not overwrite rock.
- Grass and dirt use a moisture threshold with a transition band of adjustable width and random per-cell dispersion.
- The mask and preserve operation do not change values in other cells.

## `test_procedural_surface` result

Build: `build_ninja.bat` (rme.exe) and `ninja test_procedural_surface` with an MSVC x64 Release environment. Exit 0, `PROCEDURAL SURFACE PASS`; `test_procedural_core` (v1) also passes after the changes.

| Check | Result |
| --- | --- |
| Parameter, region, mask, cell/output/work-memory limit validation, cancellation | PASS |
| Repeatability, seed influence, no `Preserve` with a full mask | PASS |
| Irregular mask plus preserve does not change other cells | PASS |
| 24 seeds 100×100: every tile has a kind; lake ≤ share limit and ≥ minimum; river ends in lake/edge and is ≥ minimum; land by a lake is Shore/Rock/River; rock patches ≥ minimum (with `riverCount=0`) | PASS |
| 6 seeds 250×250: same checks | PASS |
| Land share in open terrain (threshold set before the run: 10–60%) | 25.6–38.1% (100×100), 28.9–31.6% (250×250) |
| Seeds with lake / river / rock (100×100) | 24 / 23 / 24 of 24 |
| Transition band: grass–dirt edges, width 0 vs 16000 | 96 vs 3053 |
| `bad_alloc` injection at each of 92 allocation points (24×24) | 92 of 92 reported as `AllocationFailed`, no crash |
| Stored fingerprints (byte layout, seed 1729 100×100) | PASS (values from the first successful run) |

Timings (one series, one machine, no repetitions, not performance claims): 100×100 ≈ 1.7 ms, 500×500 ≈ 44–48 ms. Estimated 500×500 working memory: ≈ 10.3 MB (upper estimate from 40 B/cell, not a measurement).

## Not verified

- Terrain appearance. The test checks structural properties, not readability. RME review waits for the adapter.
- One of 24 100×100 seeds had no river (the short route was rejected). This is allowed by the design, not an error, but means that “there is always a river” is not guaranteed.
- Rivers end at the rectangle edge, including when it is only a fragment of a larger map.
- Share thresholds are assumptions made before the run, not plan requirements. Measured ranges are recorded above.
- Actual memory use and time in the full Preview/Place path (with resolver and snapshot) were not measured.
- v2 is not connected to the dialog or resolver. The v1 resolver indexes three brushes and does not accept the new kinds.

Next step: G2-B (adapter: kind → brush mapping, edges through the existing `GroundBorderCalculator`, tests of multi-direction edges and surroundings).

## Change after the first GUI test, 2026-11-07

The first RME review (Natural surface 250×250, several seeds) showed that lakes with sand and rock patches looked good, but dirt among grass was “shredded” and rivers were thin. Changes in `surface.cpp`:

- The grass/dirt transition band is now a smooth moisture-threshold offset (value noise, step 10), rather than per-cell noise. `transitionWidth` is the amplitude of this offset. Grass and dirt patches smaller than 24 cells (a code constant, not a parameter) are absorbed by their surroundings.
- Each river route is widened with its eastern and southern neighbor, so rivers no longer have one-tile-wide segments (lakes remain untouched).
- The dirt share in open terrain is now approximate because the offset and patch removal move it away from the `dirtPermille` quantile.

`test_procedural_surface` after the changes: PASS. Dirt share 24.4–38.6% (100×100, 24 seeds) and 28.0–32.0% (250×250), grass–dirt edges at width 0 vs 16000: 88 vs 251, allocation points 150 of 150 reported. Pinned fingerprints changed because the algorithm result is different: seed 1729 100×100 = 15556827831682428334, 500×500 = 18160721199455356238 (algorithm version in the fingerprint remains 2, v2 had not been released). A repeated `--surface` run on client 1098 (`g2-fix1-surface-20261107`) had all PASS markers (resolver, `P_SURFACE_E2E_PASS` 100 and 250), with `Ppassed: false` for the same reason as before (stderr). The fixes were **not visually assessed**: another RME review was pending. Rivers have no minimum-width test.
