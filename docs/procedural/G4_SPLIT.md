# G4-A — reference manifest, family split and similarity test (D10), 2026-11-07

**PARTIAL.** A fragment-similarity test between maps and a leave-one-family-out split were built and measured for 19 ordinary 10.98 maps. The generator-result memorization test is **not** built yet (it requires the G4-B ground-key → surface-kind mapping), and there is no split for BlackTek (one map).

## Included

- source/procedural/core/similarity.*: 16×16 window fingerprints (ground-name and tile-presence hashes, without position or floor, so a shifted copy or a copy moved to another floor is detected; content sampling, hash % 4 == 0; sparse (<50% tiles) and uniform windows skipped). compareFingerprints (containment in ‰), withoutGenericWindows (windows present in ≥ N/4 maps are general patterns, not copying evidence), text serialization.
- source/procedural/core/reference_split.*: g4-manifest/1 (alias, sha256, client, role style/sample/shape, family; one client per manifest); families = declared union contained at ≥100‰ with at least 16 shared windows (single-linkage); leave-one-family-out folds: test = family, validation = next family (cyclic), rest = training. Disabled roles: sample and shape never enter folds.
- reference_split tool (core only, no GUI) and docs/procedural/smoke/g4_make_manifest.py. The G3 probe also writes analysis/fingerprint.fp.
- Test test_procedural_similarity PASS (shifted copy, another floor and another key layout: exactly 225 shared windows from a 30×30 block; independent maps: 0; uniform and sparse skipped; refusals; round-trip serialization; manifest; content families; leak-free folds with no maps outside style; entry-order independence; generic-window filter; minimum shared-window count).

## Parameters set before results (none concerns style metrics)

Window 16, sampling 1/4, sparse <50%, merging ≥100‰ and ≥16 shared windows. **Explicit change after the first run:** the first run (without the generic-window filter) merged 9 of 12 style maps into one family because small maps share general patterns (for example, M13 is 82% contained simultaneously in M17 and M19), producing three folds. I added the rule “a window present in ≥ N/4 maps is general” (N = 19, threshold 5) and minimum 16 shared windows before calculating any style metric. I chose 1/4 from overlap-distribution data, not an independent principle. It is an assumption, not evidence.

## Result (19 maps, 10.98 client, roles: 12 style, 3 sample, 4 shape)

- **Positive control:** shape pairs M07–M09 and M08–M10 have 1000‰ in both directions (one family, as expected from names).
- **Content families (outside declarations):** M13 is 70–71% contained in M17 and M19 (83–84 specific shared windows each); M15 and M16 connect to M17 (141 and 104‰). One family of seven maps (M12–M17, M19) forms alongside M01, M05, M06 and winter (M11 + M18). The first uncorrected run is linked below.
- **Five folds** (test family → validation): seven-map family → M01; M01 → M05; M05 → M06; M06 → winter; winter → seven-map family. max_leak between test and rest: 45, 45, 9, 29, 0‰ (below 100‰ by construction).
- **Below threshold observation:** M06 (origmap) and M19 (world) have 2 072 specific shared windows (2‰ M06, 5‰ M19); M06 and M08/M10 have 1 809 each. This is small, but not zero in absolute numbers. M06 test-fold leakage is 29‰. It was not determined whether this is a shared source or a repeatable terrain pattern.
- **Not comparable (no windows):** M02 and M03 (smaller than window 16 after sparse filtering). M04 has 182 windows.
- **Repeatability:** two series of 19 fingerprints are byte-identical (0 differences), the split report is byte-identical, and G3 reports (190 files) are unchanged after extending the probe.

## First run (without filter)

The “caves” family = M01, M06, M12–M17, M19 (9 maps), three folds, one test on 9 maps. Rejected as degenerate and retained for transparency (local split1.json).

## Not verified

- Whether 16×16 is the correct scale (one was tested) or ground-only is sufficient (no traits or items).
- Whether content relatedness equals style relatedness. Families declared from filenames (swamp, winter, caves) are assumptions.
- Folds are unbalanced (7 maps versus 1). Per-map/family weighting is deferred to G4-C/F.
- Generator-result memorization test, BlackTek and a second machine.
