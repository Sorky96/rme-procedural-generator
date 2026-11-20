# G1-B — Semantic input and ground variants

Status: partial G1 integration. The full snapshot and resolver remain outside this check, and the gate is still open.

`captureGenerationInput` in `source/procedural/adapter/generation_input.*` reads the specified map synchronously on the wx main thread. It returns owned configuration and extent values, the map identity and revision, the VersionManager asset revision, and two flat vectors for the mask and protection data. It holds no references to live objects, so the input can move to a core worker. A tile and halo snapshot, complete preview staleness checks, and permission to commit are separate steps. The rejection of input after reloading assets was verified separately in [G1_RESOLVER.md](G1_RESOLVER.md).

The `empty_only` policy protects a whole tile whenever it contains ground, an item stack, a creature, a spawn, a house ID, map flags, extra data, or any state flags other than selected and modified. This keeps any UID/AID, containers, and unknown data intact. The policy also checks spawn, waypoint, and town counts and nonempty house exits on TileLocation, including locations without a Tile. It excludes the entire occupied tile.

Bounds are the intersection of the map header dimensions and the inclusive Position limits converted to exclusive bounds. Validation comes before map reads and mask allocation. `maxInputBytes` charges `sizeof(GenerationInput) + 2 * cellCount` for the owned input payload alone. The allocator overhead, snapshot and halo, sandbox, result, and history require separate G1 budgeting. Cancellation is checked before work starts and every 4,096 cells. An error returns no partial input and performs no map writes.

`GroundBrush::getPlacementItemIdForRoll` exposes the existing variant selection for an explicit roll from 1 through totalChance. Normal `draw` calls the same function with a roll from the existing RNG. The code retains the strict `<` comparison for cumulative chances and the historical fallback to the first ID, preserving the ordinary painting distribution. The method selects an ID without creating items, applying borders, or changing the RNG. The resolver still needs to check the active client, brush, and every ID against real definitions and carry out placement and autobordering in the sandbox through DrawOperations.

## Verification

The canonical build and core test logs are `local-data/procedural-smoke/g1-input-canonical-20261102.log` and `g1-input-core-tests-20261102.log`. The Release core test passed 1/1 and preserved the 100×100 and 500×500 fingerprints. Cases for default parameters, a malformed grid, and cancelled validation were added.

`tests/test_procedural_ground_brush.cpp` uses real RME objects with a synthetic map. It checks mask ownership, metadata protection, lack of mutation, the budget boundary, cancellation, refusal from a worker, exact variant boundaries, and preservation of the editor RNG sequence. It runs without a client or map files. Run it with:

```text
rtk proxy <python.exe> docs/procedural/smoke/g1_run_input_probe.py local-data/procedural-smoke/<nowy-katalog>
```

The first harness failed to link because of duplicate `main` definitions. Its log remains in `g1-input-rme-20261102/build.log`. The separate G1 builder compiles a private application.cpp object with a renamed entry point and keeps the production source, object, and executable intact. The test has its own console main and initializes wx without Application::OnInit, windows, a client, or the main loop. The build manifest binds the current sources, objects, and probe executable. This harness covers neither GUI operation nor normal RME shutdown.

The corrected probe **passed with exit code 0 and empty stderr**. See `g1-input-rme-entry-20261102/verification.json`, stdout, and the build manifest. It covers 12 positions on the synthetic map, nine protection cases, and 10,000 repeatable brush selections. Full snapshot and end-to-end size coverage are still pending. The core test intercepted all five observed allocations in one small generation. Each injected error returned AllocationFailed, and retry reproduced the exact baseline. See `g1-core-faults-tests-20261102.log`, CTest PASS 1/1 in Release. These are one-time core allocation errors. Persistent OOM, resolver failures, and commit failures are outside this test.

G1 still requires a full snapshot including the halo and assets, a resolver, checks of the concrete diff, an overall budget, preview staleness checks, atomic commit with failure injection, separate Undo and Redo checks, a minimal dialog, and end-to-end round trips for 100×100 and 500×500. G2 has yet to start.
