// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/surface.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <new>
#include <queue>
#include <utility>

namespace rme::procedural {

namespace {

constexpr uint64_t heightStream = 0x6865696768745f32ULL;   // "height_2"
constexpr uint64_t moistureStream = 0x6d6f69737475725fULL; // "moistur_"
constexpr uint64_t ditherStream = 0x6469746865725f32ULL;   // "dither_2"
constexpr uint64_t riverStream = 0x72697665725f7332ULL;    // "river_s2"
constexpr uint64_t workingBytesPerCell = 40;               // estimate: fields, flood heap, queues
constexpr uint32_t noiseBins = 65536;
constexpr int64_t fixedOne = 4096;
constexpr uint32_t transitionStep = 10;  // lattice step of the grass/dirt edge wobble
constexpr uint32_t minPatchCells = 24;   // smaller grass or dirt patches are absorbed

uint64_t mix(uint64_t value) {
	value += 0x9e3779b97f4a7c15ULL;
	value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
	value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
	return value ^ (value >> 31);
}

uint16_t lattice(uint64_t seed, uint64_t stream, uint32_t x, uint32_t y) {
	return uint16_t(mix(mix(mix(seed ^ stream) + x) + y) >> 48);
}

int64_t smooth(int64_t t) { return t * t * (3 * fixedOne - 2 * t) / (fixedOne * fixedOne); }

int64_t lerp(int64_t a, int64_t b, int64_t t) { return a + (b - a) * t / fixedOne; }

uint32_t valueNoise(uint64_t seed, uint64_t stream, uint32_t x, uint32_t y, uint32_t step) {
	const uint32_t ix = x / step, iy = y / step;
	const int64_t tx = smooth(int64_t(x % step) * fixedOne / step);
	const int64_t ty = smooth(int64_t(y % step) * fixedOne / step);
	const int64_t top = lerp(lattice(seed, stream, ix, iy), lattice(seed, stream, ix + 1, iy), tx);
	const int64_t bottom = lerp(lattice(seed, stream, ix, iy + 1), lattice(seed, stream, ix + 1, iy + 1), tx);
	return uint32_t(lerp(top, bottom, ty));
}

// Three octaves weighted 4:2:1, each at half the lattice step of the previous one.
uint16_t fractalNoise(uint64_t seed, uint64_t stream, uint32_t x, uint32_t y, uint32_t step) {
	uint32_t sum = 0;
	for (uint32_t octave = 0; octave < 3; ++octave) {
		const uint32_t octaveStep = std::max<uint32_t>(1, step >> octave);
		sum += valueNoise(seed, stream + octave * 0x1000193ULL, x, y, octaveStep) << (2 - octave);
	}
	return uint16_t(sum / 7);
}

// Smallest bin whose cumulative count exceeds permille of the cells; fewer than or
// exactly permille of the cells lie strictly below the returned value.
uint32_t lowCut(const std::vector<uint16_t>& values, uint32_t permille) {
	const uint64_t target = uint64_t(values.size()) * permille / 1000;
	std::vector<uint32_t> bins(noiseBins, 0);
	for (const auto value : values) ++bins[value];
	uint64_t cumulative = 0;
	for (uint32_t bin = 0; bin < noiseBins; ++bin) {
		cumulative += bins[bin];
		if (cumulative > target) return bin;
	}
	return noiseBins;
}

SurfaceError fromGeneration(GenerationError error) {
	switch (error) {
		case GenerationError::InvalidRegion: return SurfaceError::InvalidRegion;
		case GenerationError::InvalidMask: return SurfaceError::InvalidMask;
		case GenerationError::NoWritableCells: return SurfaceError::NoWritableCells;
		case GenerationError::CellLimit: return SurfaceError::CellLimit;
		case GenerationError::OutputLimit: return SurfaceError::OutputLimit;
		case GenerationError::SizeOverflow: return SurfaceError::SizeOverflow;
		case GenerationError::Cancelled: return SurfaceError::Cancelled;
		case GenerationError::AllocationFailed: return SurfaceError::AllocationFailed;
		case GenerationError::InvalidWeights: break;
	}
	return SurfaceError::InvalidParameters;
}

// Calls visit(neighbour) for the 4-neighbours of cell inside a width x height grid.
template <typename Visit>
void forNeighbours(size_t cell, uint32_t width, uint32_t height, Visit&& visit) {
	const uint32_t x = uint32_t(cell % width), y = uint32_t(cell / width);
	if (x > 0) visit(cell - 1);
	if (x + 1 < width) visit(cell + 1);
	if (y > 0) visit(cell - width);
	if (y + 1 < height) visit(cell + width);
}

// Clears 4-connected components of flag smaller than minCells. False when cancelled.
bool dropSmall(std::vector<uint8_t>& flag, uint32_t width, uint32_t height, uint32_t minCells,
	const std::stop_token& stop) {
	if (minCells <= 1) return true;
	std::vector<uint8_t> seen(flag.size(), 0);
	std::vector<uint32_t> stack, component;
	for (size_t start = 0; start < flag.size(); ++start) {
		if (start % 4096 == 0 && stop.stop_requested()) return false;
		if (!flag[start] || seen[start]) continue;
		component.clear();
		stack.assign(1, uint32_t(start));
		seen[start] = 1;
		while (!stack.empty()) {
			const uint32_t cell = stack.back();
			stack.pop_back();
			component.push_back(cell);
			forNeighbours(cell, width, height, [&](size_t next) {
				if (flag[next] && !seen[next]) { seen[next] = 1; stack.push_back(uint32_t(next)); }
			});
		}
		if (component.size() < minCells) for (const auto cell : component) flag[cell] = 0;
	}
	return true;
}

} // namespace

uint32_t surfaceValueNoise(uint64_t seed, uint64_t stream, uint32_t x, uint32_t y, uint32_t step) {
	return valueNoise(seed, stream, x, y, std::max<uint32_t>(1, step));
}

uint16_t surfaceLattice(uint64_t seed, uint64_t stream, uint32_t x, uint32_t y) {
	return lattice(seed, stream, x, y);
}

bool SurfaceGrid::valid() const noexcept {
	if (region.x < 0 || region.y < 0 || region.z < 0 || region.width == 0 || region.height == 0 ||
		uint64_t(region.width) * region.height != cells.size()) return false;
	return std::ranges::all_of(cells, [](SurfaceKind kind) { return kind <= SurfaceKind::River; });
}

uint64_t SurfaceGrid::fingerprint() const {
	// FNV-1a, same wire rules as SemanticGrid, but a separate version and domain tag.
	uint64_t hash = 14695981039346656037ULL;
	const auto byte = [&](uint8_t value) { hash = (hash ^ value) * 1099511628211ULL; };
	const auto word = [&](uint32_t value) {
		for (int shift = 0; shift < 32; shift += 8) byte(uint8_t(value >> shift));
	};
	word(surfaceAlgorithmVersion);
	byte('S'); byte('F');
	word(uint32_t(region.x)); word(uint32_t(region.y)); word(uint32_t(region.z));
	word(region.width); word(region.height);
	for (const auto kind : cells) byte(static_cast<uint8_t>(kind));
	return hash;
}

std::expected<SurfaceLayout, SurfaceError> validateSurface(
	const SurfaceConfig& config, MapExtent extent, SurfaceBudget budget,
	std::span<const uint8_t> mask, std::span<const uint8_t> protection, std::stop_token stop) {
	if (config.tuning.featureSize < 1 || config.tuning.featureSize > 4096 || config.tuning.lakePermille > 500 ||
		config.tuning.rockPermille > 500 || config.tuning.riverCount > 1024 || config.tuning.shoreWidth > 4 ||
		config.tuning.dirtPermille > 1000 || config.tuning.transitionWidth > 32768) {
		return std::unexpected(SurfaceError::InvalidParameters);
	}
	// Region, mask and budget rules are shared with v1 through a unit-weight config.
	const GenerationConfig shared { config.region, config.seed, 1, 0, 0 };
	const auto layout = validateGeneration(shared, extent, { budget.maxCells, budget.maxOutputBytes },
		mask, protection, stop);
	if (!layout) return std::unexpected(fromGeneration(layout.error()));
	const uint64_t count = layout->cellCount;
	if (count > std::numeric_limits<uint64_t>::max() / workingBytesPerCell - noiseBins) {
		return std::unexpected(SurfaceError::SizeOverflow);
	}
	const uint64_t working = count * workingBytesPerCell + uint64_t(noiseBins) * sizeof(uint32_t);
	if (working > budget.maxWorkingBytes) return std::unexpected(SurfaceError::WorkingLimit);
	return SurfaceLayout { layout->cellCount, layout->outputBytes, working };
}

std::expected<SurfaceGrid, SurfaceError> generateSurface(
	const SurfaceConfig& config, MapExtent extent, SurfaceBudget budget,
	std::span<const uint8_t> mask, std::span<const uint8_t> protection, std::stop_token stop) {
	if (stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
	const auto layout = validateSurface(config, extent, budget, mask, protection, stop);
	if (!layout) return std::unexpected(layout.error());
	const uint32_t width = config.region.width, height = config.region.height;
	const size_t count = layout->cellCount;
	const auto cancelled = [&] { return std::unexpected(SurfaceError::Cancelled); };
	try {
		std::vector<uint16_t> elevation(count), moisture(count);
		for (uint32_t y = 0; y < height; ++y) {
			if (stop.stop_requested()) return cancelled();
			for (uint32_t x = 0; x < width; ++x) {
				const uint32_t ax = uint32_t(config.region.x) + x, ay = uint32_t(config.region.y) + y;
				elevation[size_t(y) * width + x] = fractalNoise(config.seed, heightStream, ax, ay, config.tuning.featureSize);
				moisture[size_t(y) * width + x] = fractalNoise(config.seed, moistureStream, ax, ay,
					std::min<uint32_t>(config.tuning.featureSize * 2, 8192));
			}
		}

		// Lakes (lowest cells) and rock (highest cells), then drop patches that are too small.
		const uint32_t lakeLevel = config.tuning.lakePermille ? lowCut(elevation, config.tuning.lakePermille) : 0;
		const uint32_t rockLevel = config.tuning.rockPermille ? lowCut(elevation, 1000 - config.tuning.rockPermille) : noiseBins;
		std::vector<uint8_t> lake(count), rock(count);
		for (size_t i = 0; i < count; ++i) lake[i] = elevation[i] < lakeLevel;
		if (!dropSmall(lake, width, height, config.tuning.minLakeCells, stop)) return cancelled();
		for (size_t i = 0; i < count; ++i) rock[i] = !lake[i] && elevation[i] >= rockLevel;
		if (!dropSmall(rock, width, height, config.tuning.minRockCells, stop)) return cancelled();

		// Priority flood from lakes and the rectangle border. Every other cell records the
		// neighbour it was reached from; that chain is a loop-free 4-connected downhill route.
		constexpr int32_t noParent = -1;
		std::vector<int32_t> filled(count, 0), parent(count, noParent);
		std::vector<uint8_t> reached(count, 0);
		using Entry = std::pair<int32_t, uint32_t>;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
		for (size_t i = 0; i < count; ++i) {
			const uint32_t x = uint32_t(i % width), y = uint32_t(i / width);
			if (lake[i] || x == 0 || y == 0 || x + 1 == width || y + 1 == height) {
				reached[i] = 1;
				filled[i] = elevation[i];
				queue.emplace(filled[i], uint32_t(i));
			}
		}
		size_t popped = 0;
		while (!queue.empty()) {
			if (++popped % 4096 == 0 && stop.stop_requested()) return cancelled();
			const auto [level, cell] = queue.top();
			queue.pop();
			forNeighbours(cell, width, height, [&](size_t next) {
				if (reached[next]) return;
				reached[next] = 1;
				filled[next] = std::max<int32_t>(elevation[next], level + 1);
				parent[next] = int32_t(cell);
				queue.emplace(filled[next], uint32_t(next));
			});
		}

		// Rivers spring from rock: a source is a random rock cell and the route starts at the first
		// non-rock cell downhill, so a river leaves the edge of a rock patch instead of appearing in
		// open land. Without rock there are no rivers. A route shorter than minRiverLength own cells
		// is dropped. The draw count never depends on acceptance.
		std::vector<uint8_t> river(count, 0);
		if (config.tuning.riverCount) {
			std::vector<uint32_t> sources;
			for (size_t i = 0; i < count; ++i) {
				if (rock[i]) sources.push_back(uint32_t(i));
			}
			StableRandom random(config.seed, riverStream);
			std::vector<uint32_t> route;
			// flow: accepted routes through a cell. run: cells travelled from the farthest source
			// upstream. A tributary adds its flow and run to every cell downstream of the junction,
			// so a river is thin at its source and grows gradually towards its mouth and after merges.
			std::vector<uint16_t> flow(count, 0), run(count, 0);
			for (uint32_t index = 0; index < config.tuning.riverCount && !sources.empty(); ++index) {
				if (stop.stop_requested()) return cancelled();
				route.clear();
				int64_t cell = sources[random.bounded(sources.size())];
				while (cell != noParent && rock[cell]) cell = parent[cell];
				for (; cell != noParent && !lake[cell] && !river[cell]; cell = parent[cell]) {
					route.push_back(uint32_t(cell));
				}
				if (route.size() < config.tuning.minRiverLength) continue;
				for (size_t step = 0; step < route.size(); ++step) {
					river[route[step]] = 1;
					flow[route[step]] = 1;
					run[route[step]] = uint16_t(std::min<size_t>(step, 0xffff));
				}
				size_t travelled = route.size();
				for (int64_t down = parent[route.back()]; down != noParent && !lake[down]; down = parent[down]) {
					++flow[down];
					run[down] = std::max<uint16_t>(run[down], uint16_t(std::min<size_t>(travelled++, 0xffff)));
				}
			}
			// Widen each route cell into a block of 2, 3 or 4 cells by its size (cells travelled plus
			// 15 per merged route): under 20 -> 2, under 45 -> 3, otherwise 4. No stretch or headwater
			// tip is one cell wide. Lakes and rock are kept.
			const auto route2 = river;
			for (size_t i = 0; i < count; ++i) {
				if (!route2[i]) continue;
				const uint32_t size = uint32_t(run[i]) + 15u * (flow[i] - 1u);
				const int32_t blockWidth = size >= 45 ? 4 : size >= 20 ? 3 : 2;
				const int32_t first = -((blockWidth - 2) / 2), last = first + blockWidth - 1;
				const int32_t x = int32_t(i % width), y = int32_t(i / width);
				for (int32_t dy = first; dy <= last; ++dy) {
					for (int32_t dx = first; dx <= last; ++dx) {
						const int32_t nx = x + dx, ny = y + dy;
						if (nx < 0 || ny < 0 || nx >= int32_t(width) || ny >= int32_t(height)) continue;
						const size_t target = size_t(ny) * width + size_t(nx);
						if (!lake[target] && !rock[target]) river[target] = 1;
					}
				}
			}
		}

		// Final kinds. Priority: river > shore > lake > rock > dry/wet land.
		// Dry land splits into grass and dirt by moisture. The transition band is a smooth
		// offset (not per-cell noise), so edges wander instead of speckling, and patches
		// smaller than minPatchCells are absorbed by the surrounding kind.
		const uint32_t dirtLevel = lowCut(moisture, config.tuning.dirtPermille);
		std::vector<uint8_t> dirt(count, 0), grass(count, 0);
		for (size_t i = 0; i < count; ++i) {
			if (i % 4096 == 0 && stop.stop_requested()) return cancelled();
			if (lake[i] || rock[i]) continue;
			const uint32_t ax = uint32_t(config.region.x) + uint32_t(i % width);
			const uint32_t ay = uint32_t(config.region.y) + uint32_t(i / width);
			const int64_t offset = (int64_t(valueNoise(config.seed, ditherStream, ax, ay, transitionStep)) - noiseBins / 2) *
				int64_t(config.tuning.transitionWidth) / (noiseBins / 2);
			dirt[i] = int64_t(moisture[i]) + offset < int64_t(dirtLevel);
		}
		if (!dropSmall(dirt, width, height, minPatchCells, stop)) return cancelled();
		for (size_t i = 0; i < count; ++i) grass[i] = !lake[i] && !rock[i] && !dirt[i];
		if (!dropSmall(grass, width, height, minPatchCells, stop)) return cancelled();
		std::vector<SurfaceKind> kinds(count, SurfaceKind::Grass);
		for (size_t i = 0; i < count; ++i) {
			if (lake[i]) kinds[i] = SurfaceKind::Lake;
			else if (rock[i]) kinds[i] = SurfaceKind::Rock;
			else kinds[i] = grass[i] ? SurfaceKind::Grass : SurfaceKind::Dirt;
		}
		if (config.tuning.shoreWidth) {
			std::vector<uint8_t> wet = lake;
			for (uint32_t ring = 0; ring < config.tuning.shoreWidth; ++ring) {
				const auto previous = wet;
				for (size_t i = 0; i < count; ++i) {
					if (previous[i] || kinds[i] == SurfaceKind::Rock) continue;
					bool touches = false;
					forNeighbours(i, width, height, [&](size_t next) { touches |= previous[next] != 0; });
					if (touches) { wet[i] = 1; kinds[i] = SurfaceKind::Shore; }
				}
			}
		}
		for (size_t i = 0; i < count; ++i) if (river[i]) kinds[i] = SurfaceKind::River;

		for (size_t i = 0; i < count; ++i) {
			if ((!mask.empty() && mask[i] == 0) || (!protection.empty() && protection[i] != 0)) {
				kinds[i] = SurfaceKind::Preserve;
			}
		}
		if (stop.stop_requested()) return cancelled();
		return SurfaceGrid { config.region, std::move(kinds) };
	} catch (const std::bad_alloc&) {
		return std::unexpected(SurfaceError::AllocationFailed);
	}
}

} // namespace rme::procedural
