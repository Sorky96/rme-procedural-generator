// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_learning.h"
#include "procedural/core/doodad_class.h"
#include "procedural/core/grass_patches.h"
#include <algorithm>
#include <fstream>
#include <format>
#include <map>
#include <optional>
#include <numeric>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace rme::procedural {
namespace {
// Smallest share a second brush needs to be worth a mix; rarer ones read as noise, not style.
constexpr uint64_t minMixPermille = 50;

uint32_t permilleOf(uint64_t part, uint64_t whole) { return whole ? uint32_t(std::min<uint64_t>(1000, part * 1000 / whole)) : 0; }

// The most used candidates first, each with its share of `total`, while the share reaches minMixPermille.
template <size_t N>
std::array<StyleMix, N> topMix(std::vector<const KeyTiles*> candidates, uint64_t total) {
    std::ranges::sort(candidates, [](const KeyTiles* a, const KeyTiles* b) { return a->tiles != b->tiles ? a->tiles > b->tiles : a->key < b->key; });
    std::array<StyleMix, N> mix;
    size_t used = 0;
    for (const auto* t : candidates) {
        const uint32_t share = permilleOf(t->tiles, total);
        if (used == N || share < minMixPermille) break;
        mix[used++] = {t->key, share};
    }
    return mix;
}

// Named grass brushes after the palette's one (jungle grass, swamp mapped to Grass), each with its share
// of all grass tiles.
std::array<StyleMix, maxGrassMix> chooseGrassMix(std::span<const KeyMapEntry> mapping, std::span<const KeyTiles> keys, const std::string& primary) {
    if (primary.empty()) return {};
    uint64_t total = 0;
    std::vector<const KeyTiles*> candidates;
    for (const auto& t : keys) {
        const auto entry = std::ranges::find(mapping, t.key, &KeyMapEntry::key);
        if (entry == mapping.end() || entry->kind != StyleKind::Grass) continue;
        total += t.tiles;
        if (t.key != primary && !t.key.starts_with("item ")) candidates.push_back(&t);
    }
    return topMix<maxGrassMix>(std::move(candidates), total);
}

// Brushes of one decor kind after the palette's one, each with its share of all items of that kind.
std::array<StyleMix, maxTreeMix> chooseKindMix(std::span<const KeyTiles> items, const std::string& primary, DecorKind kind) {
    if (primary.empty()) return {};
    std::vector<const KeyTiles*> candidates;
    uint64_t total = 0;
    for (const auto& t : items) {
        if (classifyDoodadBrush(t.key) != kind) continue;
        total += t.tiles;
        if (t.key != primary) candidates.push_back(&t);
    }
    return topMix<maxTreeMix>(std::move(candidates), total);
}
// How clumped trees and bushes on grass are: r = P(an 8-neighbour on grass is planted | the cell is
// planted) / P(a grass cell is planted). r is 1 for an even spread; 3 and more for thickets with clearings.
uint32_t vegetationClump(const SemanticMap& map, std::span<const KeyMapEntry> mapping) {
    const auto& floor = map.floors().front();
    std::vector<uint8_t> grassKey(map.groundKeys().size(), 0);
    for (size_t k = 0; k < grassKey.size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        grassKey[k] = entry != mapping.end() && entry->kind == StyleKind::Grass;
    }
    const uint32_t plantBits = bit(Trait::TreeDoodad) | bit(Trait::BushDoodad);
    const auto grass = [&](size_t i) { return (floor.traits[i] & bit(Trait::Present)) && floor.ground[i] < grassKey.size() && grassKey[floor.ground[i]]; };
    const int w = floor.bounds.width, h = floor.bounds.height;
    uint64_t cells = 0, planted = 0, pairs = 0, plantedPairs = 0;
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const size_t i = size_t(y) * w + x;
        if (!grass(i)) continue;
        ++cells;
        if (!(floor.traits[i] & plantBits)) continue;
        ++planted;
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            const int nx = x + dx, ny = y + dy;
            if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            const size_t j = size_t(ny) * w + nx;
            if (!grass(j)) continue;
            ++pairs;
            plantedPairs += (floor.traits[j] & plantBits) != 0;
        }
    }
    if (!planted || !pairs || planted == cells) return 0;
    // r - 1 scaled by 500: r = 3 is full clumping.
    const double r = (double(plantedPairs) / double(pairs)) / (double(planted) / double(cells));
    return uint32_t(std::clamp((r - 1.0) * 500.0, 0.0, 1000.0));
}

// Distance in 4-steps from each land tile (present, not water) to the nearest water, as the 25th, 50th, 75th and
// 90th percentile; all 0 without water or land.
// Land edge per tile of a set of tiles, in permille: (tile in the set, 4-neighbour on land outside it) pairs per
// tile of the set. Small scattered patches score high, a few big ones low. 0 for an empty set.
template <class InSet, class Land>
uint32_t patchEdgePermille(int w, int h, InSet inSet, Land land) {
    uint64_t tiles = 0, edges = 0;
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const size_t i = size_t(y) * w + x;
        if (!inSet(i)) continue;
        ++tiles;
        for (const auto [dx, dy] : {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            const size_t j = size_t(ny) * w + nx;
            edges += land(j) && !inSet(j);
        }
    }
    return tiles ? uint32_t(edges * 1000 / tiles) : 0;
}
template <class Present, class Water>
std::array<uint32_t, 4> shoreDistances(int w, int h, Present present, Water water) {
    std::vector<int32_t> dist(size_t(w) * h, -1);
    std::vector<size_t> queue;
    for (size_t i = 0; i < dist.size(); ++i) if (water(i)) { dist[i] = 0; queue.push_back(i); }
    for (size_t head = 0; head < queue.size(); ++head) {
        const size_t i = queue[head];
        const int x = int(i % w), y = int(i / w);
        for (const auto [dx, dy] : {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            const size_t j = size_t(ny) * w + nx;
            if (dist[j] < 0 && present(j)) { dist[j] = dist[i] + 1; queue.push_back(j); }
        }
    }
    std::vector<uint32_t> land;
    for (size_t i = 0; i < dist.size(); ++i) if (dist[i] > 0) land.push_back(uint32_t(dist[i]));
    std::array<uint32_t, 4> out {};
    if (land.empty()) return out;
    std::ranges::sort(land);
    constexpr uint32_t percent[] {25, 50, 75, 90};
    for (size_t k = 0; k < out.size(); ++k) out[k] = land[land.size() * percent[k] / 100];
    return out;
}
// Water tiles 4-connected to the edge of the area or to a tile without ground: the sea around an island, not its ponds.
template <class Present, class Water>
uint64_t seaTiles(int w, int h, Present present, Water water) {
    std::vector<uint8_t> sea(size_t(w) * h, 0);
    std::vector<size_t> stack;
    constexpr std::pair<int, int> steps[] {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const size_t i = size_t(y) * w + x;
        if (!water(i)) continue;
        bool open = x == 0 || y == 0 || x + 1 == w || y + 1 == h;
        for (const auto [dx, dy] : steps)
            if (!open && !present(size_t(y + dy) * w + (x + dx))) open = true;
        if (open && !sea[i]) { sea[i] = 1; stack.push_back(i); }
    }
    uint64_t count = 0;
    while (!stack.empty()) {
        const size_t i = stack.back(); stack.pop_back(); ++count;
        const int x = int(i % w), y = int(i / w);
        for (const auto [dx, dy] : steps) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            const size_t j = size_t(ny) * w + nx;
            if (!sea[j] && water(j)) { sea[j] = 1; stack.push_back(j); }
        }
    }
    return count;
}
// Overlay items on land by use (rare ones such as spider eggs included); each kept with at least 3 items and its rates on the land tiles with and
// without a tree. Items on water or on tiles without mapped ground are left out, as for vegetation.
std::array<StyleOverlay, maxOverlays> chooseOverlays(const SemanticMap& map, std::span<const KeyMapEntry> mapping,
    std::span<const GroundOverlayItems> items) {
    std::vector<uint8_t> landKey(map.groundKeys().size(), 0);
    for (size_t k = 0; k < landKey.size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        landKey[k] = entry != mapping.end() && size_t(entry->kind) < naturalKindCount && entry->kind != StyleKind::Water;
    }
    const auto& floor = map.floors().front();
    uint64_t treeTiles = 0, groundTiles = 0;
    for (size_t i = 0; i < floor.ground.size(); ++i) {
        if (!(floor.traits[i] & bit(Trait::Present)) || floor.ground[i] >= landKey.size() || !landKey[floor.ground[i]]) continue;
        ++((floor.traits[i] & bit(Trait::TreeDoodad)) ? treeTiles : groundTiles);
    }
    std::map<std::string, std::pair<uint64_t, uint64_t>, std::less<>> byName;
    for (const auto& item : items) {
        if (item.ground >= landKey.size() || !landKey[item.ground]) continue;
        auto& counts = byName[item.name];
        counts.first += item.onTree; counts.second += item.onGround;
    }
    std::vector<std::pair<uint64_t, std::string_view>> ranked;
    for (const auto& [name, counts] : byName)
        if (counts.first + counts.second >= 3) ranked.emplace_back(counts.first + counts.second, name);
    std::ranges::sort(ranked, [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    std::array<StyleOverlay, maxOverlays> out;
    size_t used = 0;
    for (const auto& [total, name] : ranked) {
        if (used == maxOverlays) break;
        const auto& [onTree, onGround] = byName.find(name)->second;
        const auto rate = [](uint64_t n, uint64_t of) { return of ? uint32_t(std::min<uint64_t>(1000000, (n * 1000000 + of / 2) / of)) : 0u; };
        StyleOverlay overlay {std::string(name), rate(onTree, treeTiles), rate(onGround, groundTiles)};
        if (overlay.onTreePpm + overlay.onGroundPpm == 0) continue;
        out[used++] = std::move(overlay);
    }
    return out;
}
// Overlay items inside buildings by use: items on tiles of a built floor (an Other ground that is not a cave or hole),
// each kept with at least 3 items and its share of those floor tiles.
// The mountain outline: the share of the land tiles (not Rock, not water) beside a Rock tile that carry an optional border.
uint32_t measureRockOutline(const SemanticMap& map, std::span<const KeyMapEntry> mapping) {
    const auto& floor = map.floors().front();
    const int w = floor.bounds.width, h = floor.bounds.height;
    std::vector<uint8_t> kindOf(map.groundKeys().size(), uint8_t(StyleKind::Unmapped));
    for (size_t k = 0; k < kindOf.size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        if (entry != mapping.end()) kindOf[k] = uint8_t(entry->kind);
    }
    const auto kind = [&](size_t i) {
        return (floor.traits[i] & bit(Trait::Present)) && floor.ground[i] < kindOf.size() ? StyleKind(kindOf[floor.ground[i]]) : StyleKind::Unmapped;
    };
    uint64_t beside = 0, outlined = 0;
    for (size_t i = 0; i < size_t(w) * h; ++i) {
        const StyleKind own = kind(i);
        if (own == StyleKind::Rock || own == StyleKind::Water || own == StyleKind::Unmapped) continue;
        const int x = int(i % w), y = int(i / w);
        bool rock = false;
        for (int dy = -1; dy <= 1 && !rock; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            const int nx = x + dx, ny = y + dy;
            if (nx >= 0 && ny >= 0 && nx < w && ny < h && kind(size_t(ny) * w + nx) == StyleKind::Rock) { rock = true; break; }
        }
        if (!rock) continue;
        ++beside;
        outlined += (floor.traits[i] & bit(Trait::OptionalBorder)) != 0;
    }
    return beside ? uint32_t((outlined * 1000 + beside / 2) / beside) : 0;
}
// The carpet of the buildings: the carpet brush with the most pieces on built floors (as chooseInteriors), at least 3,
// with the share of the floor tiles it covers.
StyleOverlay chooseCarpet(const SemanticMap& map, std::span<const KeyMapEntry> mapping, std::span<const GroundDoodadItems> carpets) {
    std::vector<uint8_t> floorKey(map.groundKeys().size(), 0);
    for (size_t k = 0; k < floorKey.size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        floorKey[k] = entry != mapping.end() && entry->kind == StyleKind::Other && entry->rule != "cave" && entry->rule != "hole";
    }
    const auto& floor = map.floors().front();
    uint64_t floorTiles = 0;
    for (size_t i = 0; i < floor.ground.size(); ++i)
        floorTiles += (floor.traits[i] & bit(Trait::Present)) && floor.ground[i] < floorKey.size() && floorKey[floor.ground[i]];
    std::map<std::string, uint64_t, std::less<>> byName;
    for (const auto& piece : carpets)
        if (piece.ground < floorKey.size() && floorKey[piece.ground]) byName[piece.doodad] += piece.items;
    const auto top = std::ranges::max_element(byName, {}, [](const auto& entry) { return entry.second; });
    if (top == byName.end() || top->second < 3 || !floorTiles) return {};
    return {top->first, 0, uint32_t(std::min<uint64_t>(1000000, (top->second * 1000000 + floorTiles / 2) / floorTiles))};
}
std::array<StyleOverlay, maxOverlays> chooseInteriors(const SemanticMap& map, std::span<const KeyMapEntry> mapping,
    std::span<const GroundOverlayItems> items) {
    std::vector<uint8_t> floorKey(map.groundKeys().size(), 0);
    for (size_t k = 0; k < floorKey.size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        floorKey[k] = entry != mapping.end() && entry->kind == StyleKind::Other && entry->rule != "cave" && entry->rule != "hole";
    }
    const auto& floor = map.floors().front();
    uint64_t floorTiles = 0;
    for (size_t i = 0; i < floor.ground.size(); ++i)
        floorTiles += (floor.traits[i] & bit(Trait::Present)) && floor.ground[i] < floorKey.size() && floorKey[floor.ground[i]];
    std::map<std::string, uint64_t, std::less<>> byName;
    for (const auto& item : items)
        if (item.ground < floorKey.size() && floorKey[item.ground]) byName[item.name] += item.onTree + item.onGround;
    std::vector<std::pair<uint64_t, std::string_view>> ranked;
    for (const auto& [name, count] : byName) if (count >= 3) ranked.emplace_back(count, name);
    std::ranges::sort(ranked, [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    std::array<StyleOverlay, maxOverlays> out;
    for (size_t k = 0; k < std::min(ranked.size(), out.size()) && floorTiles; ++k)
        out[k] = {std::string(ranked[k].second), 0, uint32_t(std::min<uint64_t>(1000000, (ranked[k].first * 1000000 + floorTiles / 2) / floorTiles))};
    return out;
}
// Broken walls: blocking items standing in the line of a wall, with a wall piece or another such item on both sides
// along one axis and not along the other, in a run that reaches a wall piece or door within 8 tiles (corners, ends,
// items beside the wall and rows of rocks do not count). onTreePpm is the share of the straight horizontal wall tiles
// holding the item, onGroundPpm that of the vertical ones. Walls of caves and mountains are left out; each item is
// kept with at least 3 of it.
std::array<StyleOverlay, maxOverlays> chooseWallDecor(const SemanticMap& map, std::span<const KeyMapEntry> mapping,
    std::span<const PlacedItem> placed) {
    const auto& floor = map.floors().front();
    const int w = floor.bounds.width, h = floor.bounds.height;
    std::vector<uint8_t> cave(map.groundKeys().size(), 0);
    for (size_t k = 0; k < cave.size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        cave[k] = entry != mapping.end() && (entry->kind == StyleKind::Rock || entry->rule == "cave" || entry->rule == "hole");
    }
    const size_t cells = size_t(w) * h;
    const uint32_t walled = bit(Trait::Wall) | bit(Trait::Door);
    std::vector<uint8_t> line(cells, 0); // a wall piece, a door or a blocking item, not in a cave
    const auto open = [&](size_t i) {
        return (floor.traits[i] & bit(Trait::Present)) && !(floor.ground[i] < cave.size() && cave[floor.ground[i]]);
    };
    for (size_t i = 0; i < cells; ++i) line[i] = open(i) && (floor.traits[i] & walled);
    for (const auto& item : placed) if (item.blocking && item.cell < cells && open(item.cell)) line[item.cell] = 1;
    // 1 = straight horizontal, 2 = straight vertical, 0 = neither (or both), or a run of blocking items that reaches no
    // wall piece or door within 8 tiles along it (a row of rocks or a wreck is no wall).
    const auto axis = [&](size_t i) {
        const int x = int(i % w), y = int(i / w);
        const bool across = x > 0 && x + 1 < w && line[i - 1] && line[i + 1];
        const bool down = y > 0 && y + 1 < h && line[i - w] && line[i + w];
        if (across == down) return 0;
        const int dx = across ? 1 : 0, dy = across ? 0 : 1;
        for (const int sign : {-1, 1}) for (int step = 0; step <= 8; ++step) {
            const int nx = x + sign * step * dx, ny = y + sign * step * dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) break;
            const size_t j = size_t(ny) * w + nx;
            if (!line[j]) break;
            if (floor.traits[j] & walled) return across ? 1 : 2;
        }
        return 0;
    };
    std::array<uint64_t, 3> straight {};
    for (size_t i = 0; i < cells; ++i) if (line[i]) ++straight[axis(i)];
    std::map<std::string, std::array<uint64_t, 3>, std::less<>> byName;
    for (const auto& item : placed)
        if (item.blocking && item.cell < cells && line[item.cell]) ++byName[item.name][axis(item.cell)];
    std::vector<std::pair<uint64_t, std::string_view>> ranked;
    for (const auto& [name, counts] : byName)
        if (counts[1] + counts[2] >= 3) ranked.emplace_back(counts[1] + counts[2], name);
    std::ranges::sort(ranked, [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    const auto rate = [](uint64_t n, uint64_t of) { return of ? uint32_t(std::min<uint64_t>(1000000, (n * 1000000 + of / 2) / of)) : 0u; };
    std::array<StyleOverlay, maxOverlays> out;
    for (size_t k = 0; k < std::min(ranked.size(), out.size()); ++k) {
        const auto& counts = byName.find(ranked[k].second)->second;
        out[k] = {std::string(ranked[k].second), rate(counts[1], straight[1]), rate(counts[2], straight[2])};
    }
    return out;
}
// Buildings: groups (pieces at most two tiles apart) of at least 9 tiles of a built ground (Other, but not a cave or hole), a blocking wall
// or a door. Cave floors and walls standing on rock or in caves are the mountains', not buildings.
StyleBuildings findBuildings(const SemanticMap& map, std::span<const KeyMapEntry> mapping, std::span<const GroundDoodadItems> wallBrushes) {
    constexpr size_t smallest = 9;
    constexpr uint32_t walledPercent = 80; // an outline walled less than this is a ruin
    const auto& floor = map.floors().front();
    const int w = floor.bounds.width, h = floor.bounds.height;
    std::vector<uint8_t> other(map.groundKeys().size(), 0), cave(map.groundKeys().size(), 0);
    for (size_t k = 0; k < other.size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        if (entry == mapping.end()) continue;
        cave[k] = entry->kind == StyleKind::Rock || entry->rule == "cave" || entry->rule == "hole";
        other[k] = entry->kind == StyleKind::Other && !cave[k];
    }
    const uint32_t walled = bit(Trait::Wall) | bit(Trait::Door);
    const auto onCave = [&](size_t i) { return floor.ground[i] < cave.size() && cave[floor.ground[i]]; };
    const auto built = [&](size_t i) {
        return (floor.traits[i] & bit(Trait::Present)) &&
            (((floor.traits[i] & walled) && !onCave(i)) || (floor.ground[i] < other.size() && other[floor.ground[i]]));
    };
    std::vector<uint8_t> seen(size_t(w) * h, 0);
    std::vector<size_t> group, stack;
    std::map<uint16_t, uint64_t> floors;
    uint64_t buildings = 0, ruins = 0, present = 0, halls = 0, ruinArea = 0, ruinFloor = 0;
    constexpr uint64_t roomArea = 99; // the largest single room the generator builds (11 x 9)
    constexpr std::pair<int, int> steps[] {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (size_t start = 0; start < seen.size(); ++start) {
        present += (floor.traits[start] & bit(Trait::Present)) != 0;
        if (seen[start] || !built(start)) continue;
        group.clear();
        seen[start] = 1; stack.push_back(start);
        while (!stack.empty()) {
            const size_t i = stack.back(); stack.pop_back(); group.push_back(i);
            const int x = int(i % w), y = int(i / w);
            // Pieces up to two tiles apart belong together: a ruin's broken walls and floor patches are one building,
            // and rooms built wall to wall one hall.
            for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) {
                const int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                const size_t j = size_t(ny) * w + nx;
                if (!seen[j] && built(j)) { seen[j] = 1; stack.push_back(j); }
            }
        }
        if (group.size() < smallest) continue;
        ++buildings;
        uint64_t outline = 0, closed = 0;
        for (const size_t i : group) {
            const int x = int(i % w), y = int(i / w);
            bool edge = false;
            for (const auto [dx, dy] : steps) {
                const int nx = x + dx, ny = y + dy;
                edge = edge || nx < 0 || ny < 0 || nx >= w || ny >= h || !built(size_t(ny) * w + nx);
            }
            if (edge) { ++outline; closed += (floor.traits[i] & walled) != 0; }
            if (floor.ground[i] < other.size() && other[floor.ground[i]]) ++floors[floor.ground[i]];
        }
        const bool ruin = closed * 100 < outline * walledPercent;
        ruins += ruin;
        uint32_t x0 = UINT32_MAX, y0 = UINT32_MAX, x1 = 0, y1 = 0;
        uint64_t floorTiles = 0;
        for (const size_t i : group) {
            const uint32_t x = uint32_t(i % w), y = uint32_t(i / w);
            x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y);
            floorTiles += floor.ground[i] < other.size() && other[floor.ground[i]];
        }
        const uint64_t area = uint64_t(x1 - x0 + 1) * (y1 - y0 + 1);
        halls += area > roomArea;
        if (ruin) { ruinArea += area; ruinFloor += floorTiles; }
    }
    StyleBuildings out;
    const auto top = std::ranges::max_element(floors, {}, [](const auto& entry) { return entry.second; });
    if (!buildings || !present || top == floors.end()) return out;
    out.density = uint32_t(std::clamp<uint64_t>((buildings * 65536 + present / 2) / present, 1, 100));
    out.ruinPercent = uint32_t(ruins * 100 / buildings);
    out.compoundPercent = uint32_t(halls * 100 / buildings);
    out.floorPercent = ruinArea ? uint32_t(std::clamp<uint64_t>((ruinFloor * 100 + ruinArea / 2) / ruinArea, 1, 100)) : 100;
    out.floor = map.groundKeys()[top->first];
    std::map<std::string, uint64_t> walls;
    for (const auto& piece : wallBrushes)
        if (piece.ground >= cave.size() || !cave[piece.ground]) walls[piece.doodad] += piece.items;
    const auto wall = std::ranges::max_element(walls, {}, [](const auto& entry) { return entry.second; });
    if (wall != walls.end()) out.wall = wall->first;
    return out;
}
// The sea and the land-to-water distances of the sample.
void measureWater(const SemanticMap& map, std::span<const KeyMapEntry> mapping, LearningSample& sample) {
    const auto& floor = map.floors().front();
    std::vector<uint8_t> waterKey(map.groundKeys().size(), 0);
    for (size_t k = 0; k < waterKey.size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        waterKey[k] = entry != mapping.end() && entry->kind == StyleKind::Water;
    }
    const auto present = [&](size_t i) { return (floor.traits[i] & bit(Trait::Present)) != 0; };
    const auto water = [&](size_t i) { return present(i) && floor.ground[i] < waterKey.size() && waterKey[floor.ground[i]]; };
    sample.seaTiles = seaTiles(floor.bounds.width, floor.bounds.height, present, water);
    sample.shoreDistance = shoreDistances(floor.bounds.width, floor.bounds.height, present, water);
    sample.width = uint32_t(floor.bounds.width); sample.height = uint32_t(floor.bounds.height);
    std::vector<uint8_t> kindOf(map.groundKeys().size(), uint8_t(StyleKind::Unmapped));
    for (size_t k = 0; k < kindOf.size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        if (entry != mapping.end()) kindOf[k] = uint8_t(entry->kind);
    }
    const auto kind = [&](size_t i) { return present(i) && floor.ground[i] < kindOf.size() ? StyleKind(kindOf[floor.ground[i]]) : StyleKind::NoGround; };
    // Grass mix grounds together (their own names, as chosen for the profile) against all land.
    std::vector<uint8_t> mixKey(map.groundKeys().size(), 0);
    for (size_t k = 0; k < mixKey.size(); ++k)
        for (const auto& mix : sample.grassMix) mixKey[k] = mixKey[k] || (!mix.brush.empty() && map.groundKeys()[k] == mix.brush);
    sample.mixEdgePermille = patchEdgePermille(floor.bounds.width, floor.bounds.height,
        [&](size_t i) { return present(i) && floor.ground[i] < mixKey.size() && mixKey[floor.ground[i]]; },
        [&](size_t i) { const auto k = kind(i); return k == StyleKind::Grass || k == StyleKind::Dirt; });
    sample.rockEdgePermille = patchEdgePermille(floor.bounds.width, floor.bounds.height, [&](size_t i) { return kind(i) == StyleKind::Rock; },
        [&](size_t i) { const auto k = kind(i); return size_t(k) < naturalKindCount && k != StyleKind::Water; });
    sample.dirtEdgePermille = patchEdgePermille(floor.bounds.width, floor.bounds.height, [&](size_t i) { return kind(i) == StyleKind::Dirt; },
        [&](size_t i) { const auto k = kind(i); return k == StyleKind::Grass || k == StyleKind::Dirt; });
}
} // namespace

void promoteMainLand(std::vector<KeyMapEntry>& mapping, const SemanticMap& map) {
    const auto summary = summarize(map);
    uint64_t grass = 0, shore = 0;
    for (size_t k = 0; k < map.groundKeys().size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        if (entry == mapping.end()) continue;
        if (entry->kind == StyleKind::Grass) grass += summary.groundKeyStats[k].tiles;
        if (entry->kind == StyleKind::Shore) shore += summary.groundKeyStats[k].tiles;
    }
    if (grass * 10 >= shore) return;
    for (auto& entry : mapping) if (entry.kind == StyleKind::Shore) entry.kind = StyleKind::Grass;
}

std::expected<LearningSample, std::string> inspectLearningSample(const SemanticMap& map,
    std::span<const KeyMapEntry> mapping, std::span<const GroundDoodadItems> doodadItems, bool permission, std::stop_token stop,
    std::span<const GroundOverlayItems> overlayItems, std::span<const GroundDoodadItems> wallBrushes, std::span<const PlacedItem> placedItems,
    std::span<const GroundDoodadItems> carpetItems) try {
    if (!permission) return std::unexpected("Confirm your right to use this map before analysis.");
    if (stop.stop_requested()) return std::unexpected("Analysis cancelled.");
    if (map.floors().size() != 1) return std::unexpected("Choose one surface rectangle.");
    const auto& b = map.floors().front().bounds;
    if (b.z < 0 || b.z > 7 || b.width > 512 || b.height > 512)
        return std::unexpected("Choose a surface floor 0-7 and a rectangle up to 512 x 512.");
    for (const auto& entry : mapping)
        if (size_t(entry.kind) >= styleKindCount || (entry.key.empty() && entry.kind != StyleKind::NoGround))
            return std::unexpected("Unresolved ground must remain no-ground; use valid G4 classes for named brushes.");
    if (!serializeKeyMap(mapping)) return std::unexpected("Invalid or duplicate mapping keys.");
    auto stats = accumulateKindStats(map, mapping, b.z, b.z, stop);
    if (!stats) return std::unexpected("Invalid mapping or analysis cancelled.");
    LearningSample sample;
    sample.stats = *stats;
    sample.present = std::accumulate(stats->kindTiles.begin(), stats->kindTiles.end(), uint64_t{0});
    sample.recognized = std::accumulate(stats->kindTiles.begin(), stats->kindTiles.begin() + naturalKindCount, uint64_t{0});
    if (sample.recognized < 1024 || sample.recognized * 2 < sample.present)
        return std::unexpected(std::format("{} recognized, {} skipped, {} present. Need at least 1024 natural tiles and 50% coverage.",
            sample.recognized, sample.present - sample.recognized, sample.present));
    std::vector<KeyTiles> keys;
    const auto summary = summarize(map);
    for (size_t i = 0; i < map.groundKeys().size(); ++i)
        keys.push_back({map.groundKeys()[i], summary.groundKeyStats[i].tiles});
    sample.palette = choosePalette(mapping, keys);
    auto decor = extractDecorContext(map, mapping, b.z, b.z, stop);
    if (!decor) return std::unexpected("Invalid mapping or analysis cancelled.");
    sample.decor = *decor;
    sample.grassMix = chooseGrassMix(mapping, keys, sample.palette[size_t(StyleKind::Grass)]);
    // Ground key of each mix brush; its tiles keep their own vegetation, the rest is the main style.
    std::array<std::optional<uint16_t>, maxGrassMix> mixKey;
    for (size_t m = 0; m < maxGrassMix; ++m)
        for (size_t k = 0; k < map.groundKeys().size(); ++k)
            if (!sample.grassMix[m].brush.empty() && map.groundKeys()[k] == sample.grassMix[m].brush) mixKey[m] = uint16_t(k);
    const auto slotOf = [&](uint16_t ground) -> std::optional<size_t> {
        for (size_t m = 0; m < maxGrassMix; ++m) if (mixKey[m] == ground) return m;
        return std::nullopt;
    };
    // Generated decoration stands on land, so brushes placed on water never stand in for it. Mixed
    // forests rarely give one brush a quarter of the trees; the most used one still beats the built-in.
    std::map<std::string, uint64_t, std::less<>> mainItems;
    std::array<std::map<std::string, uint64_t, std::less<>>, maxGrassMix> mixItems;
    // An item on a Water tile or without ground counts as standing on water too, whatever it is called
    // (RAW items are named by their id).
    const auto onLand = [&](uint16_t ground) {
        if (ground >= map.groundKeys().size()) return false;
        const auto entry = std::ranges::find(mapping, map.groundKeys()[ground], &KeyMapEntry::key);
        return entry == mapping.end() || entry->kind != StyleKind::Water;
    };
    for (const auto& item : doodadItems) {
        if (item.doodad.find("water") != std::string::npos || !onLand(item.ground)) continue;
        const auto slot = slotOf(item.ground);
        (slot ? mixItems[*slot] : mainItems)[item.doodad] += item.items;
    }
    const auto asKeyTiles = [](const auto& counts) {
        std::vector<KeyTiles> out;
        for (const auto& [name, items] : counts) out.push_back({name, items});
        return out;
    };
    const auto landItems = asKeyTiles(mainItems);
    // A small sample has few items per brush; 20 still separates a used brush from a stray one.
    sample.decorPalette = chooseDecorPalette(landItems, 20, 0);
    sample.treeMix = chooseKindMix(landItems, sample.decorPalette[0], DecorKind::Tree);
    sample.bushMix = chooseKindMix(landItems, sample.decorPalette[1], DecorKind::Bush);
    // Per-tile rates of trees, bushes and stones on the main grass and on each mix ground.
    const auto ratesOn = [&](uint16_t ground) {
        std::array<uint64_t, 4> counts {}; // tiles, trees, bushes, stones
        const auto& floor = map.floors().front();
        for (size_t i = 0; i < floor.ground.size(); ++i) {
            if (floor.ground[i] != ground || !(floor.traits[i] & bit(Trait::Present))) continue;
            ++counts[0];
            counts[1] += (floor.traits[i] & bit(Trait::TreeDoodad)) != 0;
            counts[2] += (floor.traits[i] & bit(Trait::BushDoodad)) != 0;
            counts[3] += (floor.traits[i] & bit(Trait::StoneDoodad)) != 0;
        }
        return counts;
    };
    std::optional<uint16_t> mainKey;
    for (size_t k = 0; k < map.groundKeys().size(); ++k)
        if (map.groundKeys()[k] == sample.palette[size_t(StyleKind::Grass)]) mainKey = uint16_t(k);
    const auto base = mainKey ? ratesOn(*mainKey) : std::array<uint64_t, 4> {};
    for (size_t m = 0; m < maxGrassMix; ++m) {
        if (!mixKey[m]) continue;
        const auto own = ratesOn(*mixKey[m]);
        auto& d = sample.grassMixDecor[m];
        for (size_t k = 0; k < 3; ++k) {
            // keep = (own rate) / (base rate), both per tile; a base rate of 0 has nothing to thin.
            if (own[0] && base[0] && base[1 + k]) d.keepPermille[k] = uint32_t(std::min<uint64_t>(1000, own[1 + k] * base[0] * 1000 / (own[0] * base[1 + k])));
        }
        const auto palette = chooseDecorPalette(asKeyTiles(mixItems[m]), 10, 0);
        for (size_t k = 0; k < 3; ++k) if (palette[k] != sample.decorPalette[k]) d.brushes[k] = palette[k];
    }
    sample.clumpPermille = vegetationClump(map, mapping);
    // Trees standing in the water beside land: their share of such water tiles and their most used brush or item.
    {
        std::vector<uint8_t> water(map.groundKeys().size(), 0), land(map.groundKeys().size(), 0);
        for (size_t k = 0; k < water.size(); ++k) {
            const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
            if (entry == mapping.end() || size_t(entry->kind) >= naturalKindCount) continue;
            (entry->kind == StyleKind::Water ? water : land)[k] = 1;
        }
        const auto& floor = map.floors().front();
        const int w = floor.bounds.width, h = floor.bounds.height;
        const auto is = [&](const std::vector<uint8_t>& keys, size_t i) {
            return (floor.traits[i] & bit(Trait::Present)) && floor.ground[i] < keys.size() && keys[floor.ground[i]];
        };
        uint64_t banks = 0, treed = 0;
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
            const size_t i = size_t(y) * w + x;
            if (!is(water, i)) continue;
            const bool bank = (x && is(land, i - 1)) || (x + 1 < w && is(land, i + 1)) || (y && is(land, i - w)) || (y + 1 < h && is(land, i + w));
            if (!bank) continue;
            ++banks;
            treed += (floor.traits[i] & bit(Trait::TreeDoodad)) != 0;
        }
        std::map<std::string, uint64_t, std::less<>> waterTrees;
        for (const auto& item : doodadItems)
            if (item.ground < water.size() && water[item.ground] && classifyDoodadBrush(item.doodad) == DecorKind::Tree) waterTrees[item.doodad] += item.items;
        const auto top = std::ranges::max_element(waterTrees, {}, [](const auto& entry) { return entry.second; });
        // At least 20 trees and 1% of the bank, as for the other learned vegetation, before the sample says "mangroves".
        if (banks && treed >= 20 && treed * 100 >= banks && top != waterTrees.end()) {
            sample.shoreTreePermille = uint32_t(std::min<uint64_t>(1000, (treed * 1000 + banks / 2) / banks));
            sample.shoreTree = top->first;
        }
    }
    measureWater(map, mapping, sample);
    sample.buildings = findBuildings(map, mapping, wallBrushes);
    sample.overlays = chooseOverlays(map, mapping, overlayItems);
    sample.interiors = chooseInteriors(map, mapping, overlayItems);
    sample.wallDecor = chooseWallDecor(map, mapping, placedItems);
    sample.carpet = chooseCarpet(map, mapping, carpetItems);
    sample.rockOutlinePermille = measureRockOutline(map, mapping);
    return sample;
} catch (const std::bad_alloc&) { return std::unexpected("Not enough memory for analysis."); }

std::expected<StyleProfile, std::string> fitLearningSample(const LearningSample& sample,
    std::vector<KeyMapEntry> mapping, std::string client, std::string otb, std::string dat, std::stop_token stop) {
    if (sample.recognized < 1024 || sample.recognized * 2 < sample.present)
        return std::unexpected("Insufficient sample coverage.");
    const auto target = targetFromStats(sample.stats);
    if (!target) return std::unexpected("No natural neighbour pairs in the sample.");
    FitOptions options;
    options.eval = {160, {1, 2, 3, 4, 5, 6}};
    options.maxPasses = 4;
    auto steps = fitTuning(*target, *target, SurfaceTuning{}, options, stop);
    if (!steps) return std::unexpected(stop.stop_requested() ? "Analysis cancelled." : "Surface fit failed.");
    StyleProfile profile;
    profile.client = std::move(client); profile.otbSha256 = std::move(otb); profile.datSha256 = std::move(dat);
    // Generic provenance: never persist map names, paths, coordinates, or reference aliases.
    profile.sources = {{"user-local-surface", 1000}};
    profile.keymap = std::move(mapping); profile.stats = sample.stats; profile.target = target;
    profile.tuning = steps->back().tuning; profile.palette = sample.palette;
    // A kind the sample does not contain at all is not generated: the fit only scores shares and may
    // keep some rock or dirt that improves the neighbour pairs, which reads as foreign terrain.
    if (target->sharesPpm[size_t(StyleKind::Rock)] == 0) { profile.tuning->rockPermille = 0; profile.tuning->riverCount = 0; }
    if (target->sharesPpm[size_t(StyleKind::Dirt)] == 0) profile.tuning->dirtPermille = 0;
    // Lakes start as the lowest cells: their share is the sample's water share. The fit scores shares and
    // neighbour pairs together and can trade all water away on a small sample, so this one is set directly.
    // Water open to the sample's edge is the sea around an island: it becomes the generator's sea, and only the
    // ponds inside the land count as lakes. A sea under 5% of the sample is treated as ponds cut by the edge.
    uint64_t natural = 0;
    for (size_t k = 0; k < naturalKindCount; ++k) natural += sample.stats.kindTiles[k];
    const uint64_t water = sample.stats.kindTiles[size_t(StyleKind::Water)];
    const uint64_t sea = natural && sample.seaTiles * 20 >= natural ? std::min(sample.seaTiles, water) : 0;
    profile.tuning->islandPermille = natural ? uint32_t(std::min<uint64_t>(900, (sea * 1000 + natural / 2) / natural)) : 0;
    const uint64_t land = natural - sea;
    profile.tuning->lakePermille = land ? uint32_t(std::min<uint64_t>(500, ((water - sea) * 1000 + land / 2) / land)) : 0;
    // Learned styles draw with border brushes: no one-cell spurs, strips or channels (set before the scale fits).
    profile.tuning->cleanEdges = 1;
    // Dirt is the driest share of the land; the fit can trade it away for neighbour pairs, so it is set directly too.
    const uint64_t dry = sample.stats.kindTiles[size_t(StyleKind::Dirt)], ground = natural - water;
    profile.tuning->dirtPermille = ground ? uint32_t(std::min<uint64_t>(1000, (dry * 1000 + ground / 2) / ground)) : 0;
    // The feature size shapes the ponds and the rock alike. How far land lies from water is the scale of the ponds:
    // narrow strips between ponds and channels, or wide meadows; the land edge per rock tile is the scale of the
    // rock: one range or scattered outcrops. The fit scored all water, the sea included, which favours big smooth
    // shapes, and drops lakes smaller than minLakeCells; both are chosen again on a sample-sized area for whichever
    // of the two the sample has (a tile of distance weighs as much as a tenth of rock edge per tile).
    const bool ponds = sample.shoreDistance[1] && profile.tuning->lakePermille;
    const bool rocks = sample.rockEdgePermille && profile.tuning->rockPermille;
    if (ponds || rocks) {
        constexpr uint32_t sizes[] {3, 4, 5, 6, 8, 12, 16, 24, 32, 48, 64}, smallest[] {2, 3, 4, 6, 8, 12};
        const uint32_t w = std::clamp<uint32_t>(sample.width, 64, 256), h = std::clamp<uint32_t>(sample.height, 64, 256);
        const uint64_t cells = uint64_t(w) * h;
        uint64_t best = UINT64_MAX;
        SurfaceTuning chosen = *profile.tuning;
        for (const uint32_t size : sizes) for (const uint32_t minLake : smallest) {
            auto tuning = *profile.tuning;
            tuning.featureSize = size;
            tuning.minLakeCells = minLake;
            uint64_t score = 0;
            for (const uint64_t seed : options.eval.seeds) {
                const SurfaceConfig config {{100, 100, 7, w, h}, seed, tuning};
                const auto grid = generateSurface(config, MapExtent {65001, 65001, 16}, {cells, cells * 8, cells * 64 + (1u << 20)}, {}, {}, stop);
                if (!grid) return std::unexpected(stop.stop_requested() ? "Analysis cancelled." : "Surface fit failed.");
                const auto& c = grid->cells;
                if (ponds) {
                    const auto made = shoreDistances(int(w), int(h), [&](size_t i) { return c[i] != SurfaceKind::Preserve; },
                        [&](size_t i) { return c[i] == SurfaceKind::Lake || c[i] == SurfaceKind::River; });
                    for (size_t k = 0; k < made.size(); ++k)
                        score += 100 * (made[k] > sample.shoreDistance[k] ? made[k] - sample.shoreDistance[k] : sample.shoreDistance[k] - made[k]);
                }
                if (rocks) {
                    const uint64_t edge = patchEdgePermille(int(w), int(h), [&](size_t i) { return c[i] == SurfaceKind::Rock; },
                        [&](size_t i) { return c[i] != SurfaceKind::Preserve && c[i] != SurfaceKind::Lake && c[i] != SurfaceKind::River; });
                    score += edge > sample.rockEdgePermille ? edge - sample.rockEdgePermille : sample.rockEdgePermille - edge;
                }
            }
            if (score < best) { best = score; chosen = tuning; }
        }
        profile.tuning = chosen;
    }
    // Grass mix patches likewise: the patch noise step that gives the sample's land edge per mix tile.
    if (sample.mixEdgePermille) {
        constexpr uint32_t steps[] {2, 3, 4, 5, 6, 8, 10, 14, 20};
        std::array<uint16_t, grassPatchKinds> shares {};
        for (size_t m = 0; m < grassPatchKinds; ++m) shares[m] = uint16_t(sample.grassMix[m].permille);
        const uint32_t w = std::clamp<uint32_t>(sample.width, 64, 256), h = std::clamp<uint32_t>(sample.height, 64, 256);
        const uint64_t cells = uint64_t(w) * h;
        std::vector<SurfaceGrid> grids;
        for (const uint64_t seed : options.eval.seeds) {
            auto grid = generateSurface({{100, 100, 7, w, h}, seed, *profile.tuning}, MapExtent {65001, 65001, 16},
                {cells, cells * 8, cells * 64 + (1u << 20)}, {}, {}, stop);
            if (!grid) return std::unexpected(stop.stop_requested() ? "Analysis cancelled." : "Surface fit failed.");
            grids.push_back(std::move(*grid));
        }
        uint64_t best = UINT64_MAX;
        for (const uint32_t step : steps) {
            uint64_t score = 0;
            for (size_t g = 0; g < grids.size(); ++g) {
                const auto& c = grids[g].cells;
                const auto slot = assignGrassPatches(grids[g], options.eval.seeds[g], shares, step, stop);
                if (slot.empty()) break;
                const uint64_t edge = patchEdgePermille(int(w), int(h), [&](size_t i) { return slot[i] != 0; },
                    [&](size_t i) { return c[i] == SurfaceKind::Grass || c[i] == SurfaceKind::Dirt; });
                score += edge > sample.mixEdgePermille ? edge - sample.mixEdgePermille : sample.mixEdgePermille - edge;
            }
            if (score < best) { best = score; profile.grassPatchStep = step; }
        }
    }
    // Dirt comes in patches of the sample's size: the moisture noise step and the width of the grass/dirt band are
    // chosen so that generated dirt has the sample's land edge per tile, at the sample's share.
    if (sample.dirtEdgePermille && profile.tuning->dirtPermille) {
        constexpr uint32_t sizes[] {2, 3, 4, 5, 6, 8, 12, 16, 24}, bands[] {1000, 3000, 6000, 16000};
        const uint32_t w = std::clamp<uint32_t>(sample.width, 64, 256), h = std::clamp<uint32_t>(sample.height, 64, 256);
        const uint64_t cells = uint64_t(w) * h;
        const uint64_t wantShare = profile.tuning->dirtPermille;
        uint64_t best = UINT64_MAX;
        SurfaceTuning chosen = *profile.tuning;
        for (const uint32_t size : sizes) for (const uint32_t band : bands) {
            auto tuning = *profile.tuning;
            tuning.dirtSize = size;
            tuning.transitionWidth = band;
            uint64_t score = 0;
            for (const uint64_t seed : options.eval.seeds) {
                const SurfaceConfig config {{100, 100, 7, w, h}, seed, tuning};
                const auto grid = generateSurface(config, MapExtent {65001, 65001, 16}, {cells, cells * 8, cells * 64 + (1u << 20)}, {}, {}, stop);
                if (!grid) return std::unexpected(stop.stop_requested() ? "Analysis cancelled." : "Surface fit failed.");
                const auto& c = grid->cells;
                const auto isLand = [&](size_t i) { return c[i] == SurfaceKind::Grass || c[i] == SurfaceKind::Dirt; };
                const auto isDirt = [&](size_t i) { return c[i] == SurfaceKind::Dirt; };
                const uint64_t edge = patchEdgePermille(int(w), int(h), isDirt, isLand);
                const uint64_t land = uint64_t(std::ranges::count_if(c, [](SurfaceKind k) { return k == SurfaceKind::Grass || k == SurfaceKind::Dirt; }));
                const uint64_t share = land ? uint64_t(std::ranges::count(c, SurfaceKind::Dirt)) * 1000 / land : 0;
                score += (edge > sample.dirtEdgePermille ? edge - sample.dirtEdgePermille : sample.dirtEdgePermille - edge)
                    + 20 * (share > wantShare ? share - wantShare : wantShare - share);
            }
            if (score < best) { best = score; chosen = tuning; }
        }
        profile.tuning = chosen;
    }
    // The generator always rings water with Shore; a sample without any shore meets water with grass,
    // so the ring takes the grass brush instead of the built-in sand.
    auto& shore = profile.palette[size_t(StyleKind::Shore)];
    if (target->sharesPpm[size_t(StyleKind::Shore)] == 0 && shore.empty()) shore = profile.palette[size_t(StyleKind::Grass)];
    const MapDecor own{"user-local-surface", "user-local-surface", sample.decor};
    profile.decorTarget = buildDecorTarget(std::span(&own, 1));
    auto decorTuning = fitDecorTuning(*profile.decorTarget, *profile.tuning, options.eval, stop);
    if (!decorTuning) return std::unexpected(stop.stop_requested() ? "Analysis cancelled." : "Vegetation fit failed.");
    profile.decorTuning = *decorTuning;
    profile.decorTuning->clumpPermille = sample.clumpPermille;
    profile.decorTuning->shoreTreePermille = sample.shoreTreePermille;
    profile.shoreTree = sample.shoreTree;
    profile.decorPalette = sample.decorPalette;
    profile.grassMix = sample.grassMix;
    profile.grassMixDecor = sample.grassMixDecor;
    profile.treeMix = sample.treeMix;
    profile.bushMix = sample.bushMix;
    profile.overlays = sample.overlays;
    profile.buildings = sample.buildings;
    profile.interiors = sample.interiors;
    profile.wallDecor = sample.wallDecor;
    profile.carpet = sample.carpet;
    profile.rockOutlinePermille = sample.rockOutlinePermille;
    if (!validateProfile(profile)) return std::unexpected("The learned profile is invalid.");
    return profile;
}

bool validLearnedProfileName(std::string_view name) {
    if (name.empty() || name.size() > 64) return false;
    std::string upper;
    for (const unsigned char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
        upper += char(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
    }
    if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL") return false;
    if (upper.size() == 4 && (upper.starts_with("COM") || upper.starts_with("LPT")) && upper[3] >= '1' && upper[3] <= '9') return false;
    return true;
}

std::expected<void, std::string> saveLearnedProfile(const std::filesystem::path& directory,
    std::string_view name, const StyleProfile& profile) {
    if (!validLearnedProfileName(name)) return std::unexpected("Use 1-64 letters, digits, '-' or '_'; no paths or device names.");
    const auto text = serializeProfile(profile);
    if (!text) return std::unexpected("Invalid profile; nothing saved.");
    if (text->size() > (1u << 20)) return std::unexpected("Profile exceeds the generator's 1 MiB read limit; nothing saved.");
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) return std::unexpected("Cannot create the local profile directory.");
    const auto path = directory / (std::string(name) + ".profile");
    const auto lock = directory / ".learning-save.lock";
    if (!std::filesystem::create_directory(lock, ec)) return std::unexpected("Profile folder is busy or unavailable. Retry after the other save finishes.");
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code e; std::filesystem::remove_all(path, e); } } cleanup{lock};
    size_t profiles = 0;
    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().extension() == ".profile" && it->is_regular_file(ec)) ++profiles;
    }
    if (ec) return std::unexpected("Cannot inspect the profile directory.");
    if (profiles >= 64) return std::unexpected("The generator reads up to 64 profile files. Archive unused profiles before saving another.");
    const auto temporary = lock / "pending";
#ifdef _WIN32
    struct File {
        HANDLE handle;
        ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    };
    bool written = false;
    {
        File file{CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (file.handle == INVALID_HANDLE_VALUE) return std::unexpected("Cannot create temporary profile.");
        DWORD bytes = 0;
        written = WriteFile(file.handle, text->data(), static_cast<DWORD>(text->size()), &bytes, nullptr)
            && bytes == text->size() && FlushFileBuffers(file.handle);
    }
    // Without REPLACE_EXISTING: destination races are refused as well as existing files.
    if (!written || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH))
        return std::unexpected("Profile not saved (write error or name already exists). Choose another name.");
#else
    { std::ofstream out(temporary, std::ios::binary); out << *text; out.flush();
      if (!out) return std::unexpected("Cannot write profile."); }
    std::filesystem::create_hard_link(temporary, path, ec);
    if (ec) return std::unexpected("Cannot publish profile; name may already exist.");
#endif
    return {};
}
} // namespace rme::procedural
