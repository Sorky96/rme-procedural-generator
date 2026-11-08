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
constexpr uint64_t shoreStream = 0x73686f72655f7332ULL;    // "shore_s2"
constexpr uint64_t meanderStream = 0x6d65616e6465725fULL;  // "meander_"
constexpr uint64_t pathStream = 0x706174685f737332ULL;     // "path_ss2"
constexpr size_t meanderWave = 16;       // route steps between two random swing values; stamps are interpolated, so the line never breaks
constexpr size_t meanderRamp = 12;       // route steps over which the swing grows from the ends and from junctions
constexpr uint64_t workingBytesPerCell = 40;               // estimate: fields, flood heap, queues
constexpr uint32_t noiseBins = 65536;
constexpr int64_t fixedOne = 4096;
constexpr uint32_t islandCells = 24;     // land patches fully inside a lake and smaller than this become lake
constexpr uint32_t shoreStep = 6;        // lattice step of the beach outline
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

// Octaves weighted 2^(n-1-o), each at half the lattice step of the previous one. Three octaves (4:2:1) as long as
// the finest step stays at 6 cells or more with three; a coarser step gets more octaves, up to eight, so the
// edges keep fine detail (G4 iteration 8).
uint32_t octavesFor(uint32_t step) {
	uint32_t octaves = 1;
	while (octaves < 8 && (step >> octaves) >= 6) ++octaves;
	return std::max<uint32_t>(3, octaves);
}

uint16_t fractalNoise(uint64_t seed, uint64_t stream, uint32_t x, uint32_t y, uint32_t step, uint32_t octaves = 3) {
	uint32_t sum = 0;
	for (uint32_t octave = 0; octave < octaves; ++octave) {
		const uint32_t octaveStep = std::max<uint32_t>(1, step >> octave);
		sum += valueNoise(seed, stream + octave * 0x1000193ULL, x, y, octaveStep) << (octaves - 1 - octave);
	}
	return uint16_t(sum / ((1u << octaves) - 1));
}

// Optional paths (G4 iteration 9): winding routes over Grass, Dirt and Shore between random land cells, 3 to 4 cells
// wide, smoothed so they do not turn at right angles and kept off the edge of rock. Rivers, lakes and rock are never
// crossed. Returns false when cancelled.
bool drawPaths(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, const SurfaceConfig& config, std::stop_token stop) {
	const size_t count = kinds.size();
	const uint64_t target = std::max<uint64_t>(uint64_t(config.tuning.pathLength) * count / 65536, 24);
	const auto land = [&](size_t i) { return kinds[i] == SurfaceKind::Grass || kinds[i] == SurfaceKind::Dirt; };
	// Distance (Chebyshev, 1..4) to the nearest Rock cell, 0 farther away: routes are pushed off the edge of the rock.
	std::vector<uint8_t> rockDistance(count, 0);
	{
		std::vector<size_t> frontier, next;
		for (size_t i = 0; i < count; ++i) if (kinds[i] == SurfaceKind::Rock) frontier.push_back(i);
		for (uint8_t distance = 1; distance <= 4 && !frontier.empty(); ++distance) {
			next.clear();
			for (const size_t i : frontier) {
				const int32_t x = int32_t(i % width), y = int32_t(i / width);
				for (int32_t dy = -1; dy <= 1; ++dy) {
					for (int32_t dx = -1; dx <= 1; ++dx) {
						const int32_t nx = x + dx, ny = y + dy;
						if (nx < 0 || ny < 0 || nx >= int32_t(width) || ny >= int32_t(height)) continue;
						const size_t j = size_t(ny) * width + size_t(nx);
						if (kinds[j] == SurfaceKind::Rock || rockDistance[j]) continue;
						rockDistance[j] = distance;
						next.push_back(j);
					}
				}
			}
			frontier.swap(next);
		}
	}
	constexpr uint32_t rockPenalty[5] = {0, 60, 40, 25, 12};
	StableRandom random(config.seed, pathStream);
	std::vector<uint32_t> cost(count, std::numeric_limits<uint32_t>::max());
	std::vector<int32_t> parent(count, -1);
	std::vector<uint8_t> closed(count, 0);
	std::vector<size_t> touched, route;
	std::vector<std::pair<int32_t, int32_t>> smooth;
	uint64_t placed = 0;
	const uint32_t span = std::max(width, height);
	const uint32_t minDistance = std::min<uint32_t>(48, span / 3), maxDistance = std::max<uint32_t>(minDistance + 1, std::min<uint32_t>(160, span));
	for (uint32_t attempt = 0; attempt < 200 && placed < target; ++attempt) {
		if (stop.stop_requested()) return false;
		size_t start = count;
		for (uint32_t tries = 0; tries < 64 && start == count; ++tries) {
			const size_t candidate = size_t(random.bounded(count));
			if (land(candidate)) start = candidate;
		}
		if (start == count) continue;
		const int32_t sx = int32_t(start % width), sy = int32_t(start / width);
		size_t goal = count;
		for (uint32_t tries = 0; tries < 48 && goal == count; ++tries) {
			const int32_t dx = int32_t(random.bounded(2 * maxDistance + 1)) - int32_t(maxDistance);
			const int32_t dy = int32_t(random.bounded(2 * maxDistance + 1)) - int32_t(maxDistance);
			const int32_t gx = sx + dx, gy = sy + dy;
			if (gx < 0 || gy < 0 || gx >= int32_t(width) || gy >= int32_t(height)) continue;
			if (std::max(std::abs(dx), std::abs(dy)) < int32_t(minDistance)) continue;
			const size_t candidate = size_t(gy) * width + size_t(gx);
			if (land(candidate)) goal = candidate;
		}
		if (goal == count) continue;
		const int32_t gx = int32_t(goal % width), gy = int32_t(goal / width);
		// Octile distance: 10 per straight step, 14 per diagonal one; never above the real cost of a step.
		const auto heuristic = [&](size_t i) {
			const uint32_t dx = uint32_t(std::abs(int32_t(i % width) - gx)), dy = uint32_t(std::abs(int32_t(i / width) - gy));
			return 10u * (dx + dy) - 6u * std::min(dx, dy);
		};
		using Entry = std::pair<uint32_t, uint32_t>; // estimated total, cell
		std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
		for (const size_t i : touched) { cost[i] = std::numeric_limits<uint32_t>::max(); parent[i] = -1; closed[i] = 0; }
		touched.clear();
		cost[start] = 0;
		touched.push_back(start);
		open.push({heuristic(start), uint32_t(start)});
		bool found = false;
		uint32_t expanded = 0;
		while (!open.empty() && expanded < 60000) {
			const size_t current = open.top().second;
			open.pop();
			if (closed[current]) continue;
			closed[current] = 1;
			++expanded;
			if (current == goal) { found = true; break; }
			const int32_t x = int32_t(current % width), y = int32_t(current / width);
			for (int32_t dy = -1; dy <= 1; ++dy) {
				for (int32_t dx = -1; dx <= 1; ++dx) {
					if (!dx && !dy) continue;
					const int32_t nx = x + dx, ny = y + dy;
					if (nx < 0 || ny < 0 || nx >= int32_t(width) || ny >= int32_t(height)) continue;
					const size_t next = size_t(ny) * width + size_t(nx);
					const SurfaceKind kind = kinds[next];
					if (kind != SurfaceKind::Grass && kind != SurfaceKind::Dirt && kind != SurfaceKind::Shore && kind != SurfaceKind::Path) continue;
					if (closed[next]) continue;
					const uint32_t noise = uint32_t(uint64_t(valueNoise(config.seed, pathStream + 1, uint32_t(config.region.x) + uint32_t(nx),
						uint32_t(config.region.y) + uint32_t(ny), 5)) * 10 / noiseBins);
					uint32_t step = 10 + noise + (kind == SurfaceKind::Shore ? 20 : 0) + rockPenalty[rockDistance[next]];
					if (dx && dy) step = step * 14 / 10;
					const uint32_t candidate = cost[current] + step;
					if (candidate >= cost[next]) continue;
					if (cost[next] == std::numeric_limits<uint32_t>::max()) touched.push_back(next);
					cost[next] = candidate;
					parent[next] = int32_t(current);
					open.push({candidate + heuristic(next), uint32_t(next)});
				}
			}
		}
		if (!found) continue;
		route.clear();
		for (int32_t cell = int32_t(goal); cell != -1; cell = parent[size_t(cell)]) route.push_back(size_t(cell));
		const uint32_t chebyshev = uint32_t(std::max(std::abs(sx - gx), std::abs(sy - gy)));
		if (route.size() > 3u * std::max<uint32_t>(chebyshev, 1)) continue;
		// Moving average of the route points rounds the corners; the line between smoothed points is stamped.
		constexpr int32_t window = 6;
		smooth.clear();
		for (int32_t k = 0; k < int32_t(route.size()); ++k) {
			const int32_t lo = std::max(0, k - window), hi = std::min(int32_t(route.size()) - 1, k + window);
			int64_t sumX = 0, sumY = 0;
			for (int32_t j = lo; j <= hi; ++j) { sumX += int64_t(route[size_t(j)] % width); sumY += int64_t(route[size_t(j)] / width); }
			smooth.emplace_back(int32_t(sumX / (hi - lo + 1)), int32_t(sumY / (hi - lo + 1)));
		}
		const auto stamp = [&](int32_t x, int32_t y) {
			const uint32_t swell = valueNoise(config.seed, pathStream + 2, uint32_t(config.region.x) + uint32_t(x), uint32_t(config.region.y) + uint32_t(y), 7) >> 15;
			for (int32_t dy = -1; dy <= 1 + int32_t(swell); ++dy) {
				for (int32_t dx = -1; dx <= 1 + int32_t(swell); ++dx) {
					const int32_t px = x + dx, py = y + dy;
					if (px < 0 || py < 0 || px >= int32_t(width) || py >= int32_t(height)) continue;
					const size_t i = size_t(py) * width + size_t(px);
					if (kinds[i] == SurfaceKind::Grass || kinds[i] == SurfaceKind::Dirt || kinds[i] == SurfaceKind::Shore) kinds[i] = SurfaceKind::Path;
				}
			}
		};
		for (size_t k = 0; k + 1 < smooth.size(); ++k) {
			const auto [ax, ay] = smooth[k];
			const auto [bx, by] = smooth[k + 1];
			const int32_t steps = std::max(1, std::max(std::abs(bx - ax), std::abs(by - ay)));
			for (int32_t t = 0; t <= steps; ++t) stamp(ax + (bx - ax) * t / steps, ay + (by - ay) * t / steps);
		}
		placed += route.size();
	}
	return true;
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

// Sets every 4-connected component of unflagged cells that never touches the grid border and has
// fewer than maxCells cells: a patch of land completely surrounded by the flagged kind.
bool fillEnclosed(std::vector<uint8_t>& flag, uint32_t width, uint32_t height, uint32_t maxCells,
	const std::stop_token& stop) {
	std::vector<uint8_t> seen(flag.size(), 0);
	std::vector<uint32_t> stack, component;
	for (size_t start = 0; start < flag.size(); ++start) {
		if (start % 4096 == 0 && stop.stop_requested()) return false;
		if (flag[start] || seen[start]) continue;
		component.clear();
		stack.assign(1, uint32_t(start));
		seen[start] = 1;
		bool touchesBorder = false;
		while (!stack.empty()) {
			const uint32_t cell = stack.back();
			stack.pop_back();
			component.push_back(cell);
			const uint32_t x = cell % width, y = cell / width;
			touchesBorder |= x == 0 || y == 0 || x + 1 == width || y + 1 == height;
			forNeighbours(cell, width, height, [&](size_t next) {
				if (!flag[next] && !seen[next]) { seen[next] = 1; stack.push_back(uint32_t(next)); }
			});
		}
		if (!touchesBorder && component.size() < maxCells) for (const auto cell : component) flag[cell] = 1;
	}
	return true;
}

// Size of the 4-connected component of every flagged cell, saturated at 65535; 0 for unflagged cells.
bool componentSizes(const std::vector<uint8_t>& flag, uint32_t width, uint32_t height,
	std::vector<uint16_t>& sizes, const std::stop_token& stop) {
	sizes.assign(flag.size(), 0);
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
		const auto size = uint16_t(std::min<size_t>(component.size(), 0xffff));
		for (const auto cell : component) sizes[cell] = size;
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
	return std::ranges::all_of(cells, [](SurfaceKind kind) { return kind <= SurfaceKind::Path; });
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
		config.tuning.rockPermille > 500 || config.tuning.riverCount > 1024 || config.tuning.shoreWidth > 8 || config.tuning.pathLength > 400 ||
		config.tuning.dirtPermille > 1000 || config.tuning.transitionWidth > 32768 || config.tuning.meander > 8) {
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
		const uint32_t octaves = octavesFor(config.tuning.featureSize);
		for (uint32_t y = 0; y < height; ++y) {
			if (stop.stop_requested()) return cancelled();
			for (uint32_t x = 0; x < width; ++x) {
				const uint32_t ax = uint32_t(config.region.x) + x, ay = uint32_t(config.region.y) + y;
				elevation[size_t(y) * width + x] = fractalNoise(config.seed, heightStream, ax, ay, config.tuning.featureSize, octaves);
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
		if (!fillEnclosed(lake, width, height, islandCells, stop)) return cancelled();
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
			std::vector<uint32_t> routeCells;   // accepted routes, source to mouth, back to back
			std::vector<size_t> routeEnds;      // end offset of each accepted route in routeCells
			std::vector<uint8_t> junction(count, 0); // river cells a tributary flows into: never smoothed away
			for (uint32_t index = 0; index < config.tuning.riverCount && !sources.empty(); ++index) {
				if (stop.stop_requested()) return cancelled();
				route.clear();
				int64_t cell = sources[random.bounded(sources.size())];
				while (cell != noParent && rock[cell]) cell = parent[cell];
				for (; cell != noParent && !lake[cell] && !river[cell]; cell = parent[cell]) {
					route.push_back(uint32_t(cell));
				}
				if (route.size() < config.tuning.minRiverLength) continue;
				routeCells.insert(routeCells.end(), route.begin(), route.end());
				routeEnds.push_back(routeCells.size());
				if (const int64_t joint = parent[route.back()]; joint != noParent && !lake[joint]) junction[joint] = 1;
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
			// Draw each route as a smoothed centre line: a moving average over up to 4 cells either side
			// (fixed point, 1/16 cell), shrinking to nothing at both ends so a tributary still meets its
			// river. Around every point goes a square of half-size 1 to 2 cells that grows with the
			// size of the river there (cells travelled plus 15 per merged route), so rivers bend instead
			// of turning at right angles and widen gradually. Lakes are kept; a river cuts through rock.
			constexpr int64_t unit = 16;
			constexpr size_t smoothing = 4;
			std::fill(river.begin(), river.end(), 0);
			size_t begin = 0;
			uint32_t routeNumber = 0;
			std::vector<uint32_t> junctionDistance;
			for (const size_t end : routeEnds) {
				if (stop.stop_requested()) return cancelled();
				const size_t length = end - begin;
				const uint32_t routeId = routeNumber++;
				if (config.tuning.meander) {
					// Steps to the nearest junction cell on this route, so a tributary still meets its river.
					junctionDistance.assign(length, UINT32_MAX / 2);
					for (size_t k = 0; k < length; ++k) {
						if (junction[routeCells[begin + k]]) junctionDistance[k] = 0;
						else if (k > 0) junctionDistance[k] = std::min(junctionDistance[k], junctionDistance[k - 1] + 1);
					}
					for (size_t k = length; k-- > 1;) junctionDistance[k - 1] = std::min(junctionDistance[k - 1], junctionDistance[k] + 1);
				}
				int64_t previousX = 0, previousY = 0;
				for (size_t k = 0; k < length; ++k) {
					const uint32_t own = routeCells[begin + k];
					const size_t half = junction[own] ? 0 : std::min({ smoothing, k, length - 1 - k });
					int64_t sumX = 0, sumY = 0;
					for (size_t j = k - half; j <= k + half; ++j) {
						const uint32_t routeCell = routeCells[begin + j];
						sumX += routeCell % width;
						sumY += routeCell / width;
					}
					const int64_t points = int64_t(2 * half + 1);
					int64_t centreX = sumX * unit / points, centreY = sumY * unit / points;
					if (config.tuning.meander) {
						// Sideways swing: smooth random value along the route (lattice every wave steps,
						// smoothstep between), pushed along the normal of the local direction, zero at both ends
						// and near junctions. Integer math only.
						const size_t lattice = k / meanderWave;
						const int64_t t = int64_t(k % meanderWave) * 1024 / int64_t(meanderWave);
						const int64_t smooth = t * t * (3 * 1024 - 2 * t) >> 20;
						const int64_t v0 = int64_t(surfaceLattice(config.seed, meanderStream, routeId, uint32_t(lattice))) - 32768;
						const int64_t v1 = int64_t(surfaceLattice(config.seed, meanderStream, routeId, uint32_t(lattice + 1))) - 32768;
						int64_t swing = (v0 * (1024 - smooth) + v1 * smooth) / 1024;     // -32768..32767
						swing = swing * int64_t(config.tuning.meander) * unit / 32768;   // 1/16 cell
						const size_t grow = std::min({ meanderRamp, k, length - 1 - k, size_t(junctionDistance[k]) });
						swing = swing * int64_t(grow) / int64_t(meanderRamp);
						const size_t k0 = k >= 4 ? k - 4 : 0, k1 = std::min(length - 1, k + 4);
						const int64_t dx = int64_t(routeCells[begin + k1] % width) - int64_t(routeCells[begin + k0] % width);
						const int64_t dy = int64_t(routeCells[begin + k1] / width) - int64_t(routeCells[begin + k0] / width);
						int64_t norm = 0;
						while ((norm + 1) * (norm + 1) <= dx * dx + dy * dy) ++norm; // integer square root
						if (norm > 0) {
							centreX += -dy * swing / norm;
							centreY += dx * swing / norm;
						}
						centreX = std::clamp<int64_t>(centreX, 0, int64_t(width - 1) * unit);
						centreY = std::clamp<int64_t>(centreY, 0, int64_t(height - 1) * unit);
					}
					const int64_t size = int64_t(run[own]) + 15 * (int64_t(flow[own]) - 1);
					const int64_t radius = unit + std::min<int64_t>(size * unit / 60, unit);
					const auto stampAt = [&](int64_t cx, int64_t cy) {
						const auto firstCell = [&](int64_t centre) { return centre - radius <= 0 ? int64_t(0) : (centre - radius + unit - 1) / unit; };
						const int64_t xMax = std::min<int64_t>(width - 1, (cx + radius) / unit);
						const int64_t yMax = std::min<int64_t>(height - 1, (cy + radius) / unit);
						for (int64_t y = firstCell(cy); y <= yMax; ++y) {
							for (int64_t x = firstCell(cx); x <= xMax; ++x) {
								const size_t target = size_t(y) * width + size_t(x);
								if (!lake[target]) river[target] = 1;
							}
						}
					};
					if (config.tuning.meander && k > 0) {
						// A swung line moves sideways as well as along the route; stamp between the two centres at
						// most half a cell apart so a steep swing cannot leave a gap.
						const int64_t reach = std::max(std::llabs(centreX - previousX), std::llabs(centreY - previousY));
						const int64_t between = reach / (unit / 2);
						for (int64_t step = 1; step < between + 1; ++step) {
							stampAt(previousX + (centreX - previousX) * step / (between + 1), previousY + (centreY - previousY) * step / (between + 1));
						}
					}
					stampAt(centreX, centreY);
					previousX = centreX;
					previousY = centreY;
				}
				begin = end;
			}
			if (config.tuning.meander) {
				// A swung line can cross a lake or, rarely, break. Every river body must still reach a lake or the
				// rectangle border and touch rock (where it springs); a body that does not is dropped.
				std::vector<uint8_t> visited(count, 0);
				std::vector<size_t> body, pending;
				for (size_t first = 0; first < count; ++first) {
					if (!river[first] || visited[first]) continue;
					if (stop.stop_requested()) return cancelled();
					body.clear();
					pending.assign(1, first);
					visited[first] = 1;
					bool reachesWater = false, touchesRock = false;
					while (!pending.empty()) {
						const size_t cell = pending.back();
						pending.pop_back();
						body.push_back(cell);
						const uint32_t x = uint32_t(cell % width), y = uint32_t(cell / width);
						reachesWater |= x == 0 || y == 0 || x + 1 == width || y + 1 == height;
						forNeighbours(cell, width, height, [&](size_t next) {
							if (river[next]) {
								if (!visited[next]) { visited[next] = 1; pending.push_back(next); }
							} else {
								reachesWater |= lake[next] != 0;
								touchesRock |= rock[next] != 0;
							}
						});
					}
					if (!reachesWater || !touchesRock) for (const size_t cell : body) river[cell] = 0;
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
			// Size of the lake each wet cell belongs to or borders: a small pond gets a narrow beach.
			std::vector<uint16_t> wetSize;
			if (!componentSizes(lake, width, height, wetSize, stop)) return cancelled();
			for (uint32_t ring = 0; ring < config.tuning.shoreWidth; ++ring) {
				const auto previous = wet;
				const auto previousSize = wetSize;
				// Ring r (r >= 1) needs a lake of at least 30 * 3^(r-1) cells: 30, 90, and 90 for every ring after.
				uint32_t neededLake = 30;
				for (uint32_t step = 1; step < ring && neededLake < 90; ++step) neededLake *= 3;
				// The first ring always exists; every further ring needs smooth noise above a threshold
				// that rises with the ring, so the beach is wide in some places and grass reaches in
				// elsewhere instead of forming an even band.
				const uint32_t cut = ring == 0 ? 0 : uint32_t(uint64_t(noiseBins) * std::min<uint32_t>(5 + 10 * ring, 40) / 100);
				for (size_t i = 0; i < count; ++i) {
					if (previous[i] || kinds[i] == SurfaceKind::Rock) continue;
					bool touches = false;
					uint16_t lakeSize = 0;
					forNeighbours(i, width, height, [&](size_t next) {
						if (!previous[next]) return;
						touches = true;
						lakeSize = std::max(lakeSize, previousSize[next]);
					});
					if (!touches) continue;
					if (ring > 0 && lakeSize < neededLake) continue;
					if (cut && valueNoise(config.seed, shoreStream, uint32_t(config.region.x) + uint32_t(i % width),
						uint32_t(config.region.y) + uint32_t(i / width), shoreStep) < cut) continue;
					wet[i] = 1;
					wetSize[i] = lakeSize;
					kinds[i] = SurfaceKind::Shore;
				}
			}
		}
		for (size_t i = 0; i < count; ++i) if (river[i]) kinds[i] = SurfaceKind::River;
		if (config.tuning.pathLength && !drawPaths(kinds, width, height, config, stop)) return cancelled();

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
