# G4-0 — compatibility of data/1098/items.otb with the ordinary 10.98 client (D11, part), 2026-11-07

**PARTIAL.** items.otb and Tibia.dat compatibility was measured and is consistent. This does not prove that the maps were created with exactly this file. The user decides whether to close this part of D11.

## Measured

The read-only [g4_check_items_otb.py](smoke/g4_check_items_otb.py) script, using tools/tibia_data.py and independent of the editor, compares every OTB item with a client ID to the record for that client ID in ordinary 10.98 Tibia.dat.

| Item | Result |
| --- | --- |
| items.otb (SHA-256) | 9f73f8bf…67ee, OTB 3.57.62-10.98 header, 26 282 items, 734 deprecated |
| Tibia.dat (SHA-256) | abb36b19…d8338, 23 725 items (range 100–23 725) |
| OTB items compared with DAT | 25 548 (remainder: deprecated) |
| OTB items without client ID / with client ID outside DAT range | 0 / 0 |
| Mismatches: ground, unpassable, stackable, container, blockMissiles, pickupable | 0, 0, 0, 0, 0, 0 |
| Headers of 19 maps (items major.minor) | 3.57 on all 19, all compatible |
| unknown on maps | only M01 (43 ground + 47 items, 14 IDs) and M06 (48 items, 15 deprecated IDs); other 17 maps: 0 |

Two script series produced byte-identical JSON (one machine).

## Conclusion and limits

- OTB and DAT are consistent in six tested features for all 25 548 items. A shifted or unrelated OTB would produce mass mismatches, so zero mismatches strongly indicates the correct pair.
- **Not verified:** that the maps were saved with this specific items.otb. There is no other OTB beside the maps for comparison. Indirect evidence is version 3.57 in every header and no unknown values on 17 maps. A map saved with another OTB having the same flags but different IDs would not be detected.
- Other features (for example floorChange, hangable and elevation), names and XML attributes were not checked.
- D11 remains open for companion files, containers, HTML and a second machine.
