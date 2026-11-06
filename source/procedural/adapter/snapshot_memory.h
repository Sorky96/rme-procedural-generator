//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PROCEDURAL_ADAPTER_SNAPSHOT_MEMORY_H_
#define RME_PROCEDURAL_ADAPTER_SNAPSHOT_MEMORY_H_

#include <cstdint>
#include <expected>
#include <stop_token>

class Tile;

namespace rme::procedural {

	enum class SnapshotMemoryError : uint8_t {
		Limit, // The estimate exceeds max_bytes.
		Overflow, // A size computation would overflow uint64_t.
		DepthLimit, // Nested containers or preserved OTBM nodes are deeper than kSnapshotMaxNestingDepth.
		Cancelled, // The stop token was triggered during traversal.
		Unsupported, // The tile has a shape deepCopy cannot handle (a null item entry).
	};

	// Maximum nesting accepted for containers and, separately, for preserved OTBM node trees.
	// Deeper data is rejected instead of recursing without bound.
	inline constexpr uint32_t kSnapshotMaxNestingDepth = 64;

	// Conservative per-heap-allocation allowance for allocator headers and size-class rounding.
	inline constexpr uint64_t kSnapshotAllocationOverhead = 32;

	struct SnapshotTileMemory {
		uint64_t bytes = 0; // Conservative estimate of heap bytes a deep copy of the tile may allocate.
		uint64_t items = 0; // Item objects copied: ground, stack items and all nested container contents.
	};
	// Largest existing Item subtype plus the same per-allocation allowance.
	[[nodiscard]] uint64_t maximumSnapshotItemAllocation() noexcept;

	// Conservative upper estimate of the heap memory allocated by Tile::deepCopy() or
	// TileOperations::deepCopy() for this tile, including all preserved metadata: the Tile object,
	// cloned TileLocation and house exits, item objects (each sized as the largest Item subclass),
	// attribute maps with their keys and string values, invalid OTBM item payloads and node trees,
	// nested container contents, creature names and spawn alternatives, spawn, zone storage and
	// preserved invalid tile content. Each allocation is charged kSnapshotAllocationOverhead.
	//
	// This is an estimate built on standard-library layout assumptions, not an exact heap or RSS
	// bound. It excludes anything outside the copied tile: BaseMap/TileLocation creation in the
	// destination map, Undo/Action bookkeeping, procedural masks, outputs and sandbox memory.
	//
	// max_bytes is the budget remaining for this tile. The estimate fails with Limit as soon as the
	// running total exceeds it; a total equal to max_bytes succeeds. No partial result is returned on
	// failure. Synchronous, read-only, free of GUI calls, independent of loaded item/creature
	// definitions and performs no heap allocation, so it can run before any copy is made.
	[[nodiscard]] std::expected<SnapshotTileMemory, SnapshotMemoryError> estimateSnapshotTileMemory(const Tile& tile, uint64_t max_bytes, std::stop_token stop = {});

} // namespace rme::procedural

#endif
