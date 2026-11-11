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

constexpr uint64_t tunnelStream = 0x63617665745f7332ULL;   // "cavet_s2"
constexpr uint64_t roomStream = 0x63617665725f7332ULL;     // "caver_s2"
constexpr uint32_t noiseBins = 65536;

struct Carver {
	std::vector<CaveKind>& cells;
	int32_t width, height;
	uint64_t seed;
	int32_t x0, y0;   // absolute origin of the area, for the noise

	void put(int32_t x, int32_t y) {
		if (x < 2 || y < 2 || x > width - 3 || y > height - 3) return;
		cells[size_t(y) * size_t(width) + size_t(x)] = CaveKind::Floor;
	}
	void disc(int32_t cx, int32_t cy, int32_t radiusSquared) {
		const int32_t reach = int32_t(std::sqrt(double(radiusSquared))) + 1;
		for (int32_t dy = -reach; dy <= reach; ++dy) for (int32_t dx = -reach; dx <= reach; ++dx) if (dx * dx + dy * dy <= radiusSquared) put(cx + dx, cy + dy);
	}
	uint32_t noise(uint64_t stream, int32_t x, int32_t y, uint32_t step) const {
		return surfaceValueNoise(seed, stream, uint32_t(x0 + x), uint32_t(y0 + y), step);
	}
	// Tunnel: an axis from a to b that swings sideways (0 at both ends), 3 cells wide, with a room about every 24 cells of axis.
	void tunnel(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
		const bool alongX = std::abs(bx - ax) >= std::abs(by - ay);
		const int32_t major = alongX ? std::abs(bx - ax) : std::abs(by - ay);
		if (major == 0) { disc(ax, ay, 2); return; }
		const int32_t majorStep = (alongX ? bx - ax : by - ay) > 0 ? 1 : -1;
		const int32_t minorDelta = alongX ? by - ay : bx - ax;
		const int32_t amplitude = std::min<int32_t>(3, major / 6);
		int32_t previousMinor = 0;
		for (int32_t pos = 0; pos <= major; ++pos) {
			const int32_t baseMinor = (alongX ? ay : ax) + (minorDelta * pos + (minorDelta >= 0 ? major / 2 : -major / 2)) / major;
			const int32_t baseMajor = (alongX ? ax : ay) + majorStep * pos;
			const int32_t nx = alongX ? baseMajor : baseMinor, ny = alongX ? baseMinor : baseMajor;
			const int64_t envelope = int64_t(4) * pos * (major - pos) * 1024 / (int64_t(major) * major);
			const int64_t swing = (int64_t(noise(tunnelStream, nx, ny, 8)) - noiseBins / 2) * 2 * amplitude * envelope / (int64_t(noiseBins) * 1024);
			const int32_t minor = baseMinor + int32_t(swing);
			const int32_t from = pos == 0 ? minor : previousMinor;
			const int32_t direction = minor >= from ? 1 : -1;
			for (int32_t m = from; m != minor + direction; m += direction) {
				const int32_t cx = alongX ? baseMajor : m, cy = alongX ? m : baseMajor;
				for (int32_t dy = -1; dy <= 1; ++dy) for (int32_t dx = -1; dx <= 1; ++dx) put(cx + dx, cy + dy);
			}
			previousMinor = minor;
			if (pos % 24 == 12) {
				const int32_t cx = alongX ? baseMajor : minor, cy = alongX ? minor : baseMajor;
				const int32_t radius = 3 + int32_t(noise(roomStream, cx, cy, 6) % 3);
				disc(cx, cy, radius * radius);
			}
		}
	}
};

} // namespace

bool CaveGrid::valid() const noexcept {
	return region.x >= 0 && region.y >= 0 && region.z >= 0 && region.width > 0 && region.height > 0 &&
		uint64_t(region.width) * region.height == cells.size() &&
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
	return hash;
}

std::expected<CaveGrid, SurfaceError> generateCave(const SurfaceGrid& surface, uint64_t seed, std::stop_token stop) {
	if (!surface.valid()) return std::unexpected(SurfaceError::InvalidRegion);
	CaveGrid cave;
	cave.region = surface.region;
	++cave.region.z;
	try {
		cave.cells.assign(surface.cells.size(), CaveKind::Solid);
		const int32_t width = int32_t(surface.region.width), height = int32_t(surface.region.height);
		Carver carver {cave.cells, width, height, seed, surface.region.x, surface.region.y};
		std::vector<uint32_t> entrances = surface.entrances;
		std::ranges::sort(entrances);
		for (const uint32_t entrance : entrances) carver.disc(int32_t(entrance % uint32_t(width)), int32_t(entrance / uint32_t(width)), 5);
		for (size_t i = 0; i + 1 < entrances.size(); ++i) {
			if (stop.stop_requested()) return std::unexpected(SurfaceError::Cancelled);
			carver.tunnel(int32_t(entrances[i] % uint32_t(width)), int32_t(entrances[i] / uint32_t(width)),
				int32_t(entrances[i + 1] % uint32_t(width)), int32_t(entrances[i + 1] / uint32_t(width)));
		}
		if (entrances.size() == 1) {
			const int32_t ax = int32_t(entrances[0] % uint32_t(width)), ay = int32_t(entrances[0] / uint32_t(width));
			int32_t tx = ax + std::clamp(width / 2 - ax, -40, 40), ty = ay + std::clamp(height / 2 - ay, -40, 40);
			if (tx == ax && ty == ay) tx = std::clamp(ax + 12, 2, width - 3);
			carver.tunnel(ax, ay, std::clamp(tx, 2, width - 3), std::clamp(ty, 2, height - 3));
		}
	} catch (const std::bad_alloc&) {
		return std::unexpected(SurfaceError::AllocationFailed);
	}
	return cave;
}

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
    uint64_t limitCells = 0
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
        out.inset = inset;
        out.cells.assign(n, PlateauKind::None);
        out.landings.assign(n, 0);
        out.ramps.clear();
        out.components = 0;
        std::vector<uint8_t> candidate(n), reserved(n);
        for (const auto* cells : {&surface.doors, &surface.gaps, &surface.fires, &surface.entrances})
            for (auto i : *cells) reserved[i] = 1;
        for (size_t i = 0; i < n; ++i) candidate[i] = distance[i] >= inset;
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
                if (lowerOpen.empty() ? surface.cells[i] != SurfaceKind::Rock : lowerOpen[i] != 0) {
                    order.push_back(uint32_t(i));
                }
            }
            // Stable Fisher-Yates, independent from all existing generator streams.
            for (size_t i = order.size(); i > 1; --i) {
                std::swap(order[i - 1], order[size_t(random.bounded(i))]);
            }
            const auto first = out.ramps.size();
            const auto wanted = std::min<size_t>(4, 1 + top.size() / 400);
            for (auto p : order) {
                int x = int(p % w), y = int(p / w);
                if (!open(x, y)) {
                    continue;
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
    std::stop_token stop, uint32_t maxPercent
)
{
    return buildPlateau(surface, seed, minTop, shapes, occupied, stop, maxPercent);
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
