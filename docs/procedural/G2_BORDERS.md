# G2-B — surface resolver and existing borders, state as of 2026-11-06

**PARTIAL.** The v2 resolver and the existing `GroundBorderCalculator` work together on the real 1098-BlackTek client in the probe. They are not yet connected to Preview, Commit or the dialog (G2-B2), so there is no RME result and the criteria “readable in RME” and “no critical border errors” are not passed.

## Scope

- `source/procedural/adapter/surface_resolver.h/.cpp`: `SurfaceGrid` → item identifiers. The profile order is `grass`, `dirt`, `rocky ground`, `sand`, `sea` (lake), `sea` (river). Brush validation follows the G1 resolver, with a separate random stream per kind. G1 resolver code and `GroundProfile` (three brushes) were not changed; some validation is duplicated.
- `tests/test_procedural_surface_resolver.cpp`: a probe (wxModule) built and run by `g1_run_resolver_probe.py --surface`. The map is synthetic and contains no map files. Borders are calculated only by `GroundBorderCalculator::calculate(…, true)`, with no custom logic.

## Result

Run `g2-surface-resolver-2-20261106` (profile `g0-reference-profile-20261030`, input hashes unchanged, empty stderr, exit 0): `SURFACE_RESOLVER_PASS`. The first run, `g2-surface-resolver-20261106`, failed while loading the client (`build-ninja/config.toml` has empty client paths) before resolver code ran. It remains in `local-data` without passing.

Assertions covered plan repeatability, provenance (client, seed, asset revision), each cell's item belonging to the brush assigned to its kind, the mask not changing another cell's variant, borders not replacing or losing placed ground, a second border pass not changing stacks, borders not reaching beyond a one-tile halo, and tiles with more than one border inside the area.

| Size, seed | Tiles | With border | With ≥2 borders | Max. stack | Different-brush pairs | Pairs without border | Changed halo tiles | Resolver | Borders |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 100, 1 | 10 000 | 2151 | 1076 | 5 | 3365 | 0 | 95 | 0.50 ms | 6.2 ms |
| 100, 2 | 10 000 | 2325 | 1064 | 5 | 3112 | 0 | 0 | 0.47 ms | 5.4 ms |
| 100, 3 | 10 000 | 2378 | 1191 | 5 | 3476 | 0 | 0 | 0.48 ms | 5.5 ms |
| 100, 4 | 10 000 | 2265 | 979 | 5 | 3029 | 0 | 0 | 0.46 ms | 5.3 ms |
| 250, 1729 | 62 500 | 12 866 | 7075 | 5 | 20 943 | 0 | 50 | 2.7 ms | 38 ms |
| 500, 1729 | 250 000 | 47 699 | 26 106 | 6 | 77 147 | 0 | 0 | 10.3 ms | 165 ms |

Times are single runs, without repetitions or `DrawOperations` overhead.

## Findings

- **Borders on tiles outside the area.** A grass ring around the area was placed in only two runs (100×100 seed 1 and 250×250 seed 1729). In those runs, an area edge with a different kind than its neighbor added borders to halo tiles outside the area: 95 and 50 tiles. The other runs had no halo tiles, so “0” there says nothing about behavior next to a neighbor. The current G1 pipeline rejects such a preview (`OutOfMask`: “Borders would extend outside the allowed area”), so after v2 integration, generation next to existing terrain will often be rejected. Resolved on 2026-11-07 (see [G2_GATE_STATUS.md](G2_GATE_STATUS.md), section D5); previously left for G2-B2 (for example, shaping the area's border ring), and recorded as D5 in [TECH_DE`.md](TECH_DE`.md).
- Tiles outside the one-tile halo never changed (assertion).

## Not verified

- Appearance in RME. “Pairs without border = 0” means only that for every neighboring pair with different brushes, at least one tile has a border item. It does not prove that the edge is correct, continuous or attractive.
- A one-tile-wide narrow river, which was not separated in the metrics.
- `DrawOperations` paths (memory budgets, sandbox, diff), commit and Undo/Redo for v2.
- Effects for seeds beyond the six runs, with only one parameter configuration (the default).
- Any clients other than 1098-BlackTek.
