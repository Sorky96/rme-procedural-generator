# Procedural terrain in RME Redux

Use **Map → Generate terrain…** to preview terrain and place it in an empty area of the current map. You can also fit a surface style profile from a region of a map you have permission to use. Learning runs locally in the editor.

## Generate terrain

1. Open a target map with compatible client assets loaded. Choose **Map → Generate terrain…**.
2. Set the target area and seed. Under **Terrain**, choose a terrain type and, for **Natural surface (lakes, rivers, rock)**, select a compatible **Style profile** if you want to use one.
3. Choose **Preview** and inspect the result. Adjust the settings and preview again as needed. Preview leaves the map unchanged.
4. Choose **Place on map**. The generator fills empty tiles and records the placement as one undoable edit. Use **Edit → Undo** or **Redo** to revert or restore it.

## Learn a surface style from your map

1. Open the source map with its matching client assets. Go to **Map → Generate terrain… → Terrain → Learn from open map…**.
2. Enter the top-left X and Y, width, height and floor of the sample. Use a surface floor from 0 through 7 and a rectangle within the map, up to 512 × 512 tiles. Choose a region with at least 1,024 recognized natural tiles and at least 50% recognized coverage among present tiles.
3. Confirm your right to use the map and choose **Read rectangle**. Review the ground mapping. Each tab-separated row contains a key, class, rule and exact brush name. To correct a classification, edit only the class column. Keep the header, tabs and brush names. The available classes are `Grass`, `Dirt`, `Rock`, `Shore`, `Water`, `Other`, `Unmapped` and `NoGround`.
4. Choose **Analyze**. Check the counts for recognized and skipped tiles. If coverage is too low, choose a larger or more representative rectangle, or correct ground classifications and analyze again. Use **Stop** to cancel a read or analysis.
5. Enter a unique profile name using 1–64 ASCII letters, digits, hyphens or underscores. Choose **Save profile**. The generator refreshes the profile list. Hover over **Style profile** to find the local profile folder.
6. Choose **Natural surface (lakes, rivers, rock)** and select the saved profile. Preview in an empty area of the target map. If the target is a different map, open it with the same client assets and then reopen the generator.

Profiles record surface ground proportions, adjacency and brush names. The source map, its path and sample coordinates stay out of the profile. Underground geometry, buildings and vegetation are outside the scope of this fit. Learning and saving leave the source map and its undo history intact.

See the [local learning implementation and verification notes](docs/procedural/I56_LOCAL_LEARNING.md) for the automated check and its coverage.
