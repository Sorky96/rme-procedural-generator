# G4 — style profile in the generator window, 2026-11-08

**PARTIAL: code built and checked without GUI; manual RME testing is waiting for the user.**

## Included

- **Map → Generate terrain…** has a **Style profile** list below “Terrain style”. “Built-in (manual settings)” preserves previous behavior. The list is active only for “Natural surface”.
- Profiles are *.profile files (g4-profile/3) in procedural/styles under RME's local data directory (FileSystem::GetLocalDataDirectory()). The tooltip above the list shows the exact path. The window does not save anything or create this folder, only the already existing local data directory as elsewhere in RME.
- A profile is offered only when learned for **exactly this client and these assets**: client name, SHA-256 of items.otb (from modular client data) and SHA-256 of client metadata (Tibia.dat) must match, the profile must describe a surface with fitted tuning, and values must be within generator ranges. Incompatible profiles are not listed. The tooltip gives the count and reason for hidden profiles.
- The selected profile sets GenerationConfig::surface (surface tuning) and GenerationConfig::decor (tree, bush and stone chances, relevant when vegetation is selected). Existing preview, validation, Place and Undo paths receive them. Changing a profile invalidates the preview like any other setting change.
- DecorTuning moved to generation.h and GenerationConfig::decor was added. The preview fingerprint includes decoration tuning **only when it differs from built-in values** and vegetation is enabled, so fingerprints for previous configurations are unchanged.
- style_styles --profiles-out writes eight style profiles for 10.98 (caves, m01, m05, m06, m15, m19, swamp, winter) to local-data/procedural-smoke/g4-profiles/10.98/ (not the repository).

## Checked

- rme.exe builds without errors.
- Core tests PASS: test_procedural_core, …_surface (default decoration tuning leaves the result unchanged), …_decor, …_style_profile, …_style_profile_set (offer: matching client/assets; refusals: other client, other items.otb, other Tibia.dat, underground, missing tuning, out-of-range tuning, garbage, empty file), …_style_fit and …_similarity.
- The same functions used by the window (loadedClientAssetIds, scanStyleProfiles) ran in a probe with a genuinely loaded 10.98 client: items.otb and Tibia.dat hashes match profile hashes, **8 of 8 profiles offered**, 0 hidden (g4-offer-20261108a).
- G3 reports were not changed.

## Brush palette in the profile (g4-profile/4, after manual-test feedback)

The first RME test showed that profiles differed only in ground-kind distribution and looked almost identical (winter and swamp were similar, caves had one ground). A profile now also carries a **palette**: the most-used brush for each of the five brush kinds on that style's surface. Selecting a profile fills brush fields (fields remain editable). A brush missing from the client is skipped and named in status. Nothing is guessed.

- Selection rules recorded in advance: brush key without item prefix (a real brush, not a ground item without brush), at least 200 tiles and at least 25% of that kind in the style. Otherwise the entry is absent and the brush field remains unchanged. Data uses tiles per key at z≤7 from maps carrying the style target.
- 10.98 result: m06 (grass, jungle dirt, mountain top ground, sand, sea), m19 (grass, dirt, mountain, sand, sea), m05 (grass, mountain), winter (snowy mountain, sea), caves (earth); **swamp, m01 and m15 have no palette** because their grounds are almost entirely ground items without brushes (D13). Visible profile differences are modest: clear for winter (snowy mountain), small for the rest and none for swamp.
- Schema v4 invalidates v3 files. The window hides them as unusable and new files must be copied.
- Palettes do not change statistics, fit or G4-F iteration results (style_styles report byte-identical).
- Tests: test_procedural_style_profile (palette selection, ties, order, share/tile thresholds, save and load) PASS; v4 probe offer 8 of 8.

## Manual-test error: “Borders would extend outside the allowed area” (2026-11-08)

After selecting a profile, every Preview ended with this error. Causes:

1. **Palette brushes the generator cannot place.** Full preparation, the same as the window, on an empty 10.98 map rejected (PreviewError::OutOfMask) earth, earth (stone border), mountain, snowy mountain, icy mountain, dirt mountain ground (+ thick border), frozen mud (thick border), grass (alternate border) and sand (thick border). Their edges extend outside the generated area, while the window changes only the allowed area. snowy mountain was additionally an empty data stub (no items). Brushes that pass include grass, jungle grass, dirt, jungle dirt, mountain top ground, rocky ground, sand, sea and clear water. Manual selection of these brushes in the old window has the same issue because the list does not filter them.
2. **Previous-profile brushes remained in fields** after changing profile, for example snowy mountain from winter broke swamp.

Fixes: (a) every profile selection, including Built-in, starts with default brushes and then applies the palette. An entry is applied only when the client has the brush and it has items. (b) The tool selects palettes only from brushes that pass the same preparation on this client (21-brush list, --usable-brushes, --palette-probe). All eight profiles now pass full preparation (ok), with 8 of 8 offered.

**Side effect:** after filtering, palettes barely differentiate styles. m06: grass, jungle dirt, mountain top ground, sand, sea; m19: grass, dirt, sand, sea; m05: grass; winter: sea; caves, m01, m15, swamp: none. Winter does not get snowy mountain because the generator cannot place that brush. Meaningful visual differentiation would require changing the rule that only the allowed area changes for brushes with external borders, a decision outside this stage, or adding new ground kinds.

## External-border brushes and decoration palette (g4-profile/5, 2026-11-08)

**Correction to the earlier conclusion:** I wrote that snowy mountain was an empty stub. That was wrong. The brushes.xml entry is a forward declaration and brush items come from other files. After the fix below it passes full preparation like the others.

- **Cause of brush refusal** (earth, mountain, snowy mountain, icy mountain, dirt mountain ground, frozen mud (thick border), grass (alternate border), sand (thick border) and others): a brush with an external “border to nothing” (hasOuterZilchBorder) draws an edge on empty neighboring tiles. For the generated area these tiles are outside it, so the mask guard rejected the result (OutOfMask).
- **Fix (part 2):** when a profile brush has that border (profileNeedsEdgeClearance), surfaceKeepClear also leaves empty fields next to an unwritable neighbor without ground (for a rectangle, an outer one-tile ring; for a selection, mask holes too). The border lands on empty fields **inside** the allowed area, so nothing outside changes. The preview fingerprint accepts those tiles (no ground, border only). Profiles without these brushes are unchanged (flag disabled by default), and G1 --surface and --commit regressions PASS.
- **Visual effect:** brushes with that border produce a one-tile ring of borders without ground around the area (a seam similar to D5). This is expected.
- **Check:** full preparation on an empty 10.98 map for 31 ground brushes (each in its slot): 21 of 31 ok before, 31 of 31 after (g4-palette-20261108e).
- **Decoration palette (part 1):** a profile also carries the tree, bush and stone most used by its style. Rules as for ground: name classification, at least 25% of the kind and at least **30** items because sample maps contain little decoration. A threshold of 100 rejected almost everything and was lowered after counts were reviewed. Only brushes passing full preparation with vegetation are eligible: 41 of 45. Data uses all style-family maps. The window fills Tree, Bush and Stone brush fields and leaves the vegetation checkbox as set by the user.
- **10.98 result:** ground: caves (earth), m05 (grass, mountain), m06 (grass, jungle dirt, mountain top ground, sand, sea), m19 (grass, dirt, mountain, sand, sea), winter (snowy mountain, sea); swamp, m01 and m15 have no ground. Decorations: caves (dead mangrove tree, small rocks), m05 (trees, grass tufts, small rocks), m06 (trees, bushes, kamienie), m19 (trees, grass tufts, small rocks), swamp (swamp plants as bush), winter (small snowy rocks); m01 and m15 have no decoration.
- All eight full-palette profiles pass full preparation on an empty map with vegetation enabled (g4-final-20261108a), offer 8 of 8. The style_styles report (statistics, fit, G4-F results) is byte-identical.

## Not checked

- The window itself (layout, tab order, mnemonics, tooltip, behavior when the folder is absent), the full Preview → Validate → Place → Undo/Redo path with a profile, or offering for BlackTek (covered only by unit tests).
- Result appearance (rivers still mainly follow axes, D6; profiles reach parameter-grid edges).
- That a profile improves results on unseen maps. See G4_GATE_STATUS.md: these profiles are fitted to their own styles.
- Folders and profiles are not versioned in the repository because they come from private maps. Instructions for obtaining them are to run tools from docs/procedural/smoke and style_styles.
