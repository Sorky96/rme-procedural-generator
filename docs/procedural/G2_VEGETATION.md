# G2-C — vegetation and terrain-dependent details, state as of 2026-11-07

**PARTIAL.** The decoration layer (trees, bushes, stones) works in the core, resolver, preview, commit with one Undo and Redo on the real 1098-BlackTek client (100×100 and 250×250), and the G1 regressions pass. The dialog and appearance in native RME were not checked, so the “readable in RME” criterion remains open.

## Scope

- Surface algorithm only, with the GenerationConfig::vegetation option (disabled by default; dialog: “Add trees, bushes and stones”). Classic has no decorations; preparation rejects Classic with vegetation enabled.
- Core source/procedural/core/decoration.h/.cpp: generateDecoration(SurfaceGrid, seed) returns DecorKind (Tree, Bush, Stone) for each cell. Integer arithmetic only, using the seed and absolute coordinates. Rules (code constants, no parameters):
  - Trees only on grass, no closer than two steps (4-neighborhood) to a lake, shore or river. Noise-based groves and clearings (step 20): 11% of cells in forest, 0.6% in clearings.
  - Bushes on grass, 4% (6% near water).
  - Stones on dirt (3%) and near rock within two steps (8%, including on grass).
  - Nothing on shore, lake, river, rock or Preserve cells.
- Adapter: resolveSurfacePlan(…, decor) maps the kind to a doodad brush from GroundProfile::decor (trees, bushes, small rocks, names from the dialog). Only single items of variant 0 (getRandomSingleItem) are used; multi-tile composites are skipped. Every reachable item is checked (definition, client ID, not meta, not ground, not border). No hardcoded identifiers. A separate random stream is used per kind.
- Pipeline: GroundPlan::decor (one value per cell, 0 = none); the plan fingerprint appends the layer only when non-empty, so plans without vegetation retain their fingerprint. Preview places items after borders (the border calculator does not see them), checks plan consistency and allows only ground, borders and declared decoration items in the buffer. Commit copies the buffer, so it remains one action with one Undo.

## Results

| Check | Result |
| --- | --- |
| test_procedural_surface: determinism, no decoration outside grass/dirt, no trees near water, seed influence, stones only on dirt outside grass near rock | PASS (24 seeds 100×100, 6 seeds 250×250) |
| Tree share on land | 3.1% (100×100), 3.2% (250×250); bushes and stones occur in every series |
| bad_alloc at each of 7 decoration allocation points (24×24) | 7 of 7 reported as AllocationFailed, no crash |
| --surface on client 1098 (g2-veg1-surface-20261107) | SURFACE_RESOLVER_PASS; surface resolver fingerprints unchanged; SURFACE_E2E_PASS without vegetation (100, 250) and SURFACE_VEGETATION_E2E_PASS 100×100 (698 decorated, 13.7 MB estimate) and 250×250 (3824 decorated, 80.5 MB); Undo restores the undecorated state, Redo restores it; rejected: missing brush names, Classic with vegetation, ground brush as doodad, option changed after preparation |
| --preview, --commit (G1, g2-veg1-*) | passed: true, empty stderr, rows identical to G1_COMMIT.md |
| test_procedural_core | PASS |

The --surface runner records passed: false because of stderr, as before (D1, not traced).

## Not verified

- The dialog (choice, brush fields, messages) and appearance in native RME. It is unknown whether density and clusters look good.
- Availability of brush names trees, bushes, small rocks on clients other than 1098-BlackTek. A missing brush causes refusal, not guessing.
- Multi-tile doodads (large trees, composites), intentionally out of scope.
- Terrain accessibility: trees and some stones block passage, and their effect on walkability was not studied.
- At the default 40 MiB Undo limit, the Place size boundary decreases (vegetation adds one item per decorated tile; 250×250 is about 80.5 MB by estimate, versus 78.8 MB without vegetation).
- Decoration dependencies with a mask: distance from water and rock is calculated from a grid without Preserve cells, so cells next to a disabled fragment may receive a tree they would not receive without the mask. Other rules do not depend on the mask.
- Time and memory beyond the estimates.
