// SPDX-License-Identifier: GPL-3.0-or-later
// Linked against actual RME objects by g1_build_input_probe.py.
#include "app/main.h"
#include "brushes/ground/ground_brush.h"
#include "procedural/core/generation.h"
#include "procedural/adapter/generation_input.h"
#include "map/map.h"
#include "map/map_region.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <wx/init.h>

// Existing editor RNG, defined in util/common.cpp.
std::mt19937& getRandomGenerator();

namespace {
class FixtureBrush final : public GroundBrush {
public:
	void variants(std::initializer_list<ItemChanceBlock> values, int total) {
		border_items = values;
		total_chance = total;
	}
};
void check(bool condition, const char* message) {
	if (!condition) throw std::runtime_error(message);
}

void captureInput() {
	using namespace rme::procedural;
	Map map;
	const GenerationConfig config {{100, 100, 7, 4, 3}, 1729};
	constexpr CoreBudget coreBudget {12, 1024};
	constexpr uint64_t inputBytes = sizeof(GenerationInput) + 24;
	std::array<uint8_t, 12> mask {1,1,1,1, 1,1,1,1, 1,1,1,0};
	// Location-only metadata must protect even positions with no Tile object.
	map.createTileL(100, 100, 7)->increaseSpawnCount();
	map.createTileL(101, 100, 7)->increaseWaypointCount();
	map.createTileL(102, 100, 7)->increaseTownCount();
	map.createTileL(103, 100, 7)->createHouseExits()->push_back(42);
	map.createTile(100, 101, 7)->mapflags = TILESTATE_PROTECTIONZONE;
	map.createTile(101, 101, 7)->house_id = 42;
	map.createTile(102, 101, 7)->spawn = std::make_unique<Spawn>(3);
	map.createTile(103, 101, 7)->getOrCreateExtra();
	map.createTile(100, 102, 7)->statflags = TILESTATE_UNIQUE;
	map.createTile(101, 102, 7)->statflags = TILESTATE_SELECTED | TILESTATE_MODIFIED;
	const auto beforeRevision = map.getChangeTracker().getGeneration();
	const auto beforeCount = map.getTileCount();
	const auto beforeChanged = map.hasChanged();
	auto captured = captureGenerationInput(map, config, coreBudget, inputBytes, mask);
	check(bool(captured), "Capture rejected valid synthetic map");
	for (size_t i = 0; i < 12; ++i) {
		check(captured->protection[i] == (i < 9 ? 1 : 0), "Empty-only protection missed metadata or protected empty selection");
		check(captured->mask[i] == mask[i], "Capture changed selection mask");
	}
	check(captured->mapGeneration == map.getGeneration() && captured->mapRevision == beforeRevision,
		"Capture omitted map identity/revision");
	check(map.getTileCount() == beforeCount && map.getChangeTracker().getGeneration() == beforeRevision &&
		map.hasChanged() == beforeChanged && !map.getTile(102, 102, 7), "Capture mutated live map");
	mask.fill(0);
	map.getTile(100, 101, 7)->mapflags = 0;
	check(captured->mask[0] == 1 && captured->protection[4] == 1, "Captured input aliases live data");
	const auto semantic = generateTerrain(captured->config, captured->extent, coreBudget,
		captured->mask, captured->protection);
	check(semantic && semantic->valid(), "Owned input could not generate semantic result");
	for (size_t i = 0; i < 12; ++i) {
		check((semantic->cells[i] == Terrain::Preserve) == (i < 9 || i == 11), "Captured protection not obeyed");
	}
	const auto shortBudget = captureGenerationInput(map, config, coreBudget, inputBytes - 1);
	check(!shortBudget && shortBudget.error() == InputError::InputLimit, "Input budget not enforced");
	std::stop_source cancel;
	cancel.request_stop();
	const auto cancelled = captureGenerationInput(map, config, coreBudget, inputBytes, {}, cancel.get_token());
	check(!cancelled && cancelled.error() == InputError::Cancelled, "Cancelled capture returned input");
	bool workerRejected = false;
	std::jthread worker([&] {
		const auto result = captureGenerationInput(map, config, coreBudget, inputBytes);
		workerRejected = !result && result.error() == InputError::WrongThread;
	});
	worker.join();
	check(workerRejected, "Worker allowed access to live map");
}
}

int main() {
	try {
		wxInitializer initializer;
		check(initializer.IsOk(), "wx initialization failed");
		captureInput();
		FixtureBrush brush;
		check(!brush.getPlacementItemIdForRoll(1), "Empty brush accepted");
		brush.variants({{3, 101}, {7, 102}, {10, 103}}, 10);
		// Preserve the existing strict '<' boundary and last-roll fallback.
		constexpr std::array<uint16_t, 10> expected {101, 101, 102, 102, 102, 102, 103, 103, 103, 101};
		for (int roll = 1; roll <= 10; ++roll) {
			check(brush.getPlacementItemIdForRoll(roll) == expected[roll - 1], "Legacy variant selection changed");
		}
		check(!brush.getPlacementItemIdForRoll(0) && !brush.getPlacementItemIdForRoll(-1) &&
			!brush.getPlacementItemIdForRoll(11), "Invalid roll accepted");
		brush.variants({{1, 104}}, 1);
		check(brush.getPlacementItemIdForRoll(1) == 104, "Single variant fallback changed");
		brush.variants({{5, 0}, {10, 105}}, 10);
		check(brush.getPlacementItemIdForRoll(1) == 0, "Zero-ID legacy fallback changed");
		brush.variants({{3, 101}, {7, 102}, {10, 103}}, 10);
		rme::procedural::StableRandom first(1729, 0x67726f756e64ULL), again(1729, 0x67726f756e64ULL);
		const auto globalRandomBefore = getRandomGenerator();
		for (int i = 0; i < 10000; ++i) {
			const auto roll = int(first.bounded(10)) + 1;
			const auto sameRoll = int(again.bounded(10)) + 1;
			check(brush.getPlacementItemIdForRoll(roll) == brush.getPlacementItemIdForRoll(sameRoll),
				"Controlled stream did not reproduce brush variants");
		}
		check(getRandomGenerator() == globalRandomBefore, "Explicit variant selection advanced global RNG");
		std::cout << "PROCEDURAL INPUT AND GROUND VARIANT PASS (actual RME map/brush; no assets, drawing or commit)\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
