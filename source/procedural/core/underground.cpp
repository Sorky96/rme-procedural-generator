// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/underground.h"

#include "procedural/core/reachability.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <new>

namespace rme::procedural {

namespace {

constexpr uint64_t tunnelStream = 0x63617665745f7333ULL;   // "cavet_s3"
constexpr uint64_t widthStream = 0x63617665775f7333ULL;    // "cavew_s3"
constexpr uint64_t cavernStream = 0x63617665725f7333ULL;   // "caver_s3"
constexpr uint32_t noiseBins = 65536;
// A joint of the tunnel axis must bend: its distance from the chord of its two neighbours is at least this many cells,
// so no straight piece of the axis spans two segments (iteration 47: longest straight piece <= 20 cells).
constexpr double minimumBend = 4.0;

struct Point {
	int32_t x, y;
	bool operator==(const Point&) const = default;
};

double distanceOf(Point a, Point b) { return std::hypot(double(b.x - a.x), double(b.y - a.y)); }

// Distance of (x, y) from the segment a..b.
double segmentDistance(Point a, Point b, double x, double y) {
	const double dx = b.x - a.x, dy = b.y - a.y, length = dx * dx + dy * dy;
	const double t = length == 0 ? 0 : std::clamp(((x - a.x) * dx + (y - a.y) * dy) / length, 0.0, 1.0);
	return std::hypot(a.x + dx * t - x, a.y + dy * t - y);
}

// Distance of m from the line through a and b (from a when a == b).
double deviationOf(Point a, Point b, Point m) {
	const double length = distanceOf(a, b);
	if (length == 0) return distanceOf(a, m);
	return std::abs(double(b.x - a.x) * (m.y - a.y) - double(b.y - a.y) * (m.x - a.x)) / length;
}

// The random stream of one tunnel, cavern or branch: the seed plus the absolute cells that define it.
StableRandom streamOf(uint64_t seed, uint64_t stream, Point a, Point b, int32_t x0, int32_t y0) {
	const auto word = [](int32_t value) { return uint64_t(uint16_t(value)); };
	return StableRandom(seed, stream ^ (word(a.x + x0) << 48 | word(a.y + y0) << 32 | word(b.x + x0) << 16 | word(b.y + y0)));
}

double unit(StableRandom& random) { return double(random.bounded(10001)) / 10000.0; }

struct Carver {
	std::vector<CaveKind>& cells;
	int32_t width, height;
	uint64_t seed;
	int32_t x0, y0;   // absolute origin of the area, for the noise and the random streams
	std::vector<uint32_t>* rooms = nullptr;
	CaveLayout* layout = nullptr;
	std::span<const uint8_t> blocked {};   // CaveGrid::blocked: cells never carved

	bool inside(int32_t x, int32_t y) const { return x >= 2 && y >= 2 && x <= width - 3 && y <= height - 3; }
	void put(int32_t x, int32_t y) {
		if (!inside(x, y)) return;
		const size_t i = size_t(y) * size_t(width) + size_t(x);
		if (blocked.empty() || !blocked[i]) cells[i] = CaveKind::Floor;
	}
	void disc(int32_t cx, int32_t cy, int32_t radiusSquared) {
		const int32_t reach = int32_t(std::sqrt(double(radiusSquared))) + 1;
		for (int32_t dy = -reach; dy <= reach; ++dy) for (int32_t dx = -reach; dx <= reach; ++dx) if (dx * dx + dy * dy <= radiusSquared) put(cx + dx, cy + dy);
	}
	// A square of side `size` (2..5) on the axis cell; sides 4 and 5 lose their corners.
	void stamp(int32_t x, int32_t y, int32_t size) {
		const int32_t low = -(size - 1) / 2, high = size / 2;
		for (int32_t dy = low; dy <= high; ++dy) for (int32_t dx = low; dx <= high; ++dx) {
			if (size >= 4 && (dx == low || dx == high) && (dy == low || dy == high)) continue;
			put(x + dx, y + dy);
		}
	}
	// Tunnel width 2..5 from smooth noise along the axis, stretched so that 2 and 5 occur as well as 3 and 4.
	int32_t widthAt(int32_t x, int32_t y) const {
		const int64_t value = surfaceValueNoise(seed, widthStream, uint32_t(x0 + x), uint32_t(y0 + y), 7);
		const int64_t stretched = std::clamp<int64_t>((value - noiseBins / 2) * 2 + noiseBins / 2, 0, noiseBins - 1);
		return 2 + int32_t(stretched * 4 / noiseBins);
	}
	// Waypoints keep 3 cells from the edge (2 in a tiny area).
	int32_t margin() const { return width >= 8 && height >= 8 ? 3 : 2; }
	bool roomy(double x, double y) const {
		return x >= margin() && y >= margin() && x <= width - 1 - margin() && y <= height - 1 - margin();
	}
	Point clamped(double x, double y) const {
		return {std::clamp(int32_t(std::lround(x)), margin(), std::max(margin(), width - 1 - margin())),
			std::clamp(int32_t(std::lround(y)), margin(), std::max(margin(), height - 1 - margin()))};
	}
	// The base point moved `offset` cells along (px, py), shortened in tenths until it keeps the margin, so that waypoints
	// never pile up along the edge.
	Point across(double bx, double by, double px, double py, double offset) const {
		for (int tenth = 10; tenth > 0; --tenth) {
			const double x = bx + px * offset * tenth / 10, y = by + py * offset * tenth / 10;
			if (roomy(x, y)) return clamped(x, y);
		}
		return clamped(bx, by);
	}

	// Stamps the polyline through `points` and returns its axis (one cell per step, consecutive cells 8-adjacent).
	std::vector<uint32_t> trace(std::span<const Point> points, int32_t maxWidth) {
		std::vector<uint32_t> axis;
		for (size_t k = 0; k + 1 < points.size(); ++k) {
			const Point a = points[k], b = points[k + 1];
			const int32_t steps = std::max(std::abs(b.x - a.x), std::abs(b.y - a.y));
			for (int32_t s = axis.empty() ? 0 : 1; s <= steps; ++s) {
				const int32_t x = steps ? a.x + int32_t(std::lround(double(b.x - a.x) * s / steps)) : a.x;
				const int32_t y = steps ? a.y + int32_t(std::lround(double(b.y - a.y) * s / steps)) : a.y;
				stamp(x, y, std::min(maxWidth, widthAt(x, y)));
				axis.push_back(uint32_t(y) * uint32_t(width) + uint32_t(x));
			}
		}
		if (layout) layout->axes.push_back(axis);
		return axis;
	}

	// Waypoints from a to b: segments of 7..10 cells around a bow whose height grows with the length (up to 28 cells),
	// a sideways wiggle per waypoint, and every joint bent by at least minimumBend.
	std::vector<Point> meander(Point a, Point b, StableRandom& random) const {
		const double length = distanceOf(a, b);
		std::vector<Point> points {a};
		if (length < 1) return points;
		const double segment = 7 + double(random.bounded(4));
		const int32_t count = std::max<int32_t>(1, int32_t(std::ceil(length / segment)));
		const double px = -double(b.y - a.y) / length, py = double(b.x - a.x) / length;
		const double mx = (a.x + b.x) / 2.0, my = (a.y + b.y) / 2.0;
		double bow = (random.bounded(2) ? 1.0 : -1.0) * std::min(length / 5, 28.0) * (0.5 + unit(random) / 2);
		// Bow away from the edge when only the other side has room.
		if (!roomy(mx + px * bow, my + py * bow) && roomy(mx - px * bow, my - py * bow)) bow = -bow;
		const double wiggle = std::clamp(length / 10, 2.0, 6.0);
		const auto bent = [&](Point previous, Point middle, Point next) { return deviationOf(previous, next, middle) >= minimumBend; };
		std::vector<double> lastBase;
		for (int32_t k = 1; k < count; ++k) {
			const double t = double(k) / count;
			const double offset = bow * 4 * t * (1 - t) + wiggle * (2 * unit(random) - 1);
			const double bx = a.x + (b.x - a.x) * t, by = a.y + (b.y - a.y) * t;
			Point chosen = across(bx, by, px, py, offset);
			if (points.size() >= 2) {
				// Push this waypoint sideways until the previous joint bends; keep the most bent try otherwise.
				double best = -1;
				for (const double shift : {0.0, 4.0, -4.0, 7.0, -7.0}) {
					const Point candidate = across(bx, by, px, py, offset + shift);
					const double bend = deviationOf(points[points.size() - 2], candidate, points.back());
					if (bend > best) { best = bend; chosen = candidate; }
					if (bent(points[points.size() - 2], points.back(), candidate)) { chosen = candidate; break; }
				}
			}
			if (!(chosen == points.back())) { points.push_back(chosen); lastBase = {bx, by, offset}; }
		}
		// The end is fixed: bend the last joint by moving the last waypoint instead.
		if (!lastBase.empty() && !bent(points[points.size() - 2], points.back(), b)) {
			double best = -1;
			for (const double shift : {4.0, -4.0, 7.0, -7.0}) {
				const Point candidate = across(lastBase[0], lastBase[1], px, py, lastBase[2] + shift);
				const double bend = deviationOf(points[points.size() - 2], b, candidate);
				if (bend > best) { best = bend; points.back() = candidate; }
				if (bend >= minimumBend) break;
			}
		}
		if (!(points.back() == b)) points.push_back(b);
		// Sideways jumps can stretch a segment: split every segment longer than 10 cells into pieces of up to 8, the new
		// joints 2.5 cells to alternate sides of it.
		std::vector<Point> split {points.front()};
		for (size_t k = 1; k < points.size(); ++k) {
			const Point from = points[k - 1], to = points[k];
			const double piece = distanceOf(from, to);
			if (piece > 10) {
				const int32_t parts = int32_t(std::ceil(piece / 8));
				const double sx = -double(to.y - from.y) / piece, sy = double(to.x - from.x) / piece;
				double side = random.bounded(2) ? 2.5 : -2.5;
				for (int32_t q = 1; q < parts; ++q, side = -side) {
					const double bx = from.x + double(to.x - from.x) * q / parts, by = from.y + double(to.y - from.y) * q / parts;
					const Point joint = roomy(bx + sx * side, by + sy * side) ? clamped(bx + sx * side, by + sy * side) : clamped(bx - sx * side, by - sy * side);
					if (!(joint == split.back())) split.push_back(joint);
				}
			}
			if (!(to == split.back())) split.push_back(to);
		}
		// A joint still closer than minimumBend to the chord of its neighbours moves away from it (to the
		// side it already leans to), twice over the whole line since a move changes the neighbouring joints.
		for (int pass = 0; pass < 2; ++pass) for (size_t k = 1; k + 1 < split.size(); ++k) {
			const Point before = split[k - 1], joint = split[k], after = split[k + 1];
			const double chord = distanceOf(before, after), bend = deviationOf(before, after, joint);
			if (chord == 0 || bend >= minimumBend) continue;
			const double cross = double(after.x - before.x) * (joint.y - before.y) - double(after.y - before.y) * (joint.x - before.x);
			const double lean = cross < 0 ? -1.0 : 1.0, push = minimumBend + 0.5 - bend;
			const double nx = -double(after.y - before.y) / chord * lean, ny = double(after.x - before.x) / chord * lean;
			const Point moved = roomy(joint.x + nx * push, joint.y + ny * push) ? clamped(joint.x + nx * push, joint.y + ny * push)
				: clamped(joint.x - nx * (push + 2 * bend), joint.y - ny * (push + 2 * bend));
			if (!(moved == before) && !(moved == after)) split[k] = moved;
		}
		return split;
	}

	// An irregular cavern at the axis cell (cx, cy): an ellipse (radii 4..12, its centre up to half a radius off the axis cell)
	// with a noisy edge, four rounds of a cellular automaton (Floor with >= 5 Floor among the 8 neighbours, or staying Floor
	// with >= 4), then only the part connected to the axis cell.
	void cavern(int32_t cx, int32_t cy, StableRandom& random) {
		const int32_t rx = 4 + int32_t(random.bounded(9)), ry = 4 + int32_t(random.bounded(9));
		const double ox = (2 * unit(random) - 1) * rx / 2, oy = (2 * unit(random) - 1) * ry / 2;
		const int32_t reach = std::max(rx, ry) + 2 + int32_t(std::ceil(std::max(std::abs(ox), std::abs(oy)))), side = 2 * reach + 1;
		std::vector<uint8_t> local(size_t(side) * size_t(side)), next(local.size());
		const auto at = [side](int32_t dx, int32_t dy) { return size_t(dy) * size_t(side) + size_t(dx); };
		for (int32_t dy = 0; dy < side; ++dy) for (int32_t dx = 0; dx < side; ++dx) {
			const double ex = (dx - reach - ox) / rx, ey = (dy - reach - oy) / ry;
			const double noise = double(surfaceValueNoise(seed, cavernStream, uint32_t(x0 + cx + dx - reach), uint32_t(y0 + cy + dy - reach), 3)) / noiseBins;
			local[at(dx, dy)] = ex * ex + ey * ey + (noise - 0.5) * 0.9 < 1.0;
		}
		for (int round = 0; round < 4; ++round) {
			for (int32_t dy = 0; dy < side; ++dy) for (int32_t dx = 0; dx < side; ++dx) {
				int32_t around = 0;
				for (int32_t ny = std::max(0, dy - 1); ny <= std::min(side - 1, dy + 1); ++ny)
					for (int32_t nx = std::max(0, dx - 1); nx <= std::min(side - 1, dx + 1); ++nx) around += (nx != dx || ny != dy) && local[at(nx, ny)];
				next[at(dx, dy)] = around >= 5 || (local[at(dx, dy)] && around >= 4);
			}
			local.swap(next);
		}
		for (int32_t dy = -1; dy <= 1; ++dy) for (int32_t dx = -1; dx <= 1; ++dx) local[at(reach + dx, reach + dy)] = 1;
		std::vector<uint32_t> queue {uint32_t(at(reach, reach))};
		local[queue.front()] = 2;
		for (size_t head = 0; head < queue.size(); ++head) {
			const int32_t dx = int32_t(queue[head] % uint32_t(side)), dy = int32_t(queue[head] / uint32_t(side));
			put(cx + dx - reach, cy + dy - reach);
			const auto visit = [&](int32_t nx, int32_t ny) {
				if (nx < 0 || ny < 0 || nx >= side || ny >= side || local[at(nx, ny)] != 1) return;
				local[at(nx, ny)] = 2;
				queue.push_back(uint32_t(at(nx, ny)));
			};
			visit(dx - 1, dy); visit(dx + 1, dy); visit(dx, dy - 1); visit(dx, dy + 1);
		}
		// The centre is Floor unless it is blocked (then the cavern records no room).
		if (rooms && inside(cx, cy) && cells[size_t(cy) * size_t(width) + size_t(cx)] == CaveKind::Floor) rooms->push_back(uint32_t(cy) * uint32_t(width) + uint32_t(cx));
	}

	// A dead end of 6..14 cells leaving the axis sideways at `from`, bent once, width 2..3, with a small widening at its end.
	void branch(const std::vector<uint32_t>& axis, size_t from, StableRandom& random) {
		const auto pointOf = [&](uint32_t cell) { return Point {int32_t(cell % uint32_t(width)), int32_t(cell / uint32_t(width))}; };
		const Point start = pointOf(axis[from]);
		const Point before = pointOf(axis[from >= 3 ? from - 3 : 0]), after = pointOf(axis[std::min(axis.size() - 1, from + 3)]);
		const double along = std::max(1.0, distanceOf(before, after));
		const double ux = (after.x - before.x) / along, uy = (after.y - before.y) / along;
		const double side = random.bounded(2) ? 1.0 : -1.0, length = 6 + double(random.bounded(9));
		const double lean = (2 * unit(random) - 1) * 0.6, turn = (random.bounded(2) ? 1.0 : -1.0) * (0.4 + 0.4 * unit(random));
		const double sx = -uy * side + ux * lean, sy = ux * side + uy * lean;
		const Point middle = clamped(start.x + sx * length / 2, start.y + sy * length / 2);
		const Point end = clamped(middle.x + (sx - sy * turn) * length / 2, middle.y + (sy + sx * turn) * length / 2);
		const Point points[] = {start, middle, end};
		trace(points, 3);
		disc(end.x, end.y, 2);
		if (layout) ++layout->branches;
	}

	// Tunnel from a to b (the same cells either way round): meandering axis, width 2..5, a cavern about every 36 cells of axis
	// (at least one from 16 cells on) and, from 24 cells on, 0..2 dead-end branches.
	void tunnel(Point a, Point b) {
		if (std::pair(b.y, b.x) < std::pair(a.y, a.x)) std::swap(a, b);
		StableRandom random = streamOf(seed, tunnelStream, a, b, x0, y0);
		const auto points = meander(a, b, random);
		if (points.size() == 1) { stamp(a.x, a.y, 3); return; }
		const auto axis = trace(points, 5);
		double length = 0;
		for (size_t k = 0; k + 1 < points.size(); ++k) length += distanceOf(points[k], points[k + 1]);
		if (length >= 16) {
			const int32_t caverns = std::max<int32_t>(1, int32_t(length + 18) / 36);
			for (int32_t c = 0; c < caverns; ++c) {
				const double t = (2 * c + 1 + (unit(random) - 0.5) * 0.6) / (2.0 * caverns);
				const auto cell = axis[std::min(axis.size() - 1, size_t(t * double(axis.size())))];
				cavern(int32_t(cell % uint32_t(width)), int32_t(cell / uint32_t(width)), random);
			}
		}
		if (length >= 24) {
			const auto branches = random.bounded(3);
			for (uint64_t k = 0; k < branches; ++k) branch(axis, size_t(double(axis.size()) * (0.25 + 0.5 * unit(random))), random);
		}
	}
};

} // namespace

bool CaveGrid::valid() const noexcept {
	return region.x >= 0 && region.y >= 0 && region.z >= 0 && region.width > 0 && region.height > 0 &&
		uint64_t(region.width) * region.height == cells.size() && (blocked.empty() || blocked.size() == cells.size()) &&
		std::ranges::all_of(cells, [](CaveKind kind) { return kind <= CaveKind::Floor; });
}

uint64_t CaveGrid::fingerprint() const {
	uint64_t hash = 14695981039346656037ULL;
	const auto byte = [&](uint8_t value) { hash = (hash ^ value) * 1099511628211ULL; };
	const auto word = [&](uint32_t value) { for (int shift = 0; shift < 32; shift += 8) byte(uint8_t(value >> shift)); };
	byte('C'); byte('V');
	word(uint32_t(region.x)); word(uint32_t(region.y)); word(uint32_t(region.z));
	word(region.width); word(region.height);
	for (const auto kind : cells) byte(static_cast<uint8_t>(kind));
    if (!corridors.empty()) {
        byte('D');
        for (const auto& c : corridors) { word(c.start); word(c.door); byte(c.length); byte(c.horizontal); }
    }
	return hash;
}

std::expected<CaveGrid, SurfaceError> generateCave(const SurfaceGrid& surface, uint64_t seed, std::stop_token stop, CaveLayout* layout,
	std::span<const uint8_t> blocked) {
	if (!surface.valid() || (!blocked.empty() && blocked.size() != surface.cells.size())) return std::unexpected(SurfaceError::InvalidRegion);
	CaveGrid cave;
	cave.region = surface.region;
	++cave.region.z;
	if (layout) *layout = {};
	try {
		cave.cells.assign(surface.cells.size(), CaveKind::Solid);
		// Kept only when something is blocked, so an unblocked cave stays as before (iteration 49).
		if (std::ranges::any_of(blocked, [](uint8_t b) { return b != 0; })) cave.blocked.assign(blocked.begin(), blocked.end());
		const int32_t width = int32_t(surface.region.width), height = int32_t(surface.region.height);
		if (width < 5 || height < 5) return cave;
		Carver carver {cave.cells, width, height, seed, surface.region.x, surface.region.y, &cave.rooms, layout, cave.blocked};
		std::vector<uint32_t> entrances = surface.entrances;
		std::ranges::sort(entrances);
		entrances.erase(std::unique(entrances.begin(), entrances.end()), entrances.end());
		std::vector<Point> points;
		for (const uint32_t entrance : entrances) points.push_back({int32_t(entrance % uint32_t(width)), int32_t(entrance / uint32_t(width))});
		for (const auto& p : points) carver.disc(p.x, p.y, 5);
		const size_t n = points.size();
		const auto join = [&](size_t i, size_t j) {
			carver.tunnel(points[i], points[j]);
			if (layout) layout->links.push_back({entrances[i], entrances[j]});
		};
		if (n == 1) {
			const auto [ax, ay] = points[0];
			int32_t tx = ax + std::clamp(width / 2 - ax, -40, 40), ty = ay + std::clamp(height / 2 - ay, -40, 40);
			if (tx == ax && ty == ay) tx = std::clamp(ax + 12, 2, width - 3);
			carver.tunnel(points[0], {std::clamp(tx, 2, width - 3), std::clamp(ty, 2, height - 3)});
			return cave;
		}
		// Minimum spanning tree over the straight distances (Prim; ties go to the lower cell index), so the joins depend on
		// where the entrances are, not on their order.
		const auto squared = [&](size_t i, size_t j) {
			const int64_t dx = points[j].x - points[i].x, dy = points[j].y - points[i].y;
			return dx * dx + dy * dy;
		};
		std::vector<uint8_t> inTree(n, 0);
		std::vector<int64_t> best(n, std::numeric_limits<int64_t>::max());
		std::vector<size_t> parent(n, 0);
		std::vector<std::vector<size_t>> tree(n);
		std::vector<std::pair<size_t, size_t>> edges;
		if (n) best[0] = 0;
		for (size_t added = 0; added < n; ++added) {
			size_t next = n;
			for (size_t i = 0; i < n; ++i) if (!inTree[i] && (next == n || best[i] < best[next])) next = i;
			inTree[next] = 1;
			if (added) {
				edges.push_back({parent[next], next});
				tree[parent[next]].push_back(next);
				tree[next].push_back(parent[next]);
			}
			for (size_t i = 0; i < n; ++i) if (!inTree[i] && squared(next, i) < best[i]) { best[i] = squared(next, i); parent[i] = next; }
		}
		for (const auto& [i, j] : edges) {
			if (stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
			join(i, j);
		}
		// Loops (four entrances or more): one, or two from eight entrances on, of the pairs outside the tree that are no longer
		// than 1.5 times its longest edge and clear of the planned tunnels, preferring the largest detour along the tree compared
		// with the direct distance. Entrances that leave no clear pair (for example all in a row) get fewer loops.
		if (n >= 4) {
			std::vector<double> around(n * n, 0);
			for (size_t source = 0; source < n; ++source) {
				std::vector<size_t> stack {source};
				std::vector<uint8_t> seen(n, 0);
				seen[source] = 1;
				while (!stack.empty()) {
					const size_t at = stack.back();
					stack.pop_back();
					for (const size_t to : tree[at]) if (!seen[to]) {
						seen[to] = 1;
						around[source * n + to] = around[source * n + at] + std::sqrt(double(squared(at, to)));
						stack.push_back(to);
					}
				}
			}
			double longest = 0;
			for (const auto& [i, j] : edges) longest = std::max(longest, std::sqrt(double(squared(i, j))));
			struct Candidate { double ratio, distance; size_t i, j; };
			std::vector<Candidate> candidates;
			for (size_t i = 0; i < n; ++i) for (size_t j = i + 1; j < n; ++j) {
				const bool treeEdge = std::ranges::find(tree[i], j) != tree[i].end();
				const double distance = std::sqrt(double(squared(i, j)));
				if (!treeEdge && distance > 0 && distance <= 1.5 * longest) candidates.push_back({around[i * n + j] / distance, distance, i, j});
			}
			std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b) {
				if (a.ratio != b.ratio) return a.ratio > b.ratio;
				if (a.distance != b.distance) return a.distance < b.distance;
				return std::pair(a.i, a.j) < std::pair(b.i, b.j);
			});
			// A loop must not run beside a tunnel already planned (straight lines between the entrances): it leaves each end
			// at least 35 degrees away from the tunnels there, and its quarter, half and three-quarter points keep
			// a fifth of its length from the other tunnels.
			std::vector<std::pair<size_t, size_t>> planned = edges;
			const auto clear = [&](size_t i, size_t j) {
				const double length = std::sqrt(double(squared(i, j))), room = length / 5;
				const auto apart = [&](size_t from, size_t to, size_t other) {
					const double dot = double(points[to].x - points[from].x) * (points[other].x - points[from].x) +
						double(points[to].y - points[from].y) * (points[other].y - points[from].y);
					return dot <= 0.819 * length * std::sqrt(double(squared(from, other)));
				};
				for (const auto& [a, b] : planned) {
					if (a == i || a == j || b == i || b == j) {
						const size_t shared = a == i || b == i ? i : j, other = a == shared ? b : a;
						if (!apart(shared, shared == i ? j : i, other)) return false;
						continue;
					}
					for (const double t : {0.25, 0.5, 0.75}) {
						const double x = points[i].x + (points[j].x - points[i].x) * t, y = points[i].y + (points[j].y - points[i].y) * t;
						if (segmentDistance(points[a], points[b], x, y) < room) return false;
					}
				}
				return true;
			};
			const size_t wanted = n >= 8 ? 2 : 1;
			for (const auto& candidate : candidates) {
				if (planned.size() - edges.size() == wanted) break;
				if (!clear(candidate.i, candidate.j)) continue;
				if (stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
				join(candidate.i, candidate.j);
				planned.push_back({candidate.i, candidate.j});
				if (layout) ++layout->loops;
			}
		}
	} catch (const std::bad_alloc&) {
		return std::unexpected(SurfaceError::AllocationFailed);
	}
	return cave;
}

uint64_t caveCavernCells(const CaveGrid& cave) {
	const int64_t w = cave.region.width, h = cave.region.height;
	if (!cave.valid()) return 0;
	const auto floor = [&](int64_t x, int64_t y) { return x >= 0 && y >= 0 && x < w && y < h && cave.cells[size_t(y * w + x)] == CaveKind::Floor; };
	uint64_t count = 0;
	for (int64_t y = 0; y < h; ++y) for (int64_t x = 0; x < w; ++x) {
		if (!floor(x, y)) continue;
		int neighbours = 0;
		for (int64_t dy = -1; dy <= 1; ++dy) for (int64_t dx = -1; dx <= 1; ++dx) neighbours += (dx || dy) && floor(x + dx, y + dy);
		if (neighbours < 5) continue;
		// Width in tenths of a cell: the shortest Floor run through the cell along the two axes and the two diagonals.
		int64_t narrowest = std::numeric_limits<int64_t>::max();
		constexpr int64_t directions[4][3] = {{1, 0, 10}, {0, 1, 10}, {1, 1, 14}, {1, -1, 14}};
		for (const auto& [dx, dy, step] : directions) {
			int64_t run = 1;
			for (int64_t k = 1; floor(x + dx * k, y + dy * k); ++k) ++run;
			for (int64_t k = 1; floor(x - dx * k, y - dy * k); ++k) ++run;
			narrowest = std::min(narrowest, run * step);
		}
		count += narrowest > 50;
	}
	return count;
}

uint32_t longestStraightAxis(std::span<const uint32_t> axis, uint32_t width) {
	if (!width) return 0;
	const auto pointOf = [&](size_t k) { return Point {int32_t(axis[k] % width), int32_t(axis[k] / width)}; };
	double longest = axis.empty() ? 0 : 1;
	for (size_t i = 0; i < axis.size(); ++i) {
		for (size_t j = i + 1; j < axis.size(); ++j) {
			bool straight = true;
			for (size_t k = i + 1; k < j && straight; ++k) straight = deviationOf(pointOf(i), pointOf(j), pointOf(k)) <= 1.0;
			if (!straight) break;
			longest = std::max(longest, distanceOf(pointOf(i), pointOf(j)) + 1);
		}
	}
	return uint32_t(std::lround(longest));
}

std::expected<uint32_t, UndergroundError> removeUnenteredCaveComponents(
    CaveGrid& cave, const CaveExitPlan& exits, std::stop_token stop) try {
    if (!cave.valid()) return std::unexpected(UndergroundError::InvalidGrid);
    const auto w = cave.region.width, h = cave.region.height;
    std::vector<uint8_t> seen(cave.cells.size()), entered(cave.cells.size());
    for (const auto& exit : exits.exits) {
        if (exit.cell >= cave.cells.size() || cave.cells[exit.cell] != CaveKind::Floor)
            return std::unexpected(UndergroundError::InvalidGrid);
        if (!exit.distant) entered[exit.cell] = 1;
    }
    std::vector<uint32_t> queue;
    uint32_t removed = 0;
    for (uint32_t start = 0; start < cave.cells.size(); ++start) {
        if (stop.stop_requested()) return std::unexpected(UndergroundError::Cancelled);
        if (seen[start] || cave.cells[start] != CaveKind::Floor) continue;
        queue.clear(); queue.push_back(start); seen[start] = 1;
        bool hasEntrance = false;
        for (size_t head = 0; head < queue.size(); ++head) {
            if (head % 4096 == 0 && stop.stop_requested()) return std::unexpected(UndergroundError::Cancelled);
            const auto i = queue[head];
            hasEntrance |= entered[i] != 0;
            const auto visit = [&](uint32_t j) {
                if (!seen[j] && cave.cells[j] == CaveKind::Floor) { seen[j] = 1; queue.push_back(j); }
            };
            if (i >= w) visit(i - w);
            if (i % w) visit(i - 1);
            if (i % w + 1 < w) visit(i + 1);
            if (i / w + 1 < h) visit(i + w);
        }
        if (!hasEntrance) {
            ++removed;
            for (const auto i : queue) cave.cells[i] = CaveKind::Solid;
        }
    }
    return removed;
} catch (const std::bad_alloc&) { return std::unexpected(UndergroundError::AllocationFailed); }

std::expected<CaveExitPlan, UndergroundError> placeCaveExits(
	const SurfaceGrid& surface, const CaveGrid& cave, std::span<const ExitShape> shapes,
	std::span<const uint8_t> occupied, std::stop_token stop) {
	if (!surface.valid() || !cave.valid() || cave.region.x != surface.region.x || cave.region.y != surface.region.y ||
		cave.region.width != surface.region.width || cave.region.height != surface.region.height ||
		int64_t(cave.region.z) != int64_t(surface.region.z) + 1 ||
		(!occupied.empty() && occupied.size() != surface.cells.size()) || shapes.empty() ||
		shapes.size() > size_t(std::numeric_limits<uint8_t>::max()) + 1 ||
		surface.cells.size() > std::numeric_limits<uint32_t>::max()) return std::unexpected(UndergroundError::InvalidGrid);
	for (const auto& shape : shapes) {
		if (shape.direction > ExitDirection::West ||
			(shape.hasTail && std::abs(int(shape.tailDx)) + std::abs(int(shape.tailDy)) != 1))
			return std::unexpected(UndergroundError::InvalidGrid);
	}
	if (stop.stop_requested()) return std::unexpected(UndergroundError::Cancelled);
	try {
		const int64_t width = surface.region.width, height = surface.region.height;
		const auto inside = [&](int64_t x, int64_t y) { return x >= 0 && y >= 0 && x < width && y < height; };
		const auto cell = [&](int64_t x, int64_t y) { return uint32_t(y * width + x); };
		const size_t count = surface.cells.size();
		std::vector<uint8_t> lower(count, 0), upper(count, 0);
		for (const auto e : surface.entrances) upper[e] = 1;
		for (const auto i : surface.fires) upper[i] = 1;
		for (const auto i : surface.doors) upper[i] = 1;
		for (const auto i : surface.gaps) upper[i] = 1;
		for (const auto i : surface.sites) upper[i] = 1;
		const auto freeUpper = [&](uint32_t i) {
			const auto k = surface.cells[i];
			return !upper[i] && (occupied.empty() || !occupied[i]) &&
				(k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Shore || k == SurfaceKind::Path);
		};
		struct Offset { int dx, dy; };
		constexpr Offset directions[] = {{0, -1}, {0, 1}, {1, 0}, {-1, 0}};
		CaveExitPlan plan;
		for (size_t e = 0; e < surface.entrances.size(); ++e) {
			if (stop.stop_requested()) return std::unexpected(UndergroundError::Cancelled);
			const auto entrance = surface.entrances[e];
			const int64_t ex = entrance % surface.region.width, ey = entrance / surface.region.width;
			bool placed = false;
			const auto p = entrance;
			if (!lower[p] && cave.cells[p] == CaveKind::Floor) for (size_t s = 0; s < shapes.size(); ++s) {
				const auto& shape = shapes[s];
				const auto [dx, dy] = directions[static_cast<uint8_t>(shape.direction)];
				const int64_t destX = ex + dx, destY = ey + dy, landX = ex - dx, landY = ey - dy;
				if (!inside(destX, destY) || !inside(landX, landY)) continue;
				const auto destination = cell(destX, destY), landing = cell(landX, landY);
				if (!freeUpper(destination) || lower[landing] || cave.cells[landing] != CaveKind::Floor) continue;
				const int64_t tx = ex + shape.tailDx, ty = ey + shape.tailDy;
				if (shape.hasTail && (!inside(tx, ty) ||
					lower[cell(tx, ty)] || cave.cells[cell(tx, ty)] != CaveKind::Floor)) continue;
				plan.exits.push_back({uint32_t(e), p, uint8_t(s)});
				lower[p] = lower[landing] = 1;
				if (shape.hasTail) lower[cell(tx, ty)] = 1;
				upper[destination] = 1;
				placed = true;
				break;
			}
			if (!placed) plan.skipped.push_back(uint32_t(e));
		}
		return plan;
	} catch (const std::bad_alloc&) {
		return std::unexpected(UndergroundError::AllocationFailed);
	}
}

namespace {
template<class Visit> void caveNeighbours(uint32_t i, uint32_t w, uint32_t h, Visit visit) {
    if (i >= w) visit(i - w);
    if (i % w) visit(i - 1);
    if (i % w + 1 < w) visit(i + 1);
    if (i / w + 1 < h) visit(i + w);
}

std::vector<int32_t> caveDistances(const CaveGrid& cave, std::span<const uint32_t> sources) {
    std::vector<int32_t> distance(cave.cells.size(), -1);
    std::vector<uint32_t> queue;
    for (auto i : sources) if (cave.cells[i] == CaveKind::Floor && distance[i] < 0) {
        distance[i] = 0; queue.push_back(i);
    }
    for (size_t head = 0; head < queue.size(); ++head)
        caveNeighbours(queue[head], cave.region.width, cave.region.height, [&](uint32_t j) {
            if (cave.cells[j] == CaveKind::Floor && distance[j] < 0) {
                distance[j] = distance[queue[head]] + 1; queue.push_back(j);
            }
        });
    return distance;
}
}

std::expected<void, UndergroundError> placeFarCaveExits(
    const SurfaceGrid& surface, CaveGrid& cave, CaveExitPlan& exits,
    std::span<const ExitShape> shapes, uint64_t seed,
    std::span<const uint8_t> occupied, std::stop_token stop) try {
    // Reuse the complete legacy geometry/input validation before indexing shapes.
    auto validated = placeCaveExits(surface, cave, shapes, occupied, stop);
    if (!validated) return std::unexpected(validated.error());
    const auto w = cave.region.width, h = cave.region.height;
    const auto n = cave.cells.size();
    constexpr int dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
    const auto shift = [&](uint32_t i, int x, int y) -> std::optional<uint32_t> {
        const auto xx = int64_t(i % w) + x, yy = int64_t(i / w) + y;
        if (xx < 2 || yy < 2 || xx >= int64_t(w) - 2 || yy >= int64_t(h) - 2) return {};
        return uint32_t(yy * w + xx);
    };
    std::vector<uint8_t> reserved(n), poi(n), processed(n);
    for (const auto* cells : {&surface.doors, &surface.gaps, &surface.sites, &surface.fires})
        for (auto i : *cells) poi[i] = 1;
    const auto reserve = [&](const CaveExit& e) {
        if (e.cell >= n || e.shape >= shapes.size() || e.entrance >= surface.entrances.size()) return false;
        reserved[e.cell] = 1;
        const auto& s = shapes[e.shape]; const auto d = size_t(s.direction);
        if (auto i = shift(e.cell, -dx[d], -dy[d])) reserved[*i] = 1;
        if (s.hasTail) if (auto i = shift(e.cell, s.tailDx, s.tailDy)) reserved[*i] = 1;
        return true;
    };
    for (const auto& e : exits.exits) if (!reserve(e)) return std::unexpected(UndergroundError::InvalidGrid);
    const auto open = [&](uint32_t i) {
        const auto k = surface.cells[i];
        return !poi[i] && (occupied.empty() || !occupied[i]) &&
            (k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Path || k == SurfaceKind::Shore);
    };
    const auto spaced = [&](uint32_t p) {
        const auto distant = [&](uint32_t q) {
            return std::max(std::abs(int(p % w) - int(q % w)), std::abs(int(p / w) - int(q / w))) >= 12;
        };
        return std::ranges::all_of(surface.entrances, distant) &&
            std::ranges::all_of(exits.exits, [&](const auto& e) { return distant(e.cell); });
    };
    const auto fit = [&](uint32_t p, size_t s, bool floor) {
        const auto& shape = shapes[s]; const auto d = size_t(shape.direction);
        auto arrival = shift(p, dx[d], dy[d]), landing = shift(p, -dx[d], -dy[d]);
        if (!arrival || !landing || !open(p) || !open(*arrival) || !spaced(p) || !spaced(*arrival) || reserved[p] || reserved[*landing]) return false;
        if (floor && (cave.cells[p] != CaveKind::Floor || cave.cells[*landing] != CaveKind::Floor)) return false;
        if (shape.hasTail) {
            const auto tail = shift(p, shape.tailDx, shape.tailDy);
            if (!tail || reserved[*tail] || (floor && cave.cells[*tail] != CaveKind::Floor)) return false;
        }
        return true;
    };
    const auto original = exits.exits;
    for (const auto& entry : original) {
        if (stop.stop_requested()) return std::unexpected(UndergroundError::Cancelled);
        if (entry.distant || processed[entry.cell]) continue;
        const uint32_t source = entry.cell;
        auto component = caveDistances(cave, std::span(&source, 1));
        std::vector<uint32_t> sources;
        // Keep every entrance as a BFS source: an extension may join another
        // component, whose entrances must also remain at distance >= 24.
        for (const auto& e : original) if (!e.distant) sources.push_back(e.cell);
        auto distance = caveDistances(cave, sources);
        std::vector<uint32_t> order;
        for (uint32_t i = 0; i < n; ++i) if (component[i] >= 0) order.push_back(i);
        std::ranges::sort(order, [&](auto a, auto b) { return distance[a] != distance[b] ? distance[a] > distance[b] : a < b; });
        bool placed = false;
        for (auto p : order) {
            if (distance[p] < 24) break;
            for (size_t s = 0; s < shapes.size(); ++s) if (fit(p, s, true)) {
                exits.exits.push_back({entry.entrance, p, uint8_t(s), true}); reserve(exits.exits.back()); placed = true; break;
            }
            if (placed) break;
        }
        // A short component needs an extension. Try targets in row-major order,
        // always from its farthest cell; rejected attempts leave no carved cells.
        if (!placed && !order.empty() && distance[order.front()] < 24) {
            const auto from = order.front();
            for (uint32_t p = 0; p < n && !placed; ++p) {
                if (p % 4096 == 0 && stop.stop_requested()) return std::unexpected(UndergroundError::Cancelled);
                const auto length = std::abs(int(p % w) - int(from % w)) + std::abs(int(p / w) - int(from / w));
                if (length < 24 || length > 48) continue;
                for (size_t s = 0; s < shapes.size(); ++s) if (fit(p, s, false)) {
                    auto before = cave.cells;
                    const auto roomCount = cave.rooms.size();
                    Carver carver {cave.cells, int32_t(w), int32_t(h), seed, surface.region.x, surface.region.y, &cave.rooms, nullptr, cave.blocked};
                    carver.tunnel({int32_t(from % w), int32_t(from / w)}, {int32_t(p % w), int32_t(p / w)});
                    auto extended = caveDistances(cave, sources);
                    const bool previousExitsStillFar = std::ranges::all_of(exits.exits,
                        [&](const auto& e) { return !e.distant || extended[e.cell] >= 24; });
                    if (extended[p] >= 24 && previousExitsStillFar && fit(p, s, true)) {
                        exits.exits.push_back({entry.entrance, p, uint8_t(s), true}); reserve(exits.exits.back()); placed = true; break;
                    }
                    cave.cells = std::move(before);
                    cave.rooms.resize(roomCount);
                }
            }
        }
        component = caveDistances(cave, std::span(&source, 1));
        for (size_t i = 0; i < n; ++i) if (component[i] >= 0) processed[i] = 1;
    }
    return {};
} catch (const std::bad_alloc&) { return std::unexpected(UndergroundError::AllocationFailed); }

std::expected<void, UndergroundError> placeCaveDoorCorridors(
    CaveGrid& cave, const CaveExitPlan& exits, std::span<const ExitShape> shapes, std::stop_token stop) try {
    if (!cave.valid()) return std::unexpected(UndergroundError::InvalidGrid);
    const auto w = cave.region.width, h = cave.region.height;
    std::vector<uint8_t> protectedCells(cave.cells.size()), processed(cave.cells.size());
    for (const auto& e : exits.exits) {
        if (e.cell >= cave.cells.size() || e.shape >= shapes.size()) return std::unexpected(UndergroundError::InvalidGrid);
        const int x = int(e.cell % w), y = int(e.cell / w);
        for (int yy = std::max(0, y - 3); yy <= std::min(int(h) - 1, y + 3); ++yy)
            for (int xx = std::max(0, x - 3); xx <= std::min(int(w) - 1, x + 3); ++xx) protectedCells[size_t(yy) * w + xx] = 1;
    }
    for (auto room : cave.rooms) {
        if (room >= cave.cells.size()) return std::unexpected(UndergroundError::InvalidGrid);
        const int x = int(room % w), y = int(room / w);
        for (int yy = std::max(0, y - 5); yy <= std::min(int(h) - 1, y + 5); ++yy)
            for (int xx = std::max(0, x - 5); xx <= std::min(int(w) - 1, x + 5); ++xx) protectedCells[size_t(yy) * w + xx] = 1;
    }
    for (uint32_t first = 0; first < cave.cells.size(); ++first) {
        if (stop.stop_requested()) return std::unexpected(UndergroundError::Cancelled);
        if (processed[first] || cave.cells[first] != CaveKind::Floor) continue;
        auto component = caveDistances(cave, std::span(&first, 1));
        size_t rooms = 0;
        for (auto r : cave.rooms) rooms += component[r] >= 0;
        for (size_t i = 0; i < component.size(); ++i) if (component[i] >= 0) processed[i] = 1;
        if (rooms < 2) continue;
        bool placed = false;
        for (uint32_t p = 0; p < cave.cells.size() && !placed; ++p) {
            if (p % 4096 == 0 && stop.stop_requested()) return std::unexpected(UndergroundError::Cancelled);
            if (component[p] < 0) continue;
            const int x = int(p % w), y = int(p / w);
            if (x < 4 || y < 4 || x + 4 >= int(w) || y + 4 >= int(h)) continue;
            for (bool horizontal : {true, false}) {
                const int step = horizontal ? 1 : int(w), side = horizontal ? int(w) : 1;
                bool fits = true;
                for (int k = -2; k <= 2; ++k) {
                    const auto i = uint32_t(int(p) + k * step);
                    // The two approach cells are unchanged; only the three
                    // corridor cells and their sides need to be unreserved.
                    fits &= cave.cells[i] == CaveKind::Floor && (std::abs(k) > 1 || !protectedCells[i]);
                    if (std::abs(k) <= 1) fits &= !protectedCells[i - side] && !protectedCells[i + side];
                }
                if (!fits) continue;
                std::vector<uint32_t> removed;
                for (int k = -1; k <= 1; ++k) for (int sign : {-1, 1}) {
                    const auto i = uint32_t(int(p) + k * step + sign * side);
                    if (cave.cells[i] == CaveKind::Floor) { removed.push_back(i); cave.cells[i] = CaveKind::Solid; }
                }
                auto after = caveDistances(cave, std::span(&p, 1));
                bool connected = true;
                for (size_t i = 0; i < component.size(); ++i)
                    if (component[i] >= 0 && cave.cells[i] == CaveKind::Floor && after[i] < 0) { connected = false; break; }
                if (connected) {
                    cave.corridors.push_back({uint32_t(int(p) - step), p, 3, horizontal}); placed = true; break;
                }
                for (auto i : removed) cave.cells[i] = CaveKind::Floor;
            }
        }
        // An enabled option must never silently produce a multi-room component
        // without its required door corridor. Reject an impossible geometry.
        if (!placed) return std::unexpected(UndergroundError::InvalidGrid);
    }
    return {};
} catch (const std::bad_alloc&) { return std::unexpected(UndergroundError::AllocationFailed); }

std::expected<UndergroundReport, UndergroundError> undergroundReachabilityOf(
	const SurfaceGrid& surface, const CaveGrid& cave, bool walls, std::stop_token stop) {
	if (!surface.valid() || !cave.valid() || cave.cells.size() != surface.cells.size() || cave.region.width != surface.region.width ||
		cave.region.height != surface.region.height) return std::unexpected(UndergroundError::InvalidGrid);
	UndergroundReport report;
	try {
		const std::vector<uint32_t> openings = openingsOf(surface);
		auto labels = walkableLabelsOf(surface.cells, surface.region.width, surface.region.height, openings, walls, stop);
		if (!labels) return std::unexpected(labels.error() == ReachabilityError::Cancelled ? UndergroundError::Cancelled : UndergroundError::AllocationFailed);
		const auto start = fixedStartOf(*labels, surface.region.width, surface.region.height);
		const size_t width = cave.region.width, height = cave.region.height, count = cave.cells.size();

		// 4-connected components of cave Floor.
		std::vector<uint32_t> label(count, 0), queue;
		std::vector<uint32_t> sizes {0};
		queue.reserve(count);
		for (size_t y = 0; y < height; ++y) {
			if (stop.stop_requested()) return std::unexpected(UndergroundError::Cancelled);
			for (size_t x = 0; x < width; ++x) {
				const size_t first = y * width + x;
				if (cave.cells[first] != CaveKind::Floor || label[first]) continue;
				const uint32_t id = uint32_t(sizes.size());
				queue.assign(1, uint32_t(first));
				label[first] = id;
				for (size_t head = 0; head < queue.size(); ++head) {
					const size_t i = queue[head];
					const size_t ix = i % width, iy = i / width;
					const size_t around[4] = {ix > 0 ? i - 1 : count, ix + 1 < width ? i + 1 : count, iy > 0 ? i - width : count, iy + 1 < height ? i + width : count};
					for (const size_t n : around) {
						if (n == count || label[n] || cave.cells[n] != CaveKind::Floor) continue;
						label[n] = id;
						queue.push_back(uint32_t(n));
					}
				}
				sizes.push_back(uint32_t(queue.size()));
			}
		}
		std::vector<uint8_t> reached(sizes.size(), 0);
		for (const uint32_t entrance : surface.entrances) {
			++report.entrances;
			if (!start || !labels->label[entrance] || labels->label[entrance] != labels->label[*start]) continue;
			++report.entrancesReached;
			if (label[entrance]) reached[label[entrance]] = 1;
		}
		report.caveComponents = uint32_t(sizes.size() - 1);
		for (uint32_t id = 1; id < sizes.size(); ++id) {
			report.caveCells += sizes[id];
			if (reached[id]) report.caveReachedCells += sizes[id]; else ++report.pockets;
		}
	} catch (const std::bad_alloc&) {
		return std::unexpected(UndergroundError::AllocationFailed);
	}
	return report;
}

static std::expected<PlateauGrid, SurfaceError> buildPlateau(
    const SurfaceGrid& surface, uint64_t seed, uint32_t minTop, std::span<const ExitShape> shapes, std::span<const uint8_t> occupied,
    std::stop_token stop, uint32_t maxPercent, std::span<const uint8_t> lowerOpen = {}, std::span<const uint8_t> lowerTail = {},
    uint64_t limitCells = 0, bool entranceNotches = false
)
try {
    if (stop.stop_requested()) {
        return std::unexpected(SurfaceError::Cancelled);
    }
    if (!surface.valid() || surface.region.z <= 0) {
        return std::unexpected(SurfaceError::InvalidRegion);
    }
    if (maxPercent > 100 || (minTop && (minTop < 16 || minTop > 4096)) || shapes.size() > 256
        || (!occupied.empty() && occupied.size() != surface.cells.size())) {
        return std::unexpected(SurfaceError::InvalidParameters);
    }
    const int w = int(surface.region.width), h = int(surface.region.height);
    const size_t n = surface.cells.size();
    PlateauGrid out {surface.region, std::vector<PlateauKind>(n), std::vector<uint8_t>(n)};
    --out.region.z;
    if (!minTop) {
        return out;
    }
    const auto index = [w](int x, int y) { return size_t(y) * w + size_t(x); };
    const auto inside = [w, h](int x, int y) { return x >= 0 && y >= 0 && x < w && y < h; };
    // Chebyshev distance from non-base, including cells outside the rectangle.
    std::vector<uint32_t> distance(n);
    for (int y = 0; y < h; ++y) {
        if (stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
        for (int x = 0; x < w; ++x) {
            auto i = index(x, y);
            distance[i] = surface.cells[i] == SurfaceKind::Rock ? uint32_t(std::min({x + 1, y + 1, w - x, h - y})) : 0;
            if (distance[i] && x && y) {
                distance[i] = std::min({distance[i], distance[i - 1] + 1, distance[i - w] + 1, distance[i - w - 1] + 1});
            }
            if (distance[i] && y && x + 1 < w) {
                distance[i] = std::min(distance[i], distance[i - w + 1] + 1);
            }
        }
    }
    for (int y = h - 1; y >= 0; --y) {
        if (stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
        for (int x = w - 1; x >= 0; --x) {
            auto i = index(x, y);
            if (x + 1 < w) {
                distance[i] = std::min(distance[i], distance[i + 1] + 1);
            }
            if (y + 1 < h) {
                distance[i] = std::min(distance[i], distance[i + w] + 1);
                if (x) {
                    distance[i] = std::min(distance[i], distance[i + w - 1] + 1);
                }
                if (x + 1 < w) {
                    distance[i] = std::min(distance[i], distance[i + w + 1] + 1);
                }
            }
        }
    }
    for (uint32_t inset = lowerOpen.empty() ? 1 : 3;; ++inset) {
        if (stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
        out.inset = inset;
        out.cells.assign(n, PlateauKind::None);
        out.landings.assign(n, 0);
        out.ramps.clear();
        out.components = 0;
        std::vector<uint8_t> candidate(n), reserved(n);
        for (const auto* cells : {&surface.doors, &surface.gaps, &surface.fires, &surface.entrances})
            for (auto i : *cells) reserved[i] = 1;
        for (size_t i = 0; i < n; ++i) candidate[i] = distance[i] >= inset;
        if (entranceNotches) for (const auto entry : surface.entrances) {
            const int x = int(entry % w), y = int(entry / w);
            for (int yy = std::max(0, y - 1); yy <= std::min(h - 1, y + 1); ++yy)
                for (int xx = std::max(0, x - 1); xx <= std::min(w - 1, x + 1); ++xx) {
                    candidate[index(xx, yy)] = 0;
                    reserved[index(xx, yy)] = 1;
                }
        }
        const auto covered = std::ranges::count(candidate, uint8_t(1));
        if (!lowerOpen.empty() && maxPercent && uint64_t(covered) * 100 > limitCells * maxPercent) continue;
        const auto open = [&](int x, int y) {
            if (!inside(x, y)) return false;
            auto i = index(x, y);
            auto k = surface.cells[i];
            return !candidate[i] && !reserved[i] && (occupied.empty() || !occupied[i])
                && (lowerOpen.empty() ? (k == SurfaceKind::Grass || k == SurfaceKind::Dirt || k == SurfaceKind::Path) : bool(lowerOpen[i]));
        };
        std::vector<uint32_t> labels(n, 0), queue;
        std::vector<std::vector<uint32_t>> tops;
        constexpr int dx[] = {0, 0, 1, -1}, dy[] = {-1, 1, 0, 0};
        for (size_t start = 0; start < n; ++start) {
            if (candidate[start] && !labels[start]) {
                if (stop.stop_requested()) {
                    return std::unexpected(SurfaceError::Cancelled);
                }
                queue.clear();
                queue.push_back(uint32_t(start));
                labels[start] = uint32_t(tops.size() + 1);
                for (size_t q = 0; q < queue.size(); ++q) {
                    if (q % 4096 == 0 && stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
                    int x = int(queue[q] % w), y = int(queue[q] / w);
                    for (size_t d = 0; d < 4; ++d) {
                        if (inside(x + dx[d], y + dy[d])) {
                            auto j = index(x + dx[d], y + dy[d]);
                            if (candidate[j] && !labels[j]) {
                                labels[j] = labels[start];
                                queue.push_back(uint32_t(j));
                            }
                        }
                    }
                }
                tops.push_back(queue);
            }
        }
        StableRandom random(seed, 0x706c617465617531ULL);
        // Row-major discovery followed by a stable Fisher-Yates component permutation.
        for (size_t i = tops.size(); i > 1; --i)
            std::swap(tops[i - 1], tops[size_t(random.bounded(i))]);
        uint64_t accepted = 0;
        const bool validDirections = std::ranges::all_of(shapes, [](const auto& shape) { return shape.direction <= ExitDirection::West; });
        for (const auto& top : tops) {
            const auto component = labels[top.front()];
            if (lowerOpen.empty() && maxPercent && (accepted + top.size()) * 100 > uint64_t(n) * maxPercent) continue;
            if (top.size() < minTop || (!lowerOpen.empty() && std::ranges::any_of(top, [&](auto i) { return reserved[i]; }))) {
                continue;
            }
            if (stop.stop_requested()) {
                return std::unexpected(SurfaceError::Cancelled);
            }
            std::vector<uint32_t> order;
            for (size_t i = 0; i < n; ++i) {
                if (i % 4096 == 0 && stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
                if (lowerOpen.empty() ? surface.cells[i] != SurfaceKind::Rock : lowerOpen[i] != 0) {
                    order.push_back(uint32_t(i));
                }
            }
            // Stable Fisher-Yates, independent from all existing generator streams.
            for (size_t i = order.size(); i > 1; --i) {
                if (i % 4096 == 0 && stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
                std::swap(order[i - 1], order[size_t(random.bounded(i))]);
            }
            const auto first = out.ramps.size();
            const auto wanted = std::min<size_t>(4, 1 + top.size() / 400);
            size_t tried = 0;
            for (auto p : order) {
                if (tried++ % 4096 == 0 && stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
                int x = int(p % w), y = int(p / w);
                if (!open(x, y)) {
                    continue;
                }
                // A ramp must reach this component in one step. Reject other cells before
                // comparing them with every earlier ramp. Keep the shuffled order and RNG
                // draws unchanged; invalid directions retain their existing error path below.
                if (validDirections) {
                    bool beside = false;
                    for (size_t d = 0; d < 4; ++d) {
                        const int tx = x + dx[d], ty = y + dy[d];
                        if (inside(tx, ty) && surface.cells[index(tx, ty)] == SurfaceKind::Rock && labels[index(tx, ty)] == component) {
                            beside = true;
                            break;
                        }
                    }
                    if (!beside) continue;
                }
                bool spaced = true;
                for (const auto& ramp : out.ramps) {
                    if (std::max(std::abs(x - int(ramp.cell % w)), std::abs(y - int(ramp.cell / w))) < 16) {
                        spaced = false;
                    }
                }
                if (!spaced) {
                    continue;
                }
                for (size_t shape = 0; shape < shapes.size(); ++shape) {
                    const auto& s = shapes[shape];
                    auto d = size_t(s.direction);
                    if (d >= 4) {
                        return std::unexpected(SurfaceError::InvalidParameters);
                    }
                    int tx = x + dx[d], ty = y + dy[d];
                    if (!inside(tx, ty) || surface.cells[index(tx, ty)] != SurfaceKind::Rock) {
                        continue;
                    }
                    const auto dest = index(tx, ty);
                    const bool fits = labels[dest] == component;
                    const int tailX = x + s.tailDx, tailY = y + s.tailDy;
                    const bool tailFits = !s.hasTail
                        || (lowerOpen.empty() ? open(tailX, tailY)
                                              : (open(tailX, tailY) && lowerTail[index(tailX, tailY)]));
                    if (!fits || !tailFits) {
                        continue;
                    }
                    out.ramps.push_back({p, uint32_t(dest), out.components, uint8_t(shape)});
                    reserved[p] = 1;
                    if (s.hasTail) {
                        reserved[index(x + s.tailDx, y + s.tailDy)] = 1;
                    }
                    reserved[dest] = 1;
                    break;
                }
                if (out.ramps.size() - first == wanted) {
                    break;
                }
            }
            if (out.ramps.size() == first) {
                continue;
            }
            ++out.components;
            accepted += top.size();
            for (auto i : top) out.cells[i] = PlateauKind::Top;
        }
        for (const auto& ramp : out.ramps) {
            const int x = int(ramp.destination % w), y = int(ramp.destination / w);
            for (int yy = std::max(0, y - 1); yy <= std::min(h - 1, y + 1); ++yy)
                for (int xx = std::max(0, x - 1); xx <= std::min(w - 1, x + 1); ++xx)
                    out.landings[index(xx, yy)] = 1;
            out.landings[ramp.cell] = 1;
        }
        return out;
    }
} catch (const std::bad_alloc&) {
    return std::unexpected(SurfaceError::AllocationFailed);
}

std::expected<PlateauGrid, SurfaceError> generatePlateau(
    const SurfaceGrid& surface, uint64_t seed, uint32_t minTop, std::span<const ExitShape> shapes, std::span<const uint8_t> occupied,
    std::stop_token stop, uint32_t maxPercent, bool entranceNotches
)
{
    return buildPlateau(surface, seed, minTop, shapes, occupied, stop, maxPercent, {}, {}, 0, entranceNotches);
}

std::expected<PlateauGrid, SurfaceError>
generateNextPlateau(const PlateauGrid& lower, uint64_t seed, std::span<const ExitShape> shapes, std::stop_token stop, uint32_t maxPercent)
try {
    const size_t n = lower.cells.size();
    if (uint64_t(lower.region.width) * lower.region.height != n || lower.landings.size() != n) {
        return std::unexpected(SurfaceError::InvalidRegion);
    }
    SurfaceGrid base;
    base.region = lower.region;
    base.cells.assign(n, SurfaceKind::Grass);
    std::vector<uint8_t> open(n), tails(n), excluded(n);
    for (const auto& r : lower.ramps) {
        if (r.destination >= n || r.cell >= n) {
            return std::unexpected(SurfaceError::InvalidParameters);
        }
        excluded[r.destination] = 1;
        excluded[r.cell] = 1;
        // The lower ramp's supported tail is opposite its arrival direction.
        const int w = int(lower.region.width), h = int(lower.region.height);
        const int x = 2 * int(r.cell % w) - int(r.destination % w), y = 2 * int(r.cell / w) - int(r.destination / w);
        if (r.shape < shapes.size() && shapes[r.shape].hasTail && x >= 0 && y >= 0 && x < w && y < h) {
            excluded[size_t(y) * w + x] = 1;
        }
    }
    for (size_t i = 0; i < n; ++i) {
        if (lower.cells[i] == PlateauKind::Top) {
            base.cells[i] = SurfaceKind::Rock;
        }
        open[i] = lower.cells[i] == PlateauKind::Top && !lower.landings[i] && !excluded[i];
        tails[i] = lower.cells[i] == PlateauKind::Top;
    }
    base.entrances.clear();
    for (size_t i = 0; i < n; ++i) if (lower.landings[i] || excluded[i]) base.entrances.push_back(uint32_t(i));
    const auto covered = std::ranges::count_if(lower.cells, [](auto k) { return k != PlateauKind::None; });
    return buildPlateau(base, seed, 30, shapes, {}, stop, maxPercent, open, tails, uint64_t(covered));
} catch (const std::bad_alloc&) {
    return std::unexpected(SurfaceError::AllocationFailed);
}

std::expected<DecorGrid, SurfaceError>
generatePlateauDecoration(const PlateauGrid& plateau, uint64_t seed, const DecorTuning& tuning, std::stop_token stop)
try {
    if (stop.stop_requested()) {
        return std::unexpected(SurfaceError::Cancelled);
    }
    if (!plateau.region.width || !plateau.region.height || uint64_t(plateau.region.width) * plateau.region.height != plateau.cells.size()
        || plateau.landings.size() != plateau.cells.size()
        || std::ranges::any_of(plateau.cells, [](auto k) { return k > PlateauKind::Top; })) {
        return std::unexpected(SurfaceError::InvalidRegion);
    }
    for (const auto& ramp : plateau.ramps) {
        if (ramp.destination >= plateau.cells.size()) {
            return std::unexpected(SurfaceError::InvalidParameters);
        }
    }
    SurfaceGrid grid;
    grid.region = plateau.region;
    grid.cells.resize(plateau.cells.size(), SurfaceKind::Preserve);
    for (size_t i = 0; i < grid.cells.size(); ++i) {
        if (plateau.cells[i] == PlateauKind::Top) {
            grid.cells[i] = SurfaceKind::Grass;
        }
    }
    auto decor = generateDecoration(grid, seed ^ 0x706c61745f646563ULL, tuning, stop);
    if (!decor) {
        return decor;
    }
    const int w = int(grid.region.width), h = int(grid.region.height);
    for (const auto& ramp : plateau.ramps) {
        int x = int(ramp.destination % w), y = int(ramp.destination / w);
        for (int yy = std::max(0, y - 1); yy <= std::min(h - 1, y + 1); ++yy) {
            for (int xx = std::max(0, x - 1); xx <= std::min(w - 1, x + 1); ++xx) {
                decor->cells[size_t(yy) * w + xx] = DecorKind::None;
            }
        }
    }
    for (size_t i = 0; i < grid.cells.size(); ++i) {
        if (plateau.landings[i]) {
            decor->cells[i] = DecorKind::None;
        }
    }
    return decor;
} catch (const std::bad_alloc&) {
    return std::unexpected(SurfaceError::AllocationFailed);
}
} // namespace rme::procedural
