// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class Tile;

namespace rme::procedural {
// Exact tile-owned values, including recursive items/attributes and entities.
// Location metadata is compared separately by the snapshot (not owned by a map tile).
// Excessive nesting or malformed/unsupported attributes fail closed.
[[nodiscard]] bool snapshotTilesEqual(const Tile* first, const Tile* second) noexcept;
// Diff classification only: ignore derived flags/minimap caches, which are never
// written back for unchanged/protected tiles. Selection/modified/optional-border stay significant.
[[nodiscard]] bool snapshotTileContentsEqual(const Tile* first, const Tile* second) noexcept;
} // namespace rme::procedural
