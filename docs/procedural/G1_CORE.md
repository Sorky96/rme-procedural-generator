# G1-A — semantic core

Status: implementation of the first G1 package, without commit or GUI integration. The full G1 gate remains IN PROGRESS.

## Contract

[`generation.h`](../../source/procedural/core/generation.h) and [`generation.cpp`](../../source/procedural/core/generation.cpp) form a separate `rme_procedural_core` library. They do not depend on wxWidgets, Lua, item IDs, brushes, or a live map. Target `rme` depends on this library. The current API consumer is a C++ test, and the editor does not yet expose a generation command.

- `GenerationConfig`: explicit X/Y/Z, width/height, seed, and integer grass/dirt/water weights. This is the first independent-cell baseline, without noise, rivers, or a biome model. Terrain naturalness is not a criterion for this package.
- Adapter preflight: `MapExtent`, `CoreBudget`, and `GenerationLayout` default to zero. Empty bounds/budgets reject work. `SemanticGrid::valid()` checks a non-negative, non-empty region, exact cell count, and known enum values. The adapter must also compare the region with the request and current bounds.
- `MapExtent` contains exclusive bounds supplied by the future adapter. When obtained from RME, account for inclusive `Position::isValid()` maxima and the actual map size. The core does not accept a default/hard-coded map size.
- The flat grid covers one floor, indexed by `y * width + x`. Mask and protection are read-only `span<const uint8_t>` with the exact size and 0/1 values. Empty mask means the full rectangle. Empty protection means no protected cells. Fully excluded area is an error. Unchanged cells receive `Terrain::Preserve`, which is not a tile-deletion command.
- `StableRandom` owns an `mt19937_64` engine with `seed_seq` derived from both seed halves and a stream identifier. It does not use the global brush RNG, `random_device`, `std::hash`, or `uniform_int_distribution`. Range draws use rejection sampling. The v1 terrain stream has fixed domain `0x7465727261696e`.
- Every cell consumes one terrain roll, including cells excluded by mask/protection. Changing only the mask therefore does not shift rolls for other cells in the same rectangle. Fragment compatibility after changing rectangle size/origin and control of existing-brush RNG are not promised.
- `fingerprint()` is non-cryptographic FNV-1a over explicit little-endian bytes: algorithm version, region, and cells. It does not hash C++ padding, settings, or the complete map snapshot. Future provenance must separately include seed/config, inputs, and assets. Preview/Commit/Redo will also require a concrete-diff hash.
- `validateGeneration` rejects negative coordinates, out-of-range Z, zero/out-of-bounds sizes, zero weight sum, mismatched/non-binary masks, no writable cells, limit excess, and unrepresentable vector size. Sums/multiplications are widened before operation. Validation does not allocate the grid.
- `CoreBudget` covers cell count and output size (`sizeof(SemanticGrid)` + cells). It is not a whole-job memory limit, allocator-overhead limit, or RSS measurement. Snapshot, halo, masks, variants, sandbox, concrete tiles, and Undo need separate checked accounting before G1 commit is allowed.
- `stop_token`: refusal before allocation, checked during generation at every row boundary and before return. Cancellation/`bad_alloc` returns an error instead of a partial grid. During synchronous calls, the caller must keep input spans alive and avoid concurrent mutation. Before a worker runs, the adapter must create its own consistent snapshot.
- `validateGeneration` accepts the same token and checks cancellation during mask/protection validation every 4096 cells. Fingerprints and v1 algorithm remain unchanged. Commit must apply the stored, verified concrete diff. Rerolling during Commit/Redo is not the contract.

## Weryfikacja

[`test_procedural_core.cpp`](../../tests/test_procedural_core.cpp) calls the real library code without a Python model. It checks repeatability and RNG separation, ranges including large ones, seed/config, clean weights, wide weight sum, irregular mask, protection, unchanged inputs, bounds and limit refusals, pre-start cancellation, and a fingerprint against an independently calculated constant.

100×100 and 500×500 runs compare complete vectors, a different seed, and presence of all three terrains. Reported time covers one core generation only. It is not resolver/commit measurement, full peak RAM, or an E2E G1 size pass.

Build: canonical `rtk proxy cmd /c build_ninja.bat`, then in the MSVC x64 environment:

```text
rtk proxy cmake --build build-ninja --target test_procedural_core --parallel 2
rtk proxy ctest --test-dir build-ninja -R "^test_procedural_core$" -V
```

Results are in the current `../PROGRESS.md` entry. The test does not use `assert`, so conditions also run in Release. Extension `g1-core-faults-tests-20261102.log` checked five one-time allocation failures in a small generation and deterministic retries. It does not cover persistent OOM or later adapter stages. Cancellation during the run, concurrent input mutation, and cross-toolchain/platform fingerprint compatibility were not checked. No new RME GUI was run because this package adds no user-accessible flow. Manual testing of the complete workflow remains G1-C.

## Dalsza praca G1

1. G1-B: mask/halo/protection and asset snapshot, controlled resolver using existing DrawOperations/autoborder, full budgets, and concrete-diff range checks.
2. G1-C: odrzucanie stale preview, atomowy commit przygotowanej akcji, osobne Undo/Redo, Cancel i blokada live.
3. Minimal dialog and real 100×100/500×500 E2E with full round-trip, protected-data preservation, and resource measurements. Only this can close G1.
