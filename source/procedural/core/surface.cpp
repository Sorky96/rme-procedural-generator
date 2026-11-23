// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/surface.h"

#include "procedural/core/reachability.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <limits>
#include <new>
#include <optional>
#include <queue>
#include <ranges>
#include <span>
#include <utility>

namespace rme::procedural {

namespace {

constexpr uint64_t heightStream = 0x6865696768745f32ULL;   // "height_2"
constexpr uint64_t moistureStream = 0x6d6f69737475725fULL; // "moistur_"
constexpr uint64_t coastStream = 0x636f6173745f7331ULL;    // "coast_s1"
constexpr uint64_t ditherStream = 0x6469746865725f32ULL;   // "dither_2"
constexpr uint64_t riverStream = 0x72697665725f7332ULL;    // "river_s2"
constexpr uint64_t shoreStream = 0x73686f72655f7332ULL;    // "shore_s2"
constexpr uint64_t meanderStream = 0x6d65616e6465725fULL;  // "meander_"
constexpr uint64_t rimStream = 0x72696d5f73733130ULL;      // "rim_ss10"
constexpr uint64_t buildingStream = 0x6275696c645f7333ULL;  // "build_s3"
constexpr uint64_t pathStream = 0x706174685f737332ULL;     // "path_ss2"
constexpr uint64_t doorStream = 0x646f6f725f733132ULL;     // "door_s12"
constexpr size_t meanderWave = 16;       // route steps between two random swing values; stamps are interpolated, so the line never breaks
constexpr size_t meanderRamp = 12;       // route steps over which the swing grows from the ends and from junctions
constexpr uint64_t fordShapeStream = 0x666f72645f736831ULL;  // "ford_sh1"
constexpr uint64_t fordWidthStream = 0x666f72645f776431ULL;  // "ford_wd1"
constexpr size_t fordNaturalLength = 8;                     // crossings of at least this many water cells get the natural shape
constexpr int32_t fordSwingMax = 4;                         // largest sideways swing of the axis in cells
constexpr uint32_t fordShapeStep = 8;                       // noise step along the ford, in cells
constexpr uint32_t fordWideThreshold = 40000;               // width noise above this (of 65536) makes the ford 5 cells wide instead of 3
constexpr uint32_t waterCrossCost = 25;                     // extra A* cost of a river cell when trails may cross rivers (iteration 17)
constexpr uint32_t passCrossCost = 40;                      // extra A* cost of a rock cell when trails may cross rock (iteration 15)
constexpr int32_t fordMouthRadius = 3;                      // water within this Chebyshev distance of a ford end becomes sand too (iteration 14c)
constexpr uint64_t fordBytesPerCell = 28;                   // estimate for the ford pass: labels, order, seen, parent, depth, queue, offsets
constexpr uint64_t workingBytesPerCell = 60;               // estimate: fields, flood heap, queues, island classification
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

// Grass rim (G4 iteration 10): dirt does not touch rock or beach in the reference maps. Every Dirt cell within two cells
// (Chebyshev) of Shore becomes Grass; next to Rock about three in four do, picked by smooth noise so some contact stays.
void grassRim(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, const SurfaceConfig& config) {
	std::vector<uint8_t> rim(kinds.size(), 0);
	for (size_t i = 0; i < kinds.size(); ++i) {
		if (kinds[i] != SurfaceKind::Rock && kinds[i] != SurfaceKind::Shore) continue;
		const uint8_t mark = kinds[i] == SurfaceKind::Rock ? 1 : 2;
		const int32_t x = int32_t(i % width), y = int32_t(i / width);
		for (int32_t dy = -2; dy <= 2; ++dy) {
			for (int32_t dx = -2; dx <= 2; ++dx) {
				const int32_t nx = x + dx, ny = y + dy;
				if (nx >= 0 && ny >= 0 && nx < int32_t(width) && ny < int32_t(height)) rim[size_t(ny) * width + size_t(nx)] |= mark;
			}
		}
	}
	for (size_t i = 0; i < kinds.size(); ++i) {
		if (!rim[i] || kinds[i] != SurfaceKind::Dirt) continue;
		const bool nearShore = rim[i] & 2;
		const uint32_t noise = valueNoise(config.seed, rimStream, uint32_t(config.region.x) + uint32_t(i % width), uint32_t(config.region.y) + uint32_t(i / width), 4);
		if (nearShore || noise < noiseBins * 3 / 4) kinds[i] = SurfaceKind::Grass;
	}
}

// Routes between land cells (G4 iteration 9) and stamps them as Path: A* over Grass, Dirt, Shore and Path, smoothed so the
// route does not turn at right angles, stamped 3 to 4 cells wide and kept off the edge of rock. Rivers, lakes, rock and
// foundations are never crossed. The distance-to-rock field is taken once, when the router is built.
class PathRouter {
public:
	// `crossings` (optional, one byte per cell) switches on river crossings (iteration 17): cells of River under a route become Shore
	// and are marked there, so a later trail does not turn them into Path (Path over water gets a blocking water edge).
	PathRouter(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, const SurfaceConfig& config, std::vector<uint8_t>* crossings = nullptr)
		: kinds_(kinds), width_(width), height_(height), config_(config), crossings_(crossings),
		  rockDistance_(kinds.size(), 0), cost_(kinds.size(), std::numeric_limits<uint32_t>::max()), parent_(kinds.size(), -1), closed_(kinds.size(), 0) {
		std::vector<size_t> frontier, next;
		for (size_t i = 0; i < kinds.size(); ++i) if (kinds[i] == SurfaceKind::Rock) frontier.push_back(i);
		for (uint8_t distance = 1; distance <= 4 && !frontier.empty(); ++distance) {
			next.clear();
			for (const size_t i : frontier) {
				const int32_t x = int32_t(i % width), y = int32_t(i / width);
				for (int32_t dy = -1; dy <= 1; ++dy) {
					for (int32_t dx = -1; dx <= 1; ++dx) {
						const int32_t nx = x + dx, ny = y + dy;
						if (nx < 0 || ny < 0 || nx >= int32_t(width) || ny >= int32_t(height)) continue;
						const size_t j = size_t(ny) * width + size_t(nx);
						if (kinds[j] == SurfaceKind::Rock || rockDistance_[j]) continue;
						rockDistance_[j] = distance;
						next.push_back(j);
					}
				}
			}
			frontier.swap(next);
		}
	}

	// Stamps a route from `start` to `goal`; returns its length in cells, 0 when there is no acceptable route. Cells that
	// become Path are appended to `stamped` when it is given.
	size_t connect(size_t start, size_t goal, std::vector<uint32_t>* stamped = nullptr, bool reachEnds = false) {
		std::vector<SurfaceKind>& kinds = kinds_;
		const uint32_t width = width_, height = height_;
		const SurfaceConfig& config = config_;
		constexpr uint32_t rockPenalty[5] = {0, 60, 40, 25, 12};
		const int32_t sx = int32_t(start % width), sy = int32_t(start / width);
		const int32_t gx = int32_t(goal % width), gy = int32_t(goal / width);
		// Octile distance: 10 per straight step, 14 per diagonal one; never above the real cost of a step.
		const auto heuristic = [&](size_t i) {
			const uint32_t dx = uint32_t(std::abs(int32_t(i % width) - gx)), dy = uint32_t(std::abs(int32_t(i / width) - gy));
			return 10u * (dx + dy) - 6u * std::min(dx, dy);
		};
		using Entry = std::pair<uint32_t, uint32_t>; // estimated total, cell
		std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
		for (const size_t i : touched_) { cost_[i] = std::numeric_limits<uint32_t>::max(); parent_[i] = -1; closed_[i] = 0; }
		touched_.clear();
		cost_[start] = 0;
		touched_.push_back(start);
		open.push({heuristic(start), uint32_t(start)});
		bool found = false;
		uint32_t expanded = 0;
		while (!open.empty() && expanded < 60000) {
			const size_t current = open.top().second;
			open.pop();
			if (closed_[current]) continue;
			closed_[current] = 1;
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
					const bool rockStep = kind == SurfaceKind::Rock && config.tuning.rockPasses;
					const bool waterStep = kind == SurfaceKind::River && crossings_;
					if (!rockStep && !waterStep && kind != SurfaceKind::Grass && kind != SurfaceKind::Dirt && kind != SurfaceKind::Shore && kind != SurfaceKind::Path) continue;
					if (closed_[next]) continue;
					const uint32_t noise = uint32_t(uint64_t(valueNoise(config.seed, pathStream + 1, uint32_t(config.region.x) + uint32_t(nx),
						uint32_t(config.region.y) + uint32_t(ny), 5)) * 10 / noiseBins);
					uint32_t step = 10 + noise + (kind == SurfaceKind::Shore ? 20 : 0) + (rockStep ? passCrossCost : 0) + (waterStep ? waterCrossCost : 0) + rockPenalty[rockDistance_[next]];
					if (dx && dy) step = step * 14 / 10;
					const uint32_t candidate = cost_[current] + step;
					if (candidate >= cost_[next]) continue;
					if (cost_[next] == std::numeric_limits<uint32_t>::max()) touched_.push_back(next);
					cost_[next] = candidate;
					parent_[next] = int32_t(current);
					open.push({candidate + heuristic(next), uint32_t(next)});
				}
			}
		}
		if (!found) return 0;
		route_.clear();
		for (int32_t cell = int32_t(goal); cell != -1; cell = parent_[size_t(cell)]) route_.push_back(size_t(cell));
		const uint32_t chebyshev = uint32_t(std::max(std::abs(sx - gx), std::abs(sy - gy)));
		if (route_.size() > 3u * std::max<uint32_t>(chebyshev, 1)) return 0;
		// Only a route that really crosses a river turns river cells into sand; a trail beside a river leaves it alone.
		bool wet = false;
		for (const size_t cell : route_) wet = wet || kinds[cell] == SurfaceKind::River;
		// Moving average of the route points rounds the corners; the line between smoothed points is stamped.
		constexpr int32_t window = 6;
		smooth_.clear();
		for (int32_t k = 0; k < int32_t(route_.size()); ++k) {
			const int32_t lo = std::max(0, k - window), hi = std::min(int32_t(route_.size()) - 1, k + window);
			int64_t sumX = 0, sumY = 0;
			for (int32_t j = lo; j <= hi; ++j) { sumX += int64_t(route_[size_t(j)] % width); sumY += int64_t(route_[size_t(j)] / width); }
			smooth_.emplace_back(int32_t(sumX / (hi - lo + 1)), int32_t(sumY / (hi - lo + 1)));
		}
		const auto stamp = [&](int32_t x, int32_t y) {
			const uint32_t swell = valueNoise(config.seed, pathStream + 2, uint32_t(config.region.x) + uint32_t(x), uint32_t(config.region.y) + uint32_t(y), 7) >> 15;
			for (int32_t dy = -1; dy <= 1 + int32_t(swell); ++dy) {
				for (int32_t dx = -1; dx <= 1 + int32_t(swell); ++dx) {
					const int32_t px = x + dx, py = y + dy;
					if (px < 0 || py < 0 || px >= int32_t(width) || py >= int32_t(height)) continue;
					const size_t i = size_t(py) * width + size_t(px);
					if (crossings_ && wet && kinds[i] == SurfaceKind::River) {
						kinds[i] = SurfaceKind::Shore;
						(*crossings_)[i] = 1;
					} else if (kinds[i] == SurfaceKind::Grass || kinds[i] == SurfaceKind::Dirt || (kinds[i] == SurfaceKind::Shore && !(crossings_ && (*crossings_)[i])) || (config.tuning.rockPasses && kinds[i] == SurfaceKind::Rock)) {
						kinds[i] = SurfaceKind::Path;
						if (stamped) stamped->push_back(uint32_t(i));
					}
				}
			}
		};
		for (size_t k = 0; k + 1 < smooth_.size(); ++k) {
			const auto [ax, ay] = smooth_[k];
			const auto [bx, by] = smooth_[k + 1];
			const int32_t steps = std::max(1, std::max(std::abs(bx - ax), std::abs(by - ay)));
			for (int32_t t = 0; t <= steps; ++t) stamp(ax + (bx - ax) * t / steps, ay + (by - ay) * t / steps);
		}
		if (reachEnds && !smooth_.empty()) {
			// The moving average pulls both ends of the line inwards; join them to the real start and goal. route_ runs from the goal
			// back to the start, so the front of smooth_ is the goal end.
			const auto line = [&](int32_t ax, int32_t ay, int32_t bx, int32_t by) {
				const int32_t steps = std::max(1, std::max(std::abs(bx - ax), std::abs(by - ay)));
				for (int32_t t = 0; t <= steps; ++t) stamp(ax + (bx - ax) * t / steps, ay + (by - ay) * t / steps);
			};
			line(gx, gy, smooth_.front().first, smooth_.front().second);
			line(smooth_.back().first, smooth_.back().second, sx, sy);
		}
		return route_.size();
	}

private:
	std::vector<SurfaceKind>& kinds_;
	uint32_t width_, height_;
	const SurfaceConfig& config_;
	std::vector<uint8_t>* crossings_;
	std::vector<uint8_t> rockDistance_;  // Chebyshev distance (1..4) to the nearest Rock cell, 0 farther away
	std::vector<uint32_t> cost_;
	std::vector<int32_t> parent_;
	std::vector<uint8_t> closed_;
	std::vector<size_t> touched_, route_;
	std::vector<std::pair<int32_t, int32_t>> smooth_;
};

// Optional paths (G4 iteration 9): winding routes between random land cells. Returns false when cancelled.
} // namespace

void smoothShores(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, uint32_t passes, uint32_t radiusIn) {
	const size_t count = kinds.size();
	const int32_t radius = radiusIn ? int32_t(radiusIn) : 3, reach = 3 * radius;
	std::vector<uint8_t> eligible(count, 0);
	for (size_t i = 0; i < count; ++i) {
		if (kinds[i] != SurfaceKind::Lake) continue;
		const int32_t x = int32_t(i % width), y = int32_t(i / width);
		for (int32_t dy = -reach; dy <= reach; ++dy) {
			for (int32_t dx = -reach; dx <= reach; ++dx) {
				const int32_t nx = x + dx, ny = y + dy;
				if (nx < 0 || ny < 0 || nx >= int32_t(width) || ny >= int32_t(height)) continue;
				const size_t j = size_t(ny) * width + size_t(nx);
				// Cells next to the water (ring 0 of the beach) are never touched.
				if (std::abs(dx) <= 1 && std::abs(dy) <= 1) eligible[j] |= 2;
				else eligible[j] |= 1;
			}
		}
	}
	for (uint32_t pass = 0; pass < passes; ++pass) {
		const std::vector<SurfaceKind> before = kinds;
		for (size_t i = 0; i < count; ++i) {
			if (!(eligible[i] & 1) || (eligible[i] & 2)) continue;
			const SurfaceKind own = before[i];
			if (own != SurfaceKind::Shore && own != SurfaceKind::Grass && own != SurfaceKind::Dirt) continue;
			const int32_t x = int32_t(i % width), y = int32_t(i / width);
			uint32_t sand = 0, grass = 0, dirt = 0;
			for (int32_t dy = -radius; dy <= radius; ++dy) {
				for (int32_t dx = -radius; dx <= radius; ++dx) {
					const int32_t nx = x + dx, ny = y + dy;
					if (nx < 0 || ny < 0 || nx >= int32_t(width) || ny >= int32_t(height)) continue;
					const SurfaceKind kind = before[size_t(ny) * width + size_t(nx)];
					sand += kind == SurfaceKind::Shore || kind == SurfaceKind::Lake;
					grass += kind == SurfaceKind::Grass;
					dirt += kind == SurfaceKind::Dirt;
				}
			}
			const bool becomesSand = sand * 2 > sand + grass + dirt;
			if (becomesSand) kinds[i] = SurfaceKind::Shore;
			else if (own == SurfaceKind::Shore) kinds[i] = grass >= dirt ? SurfaceKind::Grass : SurfaceKind::Dirt;
		}
	}
}

void trimSpits(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height) {
	const size_t count = kinds.size();
	const auto isWater = [](SurfaceKind kind) { return kind == SurfaceKind::Lake || kind == SurfaceKind::River; };
	std::vector<SurfaceKind> result = kinds;
	for (size_t i = 0; i < count; ++i) {
		if (kinds[i] != SurfaceKind::Shore) continue;
		const int32_t x = int32_t(i % width), y = int32_t(i / width);
		for (int axis = 0; axis < 2; ++axis) {
			const int32_t dx = axis == 0 ? 1 : 0, dy = axis == 0 ? 0 : 1;
			// Walk to both ends of the run of Shore cells through (x, y); each end must be water inside the area.
			SurfaceKind ends[2] = {SurfaceKind::Preserve, SurfaceKind::Preserve};
			int32_t length = 1;
			bool inside = true;
			for (int side = 0; side < 2 && inside; ++side) {
				const int32_t sign = side == 0 ? 1 : -1;
				for (int32_t step = 1;; ++step) {
					const int32_t nx = x + sign * dx * step, ny = y + sign * dy * step;
					if (nx < 0 || ny < 0 || nx >= int32_t(width) || ny >= int32_t(height)) { inside = false; break; }
					const SurfaceKind kind = kinds[size_t(ny) * width + size_t(nx)];
					if (kind == SurfaceKind::Shore) { if (++length > 2) { inside = false; break; } continue; }
					ends[side] = kind;
					break;
				}
			}
			if (!inside || length > 2 || !isWater(ends[0]) || !isWater(ends[1])) continue;
			result[i] = ends[0] == SurfaceKind::River || ends[1] == SurfaceKind::River ? SurfaceKind::River : SurfaceKind::Lake;
			break;
		}
	}
	kinds.swap(result);
}

// Fords (G4 iteration 14): while a walkable component of at least reachabilityPocketCells cells outside the largest one
// has another component within `reach` water cells (River or Lake, 4-neighbour moves), the smallest such component gets the
// shortest crossing stamped as Shore (sand: Path next to water gets a blocking water edge from the auto-border), together
// with the water cells beside it (about three cells wide). Components are
// numbered row-major and tried in label order; every ford relabels the surface.
std::expected<void, SurfaceError> placeFords(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, uint32_t reach,
	std::span<const uint32_t> doors, std::stop_token stop, FordShape shape) {
	const size_t count = kinds.size();
	std::vector<uint32_t> seen(count, 0), order(count), queue;
	std::vector<int32_t> parent(count, -1);
	std::vector<uint32_t> depth(count, 0), offset;
	uint32_t token = 0;
	queue.reserve(count);
	const auto isWater = [&](size_t i) { return kinds[i] == SurfaceKind::River || kinds[i] == SurfaceKind::Lake; };
	std::optional<uint32_t> repairStart;  // the fixed start, taken from the first labelling (repair mode)
	for (;;) {
		auto labels = walkableLabelsOf(kinds, width, height, doors, true, stop);
		if (!labels) {
			if (labels.error() == ReachabilityError::Cancelled) return std::unexpected(SurfaceError::Cancelled);
			if (labels.error() == ReachabilityError::AllocationFailed) throw std::bad_alloc();
			return std::unexpected(SurfaceError::InvalidParameters);
		}
		const uint32_t components = uint32_t(labels->sizes.size());
		// Repair mode: the components that hold a door or an exit and are not the start's component.
		std::vector<uint8_t> needsFord;
		if (shape.repair) {
			if (!repairStart) repairStart = fixedStartOf(*labels, width, height);
			if (!repairStart) return {};
			needsFord.assign(components, 0);
			for (const uint32_t door : doors) needsFord[labels->label[door]] = 1;
			for (const uint32_t site : shape.sites) needsFord[labels->label[site]] = 1;
			for (const uint32_t cell : exitCellsOf(*labels, width, height)) needsFord[labels->label[cell]] = 1;
			needsFord[0] = 0;
			needsFord[labels->label[*repairStart]] = 0;
		}
		offset.assign(components + 1, 0);
		for (const uint32_t label : labels->label) ++offset[label + 1];
		for (uint32_t id = 0; id < components; ++id) offset[id + 1] += offset[id];
		std::vector<uint32_t> fill(offset.begin(), offset.end() - 1);
		for (size_t i = 0; i < count; ++i) order[fill[labels->label[i]]++] = uint32_t(i);

		bool stamped = false;
		for (uint32_t id = 1; id < components && !stamped; ++id) {
			if (shape.repair ? !needsFord[id] : (id == labels->largest || labels->sizes[id] < reachabilityPocketCells)) continue;
			if (stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
			++token;
			queue.clear();
			for (uint32_t k = offset[id]; k < offset[id + 1]; ++k) {
				queue.push_back(order[k]);
				seen[order[k]] = token;
				depth[order[k]] = 0;
				parent[order[k]] = -1;
			}
			for (size_t head = 0; head < queue.size() && !stamped; ++head) {
				const size_t i = queue[head];
				const size_t x = i % width, y = i / width;
				const size_t next[4] = {x > 0 ? i - 1 : count, x + 1 < width ? i + 1 : count, y > 0 ? i - width : count, y + 1 < height ? i + width : count};
				for (const size_t n : next) {
					if (n == count || seen[n] == token) continue;
					if (labels->label[n] != 0) {
						// Another component reached from a water cell: the water cells back to the source are the ford.
						if (depth[i] == 0) continue;
						std::vector<size_t> crossing;
						for (int32_t at = int32_t(i); at >= 0 && depth[size_t(at)] > 0; at = parent[size_t(at)]) crossing.push_back(size_t(at));
						const auto stampSquare = [&](size_t cell, int32_t radius, bool plus) {
							const int32_t cx = int32_t(cell % width), cy = int32_t(cell / width);
							for (int32_t dy = -radius; dy <= radius; ++dy) {
								for (int32_t dx = -radius; dx <= radius; ++dx) {
									if (plus && std::abs(dx) + std::abs(dy) > radius) continue;
									const int32_t mx = cx + dx, my = cy + dy;
									if (mx < 0 || my < 0 || mx >= int32_t(width) || my >= int32_t(height)) continue;
									if (const size_t m = size_t(my) * width + size_t(mx); isWater(m)) kinds[m] = SurfaceKind::Shore;
								}
							}
						};
						const auto stampStraight = [&] { for (const size_t cell : crossing) { kinds[cell] = SurfaceKind::Shore; stampSquare(cell, 1, true); } };
						bool natural = false;
						if (crossing.size() >= fordNaturalLength) {
							// Natural shape (iteration 14d): an axis that swings sideways between the two ends, with a width that varies.
							const size_t startCell = crossing.back(), endCell = crossing.front();
							const int32_t sx = int32_t(startCell % width), sy = int32_t(startCell / width), tx = int32_t(endCell % width), ty = int32_t(endCell / width);
							const bool alongX = std::abs(tx - sx) >= std::abs(ty - sy);
							const int32_t major = alongX ? std::abs(tx - sx) : std::abs(ty - sy);
							if (major > 0) {
								const int32_t majorStep = (alongX ? tx - sx : ty - sy) > 0 ? 1 : -1;
								const int32_t minorDelta = alongX ? ty - sy : tx - sx;
								const int32_t amplitude = std::min<int32_t>(fordSwingMax, int32_t(crossing.size()) / 6);
								int32_t previousMinor = 0;
								for (int32_t pos = 0; pos <= major; ++pos) {
									const int32_t baseMinor = (alongX ? sy : sx) + (minorDelta * pos + (minorDelta >= 0 ? major / 2 : -major / 2)) / major;
									const int32_t baseMajor = (alongX ? sx : sy) + majorStep * pos;
									const uint32_t bx = uint32_t(shape.x) + uint32_t(alongX ? baseMajor : baseMinor), by = uint32_t(shape.y) + uint32_t(alongX ? baseMinor : baseMajor);
									const int64_t envelope = int64_t(4) * pos * (major - pos) * 1024 / (int64_t(major) * major);
									const int64_t swing = (int64_t(valueNoise(shape.seed, fordShapeStream, bx, by, fordShapeStep)) - noiseBins / 2) * 2 * amplitude * envelope / (int64_t(noiseBins) * 1024);
									const int32_t minor = baseMinor + int32_t(swing);
									const int32_t radius = valueNoise(shape.seed, fordWidthStream, bx, by, fordShapeStep) > fordWideThreshold ? 2 : 1;
									// Fill the gap to the previous column so the axis stays 4-connected.
									const int32_t from = pos == 0 ? minor : previousMinor;
									const int32_t direction = minor >= from ? 1 : -1;
									for (int32_t m = from; m != minor + direction; m += direction) {
										const int32_t ax = alongX ? baseMajor : m, ay = alongX ? m : baseMajor;
										if (ax < 0 || ay < 0 || ax >= int32_t(width) || ay >= int32_t(height)) continue;
										const size_t axis = size_t(ay) * width + size_t(ax);
										if (isWater(axis)) kinds[axis] = SurfaceKind::Shore;
										stampSquare(axis, radius, false);
									}
									previousMinor = minor;
								}
								// The axis must really join the two ends over walkable cells; otherwise the straight crossing is added.
								for (const size_t endCellToOpen : {startCell, endCell}) stampSquare(endCellToOpen, fordMouthRadius, false);
								++token;
								std::vector<uint32_t> flood {uint32_t(startCell)};
								seen[startCell] = token;
								for (size_t head = 0; head < flood.size() && !natural; ++head) {
									const size_t at = flood[head];
									const size_t ax = at % width, ay = at / width;
									const size_t around[4] = {ax > 0 ? at - 1 : count, ax + 1 < width ? at + 1 : count, ay > 0 ? at - width : count, ay + 1 < height ? at + width : count};
									for (const size_t m : around) {
										if (m == count || seen[m] == token) continue;
										const SurfaceKind kind = kinds[m];
										if (kind != SurfaceKind::Grass && kind != SurfaceKind::Dirt && kind != SurfaceKind::Rock && kind != SurfaceKind::Shore && kind != SurfaceKind::Path) continue;
										seen[m] = token;
										if (labels->label[m] == labels->label[n]) { natural = true; break; }
										flood.push_back(uint32_t(m));
									}
								}
							}
						}
						if (!natural) stampStraight();
						for (const size_t endCellToOpen : {crossing.front(), crossing.back()}) stampSquare(endCellToOpen, fordMouthRadius, false);
						stamped = true;
						break;
					}
					if (!isWater(n) || depth[i] + 1 > reach) continue;
					seen[n] = token;
					depth[n] = depth[i] + 1;
					parent[n] = int32_t(i);
					queue.push_back(uint32_t(n));
				}
			}
		}
		if (!stamped) return {};
	}
}

namespace {

constexpr uint64_t entranceStream = 0x656e7472616e6365ULL;  // "entrance"
constexpr uint64_t entranceFallbackStream = 0x656e7472616e6366ULL;

// Draws cells of one style until the wanted count, keeping a Chebyshev gap of 24 to every earlier entrance.
bool drawEntrances(const std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, uint64_t wanted, uint64_t attempts,
	StableRandom& random, CaveEntranceStyle style, bool relaxed, std::vector<uint32_t>& entrances, std::stop_token stop) {
	for (uint64_t attempt = 0; attempt < attempts && entrances.size() < wanted; ++attempt) {
		if (attempt % 256 == 0 && stop.stop_requested()) return false;
		const size_t cell = size_t(random.bounded(kinds.size()));
		if (!caveEntranceAllowed(kinds, width, height, uint32_t(cell), style, relaxed)) continue;
		const int32_t x = int32_t(cell % width), y = int32_t(cell / width);
		const bool free = std::ranges::none_of(entrances, [&](uint32_t other) {
			return std::max(std::abs(int32_t(other % width) - x), std::abs(int32_t(other / width) - y)) < 24;
		});
		if (free) entrances.push_back(uint32_t(cell));
	}
	return true;
}

bool placeEntrances(const std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, const SurfaceConfig& config, std::vector<uint32_t>& entrances, std::stop_token stop) {
	const uint64_t wanted = uint64_t(config.tuning.caveEntrances) * kinds.size() / 65536;
	if (!wanted || width < 8 || height < 8) return true;
	const auto style = config.tuning.caveEntranceStyle;
	const bool relaxed = config.tuning.repairReach != 0;
	StableRandom random(config.seed, entranceStream);
	const uint64_t attemptsPerEntrance = relaxed && style == CaveEntranceStyle::RockFace ? 4000 : 400;
	if (!drawEntrances(kinds, width, height, wanted, (wanted + 1) * attemptsPerEntrance, random, style, relaxed, entrances, stop)) return false;
	// Iteration 48: when the rock walls have too few sites, the rest become ordinary entrances off the rock (FloorHole
	// cells), drawn from their own stream so the rock-face entrances above stay exactly as before.
	if (style != CaveEntranceStyle::RockFace || entrances.size() >= wanted) return true;
	StableRandom fallback(config.seed, entranceFallbackStream);
	return drawEntrances(kinds, width, height, wanted, (wanted - entrances.size() + 1) * 400, fallback,
		CaveEntranceStyle::FloorHole, relaxed, entrances, stop);
}

bool drawPaths(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, const SurfaceConfig& config, std::stop_token stop, std::vector<uint8_t>* crossings) {
	const size_t count = kinds.size();
	const uint64_t target = std::max<uint64_t>(uint64_t(config.tuning.pathLength) * count / 65536, 24);
	const auto land = [&](size_t i) { return kinds[i] == SurfaceKind::Grass || kinds[i] == SurfaceKind::Dirt; };
	PathRouter router(kinds, width, height, config, crossings);
	StableRandom random(config.seed, pathStream);
	uint64_t placed = 0;
	const uint32_t span = std::max(width, height);
	const uint32_t minDistance = std::min<uint32_t>(48, span / 3), maxDistance = std::max<uint32_t>(minDistance + 1, std::min<uint32_t>(160, span));
	// Iteration 16: a land cell on the far side of a rock band (3 to 30 rock cells) straight along one of 8 directions from `from`,
	// without water in between; `count` when none of 8 random directions has one within 48 steps.
	const auto crossingGoal = [&](size_t from) {
		constexpr int32_t directions[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
		for (uint32_t tries = 0; tries < 8; ++tries) {
			const auto& [dx, dy] = directions[random.bounded(8)];
			int32_t x = int32_t(from % width), y = int32_t(from / width);
			uint32_t rock = 0;
			for (int32_t step = 1; step <= 48; ++step) {
				x += dx;
				y += dy;
				if (x < 0 || y < 0 || x >= int32_t(width) || y >= int32_t(height)) break;
				const size_t cell = size_t(y) * width + size_t(x);
				if (kinds[cell] == SurfaceKind::Rock) { ++rock; continue; }
				if (rock == 0) {
					if (kinds[cell] == SurfaceKind::Lake || kinds[cell] == SurfaceKind::River) break;
					continue;
				}
				if (rock < 3 || rock > 30 || !land(cell)) break;
				size_t goalCell = cell;
				for (int32_t beyond = 1; beyond <= 4; ++beyond) {
					const int32_t bx = x + dx * beyond, by = y + dy * beyond;
					if (bx < 0 || by < 0 || bx >= int32_t(width) || by >= int32_t(height) || !land(size_t(by) * width + size_t(bx))) break;
					goalCell = size_t(by) * width + size_t(bx);
				}
				return goalCell;
			}
		}
		return count;
	};
	for (uint32_t attempt = 0; attempt < 200 && placed < target; ++attempt) {
		if (stop.stop_requested()) return false;
		size_t start = count, goal = count;
		if (config.tuning.rockPasses && attempt % 2 == 0) {
			// Up to 64 random land cells are tried as the start of a trail that crosses a rock band.
			for (uint32_t tries = 0; tries < 64 && goal == count; ++tries) {
				const size_t candidate = size_t(random.bounded(count));
				if (!land(candidate)) continue;
				goal = crossingGoal(candidate);
				if (goal != count) start = candidate;
			}
		}
		for (uint32_t tries = 0; tries < 64 && start == count; ++tries) {
			const size_t candidate = size_t(random.bounded(count));
			if (land(candidate)) start = candidate;
		}
		if (start == count) continue;
		const int32_t sx = int32_t(start % width), sy = int32_t(start / width);
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
		placed += router.connect(start, goal, nullptr, config.tuning.rockPasses != 0);
	}
	return true;
}

// Optional building foundations (G4 iterations 11 and 12): rectangles of Floor, 7-11 by 5-9 cells in either orientation,
// each with a margin of two cells that must be Grass or Dirt, so a foundation never touches water, rock, a beach, a path
// or another one. 60 % of the attempts start a cluster at a random place; the rest put a building 3 to 6 cells from the
// side of one that is already there. Returns the foundations in placement order.
struct Foundation {
	uint32_t x0, y0, x1, y1;  // inclusive
};

constexpr uint32_t clusterAnchorPercent = 60;
constexpr uint32_t clusterGapMin = 3, clusterGapSpread = 4;  // gap 3..6
constexpr uint32_t doorTrailReach = 16;                       // a door trail is routed only to a trail this close (Chebyshev, from the building centre)
constexpr uint32_t doorTargetReach = 60;                      // a door looks for a trail this close (Chebyshev, from the building centre)

constexpr uint64_t ruinStream = 0x7275696e5f733235ULL;  // "ruin_s25"
constexpr uint64_t campStream = 0x63616d705f733235ULL;  // "camp_s25"

// Ruin (iteration 25): cuts gaps (at least 2, about a quarter of the outline without the corners) into the wall of a foundation and
// records one interior site; the foundation gets no door.
void cutRuinGaps(const Foundation& f, uint32_t width, StableRandom& random, std::vector<uint32_t>& gaps, std::vector<uint32_t>& sites) {
	std::vector<uint32_t> outline;
	for (uint32_t x = f.x0 + 1; x < f.x1; ++x) { outline.push_back(f.y0 * width + x); outline.push_back(f.y1 * width + x); }
	for (uint32_t y = f.y0 + 1; y < f.y1; ++y) { outline.push_back(y * width + f.x0); outline.push_back(y * width + f.x1); }
	const size_t wanted = std::max<size_t>(2, outline.size() / 4);
	std::vector<uint8_t> used(outline.size(), 0);
	for (size_t chosen = 0; chosen < wanted;) {
		const size_t pick = size_t(random.bounded(outline.size()));
		if (used[pick]) continue;
		used[pick] = 1;
		gaps.push_back(outline[pick]);
		++chosen;
	}
	sites.push_back(((f.y0 + f.y1) / 2) * width + (f.x0 + f.x1) / 2);
}

// Ruin floor: only ruinFloorPercent of the ruin's tiles keep the floor, in small patches and single tiles (the tiles with
// the highest value noise of a 2-cell lattice); the rest is drawn with the land ground, under the walls as well. Loose
// floor tiles lie scattered on the Grass or Dirt up to two tiles beyond the outline, on a quarter of that share.
void bareRuinFloor(const Foundation& f, const std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, const SurfaceConfig& config,
	std::vector<uint32_t>& bare, std::vector<uint32_t>& strewn) {
	constexpr uint32_t apron = 2;
	std::vector<std::pair<uint32_t, uint32_t>> cells; // noise, cell
	for (uint32_t y = f.y0; y <= f.y1; ++y)
		for (uint32_t x = f.x0; x <= f.x1; ++x)
			cells.emplace_back(valueNoise(config.seed, ruinStream, uint32_t(config.region.x) + x, uint32_t(config.region.y) + y, 2), y * width + x);
	std::ranges::sort(cells);
	const size_t keep = cells.size() * config.tuning.ruinFloorPercent / 100;
	for (size_t k = 0; k + keep < cells.size(); ++k) bare.push_back(cells[k].second);
	const uint32_t loose = config.tuning.ruinFloorPercent * 65536 / 400; // of the 16-bit lattice
	for (uint32_t y = f.y0 >= apron ? f.y0 - apron : 0; y <= std::min(f.y1 + apron, height - 1); ++y)
		for (uint32_t x = f.x0 >= apron ? f.x0 - apron : 0; x <= std::min(f.x1 + apron, width - 1); ++x) {
			const size_t cell = size_t(y) * width + x;
			if ((x >= f.x0 && x <= f.x1 && y >= f.y0 && y <= f.y1) || (kinds[cell] != SurfaceKind::Grass && kinds[cell] != SurfaceKind::Dirt)) continue;
			if (lattice(config.seed, ruinStream ^ 0x4c, uint32_t(config.region.x) + x, uint32_t(config.region.y) + y) < loose)
				strewn.push_back(uint32_t(cell));
		}
}

// Camps (iterations 25 and 26): a clearing (Path disc, dx^2 + dy^2 <= 18) with a fire in the middle (a Path cell, SurfaceGrid::fires) and an open tent
// floor (Floor 3 x 3, centre 3 cells east of the middle, whose whole outline is a gap) on ground that is Grass or Dirt within 7 cells and has
// no Floor within 12 cells; centres are at least 12 apart.
bool placeCamps(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, const SurfaceConfig& config,
	std::vector<uint32_t>& gaps, std::vector<uint32_t>& sites, std::vector<uint32_t>& fires, std::stop_token stop) {
	const uint64_t wanted = uint64_t(config.tuning.campCount) * kinds.size() / 65536;
	if (width < 17 || height < 17) return true;
	StableRandom random(config.seed, campStream);
	std::vector<std::pair<int32_t, int32_t>> centres;
	for (uint64_t attempt = 0; attempt < wanted * 60 + 60 && centres.size() < wanted; ++attempt) {
		if (attempt % 64 == 0 && stop.stop_requested()) return false;
		const int32_t cx = 8 + int32_t(random.bounded(width - 16)), cy = 8 + int32_t(random.bounded(height - 16));
		bool free = true;
		for (const auto& [ox, oy] : centres) if (std::max(std::abs(ox - cx), std::abs(oy - cy)) < 12) { free = false; break; }
		for (int32_t dy = -12; dy <= 12 && free; ++dy) {
			for (int32_t dx = -12; dx <= 12; ++dx) {
				const int32_t x = cx + dx, y = cy + dy;
				if (x < 0 || y < 0 || x >= int32_t(width) || y >= int32_t(height)) continue;
				const SurfaceKind kind = kinds[size_t(y) * width + size_t(x)];
				if (kind == SurfaceKind::Floor) { free = false; break; }
				if (std::abs(dx) <= 7 && std::abs(dy) <= 7 && kind != SurfaceKind::Grass && kind != SurfaceKind::Dirt) { free = false; break; }
			}
		}
		if (!free) continue;
		centres.emplace_back(cx, cy);
		for (int32_t dy = -4; dy <= 4; ++dy) for (int32_t dx = -4; dx <= 4; ++dx) if (dx * dx + dy * dy <= 18) kinds[size_t(cy + dy) * width + size_t(cx + dx)] = SurfaceKind::Path;
		fires.push_back(uint32_t(size_t(cy) * width + size_t(cx)));
		const int32_t tentX = cx + 3;
		for (int32_t dy = -1; dy <= 1; ++dy) {
			for (int32_t dx = -1; dx <= 1; ++dx) {
				const size_t cell = size_t(cy + dy) * width + size_t(tentX + dx);
				kinds[cell] = SurfaceKind::Floor;
				if (dx || dy) gaps.push_back(uint32_t(cell));
			}
		}
		sites.push_back(uint32_t(size_t(cy) * width + size_t(tentX)));
	}
	return true;
}


std::vector<Foundation> placeBuildings(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, const SurfaceConfig& config, std::stop_token stop) {
	std::vector<Foundation> placed;
	const uint64_t wanted = uint64_t(config.tuning.buildingDensity) * kinds.size() / 65536;
	StableRandom random(config.seed, buildingStream);
	for (uint64_t attempt = 0; attempt < wanted * 40 + 40 && placed.size() < wanted; ++attempt) {
		if (attempt % 64 == 0 && stop.stop_requested()) return placed;
		uint32_t longSide = 7 + uint32_t(random.bounded(5)), shortSide = 5 + uint32_t(random.bounded(5));
		shortSide = std::min(shortSide, longSide);
		if (random.bounded(2)) std::swap(longSide, shortSide);
		if (longSide + 4 > width || shortSide + 4 > height) continue;
		int64_t x0, y0;
		// A compound room is set wall to wall beside an existing building (gap 1), and the two Floor areas join into one hall.
		// Its roll is drawn only when compounds are on, so other styles place exactly as before.
		const bool compound = config.tuning.compoundPercent && !placed.empty() && random.bounded(100) < config.tuning.compoundPercent;
		if (!compound && (placed.empty() || random.bounded(100) < clusterAnchorPercent)) {
			x0 = int64_t(random.bounded(width - (longSide + 4) + 1)) + 2;
			y0 = int64_t(random.bounded(height - (shortSide + 4) + 1)) + 2;
		} else {
			const Foundation& other = placed[size_t(random.bounded(placed.size()))];
			const int64_t side = int64_t(random.bounded(4)), gap = compound ? 1 : clusterGapMin + int64_t(random.bounded(clusterGapSpread));
			const int64_t otherW = int64_t(other.x1) - other.x0 + 1, otherH = int64_t(other.y1) - other.y0 + 1;
			const int64_t alongX = int64_t(other.x0) - (int64_t(longSide) - 1) + int64_t(random.bounded(uint64_t(otherW + longSide - 1)));
			const int64_t alongY = int64_t(other.y0) - (int64_t(shortSide) - 1) + int64_t(random.bounded(uint64_t(otherH + shortSide - 1)));
			switch (side) {
				case 0: x0 = alongX; y0 = int64_t(other.y0) - gap - (int64_t(shortSide) - 1); break;  // north
				case 1: x0 = int64_t(other.x1) + gap; y0 = alongY; break;                              // east
				case 2: x0 = alongX; y0 = int64_t(other.y1) + gap; break;                              // south
				default: x0 = int64_t(other.x0) - gap - (int64_t(longSide) - 1); y0 = alongY; break;   // west
			}
		}
		if (x0 < 2 || y0 < 2 || x0 + longSide + 2 > width || y0 + shortSide + 2 > height) continue;
		bool free = true;
		for (int64_t y = y0 - 2; y < y0 + shortSide + 2 && free; ++y) {
			for (int64_t x = x0 - 2; x < x0 + longSide + 2; ++x) {
				const SurfaceKind kind = kinds[size_t(y) * width + size_t(x)];
				// A compound room may border Floor in its margin, never cover it.
				const bool inside = x >= x0 && y >= y0 && x < x0 + longSide && y < y0 + shortSide;
				if (kind != SurfaceKind::Grass && kind != SurfaceKind::Dirt && !(compound && !inside && kind == SurfaceKind::Floor)) { free = false; break; }
			}
		}
		if (!free) continue;
		for (int64_t y = y0; y < y0 + shortSide; ++y) for (int64_t x = x0; x < x0 + longSide; ++x) kinds[size_t(y) * width + size_t(x)] = SurfaceKind::Floor;
		placed.push_back({uint32_t(x0), uint32_t(y0), uint32_t(x0 + longSide - 1), uint32_t(y0 + shortSide - 1)});
	}
	return placed;
}

// One door per foundation on the middle cell of the outline side that faces the nearest trail (or a random side when none
// is within doorTargetReach, else the nearest other building); the cell in front of the door becomes Path and is routed to that trail. Buildings are taken in
// placement order, so each new one joins the trails the earlier ones made. Returns false when cancelled.
bool placeDoorsAndTrails(std::vector<SurfaceKind>& kinds, uint32_t width, uint32_t height, const SurfaceConfig& config,
	std::span<const Foundation> foundations, std::vector<uint32_t>& doors, std::stop_token stop, std::vector<uint8_t>* crossings) {
	PathRouter router(kinds, width, height, config, crossings);
	StableRandom random(config.seed, doorStream);
	std::vector<uint32_t> trail;
	for (size_t i = 0; i < kinds.size(); ++i) if (kinds[i] == SurfaceKind::Path) trail.push_back(uint32_t(i));
	for (size_t index = 0; index < foundations.size(); ++index) {
		const Foundation& f = foundations[index];
		if (stop.stop_requested()) return false;
		const int64_t centreX2 = int64_t(f.x0) + f.x1, centreY2 = int64_t(f.y0) + f.y1;  // doubled centre
		int64_t bestDistance = int64_t(doorTargetReach) + 1;
		size_t target = kinds.size();
		for (const uint32_t cell : trail) {
			const int64_t dx = std::abs((int64_t(cell % width) * 2 - centreX2)) / 2, dy = std::abs((int64_t(cell / width) * 2 - centreY2)) / 2;
			const int64_t distance = std::max(dx, dy);
			if (distance < bestDistance) { bestDistance = distance; target = cell; }
		}
		int side;  // 0 north, 1 east, 2 south, 3 west
		bool pendingPartner = false;  // a later building within doorTrailReach will route to this door
		if (target != kinds.size()) {
			const int64_t dx = int64_t(target % width) * 2 - centreX2, dy = int64_t(target / width) * 2 - centreY2;
			side = std::abs(dx) >= std::abs(dy) ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0);
		} else {
			// No trail yet: face the partner that will join this door (the nearest later building within doorTrailReach),
			// else the nearest other building, so neighbours look at each other.
			int64_t nearest = int64_t(doorTargetReach) + 1, faceX2 = 0, faceY2 = 0, laterNearest = int64_t(doorTrailReach) + 1, laterX2 = 0, laterY2 = 0;
			for (size_t other = 0; other < foundations.size(); ++other) {
				if (other == index) continue;
				const int64_t ox2 = int64_t(foundations[other].x0) + foundations[other].x1, oy2 = int64_t(foundations[other].y0) + foundations[other].y1;
				const int64_t distance = std::max(std::abs(ox2 - centreX2), std::abs(oy2 - centreY2)) / 2;
				if (distance < nearest) { nearest = distance; faceX2 = ox2 - centreX2; faceY2 = oy2 - centreY2; }
				if (other > index && distance < laterNearest) { laterNearest = distance; laterX2 = ox2 - centreX2; laterY2 = oy2 - centreY2; pendingPartner = true; }
			}
			if (pendingPartner) { faceX2 = laterX2; faceY2 = laterY2; nearest = laterNearest; }
			if (nearest <= int64_t(doorTargetReach)) side = std::abs(faceX2) >= std::abs(faceY2) ? (faceX2 > 0 ? 1 : 3) : (faceY2 > 0 ? 2 : 0);
			else side = int(random.bounded(4));
		}
		const uint32_t midX = f.x0 + (f.x1 - f.x0) / 2, midY = f.y0 + (f.y1 - f.y0) / 2;
		// In a compound hall a side may face another room: the door takes the next side round whose front is not Floor.
		const auto front = [&](int s) {
			const size_t x = s == 1 ? f.x1 + 1 : s == 3 ? f.x0 - 1 : midX, y = s == 0 ? f.y0 - 1 : s == 2 ? f.y1 + 1 : midY;
			return kinds[y * width + x];
		};
		for (int turn = 0; turn < 4 && front(side) == SurfaceKind::Floor; ++turn) side = (side + 1) % 4;
		uint32_t doorX = midX, doorY = f.y0, stoopX = midX, stoopY = f.y0 - 1;
		if (side == 1) { doorX = f.x1; doorY = midY; stoopX = f.x1 + 1; stoopY = midY; }
		else if (side == 2) { doorY = f.y1; stoopY = f.y1 + 1; }
		else if (side == 3) { doorX = f.x0; doorY = midY; stoopX = f.x0 - 1; stoopY = midY; }
		doors.push_back(doorY * width + doorX);
		const size_t stoop = size_t(stoopY) * width + stoopX;
		const bool routed = target != kinds.size() && bestDistance <= int64_t(doorTrailReach);
		for (size_t other = index + 1; !routed && !pendingPartner && other < foundations.size(); ++other) {
			const int64_t ox2 = int64_t(foundations[other].x0) + foundations[other].x1, oy2 = int64_t(foundations[other].y0) + foundations[other].y1;
			pendingPartner = std::max(std::abs(ox2 - centreX2), std::abs(oy2 - centreY2)) / 2 <= int64_t(doorTrailReach);
		}
		// A step in front of the door only where something joins it: a trail in reach or a later building that will.
		if ((routed || pendingPartner) && (kinds[stoop] == SurfaceKind::Grass || kinds[stoop] == SurfaceKind::Dirt)) {
			kinds[stoop] = SurfaceKind::Path;
			trail.push_back(uint32_t(stoop));
		}
		if (routed) router.connect(stoop, target, &trail, true);
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
// Every cell must lie in a 2 x 2 block of its own value (outside the rectangle counts as either): flagged cells that
// do not are cleared, then unflagged ones that do not are set. One-cell spurs, strips and channels go, which the
// border brushes cannot draw cleanly. Two passes.
bool keepBlocks(std::vector<uint8_t>& flag, uint32_t width, uint32_t height, const std::stop_token& stop) {
	const auto inBlock = [&](size_t i, uint8_t value) {
		const int64_t x = int64_t(i % width), y = int64_t(i / width);
		const auto same = [&](int64_t cx, int64_t cy) {
			return cx < 0 || cy < 0 || cx >= int64_t(width) || cy >= int64_t(height) || flag[size_t(cy) * width + size_t(cx)] == value;
		};
		for (const auto [dx, dy] : {std::pair{-1, -1}, {0, -1}, {-1, 0}, {0, 0}})
			if (same(x + dx, y + dy) && same(x + dx + 1, y + dy) && same(x + dx, y + dy + 1) && same(x + dx + 1, y + dy + 1)) return true;
		return false;
	};
	std::vector<size_t> change;
	for (int pass = 0; pass < 2; ++pass) {
		for (const uint8_t value : {uint8_t(1), uint8_t(0)}) {
			if (stop.stop_requested()) return false;
			change.clear();
			for (size_t i = 0; i < flag.size(); ++i) if (flag[i] == value && !inBlock(i, value)) change.push_back(i);
			for (const size_t i : change) flag[i] = uint8_t(1 - value);
		}
	}
	return true;
}

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

bool caveEntranceAllowed(std::span<const SurfaceKind> cells, uint32_t width, uint32_t height, uint32_t e, CaveEntranceStyle style, bool relaxedRockFace) noexcept {
	if (!width || uint64_t(width) * height != cells.size() || e >= cells.size() || style > CaveEntranceStyle::FloorHole) return false;
	const auto x = e % width, y = e / width;
	if (x < 3 || y < 3 || uint64_t(x) + 3 >= width || uint64_t(y) + 3 >= height) return false;
	const auto open = [](SurfaceKind k) { return k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Shore || k == SurfaceKind::Path; };
	if (style == CaveEntranceStyle::RockFace) {
		const auto rockSides = int(cells[e - width] == SurfaceKind::Rock) + int(cells[e - 1] == SurfaceKind::Rock) + int(cells[e + 1] == SurfaceKind::Rock);
		return cells[e] == SurfaceKind::Rock && open(cells[e + width]) && rockSides >= (relaxedRockFace ? 2 : 3);
	} else {
		if ((cells[e] != SurfaceKind::Grass && cells[e] != SurfaceKind::Dirt) || !open(cells[e - 1]) ||
			!open(cells[e + 1]) || !open(cells[e - width]) || !open(cells[e + width])) return false;
	}
	for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) {
		const auto k = cells[size_t(int64_t(y) + dy) * width + size_t(int64_t(x) + dx)];
		if (k == SurfaceKind::Lake || k == SurfaceKind::River || k == SurfaceKind::Floor) return false;
	}
	return true;
}

bool caveEntranceFits(std::span<const SurfaceKind> cells, uint32_t width, uint32_t height, uint32_t e, CaveEntranceStyle style, bool relaxedRockFace) noexcept {
	return caveEntranceAllowed(cells, width, height, e, style, relaxedRockFace) ||
		(style == CaveEntranceStyle::RockFace && caveEntranceAllowed(cells, width, height, e, CaveEntranceStyle::FloorHole));
}

std::expected<void, SurfaceError> reselectCaveEntrances(SurfaceGrid& grid, const SurfaceConfig& config, std::stop_token stop) try {
    if (!grid.valid()) return std::unexpected(SurfaceError::InvalidRegion);
    grid.entrances.clear();
    if (!placeEntrances(grid.cells, grid.region.width, grid.region.height, config, grid.entrances, stop))
        return std::unexpected(SurfaceError::Cancelled);
    if (config.tuning.rockBlocks) {
        if (const auto kept = keepStartLandPois(grid, stop); !kept)
            return std::unexpected(kept.error() == ReachabilityError::Cancelled ? SurfaceError::Cancelled : SurfaceError::AllocationFailed);
    }
    // An island that carried an entrance before the reselection may have none now (iteration 48).
    return normalizeSandIslands(grid, stop);
} catch (const std::bad_alloc&) { return std::unexpected(SurfaceError::AllocationFailed); }

bool SurfaceGrid::valid() const noexcept {
	if (region.x < 0 || region.y < 0 || region.z < 0 || region.width == 0 || region.height == 0 ||
		uint64_t(region.width) * region.height != cells.size()) return false;
	if (!std::ranges::all_of(doors, [&](uint32_t door) { return door < cells.size() && cells[door] == SurfaceKind::Floor; })) return false;
	if (!std::ranges::all_of(gaps, [&](uint32_t gap) { return gap < cells.size() && cells[gap] == SurfaceKind::Floor; })) return false;
	if (!std::ranges::all_of(sites, [&](uint32_t site) { return site < cells.size() && cells[site] == SurfaceKind::Floor; })) return false;
	// Bare cells may outlive their floor (a repair can clear a building); only Floor cells among them count.
	if (!std::ranges::all_of(bare, [&](uint32_t cell) { return cell < cells.size(); })) return false;
	if (!std::ranges::all_of(strewn, [&](uint32_t cell) { return cell < cells.size(); })) return false;
	if (!std::ranges::all_of(fires, [&](uint32_t fire) { return fire < cells.size() && cells[fire] == SurfaceKind::Path; })) return false;
	if (!std::ranges::all_of(entrances, [&](uint32_t entrance) { return entrance < cells.size() && (cells[entrance] == SurfaceKind::Rock || cells[entrance] == SurfaceKind::Grass || cells[entrance] == SurfaceKind::Dirt); })) return false;
    if (!bridgeAxes.empty()) {
        if (bridgeAxes.size() != cells.size()) return false;
        for (size_t i = 0; i < cells.size(); ++i) if (bridgeAxes[i] > BridgeAxis::Vertical ||
            ((bridgeAxes[i] != BridgeAxis::None) != (cells[i] == SurfaceKind::Bridge))) return false;
    }
	return std::ranges::all_of(cells, [](SurfaceKind kind) { return kind <= SurfaceKind::Bridge; });
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
    if (!bridgeAxes.empty()) { byte('B'); for (const auto axis : bridgeAxes) byte(uint8_t(axis)); }
	if (!doors.empty()) {
		byte('D');
		word(static_cast<uint32_t>(doors.size()));
		for (const uint32_t door : doors) word(door);
	}
	if (!bare.empty()) {
		byte('R');
		word(static_cast<uint32_t>(bare.size()));
		for (const uint32_t cell : bare) word(cell);
	}
	if (!strewn.empty()) {
		byte('L');
		word(static_cast<uint32_t>(strewn.size()));
		for (const uint32_t cell : strewn) word(cell);
	}
	if (!gaps.empty()) {
		byte('G');
		word(static_cast<uint32_t>(gaps.size()));
		for (const uint32_t gap : gaps) word(gap);
	}
	if (!sites.empty()) {
		byte('T');
		word(static_cast<uint32_t>(sites.size()));
		for (const uint32_t site : sites) word(site);
	}
	if (!fires.empty()) {
		byte('R');
		word(static_cast<uint32_t>(fires.size()));
		for (const uint32_t fire : fires) word(fire);
	}
	if (!entrances.empty()) {
		byte('E');
		word(static_cast<uint32_t>(entrances.size()));
		for (const uint32_t entrance : entrances) word(entrance);
	}
	return hash;
}

std::expected<SurfaceLayout, SurfaceError> validateSurface(
	const SurfaceConfig& config, MapExtent extent, SurfaceBudget budget,
	std::span<const uint8_t> mask, std::span<const uint8_t> protection, std::stop_token stop) {
    if (config.tuning.featureSize < 1 || config.tuning.featureSize > 4096 || config.tuning.lakePermille > 500
        || config.tuning.rockPermille > 500 || config.tuning.riverCount > 1024 || config.tuning.shoreWidth > 8
        || config.tuning.pathLength > 400 || config.tuning.buildingDensity > 100 || config.tuning.fordLength > 256
        || config.tuning.rockPasses > 1 || config.tuning.shoreSmoothing > 4 || config.tuning.shoreRadius > 8
        || config.tuning.repairReach > 1 || config.tuning.ruinPercent > 100 || config.tuning.campCount > 50
        || config.tuning.caveEntrances > 20 || config.tuning.plateauLevels < 1 || config.tuning.plateauLevels > 3
        || config.tuning.plateauMaxPercent > 100 || config.tuning.passes > 8 || config.tuning.upperFloorPercent > 100
        || config.tuning.dungeonPercent > 100
        || (config.tuning.plateauMinTop && (config.tuning.plateauMinTop < 16 || config.tuning.plateauMinTop > 4096))
        || config.tuning.caveEntranceStyle > CaveEntranceStyle::FloorHole || config.tuning.dirtPermille > 1000
        || config.tuning.transitionWidth > 32768 || config.tuning.meander > 8
        || config.tuning.ruinFloorPercent > 100 || config.tuning.compoundPercent > 100
        || config.tuning.islandPermille > 900 || config.tuning.dirtSize > 4096 || config.tuning.cleanEdges > 1) {
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
	const uint64_t working = count * (workingBytesPerCell + (config.tuning.fordLength || config.tuning.repairReach ? fordBytesPerCell : 0)) + uint64_t(noiseBins) * sizeof(uint32_t);
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
					config.tuning.dirtSize ? config.tuning.dirtSize : std::min<uint32_t>(config.tuning.featureSize * 2, 8192));
			}
		}

		// Lakes (lowest cells) and rock (highest cells), then drop patches that are too small.
		const uint32_t rockLevel = config.tuning.rockPermille ? lowCut(elevation, 1000 - config.tuning.rockPermille) : noiseBins;
		std::vector<uint8_t> lake(count), rock(count);
		const auto makePonds = [&](uint32_t permille) {
			const uint32_t lakeLevel = permille ? lowCut(elevation, permille) : 0;
			for (size_t i = 0; i < count; ++i) lake[i] = elevation[i] < lakeLevel;
			return dropSmall(lake, width, height, config.tuning.minLakeCells, stop) &&
				(!config.tuning.cleanEdges || keepBlocks(lake, width, height, stop));
		};
		if (!makePonds(config.tuning.lakePermille)) return cancelled();
		if (config.tuning.cleanEdges && config.tuning.lakePermille) {
			// Cleaning drops the smallest ponds; one second cut at a raised share brings the water back to the target.
			const uint64_t wanted = uint64_t(count) * config.tuning.lakePermille / 1000;
			const uint64_t made = uint64_t(std::ranges::count(lake, uint8_t(1)));
			if (made && made < wanted && !makePonds(uint32_t(std::min<uint64_t>(500, config.tuning.lakePermille * wanted / made))))
				return cancelled();
		}
		if (config.tuning.islandPermille) {
			// Open sea: the cells nearest the rectangle edge, the coastline wobbled by its own noise at a sixth of the
			// island, so bays and capes keep their size whatever the feature size of the ponds.
			const uint32_t half = std::max<uint32_t>(1, std::min(width, height) / 2);
			const uint32_t coastStep = std::max<uint32_t>(8, half / 3);
			std::vector<uint16_t> coast(count);
			for (size_t i = 0; i < count; ++i) {
				const uint32_t x = uint32_t(i % width), y = uint32_t(i / width);
				const uint32_t edge = std::min({x, width - 1 - x, y, height - 1 - y});
				const uint32_t inland = uint32_t(std::min<uint64_t>(65535, uint64_t(edge) * 65535 / half));
				const uint32_t wobble = fractalNoise(config.seed, coastStream, uint32_t(config.region.x) + x, uint32_t(config.region.y) + y, coastStep, 3);
				coast[i] = uint16_t((inland + wobble) / 2);
			}
			const uint32_t seaLevel = lowCut(coast, config.tuning.islandPermille);
			std::vector<uint8_t> sea(count);
			for (size_t i = 0; i < count; ++i) sea[i] = coast[i] < seaLevel;
			// The fine noise of a small feature size leaves one-tile channels and spurs along the coast;
			// a 3 x 3 majority vote rounds them off. Only the sea: the narrow channels and ponds of the lakes stay.
			std::vector<uint8_t> next(count);
			for (int pass = 0; pass < 2; ++pass) {
				if (stop.stop_requested()) return cancelled();
				for (size_t i = 0; i < count; ++i) {
					const uint32_t x = uint32_t(i % width), y = uint32_t(i / width);
					uint32_t water = 0, cells = 0;
					for (uint32_t ny = y ? y - 1 : 0; ny <= std::min(y + 1, height - 1); ++ny)
						for (uint32_t nx = x ? x - 1 : 0; nx <= std::min(x + 1, width - 1); ++nx) {
							++cells;
							water += sea[size_t(ny) * width + nx];
						}
					next[i] = water * 2 > cells;
				}
				sea.swap(next);
			}
			for (size_t i = 0; i < count; ++i) lake[i] = lake[i] || sea[i];
		}
		if (config.tuning.cleanEdges && !keepBlocks(lake, width, height, stop)) return cancelled();
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
				if (route.empty() || route.size() < config.tuning.minRiverLength) continue;
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
		std::vector<uint8_t> dirt(count, 0), grass(count, 0);
		// A learned dirt scale keeps patches down to about a quarter of its noise cell; the built-in one keeps 24.
		const uint32_t smallestPatch = config.tuning.dirtSize
			? std::min<uint32_t>(minPatchCells, config.tuning.dirtSize * config.tuning.dirtSize / 4 + 1) : minPatchCells;
		const auto makeDirt = [&](uint32_t permille) {
			const uint32_t dirtLevel = lowCut(moisture, permille);
			for (size_t i = 0; i < count; ++i) {
				if (i % 4096 == 0 && stop.stop_requested()) return false;
				dirt[i] = 0;
				if (lake[i] || rock[i]) continue;
				const uint32_t ax = uint32_t(config.region.x) + uint32_t(i % width);
				const uint32_t ay = uint32_t(config.region.y) + uint32_t(i / width);
				const int64_t offset = (int64_t(valueNoise(config.seed, ditherStream, ax, ay, transitionStep)) - noiseBins / 2) *
					int64_t(config.tuning.transitionWidth) / (noiseBins / 2);
				dirt[i] = int64_t(moisture[i]) + offset < int64_t(dirtLevel);
			}
			if (!dropSmall(dirt, width, height, smallestPatch, stop)) return false;
			if (config.tuning.cleanEdges) {
				// Lake and rock cells stay out of the dirt; the blocks are judged on the land only.
				if (!keepBlocks(dirt, width, height, stop)) return false;
				for (size_t i = 0; i < count; ++i) if (lake[i] || rock[i]) dirt[i] = 0;
			}
			return true;
		};
		if (!makeDirt(config.tuning.dirtPermille)) return cancelled();
		if (config.tuning.cleanEdges && config.tuning.dirtPermille) {
			// As for the ponds: one second cut at a raised share when cleaning took dirt below its share of the land.
			const uint64_t land = uint64_t(std::ranges::count_if(std::views::iota(size_t(0), size_t(count)), [&](size_t i) { return !lake[i] && !rock[i]; }));
			const uint64_t wanted = land * config.tuning.dirtPermille / 1000;
			const uint64_t made = uint64_t(std::ranges::count(dirt, uint8_t(1)));
			if (made && made < wanted && !makeDirt(uint32_t(std::min<uint64_t>(1000, config.tuning.dirtPermille * wanted / made))))
				return cancelled();
		}
		for (size_t i = 0; i < count; ++i) grass[i] = !lake[i] && !rock[i] && !dirt[i];
		if (!dropSmall(grass, width, height, smallestPatch, stop)) return cancelled();
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
		if (config.tuning.shoreSmoothing) smoothShores(kinds, width, height, config.tuning.shoreSmoothing, config.tuning.shoreRadius);
		grassRim(kinds, width, height, config);
		for (size_t i = 0; i < count; ++i) if (river[i]) kinds[i] = SurfaceKind::River;
		if (config.tuning.shoreSmoothing) trimSpits(kinds, width, height);
		std::vector<uint8_t> crossingMask(config.tuning.fordLength ? count : 0, 0);
		std::vector<uint8_t>* crossings = config.tuning.fordLength ? &crossingMask : nullptr;
		if (config.tuning.pathLength && !drawPaths(kinds, width, height, config, stop, crossings)) return cancelled();
		std::vector<uint32_t> doors, gaps, sites, fires, entrances, bare, strewn;
		if (config.tuning.buildingDensity) {
			const std::vector<Foundation> foundations = placeBuildings(kinds, width, height, config, stop);
			if (stop.stop_requested()) return cancelled();
			std::vector<Foundation> withDoors;
			if (config.tuning.ruinPercent) {
				// Ruins get gaps instead of a door and a trail; the others are handled as before.
				StableRandom ruinRandom(config.seed, ruinStream);
				for (const Foundation& foundation : foundations) {
					if (ruinRandom.bounded(100) < config.tuning.ruinPercent) {
						cutRuinGaps(foundation, width, ruinRandom, gaps, sites);
						if (config.tuning.ruinFloorPercent) bareRuinFloor(foundation, kinds, width, height, config, bare, strewn);
					} else withDoors.push_back(foundation);
				}
			}
			if (!placeDoorsAndTrails(kinds, width, height, config, config.tuning.ruinPercent ? std::span<const Foundation>(withDoors) : std::span<const Foundation>(foundations), doors, stop, crossings)) return cancelled();
		}
		if (config.tuning.campCount && !placeCamps(kinds, width, height, config, gaps, sites, fires, stop)) return cancelled();
		if (config.tuning.caveEntrances && !placeEntrances(kinds, width, height, config, entrances, stop)) return cancelled();
		// Walkability treats doors and gaps alike (openings in a wall).
		std::vector<uint32_t> openings = doors;
		openings.insert(openings.end(), gaps.begin(), gaps.end());
		if (config.tuning.fordLength) {
			if (const auto fords = placeFords(kinds, width, height, config.tuning.fordLength, openings, stop, FordShape {config.seed, config.region.x, config.region.y}); !fords) return std::unexpected(fords.error());
		}
		// repairReach is applied to the complete multi-floor plan, after decoration and caps.
		// Classify the complete semantic surface before applying the write mask,
		// preserving the rule that masking never changes another cell's kind.
		SurfaceGrid grid { config.region, std::move(kinds), std::move(doors), std::move(gaps), std::move(sites), std::move(fires), std::move(entrances) };
		grid.bare = std::move(bare);
		grid.strewn = std::move(strewn);
		const auto islands = normalizeSandIslands(grid, stop);
		if (!islands) return std::unexpected(islands.error());

		for (size_t i = 0; i < count; ++i) {
			if ((!mask.empty() && mask[i] == 0) || (!protection.empty() && protection[i] != 0)) {
				grid.cells[i] = SurfaceKind::Preserve;
			}
		}
		if (stop.stop_requested()) return cancelled();
		std::erase_if(grid.doors, [&](uint32_t door) { return grid.cells[door] != SurfaceKind::Floor; });
		std::erase_if(grid.gaps, [&](uint32_t gap) { return grid.cells[gap] != SurfaceKind::Floor; });
		std::erase_if(grid.bare, [&](uint32_t cell) { return grid.cells[cell] != SurfaceKind::Floor; });
		std::erase_if(grid.sites, [&](uint32_t site) { return grid.cells[site] != SurfaceKind::Floor; });
		std::erase_if(grid.fires, [&](uint32_t fire) { return grid.cells[fire] != SurfaceKind::Path; });
		std::erase_if(grid.entrances, [&](uint32_t entrance) { return !caveEntranceFits(grid.cells, width, height, entrance, config.tuning.caveEntranceStyle, config.tuning.repairReach != 0); });
		if (config.tuning.rockBlocks) {
			// Impassable rock splits the land: no POI where the start cannot walk (iteration 44).
			const auto removed = keepStartLandPois(grid, stop);
			if (!removed)
				return std::unexpected(removed.error() == ReachabilityError::Cancelled ? SurfaceError::Cancelled : SurfaceError::AllocationFailed);
			// An island kept only by a POI that is gone now is classified again (ordinary entrances, iteration 48).
			if (*removed) {
				if (const auto again = normalizeSandIslands(grid, stop); !again) return std::unexpected(again.error());
			}
		}
		return grid;
	} catch (const std::bad_alloc&) {
		return std::unexpected(SurfaceError::AllocationFailed);
	}
}

std::expected<SandIslandReport, SurfaceError> sandIslandsOf(const SurfaceGrid& grid, std::stop_token stop) try {
	if (stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
	if (!grid.valid()) return std::unexpected(SurfaceError::InvalidRegion);
	SandIslandReport result;
	if (grid.region.z != 7) return result;
	const auto width = grid.region.width, height = grid.region.height;
	const auto land = [&](size_t i) {
		return grid.cells[i] != SurfaceKind::Lake && grid.cells[i] != SurfaceKind::River && grid.cells[i] != SurfaceKind::Preserve;
	};
	std::vector<uint8_t> seen(grid.cells.size()), poi(grid.cells.size());
	for (const auto* points : {&grid.doors, &grid.sites, &grid.fires, &grid.entrances})
		for (const auto i : *points) if (i < poi.size()) poi[i] = 1;
	std::vector<uint32_t> component;
	size_t largest = 0, mainBegin = 0, mainEnd = 0;
	bool mainIncluded = false, mainNonShore = false;
	for (size_t first = 0; first < grid.cells.size(); ++first) {
		if (seen[first] || !land(first)) continue;
		component.assign(1, uint32_t(first));
		seen[first] = 1;
		bool enclosed = true, hasPoi = false, nonShore = false;
		for (size_t head = 0; head < component.size(); ++head) {
			if (head % 4096 == 0 && stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
			const auto i = component[head], x = i % width, y = i / width;
			enclosed &= x > 0 && y > 0 && x + 1 < width && y + 1 < height;
			hasPoi |= poi[i] != 0;
			nonShore |= grid.cells[i] != SurfaceKind::Shore;
			forNeighbours(i, width, height, [&](size_t next) {
				if (grid.cells[next] == SurfaceKind::Preserve) enclosed = false;
				if (seen[next] || !land(next)) return;
				seen[next] = 1;
				component.push_back(uint32_t(next));
			});
		}
		const bool newMain = component.size() > largest;
		if (newMain) {
			largest = component.size();
			mainBegin = result.cells.size();
			mainIncluded = enclosed && !hasPoi;
			mainNonShore = nonShore;
		}
		if (enclosed && !hasPoi) {
			++result.islands;
			result.nonShoreIslands += nonShore;
			result.cells.insert(result.cells.end(), component.begin(), component.end());
		}
		if (newMain) mainEnd = result.cells.size();
	}
	if (mainIncluded) {
		--result.islands;
		result.nonShoreIslands -= mainNonShore;
		result.cells.erase(result.cells.begin() + mainBegin, result.cells.begin() + mainEnd);
	}
	for (const auto i : result.cells) result.nonShoreCells += grid.cells[i] != SurfaceKind::Shore;
	return result;
} catch (const std::bad_alloc&) { return std::unexpected(SurfaceError::AllocationFailed); }

std::expected<void, SurfaceError> normalizeSandIslands(SurfaceGrid& grid, std::stop_token stop) {
	const auto islands = sandIslandsOf(grid, stop);
	if (!islands) return std::unexpected(islands.error());
	for (const auto i : islands->cells) {
		grid.cells[i] = SurfaceKind::Shore;
		if (!grid.bridgeAxes.empty()) grid.bridgeAxes[i] = BridgeAxis::None;
	}
	return {};
}

std::expected<ReachableSurface, SurfaceError> generateReachableSurface(
	const SurfaceConfig& config, MapExtent extent, SurfaceBudget budget, uint32_t maxAttempts,
	std::span<const uint8_t> mask, std::span<const uint8_t> protection, std::stop_token stop) {
	if (maxAttempts == 0) return std::unexpected(SurfaceError::InvalidParameters);
	std::optional<ReachableSurface> first;
	for (uint32_t attempt = 0; attempt < maxAttempts; ++attempt) {
		SurfaceConfig tried = config;
		tried.seed = config.seed + attempt;
		auto grid = generateSurface(tried, extent, budget, mask, protection, stop);
		if (!grid) return std::unexpected(grid.error());
		const auto poi = poiReachabilityOf(*grid, true, stop);
		if (!poi) return std::unexpected(poi.error() == ReachabilityError::Cancelled ? SurfaceError::Cancelled : SurfaceError::AllocationFailed);
		const bool all = poi->hasStart && poi->entrances == poi->entrancesReached && poi->exits == poi->exitsReached;
		if (all) return ReachableSurface {std::move(*grid), tried.seed, attempt + 1, true};
		if (!first) first = ReachableSurface {std::move(*grid), tried.seed, 1, false};
	}
	first->attempts = maxAttempts;
	return std::move(*first);
}

} // namespace rme::procedural
