# G3-C — regions, topology and floors, 2026-11-07

**PARTIAL (G3-C).** Implemented: connected walkable areas, regions of the same ground (marked as non-walkable and closed), and per-floor features. Not implemented: bottlenecks, rivers/caves/routes as separate features, connections between floors, settlements and POIs. The G3 gate is not passed.

## Included

- Core source/procedural/core/topology.* (without editor, item IDs or names): union-find over SemanticMap, with four-neighbor connectivity (diagonals do not connect).
  - **Walkable area:** a tile has ground, is not blocking and has no unknown content. Per floor: component count, largest component, five largest, size buckets (powers of two), “pockets” below the threshold (16 fields by default).
  - **Ground region:** connected tiles with the same ground key. A key is non-walkable when at least 90% of its tiles are blocking (threshold in options). A region is “closed” when none of its tiles touches the edge of the occupied floor rectangle.
  - Also per floor: tiles, unknown, floor changes and teleports.
- Cancellation: std::stop_token checked per row. Working memory about 9 B per cell of the largest floor, allocated once. Allocation errors are returned as errors.
- test_procedural_topology PASS on manually calculated grids: 3 components (4, 2 and 10 fields), unknown field excluded, no diagonal connection, closed lake versus pond at edge, floor changes and teleports, cancellation before work, refusal of bad options and incompatible summary, repeatability.

## Result for forgotten.otbm (1098-BlackTek client)

Local topology-report.json (SHA-256 041ECE44…B1A2) is in local-data/, without coordinates. Two runs are byte-identical, semantic-report.json is unchanged from G3-B (5C9C1024…2B4C), runs finish unattended, stderr and wx-log.txt are empty, and the complete run takes 4.6–4.8 s wall time (topology was not separated).

- Walkable fields in total: 95 103 on 16 floors. Floor 7: 46 534 fields in 489 components, largest 22 634 (49%), second 8 332; 363 pockets under 16 fields (1 415 fields). Other floors: 34 to 383 components, largest 6 to 1 371 fields.
- Floor changes by the floor_change feature: 613 tiles. Teleports: 27.
- Non-walkable keys (at least 90% blocking, without using names): for example sea, 45 034 tiles in 48 regions (largest 18 469, 44 closed); clear water, 33 regions (all closed); lava, 21 regions; mountain, 260; earth (stone border), 118. Sum of tiles by key = 340 595 (tile count).
- Most fragmented: ground without brush/empty, 1 770 regions; cave, 430; mountain top ground, 357; grass, 306 (largest 12 851 tiles).

Run on the unknown fixture: 395 walkable fields, one component (395), five unknown fields excluded, “no key” region with three tiles in two regions, as predicted by the fixture.

## Not verified

- Walkable components and regions have no independent check (for example, another program). They are supported by manually calculated tests and matching sums. Only non-walkable tile count was independently confirmed, matching RME statistics (G3_REFERENCES.md#independent-rme-gui-editor-statistics-check).
- “Walkable” means walkability from item flags (a blocking item, such as a tree or wall, closes a tile), not full movement simulation: no diagonals, creature size, floor transitions or open/closed doors.
- Missing: bottlenecks, river length and tortuosity, cave chambers and corridors, route networks and settlements. A ground region is not a named biome: naming biomes would be guessing, so the report gives the client ground key and non-walkability data.
- “Closed” is calculated relative to the occupied floor rectangle, not map boundaries, so sea at the edge of the occupied area is “open”.
- The 90% non-walkability threshold and 16-field pocket threshold are assumptions. Sensitivity was not checked.
- Memory: 9 B/cell is a code estimate, process RSS was not measured. Time is one complete measurement, not a benchmark.
