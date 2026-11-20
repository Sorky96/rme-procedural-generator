// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/adapter/generation_input.h"
#include <array>
#include <span>
#include <string>

namespace rme::procedural {

struct GroundProfile {
	// GroundRoll: Grass, Dirt, Water (count 3). Surface: Grass, Dirt, Rock, Shore, Lake, River (count 6), or with a
	// seventh brush for Path cells (count 7), or with an eighth for Floor cells as well (count 8). A grid may only
	// contain the kinds its profile has brushes for.
	// Names are resolved in loaded assets; the same brush may serve several kinds. Entries past
	// `count` are unused and must stay empty.
	static constexpr uint8_t groundRollCount = 3;
	static constexpr uint8_t surfaceCount = 6;
	static constexpr uint8_t surfaceWithPathCount = 7;
	static constexpr uint8_t surfaceWithFloorCount = 8; // Path brush at index 6 (always filled), Floor brush at index 7
	std::array<std::string, 8> brushes;
	uint8_t count = groundRollCount;
	[[nodiscard]] std::span<const std::string> active() const noexcept { return std::span(brushes).first(count); }
	// Vegetation doodad brushes in DecorKind order after None: Tree, Bush, Stone. decorCount is 0 (none)
	// or decorationCount; entries past it must stay empty.
	static constexpr uint8_t decorationCount = 3;
	std::array<std::string, 3> decor;
	uint8_t decorCount = 0;
	// Wall brush for the outline of Floor foundations; empty = no walls. Only valid with a profile of eight brushes.
	std::string wall;
	// Door kind (a name from door_types.h) for one door per building on the outline; empty = no doors. Needs a wall brush.
	std::string door;
	// Doodad brush for the fire in the middle of every camp (G5 iteration 26); empty = no fires. Needs a profile of eight brushes.
	std::string campfire;
	[[nodiscard]] std::span<const std::string> activeDecor() const noexcept { return std::span(decor).first(decorCount); }
	// Learned style mixes; an empty name is off and its permille 0. grassMix[i]: ground brush placed in
	// noise patches over about grassMixPermille[i] of the Grass cells (surface profiles only; the shares sum to at most 1000).
	// treeMix[i]: doodad brush that takes the place of the tree brush on treeMixPermille[i] of the
	// tree cells (vegetation only; the shares sum to at most 1000).
	static constexpr size_t treeMixCount = 3;
	static constexpr size_t grassMixCount = 3;
	std::array<std::string, grassMixCount> grassMix;
	std::array<uint16_t, grassMixCount> grassMixPermille {};
	// Vegetation on grassMix[m] cells (vegetation only): grassMixDecorKeep[m][k] is the share (permille) of
	// the generated decoration of DecorKind k (Tree, Bush, Stone) such a cell keeps; grassMixDecor[m][k] is
	// the doodad brush used there instead of decor[k], empty = decor[k]. Unused entries keep the defaults.
	static constexpr std::array<uint16_t, decorationCount> keepAllDecor {1000, 1000, 1000};
	std::array<std::array<uint16_t, decorationCount>, grassMixCount> grassMixDecorKeep {keepAllDecor, keepAllDecor, keepAllDecor};
	std::array<std::array<std::string, decorationCount>, grassMixCount> grassMixDecor;
	std::array<std::string, treeMixCount> treeMix;
	std::array<uint16_t, treeMixCount> treeMixPermille {};
	bool operator==(const GroundProfile&) const = default;
	std::string caveEntrance, caveExit, caveGround;
	std::string caveWall;
    std::string caveDoor; // wall brush supplying closed, unlocked normal doors
    std::string plateauGround, plateauRamp;
    std::string plateauCliff = "mountain";
    std::string bridgeBrush; // empty: sand fords; otherwise walkable wall-brush ground variants
    std::string buildingStairs; // ramp-like doodad for storey stairs; set exactly when SurfaceTuning::buildingStoreys is
    // Iteration 45, set exactly when SurfaceTuning::buildingDungeons is: dungeon floor (ground brush), walls with closed,
    // unlocked normal doors (wall brush), the ramp-like doodad for the stairs up in the dungeon and (iteration 45b) the
    // ground or doodad brush whose floor-change-down ground is the visible stairs down inside the building.
    std::string dungeonFloor, dungeonWall, dungeonStairs, dungeonDescent;
};

struct GroundPlan {
	GenerationConfig config;
	GroundProfile profile;
	std::string client;
	uint64_t mapGeneration = 0;
	uint64_t mapRevision = 0;
	uint64_t assetRevision = 0;
	uint64_t semanticFingerprint = 0;
	// Row-major brush placement IDs; 0 means Preserve, never erase.
	std::vector<uint16_t> items;
	// Row-major vegetation item IDs placed on top of the ground; empty when vegetation is off,
	// otherwise one entry per cell with 0 meaning none.
	std::vector<uint16_t> decor;
	// Wall item per cell for the outline of a Floor foundation (the brush's first item; the real alignment is fixed by
	// RME's wall logic on the preview buffer); empty when the profile has no wall brush, otherwise one entry per cell.
	std::vector<uint16_t> walls;
	// 1 on the outline cell that gets a door (RME's DoorBrush replaces the wall piece there); empty when the profile has no door.
	std::vector<uint8_t> doors;
	// Camp fire item per cell (0 = none); empty when the profile has no campfire brush, otherwise one entry per cell.
	std::vector<uint16_t> fires;
	[[nodiscard]] uint64_t fingerprint() const;
	std::vector<uint16_t> entrances;
	std::vector<uint16_t> exits;
	// Lower-floor cells: 0 = empty Solid, 1 = Floor, 2 = Solid wall band, 3 = dungeon floor, 4 = dungeon wall or door,
	// 5 = dungeon wall at an outer corner of the ring, without ground;
	// authenticates placement and borders.
	std::vector<uint8_t> caveFloor;
    // 0 None, 1 grass, 2 cliff supporting the next cap, 3 ramp opening, 4 storey floor, 5 storey stairs opening
    std::vector<uint8_t> plateauFloor;
    std::vector<uint16_t> plateauRamps;
    // Surface plan with storeys: stairs and tail items on the ground floor (0 = none); empty otherwise.
    std::vector<uint16_t> stairs;
};

// True when a ground brush of the profile has an outer border to "nothing": the border calculator then draws
// it onto empty neighbour tiles, which for a generated area means outside it. Such profiles need
// surfaceKeepClear(snapshot, true). Brushes that do not resolve count as false; resolution reports them.
[[nodiscard]] bool profileNeedsEdgeClearance(const GroundProfile& profile);

// Iteration 44: the config with SurfaceTuning::rockBlocks set when any ground variant of the profile's rock brush is
// unpassable in the loaded client (surface algorithm only). Preparation and validation both use it, so callers keep
// passing the dialog's config.
[[nodiscard]] GenerationConfig withRockPassability(GenerationConfig config, const GroundProfile& profile);

enum class ResolveError {
	WrongThread, NoClient, StaleAssets, InvalidInput, InvalidProfile, InvalidItem,
	PlanLimit, Cancelled, AllocationFailed
};

// GUI thread only. No map/global brush/RNG mutation. This plan still needs the
// DrawOperations sandbox, halo/diff validation and stale checks before commit.
// maxPlanBytes accounts for this result's payload, not the full generation job.
std::expected<GroundPlan, ResolveError> resolveGroundPlan(
	const GenerationInput& input, const SemanticGrid& grid, const GroundProfile& profile,
	uint64_t maxPlanBytes, std::stop_token stop = {});

} // namespace rme::procedural
