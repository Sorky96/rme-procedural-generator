# G7 — gate status

**G7 open.** Criteria: [G7_METRICS.md](G7_METRICS.md).

## I57 — copy index built in the GUI (2026-11-23)

Criteria `6fe44f4`. Core `d9c2e82`, test fix `aadf14a`, GUI and probe `ab02283`.

**What it does.** The copy index (`i53-index/3`) holds 1–16 named areas instead of exactly M06 and M19. `i53-index/2` still loads. **Learn from open map…** has an option to add the analyzed area to a copy index in the local profile folder, under the profile name. The index edit is checked before the profile is written, so a refused edit saves nothing. The generator offers a **Copy check** list with None, the local indices (an index of another client or an old format is listed with the reason and cannot be chosen), and Other file…, which keeps the path field.

**Criteria results.**

1. **Parity: PASS.** The rebuilt M06/M19 index as `/2` and the same index as `/3` gave identical `candidate_rank` TSV except the similarity line, for m06 and m19, seeds 3001–3032, N=4 (8/8 blocks). The `/2` output of the new build is byte-identical to the I55 matrix produced before the change.
2. **Unit tests: PASS** (`COPY_GUARD_PASS`). They cover 1 and 16 references, a 17th refused, a declared count of 17 refused, duplicate alias and duplicate sample, path and device names, a too-small sample, `/2` and `/3` giving identical assessments, a rotated and mirrored copy of a one-reference index blocked while different terrain is clear, the sample raster projection, save without overwrite, a changed file not overwritten, and the folder listing including another client.
3. **Real-client probe: PASS** (`g1_run_resolver_probe.py --learn --visible`, client 1098-BlackTek, `LEARNING_DIALOG_PASS`, input assets unchanged). The new I57 section learned a 128×128 Natural surface area. An index name with a path saved nothing, and the area was added to a new index while the map and Undo stayed unchanged. The generator listed the index. Regenerating the same terrain was blocked at Place. Seed 4243 was evaluated below the copy thresholds (14‰) and placed with one Undo and an exact Redo. Seed 4242 was also clear (15‰) but was refused by reachability (D30), so the probe takes the first placeable seed of 4242–4249. All I56 checks passed in the same run.
4. **Regression: partly.** The canonical `rme.exe` build passed. Core tests are 23/23 after `aadf14a`, which fixes a pre-existing test that dereferenced a refused serialization (undefined behavior that passed or failed depending on the build). The Commit probe passed. G5 `--i47` with rockBlocks m06=0, m19=1 gave `G5_GATE_PASS`, 32/32 for both profiles, identical repetitions. Fingerprint 1729 is checked by the core suite. **The Surface probe fails** in its M19 cave case, also on the build without I57 changes (D30).

**Not verified.** Manual visual and keyboard review of both windows. Indices larger than one area in the GUI (covered only by unit tests). A full-size learning area (512×512) in the probe: Place of 256×256 Natural surface with caves exceeded the probe profile's Undo limit (40 MiB / 400 actions), so the probe uses 128×128.

**Found while testing.** A Place refused by the Undo limit writes its reason to the log, but the status label the probe read still showed the previous message. Not investigated.
