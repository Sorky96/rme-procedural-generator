// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/decoration.h"
#include "procedural/core/scale_stats.h"
#include "procedural/core/surface.h"
#include "procedural/core/reachability.h"
#include "procedural/core/underground.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace rme::procedural;

namespace {
thread_local bool allocationRecording = false;
thread_local size_t allocationCount = 0;
thread_local size_t failedAllocation = 0;
}

// Test executable only: no allocator hook is linked into RME or the core library.
void* operator new(std::size_t size) {
	if (allocationRecording && ++allocationCount == failedAllocation) throw std::bad_alloc();
	if (void* memory = std::malloc(size ? size : 1)) return memory;
	throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
constexpr MapExtent extent { 65001, 65001, 16 };
constexpr SurfaceBudget budget { 250000, 4 * 1024 * 1024, 64 * 1024 * 1024 };

void require(bool value, std::string_view message) {
	if (!value) throw std::runtime_error(std::string(message));
}

SurfaceConfig makeConfig(uint32_t side, uint64_t seed) { return { { 100, 100, 7, side, side }, seed }; }

size_t countOf(const SurfaceGrid& grid, SurfaceKind kind) {
	return size_t(std::ranges::count(grid.cells, kind));
}

// Labels 4-connected components of cells matching predicate; returns component sizes and
// whether each touches the border or a 4-neighbour matching `touch`.
struct Component { size_t size = 0; bool touchesBorder = false; bool touches = false; };

template <typename Match, typename Touch>
std::vector<Component> components(const SurfaceGrid& grid, Match match, Touch touch) {
	const uint32_t w = grid.region.width, h = grid.region.height;
	std::vector<uint8_t> seen(grid.cells.size(), 0);
	std::vector<Component> result;
	std::vector<size_t> stack;
	for (size_t start = 0; start < grid.cells.size(); ++start) {
		if (seen[start] || !match(grid.cells[start])) continue;
		Component component;
		stack.assign(1, start);
		seen[start] = 1;
		while (!stack.empty()) {
			const size_t cell = stack.back();
			stack.pop_back();
			++component.size;
			const uint32_t x = uint32_t(cell % w), y = uint32_t(cell / w);
			component.touchesBorder |= x == 0 || y == 0 || x + 1 == w || y + 1 == h;
			const auto visit = [&](size_t next) {
				component.touches |= touch(grid.cells[next]);
				if (!seen[next] && match(grid.cells[next])) { seen[next] = 1; stack.push_back(next); }
			};
			if (x > 0) visit(cell - 1);
			if (x + 1 < w) visit(cell + 1);
			if (y > 0) visit(cell - w);
			if (y + 1 < h) visit(cell + w);
		}
		result.push_back(component);
	}
	return result;
}

void parametersAndLimits() {
	const auto base = makeConfig(32, 1);
	const auto expectError = [&](const SurfaceConfig& bad, SurfaceBudget limit, SurfaceError expected, std::string_view what) {
		const auto result = generateSurface(bad, extent, limit);
		require(!result && result.error() == expected, what);
	};
	auto bad = base; bad.tuning.featureSize = 0;
	expectError(bad, budget, SurfaceError::InvalidParameters, "featureSize 0 accepted");
	bad = base; bad.tuning.featureSize = 4097;
	expectError(bad, budget, SurfaceError::InvalidParameters, "featureSize 4097 accepted");
	bad = base; bad.tuning.lakePermille = 501;
	expectError(bad, budget, SurfaceError::InvalidParameters, "lakePermille 501 accepted");
	bad = base; bad.tuning.rockPermille = 501;
	expectError(bad, budget, SurfaceError::InvalidParameters, "rockPermille 501 accepted");
	bad = base; bad.tuning.riverCount = 1025;
	expectError(bad, budget, SurfaceError::InvalidParameters, "riverCount 1025 accepted");
	bad = base; bad.tuning.meander = 9;
	expectError(bad, budget, SurfaceError::InvalidParameters, "meander 9 accepted");
	bad = base; bad.tuning.meander = 8;
	require(bool(generateSurface(bad, extent, budget)), "meander 8 refused");
	bad = base; bad.tuning.shoreWidth = 9;
	expectError(bad, budget, SurfaceError::InvalidParameters, "shoreWidth 9 accepted");
	bad = base; bad.tuning.buildingDensity = 101;
	expectError(bad, budget, SurfaceError::InvalidParameters, "buildingDensity 101 accepted");
	bad = base; bad.tuning.pathLength = 401;
	expectError(bad, budget, SurfaceError::InvalidParameters, "pathLength 401 accepted");
	bad = base; bad.tuning.dirtPermille = 1001;
	expectError(bad, budget, SurfaceError::InvalidParameters, "dirtPermille 1001 accepted");
	bad = base; bad.tuning.transitionWidth = 32769;
	expectError(bad, budget, SurfaceError::InvalidParameters, "transitionWidth 32769 accepted");
	bad = base; bad.region.width = 0;
	expectError(bad, budget, SurfaceError::InvalidRegion, "Empty region accepted");
	bad = base; bad.region.x = 65000;
	expectError(bad, budget, SurfaceError::InvalidRegion, "Region outside extent accepted");
	bad = base; bad.region.z = 16;
	expectError(bad, budget, SurfaceError::InvalidRegion, "Layer outside extent accepted");
	expectError(base, { 100, budget.maxOutputBytes, budget.maxWorkingBytes }, SurfaceError::CellLimit, "Cell limit ignored");
	expectError(base, { budget.maxCells, 100, budget.maxWorkingBytes }, SurfaceError::OutputLimit, "Output limit ignored");
	expectError(base, { budget.maxCells, budget.maxOutputBytes, 1000 }, SurfaceError::WorkingLimit, "Working limit ignored");
	const auto layout = validateSurface(base, extent, budget);
	require(layout && layout->cellCount == 1024 && layout->workingBytes > 1024 * 40, "Layout not reported");

	std::vector<uint8_t> mask(1024, 1), protection(1024, 0);
	const auto shortMask = generateSurface(base, extent, budget, std::span(mask).first(10));
	require(!shortMask && shortMask.error() == SurfaceError::InvalidMask, "Short mask accepted");
	mask[0] = 2;
	const auto nonBinary = generateSurface(base, extent, budget, mask);
	require(!nonBinary && nonBinary.error() == SurfaceError::InvalidMask, "Nonbinary mask accepted");
	mask.assign(1024, 0);
	const auto empty = generateSurface(base, extent, budget, mask);
	require(!empty && empty.error() == SurfaceError::NoWritableCells, "Empty mask accepted");
	protection.assign(1024, 1);
	const auto locked = generateSurface(base, extent, budget, {}, protection);
	require(!locked && locked.error() == SurfaceError::NoWritableCells, "Fully protected area accepted");

	std::stop_source source;
	source.request_stop();
	const auto stopped = generateSurface(base, extent, budget, {}, {}, source.get_token());
	require(!stopped && stopped.error() == SurfaceError::Cancelled, "Stop request ignored");
}

void deterministicAndMaskIndependent() {
	const auto config = makeConfig(100, 1729);
	const auto first = generateSurface(config, extent, budget);
	const auto again = generateSurface(config, extent, budget);
	require(first && again && first->cells == again->cells && first->valid(), "Surface did not reproduce");
	require(first->fingerprint() == again->fingerprint(), "Fingerprint did not repeat");
	auto other = config; ++other.seed;
	const auto different = generateSurface(other, extent, budget);
	require(different && different->cells != first->cells, "Seed had no effect");
	require(std::ranges::find(first->cells, SurfaceKind::Preserve) == first->cells.end(), "Full mask left Preserve");

	// A mask or protection must not change any other cell, including thresholds and rivers.
	std::vector<uint8_t> mask(first->cells.size(), 1), protection(first->cells.size(), 0);
	for (size_t i = 0; i < mask.size(); ++i) {
		mask[i] = (i * 7 + i / 100) % 5 != 0;
		protection[i] = (i % 97) == 3;
	}
	const auto selected = generateSurface(config, extent, budget, mask, protection);
	require(bool(selected), "Irregular mask rejected");
	for (size_t i = 0; i < mask.size(); ++i) {
		require(selected->cells[i] == ((mask[i] && !protection[i]) ? first->cells[i] : SurfaceKind::Preserve),
			"Mask/protection changed another cell");
	}

	SurfaceGrid invalid = *first;
	invalid.cells[0] = static_cast<SurfaceKind>(255);
	require(!invalid.valid(), "Unknown kind accepted");
	invalid = *first; invalid.cells.pop_back();
	require(!invalid.valid(), "Short grid accepted");
	auto moved = *first; ++moved.region.z;
	require(moved.fingerprint() != first->fingerprint(), "Fingerprint omitted Z");
	moved = *first; moved.cells[0] = moved.cells[0] == SurfaceKind::Rock ? SurfaceKind::Grass : SurfaceKind::Rock;
	require(moved.fingerprint() != first->fingerprint(), "Fingerprint omitted kind");
	const SurfaceGrid tiny { { 1, 2, 3, 1, 1 }, { SurfaceKind::Lake } };
	// Pinned from the first passing run of algorithm v2; a change means output or wire layout changed.
	require(tiny.fingerprint() == 17245445524122864401ULL, "Surface fingerprint wire layout changed");
	require(first->fingerprint() == 17159159863505677800ULL, "Seed 1729 100x100 surface output changed");
}

struct Tally { size_t cells = 0, grass = 0, dirt = 0, rock = 0, shore = 0, lake = 0, river = 0; std::array<size_t, 5> shoreAt {}; };

Tally structure(const SurfaceConfig& config, const SurfaceGrid& grid) {
	Tally tally { grid.cells.size(), countOf(grid, SurfaceKind::Grass), countOf(grid, SurfaceKind::Dirt),
		countOf(grid, SurfaceKind::Rock), countOf(grid, SurfaceKind::Shore), countOf(grid, SurfaceKind::Lake),
		countOf(grid, SurfaceKind::River) };
	require(tally.grass + tally.dirt + tally.rock + tally.shore + tally.lake + tally.river == tally.cells,
		"A cell has no surface kind");
	require(tally.lake <= tally.cells * config.tuning.lakePermille / 1000, "Lake share above its cap");
	for (const auto& lake : components(grid, [](SurfaceKind k) { return k == SurfaceKind::Lake; }, [](SurfaceKind) { return false; })) {
		require(lake.size >= config.tuning.minLakeCells, "Lake smaller than minimum");
	}
	// A river component must end at a lake or the rectangle border and meet the length minimum.
	for (const auto& river : components(grid, [](SurfaceKind k) { return k == SurfaceKind::River; },
		[](SurfaceKind k) { return k == SurfaceKind::Lake; })) {
		require(river.touchesBorder || river.touches, "River ends inside the land");
		require(river.size >= config.tuning.minRiverLength, "River shorter than minimum");
	}
	// A lake leaves no island: land fully surrounded by lake has at least 24 cells (islandCells).
	for (const auto& land : components(grid, [](SurfaceKind k) { return k != SurfaceKind::Lake; }, [](SurfaceKind) { return false; })) {
		require(land.touchesBorder || land.size >= 24, "Land patch enclosed by a lake");
	}
	// Rivers spring from rock: every river touches a rock cell (its source).
	for (const auto& river : components(grid, [](SurfaceKind k) { return k == SurfaceKind::River; },
		[](SurfaceKind k) { return k == SurfaceKind::Rock; })) {
		require(river.touches, "River does not start at rock");
	}
	// With shoreWidth >= 1, land next to a lake is shore unless it is rock or river.
	const uint32_t w = grid.region.width, h = grid.region.height;
	for (size_t cell = 0; cell < grid.cells.size(); ++cell) {
		const auto kind = grid.cells[cell];
		if (kind == SurfaceKind::Lake || kind == SurfaceKind::Shore || kind == SurfaceKind::Rock || kind == SurfaceKind::River) continue;
		const uint32_t x = uint32_t(cell % w), y = uint32_t(cell / w);
		const bool nextToLake = (x > 0 && grid.cells[cell - 1] == SurfaceKind::Lake) ||
			(x + 1 < w && grid.cells[cell + 1] == SurfaceKind::Lake) ||
			(y > 0 && grid.cells[cell - w] == SurfaceKind::Lake) ||
			(y + 1 < h && grid.cells[cell + w] == SurfaceKind::Lake);
		require(!nextToLake || config.tuning.shoreWidth == 0, "Land touches a lake without a shore");
	}
	// A river is never one cell wide: each river cell sits in a 2x2 window of river/lake cells,
	// unless a lake or the rock it springs from clips the block at that cell.
	const auto wet = [&](uint32_t x, uint32_t y) {
		const auto kind = grid.cells[size_t(y) * w + x];
		return kind == SurfaceKind::River || kind == SurfaceKind::Lake;
	};
	for (size_t cell = 0; cell < grid.cells.size(); ++cell) {
		if (grid.cells[cell] != SurfaceKind::River) continue;
		const uint32_t x = uint32_t(cell % w), y = uint32_t(cell / w);
		bool wide = false;
		for (uint32_t wy = (y > 0 ? y - 1 : 0); wy <= y && wy + 1 < h && !wide; ++wy) {
			for (uint32_t wx = (x > 0 ? x - 1 : 0); wx <= x && wx + 1 < w; ++wx) {
				wide |= wet(wx, wy) && wet(wx + 1, wy) && wet(wx, wy + 1) && wet(wx + 1, wy + 1);
			}
		}
		const bool nextToLake = (x > 0 && grid.cells[cell - 1] == SurfaceKind::Lake) ||
			(x + 1 < w && grid.cells[cell + 1] == SurfaceKind::Lake) ||
			(y > 0 && grid.cells[cell - w] == SurfaceKind::Lake) ||
			(y + 1 < h && grid.cells[cell + w] == SurfaceKind::Lake);
		const bool nextToRock = (x > 0 && grid.cells[cell - 1] == SurfaceKind::Rock) ||
			(x + 1 < w && grid.cells[cell + 1] == SurfaceKind::Rock) ||
			(y > 0 && grid.cells[cell - w] == SurfaceKind::Rock) ||
			(y + 1 < h && grid.cells[cell + w] == SurfaceKind::Rock);
		require(wide || nextToLake || nextToRock, "One-cell-wide river stretch");
	}
	// Mainland shores obey shoreWidth; isolated islands are entirely sand (I40).
	const auto islands = sandIslandsOf(grid);
	require(bool(islands), "Island classification failed");
	std::vector<uint8_t> island(grid.cells.size());
	for (const auto i : islands->cells) island[i] = 1;
	std::vector<uint32_t> distance(grid.cells.size(), UINT32_MAX);
	std::vector<size_t> queue;
	for (size_t cell = 0; cell < grid.cells.size(); ++cell) {
		if (grid.cells[cell] == SurfaceKind::Lake) { distance[cell] = 0; queue.push_back(cell); }
	}
	for (size_t head = 0; head < queue.size(); ++head) {
		const size_t cell = queue[head];
		const uint32_t x = uint32_t(cell % w), y = uint32_t(cell / w);
		const auto visit = [&](size_t next) {
			if (distance[next] == UINT32_MAX) { distance[next] = distance[cell] + 1; queue.push_back(next); }
		};
		if (x > 0) visit(cell - 1);
		if (x + 1 < w) visit(cell + 1);
		if (y > 0) visit(cell - w);
		if (y + 1 < h) visit(cell + w);
	}
	for (size_t cell = 0; cell < grid.cells.size(); ++cell) {
		require(grid.cells[cell] != SurfaceKind::Shore || island[cell] || distance[cell] <= config.tuning.shoreWidth,
			"Shore farther from a lake than shoreWidth");
		if (grid.cells[cell] == SurfaceKind::Shore && distance[cell] < tally.shoreAt.size()) ++tally.shoreAt[distance[cell]];
	}
	return tally;
}

// Rivers with a sideways swing keep every structural rule (they end at a lake or the border, start at rock, are
// never one cell wide, tributaries stay joined) and stay deterministic; meander 0 is the original output.
void emptyRiverRoutes() {
	auto config = makeConfig(32, 1);
	config.region.height = 1; // Every cell is an outlet in the priority flood.
	config.tuning.lakePermille = 0;
	config.tuning.rockPermille = 500;
	config.tuning.minRockCells = 1;
	config.tuning.minRiverLength = 0;
	config.tuning.riverCount = 0;
	const auto withoutRivers = generateSurface(config, extent, budget);
	require(withoutRivers && countOf(*withoutRivers, SurfaceKind::Rock) > 0, "empty-route fixture has rock sources");
	config.tuning.riverCount = 1;
	const auto withRivers = generateSurface(config, extent, budget);
	require(withRivers && withRivers->cells == withoutRivers->cells, "empty river routes leave terrain intact");
}

void meanderingRivers(uint32_t side, uint32_t seeds) {
	size_t seedsWithRiver = 0, changed = 0;
	for (const uint32_t meander : {2u, 5u, 8u}) {
		for (uint32_t seed = 1; seed <= seeds; ++seed) {
			auto config = makeConfig(side, seed);
			const auto plain = generateSurface(config, extent, budget);
			require(plain && plain->valid(), "Surface generation failed");
			config.tuning.meander = meander;
			const auto first = generateSurface(config, extent, budget);
			const auto again = generateSurface(config, extent, budget);
			require(first && first->valid() && again && first->cells == again->cells, "Meandering surface is not repeatable");
			(void)structure(config, *first);
			require(countOf(*first, SurfaceKind::River) > 0 || countOf(*plain, SurfaceKind::River) == 0 || meander > 0,
				"A river disappeared");
			if (meander == 5) {
				seedsWithRiver += countOf(*plain, SurfaceKind::River) > 0;
				changed += first->cells != plain->cells;
			}
			// Lakes and rock do not depend on the river swing.
			require(countOf(*first, SurfaceKind::Lake) == countOf(*plain, SurfaceKind::Lake), "Meander changed the lakes");
			auto zero = config; zero.tuning.meander = 0;
			const auto restored = generateSurface(zero, extent, budget);
			require(restored && restored->cells == plain->cells, "Meander 0 is not the original surface");
		}
	}
	require(seedsWithRiver > 0 && changed > 0, "Meander never changed a river");
	std::cout << "meander side=" << side << " seeds=" << seeds << " with_river=" << seedsWithRiver << " changed=" << changed << '\n';
}

void distributions(uint32_t side, uint32_t seeds) {
	size_t riverSeeds = 0, lakeSeeds = 0, rockSeeds = 0, dirtSeeds = 0, riverCells = 0, thickRiverCells = 0;
	std::array<size_t, 5> shoreAt {};
	double minDirt = 1, maxDirt = 0;
	for (uint32_t seed = 1; seed <= seeds; ++seed) {
		const auto config = makeConfig(side, seed);
		const auto grid = generateSurface(config, extent, budget);
		require(grid && grid->valid(), "Surface generation failed");
		const auto tally = structure(config, *grid);
		require(tally.rock <= tally.cells * (config.tuning.rockPermille + 20) / 1000, "Rock share far above its cap");
		// Rivers thicken after a merge: count river cells that are the centre of a full 3x3 block.
		const uint32_t gw = grid->region.width, gh = grid->region.height;
		for (uint32_t y = 1; y + 1 < gh; ++y) {
			for (uint32_t x = 1; x + 1 < gw; ++x) {
				if (grid->cells[size_t(y) * gw + x] != SurfaceKind::River) continue;
				++riverCells;
				bool full = true;
				for (uint32_t dy = 0; dy < 3 && full; ++dy) for (uint32_t dx = 0; dx < 3; ++dx) {
					full &= grid->cells[size_t(y - 1 + dy) * gw + (x - 1 + dx)] == SurfaceKind::River;
				}
				thickRiverCells += full;
			}
		}
		for (size_t ring = 1; ring < shoreAt.size(); ++ring) shoreAt[ring] += tally.shoreAt[ring];
		riverSeeds += tally.river > 0; lakeSeeds += tally.lake > 0; rockSeeds += tally.rock > 0;
		const double dirtShare = double(tally.dirt) / double(std::max<size_t>(1, tally.grass + tally.dirt));
		minDirt = std::min(minDirt, dirtShare); maxDirt = std::max(maxDirt, dirtShare);
		dirtSeeds += tally.dirt > 0 && tally.grass > 0;
		// Without rivers, every accepted rock patch keeps its minimum size.
		auto noRivers = config; noRivers.tuning.riverCount = 0;
		const auto dry = generateSurface(noRivers, extent, budget);
		require(dry && countOf(*dry, SurfaceKind::River) == 0, "riverCount 0 produced a river");
		for (const auto& rock : components(*dry, [](SurfaceKind k) { return k == SurfaceKind::Rock; }, [](SurfaceKind) { return false; })) {
			require(rock.size >= config.tuning.minRockCells, "Rock patch smaller than minimum");
		}
	}
	require(dirtSeeds == seeds, "Some seed lacked grass or dirt");
	require(minDirt >= 0.10 && maxDirt <= 0.60, "Dirt share of open land outside 10-60%");
	std::cout << side << 'x' << side << " seeds=" << seeds << " with_lake=" << lakeSeeds << " with_river=" << riverSeeds
		<< " with_rock=" << rockSeeds << " river_cells=" << riverCells << " in_3x3_block=" << thickRiverCells << " shore_at_1/2/3=" << shoreAt[1] << '/' << shoreAt[2] << '/' << shoreAt[3] << " dirt_share_of_open_land=" << minDirt << ".." << maxDirt << '\n';
	require(lakeSeeds > 0 && riverSeeds > 0 && rockSeeds > 0, "A feature never appeared in any seed");
	// The beach is irregular: outer rings exist, but only partly, so the band is not an even width.
	require(shoreAt[2] > 0 && shoreAt[3] > 0 && shoreAt[2] < shoreAt[1] && shoreAt[3] < shoreAt[2], "Shore band has an even width");
}

void transitionBand() {
	// A zero-width band is a hard threshold; a wide band must mix both kinds near it.
	auto hard = makeConfig(100, 5); hard.tuning.transitionWidth = 0;
	auto soft = hard; soft.tuning.transitionWidth = 16000;
	const auto a = generateSurface(hard, extent, budget);
	const auto b = generateSurface(soft, extent, budget);
	require(a && b && a->cells != b->cells, "Transition width had no effect");
	const auto mixed = [](const SurfaceGrid& grid) {
		size_t edges = 0;
		for (size_t i = 0; i + 1 < grid.cells.size(); ++i) {
			if (i % grid.region.width == grid.region.width - 1) continue;
			const auto l = grid.cells[i], r = grid.cells[i + 1];
			edges += (l == SurfaceKind::Grass && r == SurfaceKind::Dirt) || (l == SurfaceKind::Dirt && r == SurfaceKind::Grass);
		}
		return edges;
	};
	std::cout << "grass_dirt_edges hard=" << mixed(*a) << " soft=" << mixed(*b) << '\n';
	require(mixed(*b) > mixed(*a), "Soft transition did not add grass/dirt interleaving");
}

void allocationFailures(const SurfaceConfig& config, bool expectStructures) {
	allocationRecording = true; allocationCount = 0; failedAllocation = 0;
	const auto baseline = generateSurface(config, extent, budget);
	allocationRecording = false;
	require(bool(baseline), "Baseline failed");
	if (expectStructures) {
		require(!baseline->doors.empty() && !baseline->entrances.empty(),
			"Allocation fixture did not generate buildings and cave entrances");
	}
	const size_t total = allocationCount;
	size_t failures = 0;
	for (size_t fail = 1; fail <= total; ++fail) {
		allocationRecording = true; allocationCount = 0; failedAllocation = fail;
		const auto result = generateSurface(config, extent, budget);
		allocationRecording = false;
		require(allocationCount >= fail, "Allocation fault was not reached");
		if (!result) { require(result.error() == SurfaceError::AllocationFailed, "Unexpected error on bad_alloc"); ++failures; }
		else require(result->valid() && result->region == baseline->region && result->cells == baseline->cells &&
			result->doors == baseline->doors && result->gaps == baseline->gaps && result->sites == baseline->sites &&
			result->fires == baseline->fires && result->entrances == baseline->entrances &&
			result->bridgeAxes == baseline->bridgeAxes, "Result changed after skipped failure");
	}
	std::cout << "allocation_points=" << total << " injected_failures_reported=" << failures << '\n';
	require(failures > 0, "No allocation failure was reported");
}

void caveAllocationFailures() {
	SurfaceGrid surface;
	surface.region = {100, 100, 7, 40, 20};
	surface.cells.assign(800, SurfaceKind::Rock);
	surface.entrances = {410, 430};
	std::vector<uint8_t> blocked(surface.cells.size());
	blocked[420] = 1;
	const auto generate = [&] { return generateCave(surface, 7, {}, nullptr, blocked); };

	allocationCount = 0;
	failedAllocation = 0;
	allocationRecording = true;
	const auto baseline = generate();
	allocationRecording = false;
	const auto total = allocationCount;
	require(baseline && baseline->valid() && baseline->blocked == blocked &&
		baseline->cells[410] == CaveKind::Floor && baseline->cells[430] == CaveKind::Floor &&
		baseline->cells[420] == CaveKind::Solid && total > 0, "Cave allocation fixture failed");

	for (size_t fault = 1; fault <= total; ++fault) {
		allocationCount = 0;
		failedAllocation = fault;
		allocationRecording = true;
		const auto result = generate();
		allocationRecording = false;
		require(allocationCount >= fault && !result && result.error() == SurfaceError::AllocationFailed,
			"Cave allocation fault returned partial output or the wrong error");
	}
	const auto retry = generate();
	require(retry && retry->region == baseline->region && retry->cells == baseline->cells &&
		retry->rooms == baseline->rooms && retry->corridors == baseline->corridors &&
		retry->blocked == baseline->blocked, "Cave allocation fault changed retry output");
	std::cout << "cave_allocation_failure_cases=" << total << '\n';
}

void cavePruningAllocationFailures() {
	CaveGrid cave;
	cave.region = {100, 100, 8, 8, 8};
	cave.cells.assign(64, CaveKind::Solid);
	cave.cells[9] = CaveKind::Floor;
	for (size_t i = 33; i <= 38; ++i) cave.cells[i] = CaveKind::Floor;
	CaveExitPlan exits;
	exits.exits.push_back({0, 33, 0});

	auto pruned = cave;
	allocationCount = 0;
	failedAllocation = 0;
	allocationRecording = true;
	const auto baseline = removeUnenteredCaveComponents(pruned, exits);
	allocationRecording = false;
	const auto total = allocationCount;
	require(baseline == 1 && pruned.cells[9] == CaveKind::Solid && pruned.cells[33] == CaveKind::Floor &&
		total > 3, "Cave pruning fixture did not grow the queue after finding an isolated component");

	for (size_t fault = 1; fault <= total; ++fault) {
		auto candidate = cave;
		allocationCount = 0;
		failedAllocation = fault;
		allocationRecording = true;
		const auto result = removeUnenteredCaveComponents(candidate, exits);
		allocationRecording = false;
		require(allocationCount >= fault && !result && result.error() == UndergroundError::AllocationFailed &&
			candidate.cells == cave.cells, "Failed cave pruning changed the source grid");
	}
	std::cout << "cave_pruning_allocation_failure_cases=" << total << '\n';
}

void measure(uint32_t side) {
	const auto config = makeConfig(side, 1729);
	constexpr SurfaceBudget big { 1000000, 8 * 1024 * 1024, 128 * 1024 * 1024 };
	const auto started = std::chrono::steady_clock::now();
	const auto grid = generateSurface(config, extent, big);
	const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
	require(grid && grid->valid(), "Measured run failed");
	const auto layout = validateSurface(config, extent, big);
	std::cout << side << 'x' << side << " generation_ms=" << elapsed << " working_bytes_estimate=" << layout->workingBytes
		<< " fingerprint=" << grid->fingerprint() << " (single run)\n";
}

void decoration(uint32_t side, uint32_t seeds) {
	size_t trees = 0, bushes = 0, stones = 0, landCells = 0, forestSeeds = 0;
	for (uint32_t seed = 1; seed <= seeds; ++seed) {
		const auto config = makeConfig(side, seed);
		const auto grid = generateSurface(config, extent, budget);
		require(grid && grid->valid(), "Surface generation failed");
		const auto decor = generateDecoration(*grid, seed);
		const auto again = generateDecoration(*grid, seed);
		require(decor && again && decor->valid() && decor->cells == again->cells, "Decoration not deterministic");
		require(decor->region == grid->region, "Decoration region differs");
		const uint32_t w = side;
		size_t seedTrees = 0;
		for (size_t i = 0; i < grid->cells.size(); ++i) {
			const SurfaceKind kind = grid->cells[i];
			const DecorKind placed = decor->cells[i];
			if (kind == SurfaceKind::Grass || kind == SurfaceKind::Dirt) ++landCells;
			if (placed == DecorKind::None) continue;
			require(kind == SurfaceKind::Grass || kind == SurfaceKind::Dirt, "Decoration on water, shore or rock");
			require(!(kind == SurfaceKind::Dirt && placed != DecorKind::Stone), "Non-stone decoration on dirt");
			if (placed == DecorKind::Tree) {
				++trees; ++seedTrees;
				// No tree within two 4-steps of water: Manhattan distance 2 around the cell.
				const int x = int(i % w), y = int(i / w);
				for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) {
					if (std::abs(dx) + std::abs(dy) > 2) continue;
					const int nx = x + dx, ny = y + dy;
					if (nx < 0 || ny < 0 || nx >= int(side) || ny >= int(side)) continue;
					const auto near = grid->cells[size_t(ny) * w + size_t(nx)];
					require(near != SurfaceKind::Lake && near != SurfaceKind::Shore && near != SurfaceKind::River, "Tree next to water");
				}
			}
			bushes += placed == DecorKind::Bush; stones += placed == DecorKind::Stone;
		}
		forestSeeds += seedTrees > 0;
		auto other = *decor; (void)other;
		const auto different = generateDecoration(*grid, seed + 1000);
		require(different && different->cells != decor->cells, "Decoration seed had no effect");
	}
	const double treeShare = double(trees) / double(landCells);
	require(forestSeeds == seeds, "A seed produced no trees");
	require(trees > 0 && bushes > 0 && stones > 0, "A decoration kind never appeared");
	require(treeShare >= 0.01 && treeShare <= 0.12, "Tree share of land outside 1-12%");
	std::cout << "decoration " << side << 'x' << side << " seeds=" << seeds << " tree_share_of_land=" << treeShare
		<< " bushes=" << bushes << " stones=" << stones << '\n';
}

void decorationFailures() {
	const auto grid = generateSurface(makeConfig(24, 3), extent, budget);
	require(bool(grid), "Surface baseline failed");
	SurfaceGrid broken = *grid; broken.cells.pop_back();
	const auto invalid = generateDecoration(broken, 1);
	require(!invalid, "Short grid accepted by decoration");
	std::stop_source source; source.request_stop();
	const auto stopped = generateDecoration(*grid, 1, source.get_token());
	require(!stopped && stopped.error() == SurfaceError::Cancelled, "Decoration ignored stop");
	allocationRecording = true; allocationCount = 0; failedAllocation = 0;
	const auto baseline = generateDecoration(*grid, 1);
	allocationRecording = false;
	require(bool(baseline), "Decoration baseline failed");
	const size_t total = allocationCount;
	size_t failures = 0;
	for (size_t fail = 1; fail <= total; ++fail) {
		allocationRecording = true; allocationCount = 0; failedAllocation = fail;
		const auto result = generateDecoration(*grid, 1);
		allocationRecording = false;
		if (!result) { require(result.error() == SurfaceError::AllocationFailed, "Unexpected decoration error"); ++failures; }
		else require(result->cells == baseline->cells, "Decoration changed after skipped failure");
	}
	require(failures > 0, "No decoration allocation failure reported");
	std::cout << "decoration allocation_points=" << total << " injected_failures_reported=" << failures << '\n';
}
} // namespace

// Optional paths (G4 iteration 9): off by default and bit-identical to the output without paths; when on, a path only
// replaces Grass, Dirt or Shore, so rock, lakes and rivers keep their cells, and the result is deterministic.
void paths(uint32_t side, uint64_t seeds) {
	for (uint64_t seed = 1; seed <= seeds; ++seed) {
		auto config = makeConfig(side, seed);
		const auto plain = generateSurface(config, extent, budget);
		require(plain.has_value(), "plain surface");
		require(countOf(*plain, SurfaceKind::Path) == 0, "paths appeared with pathLength 0");
		config.tuning.pathLength = 80;
		const auto first = generateSurface(config, extent, budget);
		const auto second = generateSurface(config, extent, budget);
		require(first.has_value() && second.has_value() && first->cells == second->cells, "paths are not deterministic");
		require(countOf(*first, SurfaceKind::Path) > 0, "pathLength 80 produced no path");
		for (size_t i = 0; i < first->cells.size(); ++i) {
			if (first->cells[i] == plain->cells[i]) continue;
			require(first->cells[i] == SurfaceKind::Path, "a path run changed a cell to something other than a path");
			const auto old = plain->cells[i];
			require(old == SurfaceKind::Grass || old == SurfaceKind::Dirt || old == SurfaceKind::Shore, "a path replaced rock, a lake or a river");
		}
		require(first->valid(), "grid with paths is not valid");
	}
}

// Grass rim (G4 iteration 10): no Dirt cell lies within two cells of Shore, and rock keeps some, but not all, dirt contact.
void grassRimRule(uint32_t side, uint64_t seeds) {
	size_t dirtBesideRock = 0;
	for (uint64_t seed = 1; seed <= seeds; ++seed) {
		const auto grid = generateSurface(makeConfig(side, seed), extent, budget);
		require(grid.has_value(), "surface for the rim test");
		for (uint32_t y = 0; y < side; ++y) {
			for (uint32_t x = 0; x < side; ++x) {
				if (grid->cells[size_t(y) * side + x] != SurfaceKind::Dirt) continue;
				for (int32_t dy = -2; dy <= 2; ++dy) {
					for (int32_t dx = -2; dx <= 2; ++dx) {
						const int32_t nx = int32_t(x) + dx, ny = int32_t(y) + dy;
						if (nx < 0 || ny < 0 || nx >= int32_t(side) || ny >= int32_t(side)) continue;
						require(grid->cells[size_t(ny) * side + size_t(nx)] != SurfaceKind::Shore, "dirt within two cells of the shore");
						dirtBesideRock += std::abs(dx) + std::abs(dy) == 1 && grid->cells[size_t(ny) * side + size_t(nx)] == SurfaceKind::Rock;
					}
				}
			}
		}
	}
	require(dirtBesideRock > 0, "the rim removed every contact between dirt and rock");
}

// Optional building foundations (G4 iterations 11-12): off by default; when on, only Grass and Dirt become Floor (door trails
// may also turn Grass, Dirt and Shore into Path), every foundation is a solid rectangle of 7-11 by 5-9 cells, and no two
// foundations or a foundation and another kind touch.
void buildings(uint32_t side, uint64_t seeds) {
	for (uint64_t seed = 1; seed <= seeds; ++seed) {
		auto config = makeConfig(side, seed);
		const auto plain = generateSurface(config, extent, budget);
		require(plain.has_value() && countOf(*plain, SurfaceKind::Floor) == 0, "floors appeared with buildingDensity 0");
		config.tuning.buildingDensity = 40;
		const auto first = generateSurface(config, extent, budget);
		const auto second = generateSurface(config, extent, budget);
		require(first.has_value() && second.has_value() && first->cells == second->cells, "buildings are not deterministic");
		require(first->valid(), "grid with buildings is not valid");
		for (size_t i = 0; i < first->cells.size(); ++i) {
			if (first->cells[i] == plain->cells[i]) continue;
			require(first->cells[i] == SurfaceKind::Floor || first->cells[i] == SurfaceKind::Path, "a building run changed a cell to something other than a floor or a trail");
			if (first->cells[i] == SurfaceKind::Floor) require(plain->cells[i] == SurfaceKind::Grass || plain->cells[i] == SurfaceKind::Dirt, "a foundation replaced water, rock, shore or a river");
			else require(plain->cells[i] == SurfaceKind::Grass || plain->cells[i] == SurfaceKind::Dirt || plain->cells[i] == SurfaceKind::Shore, "a door trail replaced water, rock or a river");
		}
		const auto shapes = floorBuildingsOf(*first);
		for (const Building& b : shapes.buildings) {
			require(b.cells == b.longSide * b.shortSide, "a foundation is not a solid rectangle");
			require(b.longSide >= 7 && b.longSide <= 11 && b.shortSide >= 5 && b.shortSide <= 9 || b.longSide >= 5 && b.longSide <= 9 && b.shortSide >= 5, "a foundation has an unexpected size");
		}
		// The wall outline the resolver derives (Floor cells beside another kind) is 2 * (long + short) - 4 per foundation.
		size_t outlineCells = 0, expectedOutline = 0;
		for (size_t i = 0; i < first->cells.size(); ++i) {
			if (first->cells[i] != SurfaceKind::Floor) continue;
			const size_t x = i % side, y = i / side;
			outlineCells += x == 0 || y == 0 || x + 1 == side || y + 1 == side || first->cells[i - 1] != SurfaceKind::Floor ||
				first->cells[i + 1] != SurfaceKind::Floor || first->cells[i - side] != SurfaceKind::Floor || first->cells[i + side] != SurfaceKind::Floor;
		}
		for (const Building& b : shapes.buildings) expectedOutline += 2 * (b.longSide + b.shortSide) - 4;
		require(outlineCells == expectedOutline, "the outline of the foundations is not 2 * (long + short) - 4 per building");
		if (side >= 250) require(!shapes.buildings.empty(), "buildingDensity 40 produced no foundation");
	}
}

// Settlements (G4 iteration 12): exactly one door per foundation, on an outline cell; the cell in front of it is Path;
// no doors without buildings; clusters keep a gap of at least 3 cells; doors survive only on cells that stay Floor.
void settlements(uint32_t side, uint64_t seeds) {
	for (uint64_t seed = 1; seed <= seeds; ++seed) {
		auto config = makeConfig(side, seed);
		const auto plain = generateSurface(config, extent, budget);
		require(plain.has_value() && plain->doors.empty(), "doors appeared with buildingDensity 0");
		config.tuning.buildingDensity = 40;
		config.tuning.pathLength = seed % 2 ? 40 : 0;
		const auto first = generateSurface(config, extent, budget);
		const auto second = generateSurface(config, extent, budget);
		require(first.has_value() && second.has_value() && first->doors == second->doors && first->cells == second->cells, "settlements are not deterministic");
		require(first->fingerprint() == second->fingerprint() && first->valid(), "settlement grid fingerprint or validity");
		const std::vector<BuildingBox> boxes = floorBoxesOf(*first);
		require(first->doors.size() == boxes.size(), "not exactly one door per foundation");
		size_t stoops = 0;  // doors with a Path cell in front; a building that nothing can join gets none
		for (const BuildingBox& box : boxes) {
			size_t inside = 0;
			for (const uint32_t door : first->doors) {
				const uint32_t x = door % side, y = door / side;
				if (x < box.x0 || x > box.x1 || y < box.y0 || y > box.y1) continue;
				++inside;
				const bool north = y == box.y0, south = y == box.y1, west = x == box.x0, east = x == box.x1;
				require(north || south || west || east, "a door is not on the outline");
				const size_t stoop = north ? door - side : south ? door + side : west ? door - 1 : door + 1;
				stoops += first->cells[stoop] == SurfaceKind::Path;
			}
			require(inside == 1, "a foundation has no door or more than one");
		}
		if (side >= 250) require(stoops > 0 && stoops <= boxes.size(), "no door got a trail in front of it");
		// Gap between two foundations: at least 3 cells (margin of two plus the cell between).
		for (size_t a = 0; a < boxes.size(); ++a) {
			for (size_t b = a + 1; b < boxes.size(); ++b) {
				const int64_t dx = std::max<int64_t>({0, int64_t(boxes[a].x0) - boxes[b].x1, int64_t(boxes[b].x0) - boxes[a].x1});
				const int64_t dy = std::max<int64_t>({0, int64_t(boxes[a].y0) - boxes[b].y1, int64_t(boxes[b].y0) - boxes[a].y1});
				require(std::max(dx, dy) >= 3, "two foundations are closer than 3 cells");
			}
		}
		require(first->fingerprint() != plain->fingerprint(), "buildings did not change the fingerprint");
		// A mask that removes a foundation also removes its door.
		auto masked = config;
		std::vector<uint8_t> mask(size_t(side) * side, 1);
		for (size_t i = 0; i < mask.size() / 2; ++i) mask[i] = 0;
		masked.region.x = config.region.x;
		const auto cut = generateSurface(masked, extent, budget, mask);
		if (cut) {
			require(cut->valid(), "masked settlement grid is not valid");
			for (const uint32_t door : cut->doors) require(cut->cells[door] == SurfaceKind::Floor, "a door stayed on a cell that is not a floor");
		}
	}
}

// G4 iteration 16: trails that cross rock bands. Off by default; on, the result is deterministic and differs.
void rockPasses() {
	auto config = makeConfig(160, 2024);
	config.tuning.pathLength = 80;
	const auto off = generateSurface(config, extent, budget);
	config.tuning.rockPasses = 1;
	const auto on = generateSurface(config, extent, budget);
	const auto again = generateSurface(config, extent, budget);
	require(off.has_value() && on.has_value() && again.has_value(), "rockPasses: generation");
	require(on->fingerprint() == again->fingerprint(), "rockPasses: not deterministic");
	require(on->fingerprint() != off->fingerprint(), "rockPasses: no change in the result");
	config.tuning.rockPasses = 2;
	require(!validateSurface(config, extent, budget).has_value(), "rockPasses above 1 was accepted");
}

// G4 iteration 23: seed rejection. A seed whose first attempt fails is found by search; the result is deterministic.
void reachableSurface() {
	size_t rejected = 0;
	for (uint64_t seed = 1; seed <= 40 && !rejected; ++seed) {
		auto config = makeConfig(100, seed);
		config.tuning.buildingDensity = 40;
		config.tuning.lakePermille = 300;
		config.tuning.riverCount = 8;
		const auto result = generateReachableSurface(config, extent, budget, 8);
		require(result.has_value(), "reachable surface: generation");
		if (result->attempts == 1) { require(result->allReached && result->seed == seed, "reachable surface: first attempt"); continue; }
		rejected = size_t(seed);
		require(result->seed == seed + result->attempts - 1 || !result->allReached, "reachable surface: seed arithmetic");
		const auto again = generateReachableSurface(config, extent, budget, 8);
		require(again.has_value() && again->seed == result->seed && again->attempts == result->attempts && again->grid.cells == result->grid.cells, "reachable surface: not deterministic");
		const auto one = generateReachableSurface(config, extent, budget, 1);
		const auto plain = generateSurface(config, extent, budget);
		require(one.has_value() && plain.has_value() && one->seed == seed && one->attempts == 1 && !one->allReached && one->grid.cells == plain->cells, "reachable surface: one attempt returns the first surface unreached");
		if (result->allReached) {
			const auto poi = poiReachabilityOf(result->grid, true);
			require(poi.has_value() && poi->entrances == poi->entrancesReached && poi->exits == poi->exitsReached, "reachable surface: reported as reached but is not");
		}
	}
	require(rejected != 0, "reachable surface: no seed needed a second attempt in 40 (test setup)");
	require(!generateReachableSurface(makeConfig(100, 1), extent, budget, 0).has_value(), "reachable surface: zero attempts accepted");
}

// G5 iteration 25: ruins and camps. Off by default; on, the grid is valid, deterministic and every site is reached after repair.
void ruinsAndCamps() {
	for (uint64_t seed = 1; seed <= 4; ++seed) {
		auto config = makeConfig(250, seed);
		config.tuning.pathLength = 40;
		config.tuning.buildingDensity = 40;
		const auto plain = generateSurface(config, extent, budget);
		require(plain.has_value() && plain->gaps.empty() && plain->sites.empty(), "ruins: gaps or sites appeared with the options off");
		config.tuning.ruinPercent = 40;
		config.tuning.campCount = 8;
		config.tuning.repairReach = 1;
		const auto first = generateSurface(config, extent, budget);
		const auto second = generateSurface(config, extent, budget);
		require(first.has_value() && second.has_value() && first->cells == second->cells && first->gaps == second->gaps && first->sites == second->sites, "ruins: not deterministic");
		require(first->valid() && first->fingerprint() != plain->fingerprint(), "ruins: invalid grid or no change");
		require(!first->sites.empty() && !first->gaps.empty(), "ruins: no site or gap produced");
		const auto boxes = floorBoxesOf(*first);
		require(first->doors.size() + first->sites.size() >= boxes.size() - 0 && first->doors.size() < boxes.size(), "ruins: doors and sites do not cover the foundations");
		const auto poi = poiReachabilityOf(*first, true);
		require(poi.has_value() && poi->sites == first->sites.size() && poi->sitesReached == poi->sites, "ruins: a site is not reached after repair");
	}
	auto camps = makeConfig(250, 1);
	camps.tuning.campCount = 6;
	const auto onlyCamps = generateSurface(camps, extent, budget);
	require(onlyCamps.has_value() && onlyCamps->doors.empty() && !onlyCamps->sites.empty() && onlyCamps->gaps.size() == 8 * onlyCamps->sites.size(), "camps: each camp has one site and eight gaps");
	require(onlyCamps->fires.size() == onlyCamps->sites.size(), "camps: one fire per camp");
	// Geometry of one camp, counted by hand: the clearing is the 61 cells with dx^2 + dy^2 <= 18 around the fire; the tent is the 3 x 3 Floor
	// centred 3 cells east, all of it inside the clearing (its farthest cells are 4 east and 1 north or south: 16 + 1 = 17 <= 18); so the clearing
	// keeps 61 - 9 = 52 Path cells, the fire is one of them, the tent has 8 outline gaps and its centre is the site.
	bool checkedOne = false;
	for (uint64_t seed = 1; seed <= 80 && !checkedOne; ++seed) {
		auto one = makeConfig(37, seed);
		one.tuning.campCount = 50;
		const auto single = generateSurface(one, extent, budget);
		require(single.has_value(), "camp geometry: generation");
		if (single->sites.size() != 1) continue;
		checkedOne = true;
		const int32_t fire = int32_t(single->fires[0]), fx = fire % 37, fy = fire / 37;
		require(single->cells[size_t(fire)] == SurfaceKind::Path, "camp geometry: the fire is on a Path cell");
		require(single->sites[0] == uint32_t(fy * 37 + fx + 3), "camp geometry: the site is the tent centre 3 east of the fire");
		require(single->gaps.size() == 8, "camp geometry: eight gaps");
		size_t floor = 0, pathInDisc = 0;
		for (int32_t dy = -4; dy <= 4; ++dy) {
			for (int32_t dx = -4; dx <= 4; ++dx) {
				if (dx * dx + dy * dy > 18) continue;
				const SurfaceKind kind = single->cells[size_t((fy + dy) * 37 + fx + dx)];
				floor += kind == SurfaceKind::Floor;
				pathInDisc += kind == SurfaceKind::Path;
			}
		}
		require(floor == 9 && pathInDisc == 52, "camp geometry: 9 tent cells and 52 Path cells in the clearing");
	}
	require(checkedOne, "camp geometry: no seed placed exactly one camp");
	camps.tuning.campCount = 51;
	require(!validateSurface(camps, extent, budget).has_value(), "camps above 50 accepted");
}

void sandIslands() {
	SurfaceGrid grid {{0, 0, 7, 16, 12}, std::vector<SurfaceKind>(192, SurfaceKind::Lake)};
	for (uint32_t y = 0; y < 12; ++y) for (uint32_t x = 0; x < 4; ++x) grid.cells[y * 16 + x] = SurfaceKind::Grass;
	for (uint32_t y = 4; y < 7; ++y) for (uint32_t x = 9; x < 12; ++x) grid.cells[y * 16 + x] = SurfaceKind::Dirt;
	grid.cells[5 * 16 + 9] = SurfaceKind::Rock;
	grid.cells[5 * 16 + 10] = SurfaceKind::Path;
	const auto original = grid;
	const auto islands = sandIslandsOf(grid);
	require(islands && islands->islands == 1 && islands->nonShoreCells == 9, "island: hand-counted classification");
	require(bool(normalizeSandIslands(grid)), "island: normalize");
	for (const auto i : islands->cells) require(grid.cells[i] == SurfaceKind::Shore, "island: not pure sand");
	const auto decor = generateDecoration(grid, 1729);
	require(bool(decor), "island: decoration");
	for (const auto i : islands->cells) require(decor->cells[i] == DecorKind::None, "island: decoration on sand");
	for (const auto connector : {SurfaceKind::Shore, SurfaceKind::Bridge, SurfaceKind::Path}) {
		auto connected = original;
		for (uint32_t x = 4; x < 9; ++x) connected.cells[5 * 16 + x] = connector;
		const auto before = connected.cells;
		require(bool(normalizeSandIslands(connected)) && before == connected.cells, "island: connected land changed");
	}
	for (int poi = 0; poi < 4; ++poi) {
		auto protectedIsland = original;
		auto* points = poi == 0 ? &protectedIsland.doors : poi == 1 ? &protectedIsland.sites : poi == 2 ? &protectedIsland.fires : &protectedIsland.entrances;
		points->push_back(5 * 16 + 10);
		protectedIsland.cells[5 * 16 + 10] = poi < 2 ? SurfaceKind::Floor : poi == 2 ? SurfaceKind::Path : SurfaceKind::Rock;
		const auto before = protectedIsland.cells;
		require(bool(normalizeSandIslands(protectedIsland)) && protectedIsland.cells == before, "island: POI changed");
	}
	auto preserved = original;
	preserved.cells[5 * 16 + 8] = SurfaceKind::Preserve;
	const auto beforePreserve = preserved.cells;
	require(bool(normalizeSandIslands(preserved)) && preserved.cells == beforePreserve, "island: Preserve boundary changed");
	auto otherFloor = original; otherFloor.region.z = 8;
	require(bool(normalizeSandIslands(otherFloor)) && otherFloor.cells == original.cells, "island: other floor changed");
	SurfaceGrid enclosedMain {{0, 0, 7, 10, 5}, std::vector<SurfaceKind>(50, SurfaceKind::River)};
	enclosedMain.cells[11] = SurfaceKind::Dirt;
	enclosedMain.cells[16] = enclosedMain.cells[17] = SurfaceKind::Grass;
	require(bool(normalizeSandIslands(enclosedMain)) && enclosedMain.cells[11] == SurfaceKind::Shore &&
		enclosedMain.cells[16] == SurfaceKind::Grass, "island: largest enclosed component changed");
	auto edge = original;
	for (uint32_t y = 0; y < 4; ++y) edge.cells[y * 16 + 9] = SurfaceKind::Dirt;
	const auto beforeEdge = edge.cells;
	require(bool(normalizeSandIslands(edge)) && edge.cells == beforeEdge, "island: rectangle boundary changed");
	auto orphanBridge = original;
	orphanBridge.cells[5 * 16 + 10] = SurfaceKind::Bridge;
	orphanBridge.bridgeAxes.resize(192);
	orphanBridge.bridgeAxes[5 * 16 + 10] = BridgeAxis::Horizontal;
	require(bool(normalizeSandIslands(orphanBridge)) && orphanBridge.valid() &&
		orphanBridge.bridgeAxes[5 * 16 + 10] == BridgeAxis::None, "island: orphan bridge axis survived");
	std::stop_source stopped; stopped.request_stop();
	const auto cancelled = normalizeSandIslands(grid, stopped.get_token());
	require(!cancelled && cancelled.error() == SurfaceError::Cancelled, "island: ignored cancellation");

	// Independent DFS labels and boundary checks on 120 generated plans. It does
	// not use the production classifier to select the cells under assertion.
	size_t checkedIslands = 0;
	for (uint64_t seed = 1; seed <= 120; ++seed) {
		auto config = makeConfig(100, seed);
		config.tuning.pathLength = 40; config.tuning.fordLength = 12;
		config.tuning.buildingDensity = 30; config.tuning.campCount = 4; config.tuning.caveEntrances = 16;
		const auto generated = generateSurface(config, extent, budget);
		require(bool(generated), "island: random generation");
		std::vector<int> labels(10000, -1);
		std::vector<std::vector<size_t>> bodies;
		std::vector<bool> enclosed, hasPoi;
		for (size_t first = 0; first < labels.size(); ++first) {
			if (labels[first] != -1 || generated->cells[first] == SurfaceKind::Lake || generated->cells[first] == SurfaceKind::River) continue;
			const int label = int(bodies.size());
			std::vector<size_t> pending {first}, body;
			labels[first] = label;
			bool inside = true;
			while (!pending.empty()) {
				const auto i = pending.back(); pending.pop_back(); body.push_back(i);
				const int x = int(i % 100), y = int(i / 100);
				inside &= x > 0 && y > 0 && x < 99 && y < 99;
				for (const auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
					if (x + dx < 0 || y + dy < 0 || x + dx >= 100 || y + dy >= 100) continue;
					const auto n = size_t(y + dy) * 100 + size_t(x + dx);
					if (labels[n] != -1 || generated->cells[n] == SurfaceKind::Lake || generated->cells[n] == SurfaceKind::River) continue;
					labels[n] = label; pending.push_back(n);
				}
			}
			bodies.push_back(std::move(body)); enclosed.push_back(inside); hasPoi.push_back(false);
		}
		for (const auto* points : {&generated->doors, &generated->sites, &generated->fires, &generated->entrances})
			for (const auto i : *points) hasPoi[size_t(labels[i])] = true;
		size_t main = 0;
		for (size_t b = 1; b < bodies.size(); ++b) if (bodies[b].size() > bodies[main].size()) main = b;
		for (size_t b = 0; b < bodies.size(); ++b) if (b != main && enclosed[b] && !hasPoi[b]) {
			++checkedIslands;
			for (const auto i : bodies[b]) require(generated->cells[i] == SurfaceKind::Shore, "island: random non-sand cell");
		}
		require(generateSurface(config, extent, budget)->cells == generated->cells, "island: determinism");
	}
	require(checkedIslands > 0, "island: randomized check was vacuous");
	std::cout << "SAND_ISLAND_CROSS_CHECK 120/120 islands=" << checkedIslands << '\n';
}

int main() {
	try {
		emptyRiverRoutes(); sandIslands(); parametersAndLimits(); deterministicAndMaskIndependent(); transitionBand();
		meanderingRivers(100, 24); meanderingRivers(250, 4); distributions(100, 24); distributions(250, 6); distributions(500, 3);
		allocationFailures(makeConfig(24, 3), false);
		auto structures = makeConfig(100, 1);
		structures.tuning.buildingDensity = 40;
		structures.tuning.pathLength = 40;
		structures.tuning.caveEntrances = 20;
		allocationFailures(structures, true);
		caveAllocationFailures();
		cavePruningAllocationFailures();
		measure(100); measure(500);
		decoration(100, 24); decoration(250, 6); decorationFailures(); paths(100, 8); paths(250, 3); grassRimRule(100, 24); buildings(100, 6); buildings(250, 3); settlements(100, 6); settlements(250, 3); rockPasses(); reachableSurface(); ruinsAndCamps();
		std::cout << "PROCEDURAL SURFACE PASS (no editor, resolver, commit or GUI)\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "PROCEDURAL SURFACE FAIL: " << error.what() << '\n';
		return 1;
	}
}
