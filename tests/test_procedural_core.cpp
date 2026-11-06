// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/generation.h"

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
thread_local bool allocationFailureObserved = false;
thread_local size_t allocationCount = 0;
thread_local size_t failedAllocation = 0;
}

// Test executable only: no allocator hook is linked into RME or the core library.
void* operator new(std::size_t size) {
	if (allocationRecording && ++allocationCount == failedAllocation) {
		allocationFailureObserved = true;
		throw std::bad_alloc();
	}
	if (void* memory = std::malloc(size ? size : 1)) return memory;
	throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
constexpr MapExtent extent { 65001, 65001, 16 };
constexpr CoreBudget budget { 250000, 4 * 1024 * 1024 };

void require(bool value, std::string_view message) {
	if (!value) throw std::runtime_error(std::string(message));
}

void randomStreams() {
	StableRandom a(1729, 1), same(1729, 1), other(1729, 2);
	bool differs = false;
	for (int i = 0; i < 256; ++i) {
		const auto value = a.next();
		require(value == same.next(), "RNG stream did not repeat");
		differs |= value != other.next();
	}
	require(differs, "RNG streams not separated");
	StableRandom independent(1729, 1);
	StableRandom expected(1729, 1);
	for (int i = 0; i < 200; ++i) other.next();
	require(independent.next() == expected.next(), "Other stream advanced terrain stream");
	for (uint64_t bound : { uint64_t{1}, uint64_t{3}, uint64_t{10}, uint64_t{0x8000000000000001ULL}, UINT64_MAX }) {
		for (int i = 0; i < 256; ++i) require(a.bounded(bound) < bound, "Bounded RNG escaped its range");
	}
	bool rejected = false;
	try { a.bounded(0); } catch (const std::invalid_argument&) { rejected = true; }
	require(rejected, "Zero RNG bound accepted");
}

void allocationFailures() {
	const GenerationConfig config {{100, 100, 7, 4, 3}, 1729};
	const std::array<uint8_t, 12> mask {1,1,0,1, 1,1,1,0, 1,0,1,1};
	allocationCount = 0;
	failedAllocation = 0;
	allocationRecording = true;
	const auto baseline = generateTerrain(config, extent, budget, mask);
	allocationRecording = false;
	const auto count = allocationCount;
	require(baseline && count > 0, "No core allocations observed");
	for (size_t ordinal = 1; ordinal <= count; ++ordinal) {
		allocationCount = 0;
		failedAllocation = ordinal;
		allocationFailureObserved = false;
		allocationRecording = true;
		const auto result = generateTerrain(config, extent, budget, mask);
		allocationRecording = false;
		require(allocationFailureObserved && !result && result.error() == GenerationError::AllocationFailed,
			"Core allocation failure returned partial output or wrong error");
		const auto retry = generateTerrain(config, extent, budget, mask);
		require(retry && retry->cells == baseline->cells && retry->region == baseline->region,
			"Core allocation failure affected retry determinism");
	}
	std::cout << "core_allocation_failure_cases=" << count << '\n';
}

void invalidInputs() {
	GenerationConfig base { {100, 100, 7, 100, 100}, 1729 };
	MapExtent defaultExtent;
	CoreBudget defaultBudget;
	GenerationLayout defaultLayout;
	require(defaultExtent.width == 0 && defaultExtent.height == 0 && defaultExtent.layers == 0 &&
		defaultBudget.maxCells == 0 && defaultBudget.maxOutputBytes == 0 &&
		defaultLayout.cellCount == 0 && defaultLayout.outputBytes == 0, "Default parameters not zero initialized");
	const auto noBounds = validateGeneration(base, defaultExtent, budget);
	require(!noBounds && noBounds.error() == GenerationError::InvalidRegion, "Default bounds did not fail closed");
	const auto noBudget = validateGeneration(base, extent, defaultBudget);
	require(!noBudget && noBudget.error() == GenerationError::CellLimit, "Default budget did not fail closed");
	const auto rejects = [&](GenerationConfig config, GenerationError error) {
		const auto result = generateTerrain(config, extent, budget);
		require(!result && result.error() == error, "Wrong configuration rejection");
	};
	for (int axis = 0; axis < 3; ++axis) {
		auto config = base;
		if (axis == 0) config.region.x = -1;
		if (axis == 1) config.region.y = -1;
		if (axis == 2) config.region.z = -1;
		rejects(config, GenerationError::InvalidRegion);
	}
	auto config = base; config.region.width = 0;
	rejects(config, GenerationError::InvalidRegion);
	config = base; config.region.height = 0;
	rejects(config, GenerationError::InvalidRegion);
	config = base; config.region.z = 16;
	rejects(config, GenerationError::InvalidRegion);
	config = base; config.region.x = 65000;
	rejects(config, GenerationError::InvalidRegion);
	config = base; config.region.height = UINT32_MAX;
	rejects(config, GenerationError::InvalidRegion);
	config = base; config.grassWeight = config.dirtWeight = config.waterWeight = 0;
	rejects(config, GenerationError::InvalidWeights);
	config = base; config.region.width = config.region.height = 501;
	rejects(config, GenerationError::CellLimit);
	const auto layout = validateGeneration(base, extent, budget);
	require(bool(layout), "Valid layout rejected");
	require(bool(validateGeneration(base, extent, {10000, layout->outputBytes})), "Exact budget rejected");
	const auto shortBudget = generateTerrain(base, extent, {10000, layout->outputBytes - 1});
	require(!shortBudget && shortBudget.error() == GenerationError::OutputLimit, "Output budget not enforced");
	config = { {0, 0, 0, UINT32_MAX, UINT32_MAX}, 0 };
	const auto overflow = validateGeneration(config, {UINT32_MAX, UINT32_MAX, 1}, {UINT64_MAX, UINT64_MAX});
	require(!overflow && overflow.error() == GenerationError::SizeOverflow, "Unrepresentable vector accepted");
	config = { {65000, 65000, 15, 1, 1}, 0, 1, 0, 0 };
	require(bool(generateTerrain(config, extent, budget)), "Last valid map position rejected");
	std::stop_source cancel;
	cancel.request_stop();
	const auto validationStopped = validateGeneration(base, extent, budget, {}, {}, cancel.get_token());
	require(!validationStopped && validationStopped.error() == GenerationError::Cancelled, "Cancelled validation accepted");
	const auto stopped = generateTerrain(base, extent, budget, {}, {}, cancel.get_token());
	require(!stopped && stopped.error() == GenerationError::Cancelled, "Pre-cancelled job produced output");
}

void masksAndWeights() {
	GenerationConfig config { {100, 100, 7, 4, 3}, 1729 };
	std::array<uint8_t, 12> mask {1,1,0,0, 1,0,1,0, 1,1,1,1};
	std::array<uint8_t, 12> protection {0,1,0,0, 0,0,0,0, 0,0,1,0};
	const auto originalMask = mask, originalProtection = protection;
	const auto all = generateTerrain(config, extent, budget);
	const auto selected = generateTerrain(config, extent, budget, mask, protection);
	require(all && selected, "Irregular mask rejected");
	for (size_t i = 0; i < mask.size(); ++i) {
		require(selected->cells[i] == ((mask[i] && !protection[i]) ? all->cells[i] : Terrain::Preserve),
			"Mask/protection changed another cell's RNG or wrote excluded cell");
	}
	require(mask == originalMask && protection == originalProtection, "Borrowed input mutated");
	const auto shortMask = generateTerrain(config, extent, budget, std::span(mask).first(11));
	require(!shortMask && shortMask.error() == GenerationError::InvalidMask, "Short mask accepted");
	mask[0] = 2;
	const auto invalidMask = generateTerrain(config, extent, budget, mask);
	require(!invalidMask && invalidMask.error() == GenerationError::InvalidMask, "Nonbinary mask accepted");
	mask.fill(0);
	const auto empty = generateTerrain(config, extent, budget, mask);
	require(!empty && empty.error() == GenerationError::NoWritableCells, "Empty selection accepted");
	protection.fill(1);
	const auto protectedArea = generateTerrain(config, extent, budget, {}, protection);
	require(!protectedArea && protectedArea.error() == GenerationError::NoWritableCells, "Entirely protected area accepted");
	protection[0] = 2;
	const auto invalidProtection = generateTerrain(config, extent, budget, {}, protection);
	require(!invalidProtection && invalidProtection.error() == GenerationError::InvalidMask, "Nonbinary protection accepted");
	for (const auto terrain : {Terrain::Grass, Terrain::Dirt, Terrain::Water}) {
		config.grassWeight = terrain == Terrain::Grass ? UINT32_MAX : 0;
		config.dirtWeight = terrain == Terrain::Dirt ? UINT32_MAX : 0;
		config.waterWeight = terrain == Terrain::Water ? UINT32_MAX : 0;
		const auto pure = generateTerrain(config, extent, budget);
		require(pure && std::ranges::all_of(pure->cells, [=](Terrain cell) { return cell == terrain; }),
			"Zero-weight terrain selected");
	}
	config.grassWeight = config.dirtWeight = config.waterWeight = UINT32_MAX;
	require(bool(generateTerrain(config, extent, budget)), "Wide weight sum overflowed");
	SemanticGrid first {{1,2,3,1,1}, {Terrain::Grass}};
	require(first.valid(), "Valid grid rejected");
	SemanticGrid invalid;
	require(!invalid.valid(), "Empty grid accepted");
	invalid = first; invalid.cells.clear();
	require(!invalid.valid(), "Short grid accepted");
	invalid = first; invalid.cells.push_back(Terrain::Water);
	require(!invalid.valid(), "Long grid accepted");
	invalid = first; invalid.region.x = -1;
	require(!invalid.valid(), "Negative grid region accepted");
	invalid = first; invalid.cells[0] = static_cast<Terrain>(255);
	require(!invalid.valid(), "Unknown semantic terrain accepted");
	require(first.fingerprint() == 13378096426848191951ULL, "Fingerprint wire layout changed");
	auto moved = first;
	++moved.region.z;
	require(first.fingerprint() != moved.fingerprint(), "Fingerprint omitted Z");
	moved = first; moved.cells[0] = Terrain::Dirt;
	require(first.fingerprint() != moved.fingerprint(), "Fingerprint omitted terrain");
}

void repeatAndMeasure(uint32_t side) {
	GenerationConfig config { {100, 100, 7, side, side}, 1729 };
	const auto started = std::chrono::steady_clock::now();
	const auto first = generateTerrain(config, extent, budget);
	const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
	const auto again = generateTerrain(config, extent, budget);
	require(first && again && first->cells == again->cells && first->region == again->region,
		"Seed/config did not reproduce exact semantic output");
	require(first->fingerprint() == again->fingerprint(), "Fingerprint did not repeat");
	++config.seed;
	const auto other = generateTerrain(config, extent, budget);
	require(other && first->cells != other->cells, "Different seed did not affect terrain");
	for (const auto terrain : {Terrain::Grass, Terrain::Dirt, Terrain::Water}) {
		require(std::ranges::find(first->cells, terrain) != first->cells.end(), "Weighted terrain missing");
	}
	const auto layout = validateGeneration(config, extent, budget);
	std::cout << side << 'x' << side << " cells=" << first->cells.size()
		<< " core_output_bytes=" << layout->outputBytes << " generation_ms=" << elapsed
		<< " fingerprint=" << first->fingerprint() << '\n';
}
} // namespace

int main() {
	try {
		randomStreams(); invalidInputs(); masksAndWeights(); allocationFailures();
		repeatAndMeasure(100); repeatAndMeasure(500);
		std::cout << "PROCEDURAL CORE PASS (no editor, resolver, commit or GUI)\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "PROCEDURAL CORE FAIL: " << error.what() << '\n';
		return 1;
	}
}
