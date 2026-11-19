# I55 new-family intake (bd89a30)

This implements the intake preregistration in [G6_METRICS.md](../G6_METRICS.md).
I55 and G6 remain **OPEN**. No real new sources, consent, ratings or acceptance
decision are supplied by this change. The tool never reads the blind key.
All commands use the Python standard library, with no build or installation.

## Prepare the private inputs

```text
rtk proxy py -B -I docs/procedural/smoke/g6_i55_intake.py template local-data/scratchpad/i55-intake/input.json
```

The generated JSON is an intentionally ineligible form, not evidence. Fill it
locally. Every private file and output must resolve beneath a directory named
`scratchpad`; `local-data/scratchpad` also satisfies the existing import runner.
Both root `scratchpad/` and `local-data/` are ignored by Git. Use absolute paths.
Nothing is uploaded, and no source or consent document belongs in Git.

The form contains all 19 previously exposed M01–M19 maps, including sample and
shape maps, plus a NEW01 placeholder. Preserve historical identities and declared
G4 families. Add **every other** previously analyzed/tuned source as `prior: true`;
the mandatory history declaration describes the scope, curator, date and evidence
for registry completeness. The tool cannot discover unreported earlier exposure.
Do not reclassify known maps as new, even after renaming, cropping or conversion.

For each entry provide:

- `alias`, `family`, `prior`, actual `source` file and `bundles` root.
- `identifiers`: canonical exact strings for the world, author/source lineage,
  original identifiers and all known related maps. Reuse the **same** identifiers
  across entries for shared worlds/lineages/relations. They are case sensitive;
  the reviewer must reconcile alternate spellings. Crops of one world share an
  identifier or declared family regardless of whether their windows overlap.
- For new sources, the `declaration` names provenance, rights holder, legal basis,
  attesting person and date. It requires `permissionToUse: true` and
  `neverUsedInAnalysisOrTuning: true`, with separate evidence paths for provenance,
  permission and exposure. Empty assertions and false flags block intake.

Set `evidenceKind` explicitly to `real-evaluation` only for actual verified inputs.
Use `synthetic-test` for fixtures. Synthetic reports always have
`countsTowardI55: false` and `eligibleQuantitativeEvidence: false`, even when the
numeric threshold passes. No tool can establish the truth of a signed declaration
or a legal right from a checkbox: a human reviews the documents before freeze.

Provide the existing m06/m19 profiles and the actual approved Tibia.dat,
Tibia.spr and items.otb under `assets`. They must match the historical audited
bundles, profiles and key mapping. The loader accepts only client `10.98`, surface
z=0–7, g4-profile/6 and the committed G6 bands. It retains all 26 positions and
the historical G4 definitional N/A rules.

## From a map to a verified bundle

Use the existing read-only [G3 import runner](g3_run_reference_import.py) and an
independently verified probe with the required G4 metric definitions. This task
does not build a probe or run imports on real maps. Keep an unchanged private copy
of the source and its companion XML, and use the accepted G4 keymap. Example for
one new source and one compatibility slot:

```text
rtk proxy py -B -I docs/procedural/smoke/g3_run_reference_import.py local-data/scratchpad/i55-intake/bundles/g4-channels-20261108a/NEW01 MAP PROFILE_DATA CONFIG --exe VERIFIED_PROBE --client-id 10.98 --client-path CLIENT_ASSETS --keymap ACCEPTED_KEYMAP
rtk proxy py -B -I docs/procedural/smoke/g3_verify_semantic_png.py local-data/scratchpad/i55-intake/bundles/g4-channels-20261108a/NEW01
```

`PROFILE_DATA` must actually define client `10.98`; do not change a receipt to
make a differently named/client-version run pass. A failed import, incomplete
summary or missing verification receipt is not an acceptable bundle.

The audited loader preserves four explicit compatibility slots beneath each
entry's `bundles` root:

| Slot | Authoritative summaries under `ALIAS/run/analysis` |
| --- | --- |
| g4-channels-20261108a | channels-surface.txt |
| g4-scale-20261108a | scale-surface.txt |
| g4-trans-20261108a | patches-surface.txt, transitions-surface.txt |
| g4-build-20261108a | paths-surface.txt, buildings-surface.txt |

Every slot also needs `ALIAS/verification.json`, `run/reference-report.json`,
`semantic-report.json`, `client-id.txt`, `keymap.tsv`, `data/*/items.otb` (exactly
one), and the **ground-name G4** `analysis/fingerprint.fp`. The fingerprint is
produced by the import probe from the normalized map, before semantic projection.
Do not substitute the I53 semantic-class fingerprint. Record the probe's identity
and preparation/verification commands in provenance evidence.

The current probe emits all six summaries. A human may approve a single complete
verified run and copy it byte-for-byte into the four compatibility slots, recording
this in evidence. Slot names preserve the existing loader layout; they do not
assert the date of a new run. Never fabricate verification receipts or rename a
partial historical summary into a different metric definition. Repeated channels,
scale and paths must agree across slots. All source identities, client assets,
OTB, mapping and all four fingerprints must agree. Historical bundles remain
diagnostic/prior evidence and never become new evaluation units.

## Intake, human review and freeze

```text
rtk proxy py -B -I docs/procedural/smoke/g6_i55_intake.py prepare local-data/scratchpad/i55-intake/input.json local-data/scratchpad/i55-intake/review.json
```

`prepare` audits packages without reading any candidate matrix or calculating
L2/L3 distances. It writes an unaccepted review request containing the complete
path/SHA-256 inventory, deterministic family allocation, units, fingerprint counts
before/after filtering, all pairwise comparisons/denominators and accepted merge
links. Missing new sources block it.

Relatedness uses the same G4 rules on the entire registered historical + new pool:
16×16 ground-name windows, modulo 4 sampling, minimum 50% present; remove windows
present in at least `max(2, ceil(N/4))` maps, then merge declared families/shared
identifiers and content overlaps by single linkage. Content requires at least 16
shared windows and maximum directional containment at least 100 permille. Any
component containing both prior and new sources blocks intake. All source bytes
are hashed, so renamed identical files also block. Historical empty fingerprints
remain explicit noncomparables; a new source needs at least 16 surviving windows
or intake blocks rather than treating absence of evidence as independence.

Review the source identities against historical records, lineage identifiers,
rights/provenance/exposure documents, probe evidence, mapping and summary
definitions, family allocation and complete evaluation units. Only the human
reviewer sets `accepted: true`, `reviewer` and `date` in the review request. Do not
change `inputSha256`; it binds the reviewed spec, declarations and evidence.
The tool neither fabricates nor automatically accepts this review.

```text
rtk proxy py -B -I docs/procedural/smoke/g6_i55_intake.py freeze local-data/scratchpad/i55-intake/input.json local-data/scratchpad/i55-intake/review.json local-data/scratchpad/i55-intake/frozen
rtk proxy py -B -I docs/procedural/smoke/g6_i55_intake.py verify local-data/scratchpad/i55-intake/frozen/manifest.json
```

Freeze re-audits the inputs and checks the human acceptance hash before creating
a **new** directory. The manifest binds source bytes, client assets, reports,
metric summaries, mappings, fingerprints, evidence, reviewed JSON, scripts,
profiles, bands, reference values, families and units. It records the unchanged
aggregation, seeds and 23/32 requirement. `inputSha256` identifies the inventory
reviewed before adding the review itself; `frozenInputSha256` includes the review.
`seal.json` separately binds the manifest bytes. Existing outputs are never
overwritten. Changes to any bound file or manifest block verification/evaluation.
Keep both frozen files and their hash in the external review record; the seal
detects changes, but is not a digital signature against someone rewriting both.
There is no thaw/update command. Technical repairs require a new review and new
run, retaining invalid evidence; do not replace units after seeing results.

## Measure only after freeze, then evaluate

```text
rtk proxy py -B -I docs/procedural/smoke/g6_i55.py measure EXE PROFILE_DIR COPY_INDEX local-data/scratchpad/i55-matrix-new --family-manifest local-data/scratchpad/i55-intake/frozen/manifest.json
rtk proxy py -B -I docs/procedural/smoke/g6_i55_intake.py evaluate local-data/scratchpad/i55-intake/frozen/manifest.json local-data/scratchpad/i55-matrix-new local-data/scratchpad/i55-unseen-result.json
```

`measure` verifies the frozen intake before generation and binds its hash in the
matrix's pre-generation manifest. An older diagnostic matrix with no such binding
is rejected; attaching references after measurement is not this workflow.
Evaluation reuses `g6_i55.read_matrix` to verify both repeats, 32 seeds per profile,
N=4, settings, ranking, SplitMix64 candidate seeds and I53 states. It checks the
matrix's profiles and I53 client/assets/M06/M19 identities against intake.

L2 is the first admissible candidate; L3 is the admissible ranking-A winner.
Distances keep the fixed 26-metric denominator, logarithmic/asinh contributions
and ln(64) N/A cap. Maps are weighted equally within each merged new family;
families receive equal weight. The report contains per-seed/map/family distances,
strict wins, ties, difference of medians, median paired difference and coverage.
The quantitative condition remains **at least 23 of all 32 strict L3 wins in
each profile separately**. No admissible candidate blocks a complete comparison;
no seeds are replaced. Exit 0 means the numeric condition passed; exit 1 means a
valid comparison failed it; exit 2 means blocked/malformed evidence. Check
`countsTowardI55` and `eligibleQuantitativeEvidence` for synthetic results.

No blind package/key/ratings are read, and the tool never makes a user's rating.
This quantitative report does not close I55 or G6 or replace the registered blind
test, regression and gameplay probes. Real provenance/legal truth, undisclosed
exposure and a real new-family result remain unverified until supplied/reviewed.

## Synthetic validation

```text
rtk proxy py -B -I tests/test_g6_i55_intake.py
rtk proxy py -B -I tests/test_g6_i55.py
rtk proxy py -B -I tests/test_g6_holdout.py
rtk proxy py -B -I tests/test_g6_i51b.py
```

The intake suite exercises synthetic verified bundles, refusal cases, immutable
inputs and manifest, G4 boundary/generic/single-linkage rules, equal family
weighting, N/A, the 22/23 boundary, old-matrix refusal and an unmocked synthetic
TSV reader/evaluation flow. A CTest entry is provided; no C++ build is needed for
these direct Python commands. No real maps are accepted by the tests.
