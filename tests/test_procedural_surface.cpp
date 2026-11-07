// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/surface.h"

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
	bad = base; bad.tuning.shoreWidth = 5;
	expectError(bad, budget, SurfaceError::InvalidParameters, "shoreWidth 5 accepted");
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
	require(first->fingerprint() == 15556827831682428334ULL, "Seed 1729 100x100 surface output changed");
}

struct Tally { size_t cells = 0, grass = 0, dirt = 0, rock = 0, shore = 0, lake = 0, river = 0; };

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
	// With shoreWidth 1, land next to a lake is shore unless it is rock or river.
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
	return tally;
}

void distributions(uint32_t side, uint32_t seeds) {
	size_t riverSeeds = 0, lakeSeeds = 0, rockSeeds = 0, dirtSeeds = 0;
	double minDirt = 1, maxDirt = 0;
	for (uint32_t seed = 1; seed <= seeds; ++seed) {
		const auto config = makeConfig(side, seed);
		const auto grid = generateSurface(config, extent, budget);
		require(grid && grid->valid(), "Surface generation failed");
		const auto tally = structure(config, *grid);
		require(tally.rock <= tally.cells * (config.tuning.rockPermille + 20) / 1000, "Rock share far above its cap");
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
		<< " with_rock=" << rockSeeds << " dirt_share_of_open_land=" << minDirt << ".." << maxDirt << '\n';
	require(lakeSeeds > 0 && riverSeeds > 0 && rockSeeds > 0, "A feature never appeared in any seed");
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

void allocationFailures() {
	const auto config = makeConfig(24, 3);
	allocationRecording = true; allocationCount = 0; failedAllocation = 0;
	const auto baseline = generateSurface(config, extent, budget);
	allocationRecording = false;
	require(bool(baseline), "Baseline failed");
	const size_t total = allocationCount;
	size_t failures = 0;
	for (size_t fail = 1; fail <= total; ++fail) {
		allocationRecording = true; allocationCount = 0; failedAllocation = fail;
		const auto result = generateSurface(config, extent, budget);
		allocationRecording = false;
		if (!result) { require(result.error() == SurfaceError::AllocationFailed, "Unexpected error on bad_alloc"); ++failures; }
		else require(result->cells == baseline->cells, "Result changed after skipped failure");
	}
	std::cout << "allocation_points=" << total << " injected_failures_reported=" << failures << '\n';
	require(failures > 0, "No allocation failure was reported");
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
} // namespace

int main() {
	try {
		parametersAndLimits(); deterministicAndMaskIndependent(); transitionBand();
		distributions(100, 24); distributions(250, 6); allocationFailures();
		measure(100); measure(500);
		std::cout << "PROCEDURAL SURFACE PASS (no editor, resolver, commit or GUI)\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "PROCEDURAL SURFACE FAIL: " << error.what() << '\n';
		return 1;
	}
}
