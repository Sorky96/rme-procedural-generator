# G3 — semantic-map export, 2026-11-07

**PASS for 20 maps within the scope below; for G3 overall, see G3_GATE_STATUS.md.**

## Included

- Core source/procedural/core/png_writer.* (no dependencies): an 8-bit indexed PNG encoder with stored deflate blocks. Output depends only on the input, so identical data produces identical bytes; file size is close to raw pixels.
- Core source/procedural/core/semantic_image.*: renderSemanticImages converts SemanticMap and TopologyReport into one PNG per floor (one pixel per tile, pixel (0,0) is the floor rectangle corner) and a legend. Classes in priority order: unknown content, wall, door, teleport, floor change, item blocking on walkable ground, then ground-key color (non-walkable keys are darker), tile without ground, ground without brush, no tile. Colors use integer HSV, so the palette is exact. Above 247 ground keys, slots repeat and the legend lists each key at its slot.
- renderSemanticLegendJson: legend, floor-rectangle sizes and offsets (map coordinates are for local use only, not the repository). stop_token cancellation per row; working memory about 2 B per cell of the largest floor (included in analysisWorkingBytes, now 16 B per cell of the largest floor).
- The probe writes analysis/semantic/floor-NN.png and legend.json.

## Tests and evidence

- test_procedural_semantic_image PASS: encoder checked with independent CRC and Adler implementations in the test (known IEND CRC), multi-block image (>65 535 B), refusals (bad size, palette, index outside palette), manually calculated 7×4 grid with every class, negative-x offset, slot reuse at 250 keys, cancellation, incompatible topology, repeatability.
- g3_verify_semantic_png.py decodes PNG with Python's standard library (zlib, struct), rather than the tested code, and compares it with the report: CRC of all chunks, filters, dimensions equal to the floor rectangle, non-empty pixels = tiles, “unknown” pixels = unknown cells, wall/door/teleport/floor-change pixels do not exceed feature counts, palette matches the legend. Result: all checks PASS for 19 ordinary 10.98 maps (707 checks total, 9 to 129 per map) and forgotten.otbm (129/129).
- Two complete series (19 maps): 105 files in semantic/ are byte-identical. The remaining reports (190 files) are unchanged from the series before export.
- Map memory peak of 71 MiB after adding export: 1 899 MiB (unchanged from 1 898–1 899 MiB previously, within measurement range).
- One image was viewed (floor 7 of forgotten.otbm): clear islands, sea as a non-walkable area, buildings and marked elements.

## Not verified

- Only one floor of one map was viewed. Other images were not assessed. Pixel-classification correctness is confirmed by matching counts and a manually calculated test, not visual review.
- The independent verifier checks sums and structure, not individual-pixel positions on real maps (only the manually calculated grid checks that).
- PNG size is close to raw pixels (without compression): 5000×5000 maps produce about 25 MB per floor. It was not checked whether this causes problems for larger maps.
- Repeatability was checked on one machine and one EXE.
