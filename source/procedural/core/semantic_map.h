// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rme::procedural {

// Structural traits of one tile, derived by the adapter from the loaded client's item
// definitions and brushes. No item id appears here; unresolvable content sets Unknown.
enum class Trait : uint32_t {
	HasGround = 1 << 0,
	Blocking = 1 << 1,       // some item on the tile is unpassable
	BlocksMissiles = 1 << 2,
	BlocksPath = 1 << 3,
	Wall = 1 << 4,
	Door = 1 << 5,
	Table = 1 << 6,
	Carpet = 1 << 7,
	Border = 1 << 8,
	FloorChange = 1 << 9,
	Teleport = 1 << 10,
	MagicField = 1 << 11,
	Container = 1 << 12,
	Doodad = 1 << 13,        // stacked item owned by a doodad brush
	LooseItem = 1 << 14,     // moveable stacked item
	Unknown = 1 << 15,       // missing definition or invalid OTBM item
	Present = 1 << 16        // the tile exists; set by SemanticMap::set, not reported as a trait
};
inline constexpr size_t traitCount = 16; // reportable traits; Present is not one of them

[[nodiscard]] constexpr uint32_t bit(Trait trait) noexcept { return static_cast<uint32_t>(trait); }
[[nodiscard]] const char* traitName(size_t index) noexcept;

inline constexpr uint16_t noGroundKey = 0; // tile has no ground, or ground without a brush

struct FloorBounds {
	int z = 0;
	int x0 = 0, y0 = 0;
	int width = 0, height = 0;
};

// One floor stored as a dense box; cells outside the box are empty by definition.
struct SemanticFloor {
	FloorBounds bounds;
	std::vector<uint16_t> ground; // ground key per cell, row-major
	std::vector<uint32_t> traits; // OR of Trait bits per cell; Present marks an existing tile, 0 = no tile
};

struct SemanticBudget { uint64_t maxCells = 0; };

enum class SemanticError : uint8_t { InvalidBounds, CellLimit, TooManyGroundKeys, OutOfBounds, AllocationFailed };

class SemanticMap {
public:
	// Floors are kept ordered by z. Duplicate z or non-positive size is refused.
	[[nodiscard]] static std::expected<SemanticMap, SemanticError> create(
		std::span<const FloorBounds> bounds, const SemanticBudget& budget);

	// Key 0 is reserved for noGroundKey. Keys are interned in first-seen order, so the
	// caller must feed names in a deterministic order for a repeatable table.
	[[nodiscard]] std::expected<uint16_t, SemanticError> internGroundKey(std::string_view name);

	[[nodiscard]] std::expected<void, SemanticError> set(int x, int y, int z, uint16_t groundKey, uint32_t traits);

	[[nodiscard]] const std::vector<SemanticFloor>& floors() const noexcept { return floors_; }
	[[nodiscard]] const std::vector<std::string>& groundKeys() const noexcept { return keys_; }
	[[nodiscard]] uint64_t cellCount() const noexcept { return cells_; }

private:
	std::vector<SemanticFloor> floors_;
	std::vector<std::string> keys_ {std::string {}};
	uint64_t cells_ = 0;
};

struct GroundKeyStat {
	uint64_t tiles = 0;
	uint64_t blockingTiles = 0;
};

struct SemanticSummary {
	std::array<uint64_t, 16> tilesPerFloor {};
	std::array<uint64_t, traitCount> tilesPerTrait {};
	std::vector<GroundKeyStat> groundKeyStats; // indexed like SemanticMap::groundKeys()
};

// Pure function of the map: same input gives the same summary, independent of time or addresses.
[[nodiscard]] SemanticSummary summarize(const SemanticMap& map);

} // namespace rme::procedural
