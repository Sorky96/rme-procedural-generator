# G4: visual comparison of the generator with references (2026-11-08)

Tools: style_render (generator → PNG of kinds, one pixel per tile) and g3_reference_import_probe (kind-map-zNN.png for reference maps, local only in local-data/, not the repository). Profile v7, 256×256 slices at z=7 (reference slice selected automatically as the area with the most land), same color per kind. **This is an inspection of one slice per style, not a measurement**; reference images are private and are not in the repository.

## Observations (M06 and M19 versus m06 and m19 profiles)

1. **Structure scale.** References have large connected structures: continents and islands surrounded by sea (M19), extensive grass plains (M06). The generator uses one noise scale: an even mosaic of lakes, rock and dirt across the whole surface, especially in profile m19.
2. **Rivers (corrected 2026-11-08 after measurement).** The initial statement “2–3 tiles in the generator, 6–10 in references” was wrong. style_render measurement (m06/m19 profile, 6 seeds, 256×256): generator river width W is 4.4–6.6 tiles, with 53–613 river-skeleton tiles per slice. The same measurement on M06 gives W ≈ 1.2–2.2 tiles and about 50 skeleton tiles per 256×256 slice on average (1829 for the whole map). The M06 slice used for inspection was selected as “most land”, so it is a rare fragment with exceptionally many rivers and is not representative of the map.
   The measurement shows that the generator has **more** and **wider** rivers than what iteration-4 metrics measure. That metric omits large river networks connected to the sea (a water component ≥ 20 000 tiles is classified as sea and skipped), so **wide rivers visible in the M06 slice were excluded from the measurement**. Iteration-4 metrics (tortuosity, straight segments, width) therefore describe only narrow, short reference streams, and D6 conclusions from iteration 4 apply only to them.
3. **Dirt.** In M06 it appears as rectangular and round patches scattered through grass (hand-painted). In the generator, dirt is found mainly as a band beside rock and water.
4. **Rock.** M19 has compact ranges with jagged edges; the generator has smaller, cut-up patches.
5. **Shore.** M19 has wide beaches; the generator always has a thin band (about 1–2 tiles).
6. **Human features.** References have buildings, roads and unusual grounds (Other/Unmapped colors). The generator does not create them (out of phase scope).

## Conclusion

Matching kind shares (the current G4 measure) does not produce shape similarity. The main missing pieces are multi-scale structure (large land and water masses), a river network connected to sea (rivers do not end at lakes and have tributaries), dirt patches in grass and wider shores. Appearance similarity was not measured. The assessment is visual and subjective.
