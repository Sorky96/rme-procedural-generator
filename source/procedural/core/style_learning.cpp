// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_learning.h"
#include "procedural/core/doodad_class.h"
#include "procedural/core/grass_patches.h"
#include <algorithm>
#include <fstream>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <tuple>
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
// Overlay items on land by use (rare ones such as spider eggs included; items of the learned wall stacks left out); each
// kept with at least 2 items and its rates on the land tiles with and
// without a tree. Items on water or on tiles without mapped ground are left out, as for vegetation.
std::array<StyleOverlay, maxOverlays> chooseOverlays(const SemanticMap& map, std::span<const KeyMapEntry> mapping,
    std::span<const GroundOverlayItems> items, std::span<const StyleWallStack> wallStacks) {
    // Items of the learned wall stacks belong to the walls, not to the open land.
    std::set<std::string, std::less<>> walled;
    for (const auto& stack : wallStacks) for (const uint16_t id : stack.items) walled.insert(rawDecorName(id, DecorKind::None));
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
        if (item.ground >= landKey.size() || !landKey[item.ground] || walled.contains(item.name)) continue;
        auto& counts = byName[item.name];
        counts.first += item.onTree; counts.second += item.onGround;
    }
    std::vector<std::pair<uint64_t, std::string_view>> ranked;
    for (const auto& [name, counts] : byName)
        if (counts.first + counts.second >= 2) ranked.emplace_back(counts.first + counts.second, name);
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
// What one cannot walk to on the sample's floor (see StyleProfile::unreachedRockPermille): {rock, walk} permille. A walkable
// tile is present, has ground, holds nothing unpassable and is neither water nor rock; tiles connect 8-way.
std::pair<uint32_t, uint32_t> measureUnreached(const SemanticMap& map, std::span<const KeyMapEntry> mapping) {
    const auto& floor = map.floors().front();
    const int w = floor.bounds.width, h = floor.bounds.height;
    const size_t cells = size_t(w) * h;
    std::vector<uint8_t> kindOf(map.groundKeys().size(), uint8_t(StyleKind::Unmapped));
    for (size_t k = 0; k < kindOf.size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        if (entry != mapping.end()) kindOf[k] = uint8_t(entry->kind);
    }
    const auto kind = [&](size_t i) {
        return (floor.traits[i] & bit(Trait::Present)) && floor.ground[i] < kindOf.size() ? StyleKind(kindOf[floor.ground[i]]) : StyleKind::Unmapped;
    };
    const auto walkable = [&](size_t i) {
        const StyleKind k = kind(i);
        return (floor.traits[i] & bit(Trait::HasGround)) && !(floor.traits[i] & bit(Trait::Blocking)) && k != StyleKind::Unmapped &&
            k != StyleKind::Water && k != StyleKind::Rock;
    };
    std::vector<uint8_t> seen(cells, 0);
    std::vector<size_t> stack;
    // The component of `start` among the tiles `inside` accepts; `visit` sees each of its tiles.
    const auto flood = [&](size_t start, auto&& inside, auto&& visit) {
        seen[start] = 1; stack.push_back(start);
        while (!stack.empty()) {
            const size_t i = stack.back(); stack.pop_back(); visit(i);
            const int x = int(i % w), y = int(i / w);
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                const int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                const size_t j = size_t(ny) * w + nx;
                if (!seen[j] && inside(j)) { seen[j] = 1; stack.push_back(j); }
            }
        }
    };
    uint64_t walk = 0, largest = 0;
    for (size_t i = 0; i < cells; ++i) {
        if (seen[i] || !walkable(i)) continue;
        uint64_t size = 0;
        flood(i, walkable, [&](size_t) { ++size; });
        walk += size; largest = std::max(largest, size);
    }
    std::ranges::fill(seen, 0);
    const auto rock = [&](size_t i) { return kind(i) == StyleKind::Rock; };
    const auto floorChange = [&](size_t i) { return (floor.traits[i] & bit(Trait::FloorChange)) != 0; };
    uint64_t rocks = 0, closed = 0;
    for (size_t i = 0; i < cells; ++i) {
        if (seen[i] || !rock(i)) continue;
        uint64_t size = 0;
        bool access = false;
        flood(i, rock, [&](size_t j) {
            ++size;
            const int x = int(j % w), y = int(j / w);
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                const int nx = x + dx, ny = y + dy;
                if (nx >= 0 && ny >= 0 && nx < w && ny < h) access = access || floorChange(size_t(ny) * w + nx);
            }
        });
        rocks += size;
        if (!access) closed += size;
    }
    return {rocks ? uint32_t((closed * 1000 + rocks / 2) / rocks) : 0, walk ? uint32_t(((walk - largest) * 1000 + walk / 2) / walk) : 0};
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
// Structures: blocking stacks and the grounds a structure brought (a ground without a brush, or an Other ground too rare
// to be a building floor: under a twentieth of the Other tiles), 8-connected, such as a ship wreck with its deck and
// gangway. A group belongs to no building when its blocking tiles keep more than two tiles away from any wall piece,
// door or building floor (its grounds stop short of them); it takes the stacks one step beside its blocking tiles as
// well, needs at least 4 blocking tiles and 16 tiles in all (a pile of rocks or bones is decoration, not a structure) and must fit maxStructureSide and maxStructureCells. It keeps which tiles of its
// box were water. Equal ones are counted together; the 8 with the most blocking tiles are kept, each with its rate per
// present tile. `structureCell` marks the tiles taken, which no wall learns from.
std::vector<StyleStructure> chooseStructures(const SemanticMap& map, std::span<const KeyMapEntry> mapping,
    std::span<const PlacedStack> placed, std::span<const uint16_t> groundItems, std::vector<uint8_t>& structureCell) {
    constexpr size_t smallest = 4, smallestStructure = 16;
    const auto& floor = map.floors().front();
    const int w = floor.bounds.width, h = floor.bounds.height;
    const size_t cells = size_t(w) * h;
    structureCell.assign(cells, 0);
    if (groundItems.size() != cells) return {};
    const size_t keys = map.groundKeys().size();
    std::vector<uint8_t> other(keys, 0), cave(keys, 0), water(keys, 0), builtKey(keys, 0);
    for (size_t k = 0; k < keys; ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        if (entry == mapping.end()) continue;
        cave[k] = entry->kind == StyleKind::Rock || entry->rule == "cave" || entry->rule == "hole";
        other[k] = entry->kind == StyleKind::Other && !cave[k];
        water[k] = entry->kind == StyleKind::Water;
    }
    const auto present = [&](size_t i) { return (floor.traits[i] & bit(Trait::Present)) != 0; };
    const auto onKey = [&](const std::vector<uint8_t>& set, size_t i) { return present(i) && floor.ground[i] < set.size() && set[floor.ground[i]]; };
    std::vector<uint64_t> otherTiles(keys, 0);
    uint64_t allOther = 0;
    for (size_t i = 0; i < cells; ++i) if (onKey(other, i)) { ++otherTiles[floor.ground[i]]; ++allOther; }
    for (size_t k = 0; k < keys; ++k) builtKey[k] = other[k] && otherTiles[k] * 20 >= allOther;
    const uint32_t walled = bit(Trait::Wall) | bit(Trait::Door);
    std::vector<int32_t> stackAt(cells, -1);
    for (size_t k = 0; k < placed.size(); ++k)
        if (placed[k].cell < cells && present(placed[k].cell) && !onKey(cave, placed[k].cell) && !placed[k].items.empty() &&
            placed[k].items.size() <= maxStructureCellItems) stackAt[placed[k].cell] = int32_t(k);
    // Tiles within two steps of something built.
    std::vector<uint8_t> nearBuilt(cells, 0);
    for (size_t i = 0; i < cells; ++i) {
        if (!present(i) || !(((floor.traits[i] & walled) && !onKey(cave, i)) || onKey(builtKey, i))) continue;
        const int x = int(i % w), y = int(i / w);
        for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) {
            const int nx = x + dx, ny = y + dy;
            if (nx >= 0 && ny >= 0 && nx < w && ny < h) nearBuilt[size_t(ny) * w + nx] = 1;
        }
    }
    const auto blockingAt = [&](size_t i) { return stackAt[i] >= 0 && placed[size_t(stackAt[i])].blocking; };
    // A ground the structure brought: without a brush, or an Other ground too rare to be a building floor.
    const auto ownGround = [&](size_t i) {
        return groundItems[i] && present(i) && !onKey(cave, i) && (floor.ground[i] == noGroundKey || (onKey(other, i) && !onKey(builtKey, i)));
    };
    const auto neighbours = [&](size_t i, auto&& visit) {
        const int x = int(i % w), y = int(i / w);
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            const int nx = x + dx, ny = y + dy;
            if ((dx || dy) && nx >= 0 && ny >= 0 && nx < w && ny < h) visit(size_t(ny) * w + nx);
        }
    };
    // (blocking tiles, structure counting its occurrences in ppm until the rate is set), keyed by its contents
    std::map<std::vector<uint32_t>, std::pair<uint64_t, StyleStructure>> found;
    std::vector<uint8_t> seen(cells, 0), member(cells, 0);
    std::vector<size_t> group, stack, members;
    for (size_t start = 0; start < cells; ++start) {
        if (seen[start] || !blockingAt(start)) continue;
        // The blocking tiles and the structure's own grounds, connected; then the stacks beside its blocking tiles.
        group.clear();
        seen[start] = 1; stack.push_back(start);
        bool built = false;
        size_t blocking = 0;
        while (!stack.empty()) {
            const size_t i = stack.back(); stack.pop_back(); group.push_back(i);
            built = built || (nearBuilt[i] && blockingAt(i));
            blocking += blockingAt(i);
            neighbours(i, [&](size_t j) { if (!seen[j] && (blockingAt(j) || (ownGround(j) && !nearBuilt[j]))) { seen[j] = 1; stack.push_back(j); } });
        }
        if (built || blocking < smallest || group.size() > maxStructureCells) continue;
        members.clear();
        for (const size_t i : group) { member[i] = 1; members.push_back(i); }
        for (const size_t i : group) {
            if (!blockingAt(i)) continue;
            neighbours(i, [&](size_t j) {
                if (member[j] || nearBuilt[j] || stackAt[j] < 0) return;
                member[j] = 1; members.push_back(j);
            });
        }
        const bool tooBig = members.size() > maxStructureCells;
        int x0 = w, y0 = h, x1 = -1, y1 = -1;
        for (const size_t i : members) {
            member[i] = 0;
            const int x = int(i % w), y = int(i / w);
            x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y);
        }
        if (tooBig || members.size() < smallestStructure || x1 - x0 + 1 > int(maxStructureSide) || y1 - y0 + 1 > int(maxStructureSide)) continue;
        std::ranges::sort(members); // row-major, so equal structures read alike
        StyleStructure structure {0, uint32_t(x1 - x0 + 1), uint32_t(y1 - y0 + 1), {}, {}};
        std::vector<uint32_t> key;
        for (const size_t i : members) {
            StyleStructureCell cell {uint32_t(int(i % w) - x0), uint32_t(int(i / w) - y0), ownGround(i) ? groundItems[i] : 0u, {}};
            if (stackAt[i] >= 0) cell.items = placed[size_t(stackAt[i])].items;
            key.insert(key.end(), {cell.dx, cell.dy, cell.ground, uint32_t(cell.items.size())});
            key.insert(key.end(), cell.items.begin(), cell.items.end());
            structure.cells.push_back(std::move(cell));
            structureCell[i] = 1;
        }
        // Water of the box, for tiles without a ground of the structure.
        bool wet = false;
        std::vector<uint8_t> box(size_t(structure.width) * structure.height, 0);
        for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
            const size_t i = size_t(y) * w + x;
            const bool cell = onKey(water, i) && !ownGround(i);
            box[size_t(y - y0) * structure.width + size_t(x - x0)] = cell;
            wet = wet || cell;
        }
        if (wet) structure.water = std::move(box);
        key.insert(key.end(), structure.water.begin(), structure.water.end());
        auto& entry = found[std::move(key)];
        if (!entry.second.ppm++) {
            entry.first = blocking;
            entry.second.width = structure.width; entry.second.height = structure.height;
            entry.second.cells = std::move(structure.cells);
            entry.second.water = std::move(structure.water);
        }
    }
    uint64_t presentTiles = 0;
    for (size_t i = 0; i < cells; ++i) presentTiles += present(i);
    std::vector<const std::pair<uint64_t, StyleStructure>*> ranked;
    for (const auto& entry : found) ranked.push_back(&entry.second);
    std::ranges::stable_sort(ranked, [](const auto* a, const auto* b) { return a->first > b->first; });
    std::vector<StyleStructure> out;
    for (const auto* entry : ranked) {
        if (out.size() == maxStructures || !presentTiles) break;
        StyleStructure structure = entry->second;
        structure.ppm = uint32_t(std::clamp<uint64_t>((uint64_t(structure.ppm) * 1000000 + presentTiles / 2) / presentTiles, 1, 1000000));
        out.push_back(std::move(structure));
    }
    return out;
}
// Wall tiles as the sample built them. A tile is in a wall when it holds a wall piece, a door or a blocking stack and is
// one (or a stack named as a piece of a wall), or reaches one within 8 tiles along a straight run of such tiles (a row of rocks or a wreck is no wall); caves and
// mountains are left out. Per side mask of the neighbouring wall tiles, each stack with a piece that is not of a wall
// brush and seen at least twice is kept (the 8 most seen per mask, 64 in all) with its share of the wall tiles of that mask.
std::vector<StyleWallStack> chooseWallStacks(const SemanticMap& map, std::span<const KeyMapEntry> mapping,
    std::span<const PlacedStack> placed, std::span<const uint8_t> structureCell) {
    const auto& floor = map.floors().front();
    const int w = floor.bounds.width, h = floor.bounds.height;
    std::vector<uint8_t> cave(map.groundKeys().size(), 0);
    for (size_t k = 0; k < cave.size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        cave[k] = entry != mapping.end() && (entry->kind == StyleKind::Rock || entry->rule == "cave" || entry->rule == "hole");
    }
    const size_t cells = size_t(w) * h;
    const uint32_t walled = bit(Trait::Wall) | bit(Trait::Door);
    const auto open = [&](size_t i) {
        return (floor.traits[i] & bit(Trait::Present)) && !(floor.ground[i] < cave.size() && cave[floor.ground[i]]);
    };
    std::vector<uint8_t> line(cells, 0);
    std::vector<int32_t> stackAt(cells, -1);
    std::vector<uint8_t> anchor(cells, 0); // a wall piece, a door, or a stack named as a piece of a wall
    for (size_t i = 0; i < cells; ++i) anchor[i] = line[i] = open(i) && (floor.traits[i] & walled);
    for (size_t k = 0; k < placed.size(); ++k) {
        const uint32_t cell = placed[k].cell;
        if (cell >= cells || !open(cell) || !placed[k].blocking || structureCell[cell] || placed[k].items.empty() ||
            placed[k].items.size() > maxStackItems) continue;
        line[cell] = 1;
        stackAt[cell] = int32_t(k);
        anchor[cell] = anchor[cell] || placed[k].wallLike;
    }
    constexpr std::array<std::pair<int, int>, 4> sides {{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}}; // north, east, south, west
    const auto anchored = [&](int x, int y) {
        if (anchor[size_t(y) * w + x]) return true;
        for (const auto [dx, dy] : sides) for (int step = 1; step <= 8; ++step) {
            const int nx = x + dx * step, ny = y + dy * step;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h || !line[size_t(ny) * w + nx]) break;
            if (anchor[size_t(ny) * w + nx]) return true;
        }
        return false;
    };
    std::array<uint64_t, 16> wallTiles {};
    std::map<std::pair<uint32_t, std::vector<uint16_t>>, uint64_t> seen; // (sides, stack) -> tiles
    for (size_t i = 0; i < cells; ++i) {
        const int x = int(i % w), y = int(i / w);
        if (!line[i] || !anchored(x, y)) continue;
        uint32_t mask = 0;
        for (size_t s = 0; s < sides.size(); ++s) {
            const int nx = x + sides[s].first, ny = y + sides[s].second;
            if (nx >= 0 && ny >= 0 && nx < w && ny < h && line[size_t(ny) * w + nx]) mask |= 1u << s;
        }
        ++wallTiles[mask];
        if (stackAt[i] >= 0 && !placed[size_t(stackAt[i])].plainWall) ++seen[{mask, placed[size_t(stackAt[i])].items}];
    }
    std::vector<std::pair<uint64_t, const std::pair<uint32_t, std::vector<uint16_t>>*>> ranked;
    for (const auto& entry : seen) if (entry.second >= 2) ranked.emplace_back(entry.second, &entry.first);
    std::ranges::sort(ranked, [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : *a.second < *b.second; });
    std::array<size_t, 16> perMask {};
    std::vector<StyleWallStack> out;
    for (const auto& [count, key] : ranked) {
        if (out.size() == maxWallStacks) break;
        if (perMask[key->first]++ == 8) continue;
        out.push_back({key->first, uint32_t(count * 1000000 / wallTiles[key->first]), key->second});
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
    std::span<const GroundOverlayItems> overlayItems, std::span<const GroundDoodadItems> wallBrushes, std::span<const PlacedStack> placedStacks,
    std::span<const GroundDoodadItems> carpetItems, std::span<const uint16_t> groundItems) try {
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
    std::vector<uint8_t> structureCell;
    sample.structures = chooseStructures(map, mapping, placedStacks, groundItems, structureCell);
    sample.wallStacks = chooseWallStacks(map, mapping, placedStacks, structureCell);
    sample.overlays = chooseOverlays(map, mapping, overlayItems, sample.wallStacks);
    sample.interiors = chooseInteriors(map, mapping, overlayItems);
    sample.carpet = chooseCarpet(map, mapping, carpetItems);
    sample.rockOutlinePermille = measureRockOutline(map, mapping);
    std::tie(sample.unreachedRockPermille, sample.unreachedWalkPermille) = measureUnreached(map, mapping);
    // The natural classes as a surface grid for the ranking metrics; water is lake, everything else is preserved.
    {
        const auto& floor = map.floors().front();
        SurfaceGrid grid;
        grid.region = {b.x0, b.y0, b.z, uint32_t(b.width), uint32_t(b.height)};
        grid.cells.assign(floor.ground.size(), SurfaceKind::Preserve);
        std::vector<SurfaceKind> kinds(map.groundKeys().size(), SurfaceKind::Preserve);
        for (size_t k = 0; k < kinds.size(); ++k) {
            const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
            if (entry == mapping.end()) continue;
            constexpr SurfaceKind natural[] {SurfaceKind::Grass, SurfaceKind::Dirt, SurfaceKind::Rock, SurfaceKind::Shore, SurfaceKind::Lake};
            if (size_t(entry->kind) < naturalKindCount) kinds[k] = natural[size_t(entry->kind)];
        }
        for (size_t i = 0; i < floor.ground.size(); ++i)
            if ((floor.traits[i] & bit(Trait::Present)) && floor.ground[i] < kinds.size()) grid.cells[i] = kinds[floor.ground[i]];
        auto metrics = candidateMetricsOf(grid, stop);
        if (!metrics) return std::unexpected(metrics.error() == CandidateScoreError::Cancelled ? "Analysis cancelled." : "Measuring the sample failed.");
        sample.metrics = *metrics;
    }
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
    profile.wallStacks = sample.wallStacks;
    profile.structures = sample.structures;
    profile.carpet = sample.carpet;
    profile.rockOutlinePermille = sample.rockOutlinePermille;
    profile.unreachedRockPermille = sample.unreachedRockPermille;
    profile.unreachedWalkPermille = sample.unreachedWalkPermille;
    // Several samples are named by their order only, weighted by their tiles (largest remainder to 1000).
    if (sample.sampleWeights.size() > 1) {
        const uint64_t total = std::accumulate(sample.sampleWeights.begin(), sample.sampleWeights.end(), uint64_t {0});
        std::vector<std::pair<uint64_t, size_t>> remainders;
        uint32_t given = 0;
        profile.sources.clear();
        for (size_t i = 0; i < sample.sampleWeights.size(); ++i) {
            const uint64_t scaled = sample.sampleWeights[i] * 1000;
            profile.sources.push_back({std::format("sample-{}", i + 1), uint32_t(scaled / total)});
            given += uint32_t(scaled / total);
            remainders.push_back({scaled % total, i});
        }
        std::ranges::stable_sort(remainders, std::greater {}, &std::pair<uint64_t, size_t>::first);
        for (size_t i = 0; given < 1000; ++i, ++given) ++profile.sources[remainders[i].second].weightPermille;
    }
    for (size_t m = 0; m < candidateRankingMetricCount; ++m)
        if (sample.metrics[m]) if (const auto band = learnedCandidateBand(m, *sample.metrics[m])) profile.bands.push_back(*band);
    if (!validateProfile(profile)) return std::unexpected("The learned profile is invalid.");
    return profile;
}

namespace {
// Weight-rounded mean of one value over the samples.
template <typename Get>
uint64_t weightedMean(std::span<const LearningSample> samples, uint64_t total, Get get) {
    uint64_t sum = 0;
    for (const auto& s : samples) sum += uint64_t(get(s)) * s.present;
    return total ? uint64_t((sum + total / 2) / total) : 0;
}
// The name of the heaviest sample that has one (ties by the smaller name).
template <typename Get>
std::string heaviestName(std::span<const LearningSample> samples, Get get) {
    const std::string* best = nullptr;
    uint64_t weight = 0;
    for (const auto& s : samples) {
        const std::string& name = get(s);
        if (name.empty()) continue;
        if (!best || s.present > weight || (s.present == weight && name < *best)) { best = &name; weight = s.present; }
    }
    return best ? *best : std::string();
}
// Entries combined by key with weighted rates, ordered by rate descending then key, cut to `limit`.
template <typename Key>
struct Combined { Key key; uint64_t rate = 0; size_t sample = 0; size_t index = 0; };
// `tieBreak(a, ia, b, ib)` says whether entry ia of sample a stands for the key instead of entry ib of an equally heavy b.
template <typename Key, typename Each, typename TieBreak = bool (*)(const LearningSample&, size_t, const LearningSample&, size_t)>
std::vector<Combined<Key>> combine(std::span<const LearningSample> samples, uint64_t total, size_t limit, Each each,
    TieBreak tieBreak = [](const LearningSample&, size_t, const LearningSample&, size_t) { return false; }) {
    std::map<Key, std::pair<uint64_t, std::pair<size_t, size_t>>> sums; // key -> weighted sum, heaviest (sample, index)
    for (size_t i = 0; i < samples.size(); ++i)
        each(samples[i], [&](const Key& key, uint64_t rate, size_t index) {
            auto [entry, added] = sums.try_emplace(key);
            entry->second.first += (uint64_t)rate * samples[i].present;
            const auto [held, heldIndex] = entry->second.second;
            if (added || samples[i].present > samples[held].present ||
                (samples[i].present == samples[held].present && tieBreak(samples[i], index, samples[held], heldIndex)))
                entry->second.second = {i, index};
        });
    std::vector<Combined<Key>> out;
    for (const auto& [key, value] : sums) {
        const uint64_t rate = total ? uint64_t((value.first + total / 2) / total) : 0;
        if (rate) out.push_back({key, rate, value.second.first, value.second.second});
    }
    std::ranges::stable_sort(out, std::greater {}, &Combined<Key>::rate); // equal rates stay in key order
    if (out.size() > limit) out.resize(limit);
    return out;
}
template <size_t Count>
std::array<StyleMix, Count> combineMix(std::span<const LearningSample> samples, uint64_t total, const std::string& primary,
    std::array<StyleMix, Count> LearningSample::* field, std::vector<Combined<std::string>>* chosen = nullptr) {
    auto all = combine<std::string>(samples, total, SIZE_MAX, [&](const LearningSample& s, auto add) {
        for (size_t m = 0; m < Count; ++m) if (!(s.*field)[m].brush.empty()) add((s.*field)[m].brush, (s.*field)[m].permille, m);
    }, [](const LearningSample& a, size_t ia, const LearningSample& b, size_t ib) {
        if constexpr (Count == maxGrassMix)
            return std::tie(a.grassMixDecor[ia].keepPermille, a.grassMixDecor[ia].brushes) < std::tie(b.grassMixDecor[ib].keepPermille, b.grassMixDecor[ib].brushes);
        else return false;
    });
    // A mix keeps at least 5% of its kind, as when read from one sample, and never repeats the palette's brush.
    std::erase_if(all, [&](const auto& c) { return c.key == primary || c.rate < 50; });
    if (all.size() > Count) all.resize(Count);
    std::array<StyleMix, Count> out {};
    for (size_t m = 0; m < all.size(); ++m) out[m] = {all[m].key, uint32_t(std::min<uint64_t>(1000, all[m].rate))};
    if (chosen) *chosen = std::move(all);
    return out;
}
} // namespace

std::expected<LearningSample, std::string> mergeLearningSamples(std::span<const LearningSample> samples) try {
    if (samples.empty() || samples.size() > maxLearningSamples)
        return std::unexpected(std::format("Use 1 to {} samples.", maxLearningSamples));
    if (samples.size() == 1) return samples.front();
    uint64_t total = 0;
    for (const auto& s : samples) {
        if (!s.present) return std::unexpected("A sample has no tiles.");
        total += s.present;
    }
    LearningSample out;
    for (const auto& s : samples) {
        out.sampleWeights.push_back(s.present);
        for (size_t a = 0; a < styleKindCount; ++a) {
            out.stats.kindTiles[a] += s.stats.kindTiles[a];
            for (size_t b = 0; b < styleKindCount; ++b) out.stats.pairs[a][b] += s.stats.pairs[a][b];
        }
        out.present += s.present; out.recognized += s.recognized; out.seaTiles += s.seaTiles;
        for (size_t c = 0; c < out.decor.cell.size(); ++c) {
            out.decor.cell[c].tiles += s.decor.cell[c].tiles; out.decor.cell[c].trees += s.decor.cell[c].trees;
            out.decor.cell[c].bushes += s.decor.cell[c].bushes; out.decor.cell[c].stones += s.decor.cell[c].stones;
        }
    }
    const auto mean = [&](auto get) { return uint32_t(weightedMean(samples, total, get)); };
    for (size_t k = 0; k < naturalKindCount; ++k)
        out.palette[k] = heaviestName(samples, [k](const LearningSample& s) -> const std::string& { return s.palette[k]; });
    for (size_t k = 0; k < out.decorPalette.size(); ++k)
        out.decorPalette[k] = heaviestName(samples, [k](const LearningSample& s) -> const std::string& { return s.decorPalette[k]; });
    std::vector<Combined<std::string>> grassChosen;
    out.grassMix = combineMix(samples, total, out.palette[size_t(StyleKind::Grass)], &LearningSample::grassMix, &grassChosen);
    for (size_t m = 0; m < grassChosen.size(); ++m) out.grassMixDecor[m] = samples[grassChosen[m].sample].grassMixDecor[grassChosen[m].index];
    out.treeMix = combineMix(samples, total, out.decorPalette[0], &LearningSample::treeMix);
    out.bushMix = combineMix(samples, total, out.decorPalette[1], &LearningSample::bushMix);
    out.clumpPermille = mean([](const LearningSample& s) { return s.clumpPermille; });
    out.shoreTree = heaviestName(samples, [](const LearningSample& s) -> const std::string& { return s.shoreTree; });
    out.shoreTreePermille = out.shoreTree.empty() ? 0 : std::max<uint32_t>(1, mean([](const LearningSample& s) { return s.shoreTreePermille; }));
    const auto overlays = [&](std::array<StyleOverlay, maxOverlays> LearningSample::* field, bool onTree) {
        std::array<StyleOverlay, maxOverlays> result {};
        // Ranked by both rates together; each rate is its own weighted mean.
        const auto all = combine<std::string>(samples, total, maxOverlays, [&](const LearningSample& s, auto add) {
            for (size_t i = 0; i < maxOverlays; ++i)
                if (const auto& o = (s.*field)[i]; !o.name.empty()) add(o.name, uint64_t(o.onTreePpm) + o.onGroundPpm, i);
        });
        for (size_t i = 0; i < all.size(); ++i) {
            const auto rate = [&](bool tree) {
                return uint32_t(weightedMean(samples, total, [&](const LearningSample& s) {
                    const auto found = std::ranges::find(s.*field, all[i].key, &StyleOverlay::name);
                    return found == (s.*field).end() ? 0u : (tree ? found->onTreePpm : found->onGroundPpm);
                }));
            };
            result[i] = {all[i].key, onTree ? rate(true) : 0, rate(false)};
            if (!result[i].onTreePpm && !result[i].onGroundPpm) result[i].onGroundPpm = 1;
        }
        return result;
    };
    out.overlays = overlays(&LearningSample::overlays, true);
    out.interiors = overlays(&LearningSample::interiors, false);
    out.buildings.density = mean([](const LearningSample& s) { return s.buildings.density; });
    if (out.buildings.density) {
        out.buildings.ruinPercent = mean([](const LearningSample& s) { return s.buildings.ruinPercent; });
        out.buildings.floorPercent = std::max<uint32_t>(1, mean([](const LearningSample& s) { return s.buildings.floorPercent; }));
        out.buildings.compoundPercent = mean([](const LearningSample& s) { return s.buildings.compoundPercent; });
        out.buildings.floor = heaviestName(samples, [](const LearningSample& s) -> const std::string& { return s.buildings.floor; });
        out.buildings.wall = heaviestName(samples, [](const LearningSample& s) -> const std::string& { return s.buildings.wall; });
        out.carpet.name = heaviestName(samples, [](const LearningSample& s) -> const std::string& { return s.carpet.name; });
        if (!out.carpet.name.empty())
            out.carpet.onGroundPpm = std::max<uint32_t>(1, mean([&](const LearningSample& s) { return s.carpet.name == out.carpet.name ? s.carpet.onGroundPpm : 0u; }));
        const auto stacks = combine<std::pair<uint32_t, std::vector<uint16_t>>>(samples, total, maxWallStacks, [](const LearningSample& s, auto add) {
            for (size_t i = 0; i < s.wallStacks.size(); ++i) add(std::pair(s.wallStacks[i].sides, s.wallStacks[i].items), s.wallStacks[i].ppm, i);
        });
        for (const auto& c : stacks) out.wallStacks.push_back({c.key.first, uint32_t(c.rate), c.key.second});
    } else {
        std::ranges::fill(out.interiors, StyleOverlay {});
    }
    // Structures are keyed by their whole content (box, cells, water); the heaviest rates are kept.
    const auto structureKey = [](const StyleStructure& s) {
        std::vector<uint32_t> key {s.width, s.height};
        for (const auto& cell : s.cells) {
            key.insert(key.end(), {cell.dx, cell.dy, cell.ground, uint32_t(cell.items.size())});
            key.insert(key.end(), cell.items.begin(), cell.items.end());
        }
        key.insert(key.end(), s.water.begin(), s.water.end());
        return key;
    };
    const auto structures = combine<std::vector<uint32_t>>(samples, total, maxStructures, [&](const LearningSample& s, auto add) {
        for (size_t k = 0; k < s.structures.size(); ++k) add(structureKey(s.structures[k]), s.structures[k].ppm, k);
    });
    for (const auto& c : structures) {
        auto structure = samples[c.sample].structures[c.index];
        structure.ppm = uint32_t(std::max<uint64_t>(1, c.rate));
        out.structures.push_back(std::move(structure));
    }
    out.rockOutlinePermille = mean([](const LearningSample& s) { return s.rockOutlinePermille; });
    out.unreachedRockPermille = mean([](const LearningSample& s) { return s.unreachedRockPermille; });
    out.unreachedWalkPermille = mean([](const LearningSample& s) { return s.unreachedWalkPermille; });
    for (size_t k = 0; k < out.shoreDistance.size(); ++k) out.shoreDistance[k] = mean([k](const LearningSample& s) { return s.shoreDistance[k]; });
    const auto largest = std::ranges::max_element(samples, {}, [](const LearningSample& s) { return uint64_t(s.width) * s.height; });
    out.width = largest->width; out.height = largest->height;
    out.mixEdgePermille = out.grassMix[0].brush.empty() ? 0 : mean([](const LearningSample& s) { return s.mixEdgePermille; });
    out.rockEdgePermille = mean([](const LearningSample& s) { return s.rockEdgePermille; });
    out.dirtEdgePermille = mean([](const LearningSample& s) { return s.dirtEdgePermille; });
    for (size_t m = 0; m < out.metrics.size(); ++m) {
        int64_t sum = 0;
        uint64_t weight = 0;
        for (const auto& s : samples) if (s.metrics[m]) { sum += *s.metrics[m] * int64_t(s.present); weight += s.present; }
        if (weight) out.metrics[m] = sum >= 0 ? (sum + int64_t(weight / 2)) / int64_t(weight) : -((-sum + int64_t(weight / 2)) / int64_t(weight));
    }
    // Heaviest first: the profile's sample-1..N do not depend on the order the samples were given in.
    std::ranges::sort(out.sampleWeights, std::greater {});
    return out;
} catch (const std::bad_alloc&) { return std::unexpected("Not enough memory for analysis."); }

std::expected<std::vector<KeyMapEntry>, std::string> mergeLearningMappings(std::span<const std::vector<KeyMapEntry>> mappings) try {
    std::vector<KeyMapEntry> out;
    for (const auto& mapping : mappings)
        for (const auto& entry : mapping) {
            const auto found = std::ranges::find(out, entry.key, &KeyMapEntry::key);
            if (found == out.end()) out.push_back(entry);
            else if (found->kind != entry.kind)
                return std::unexpected(std::format("Ground \"{}\" is {} in one sample and {} in another.", entry.key, styleKindName(found->kind), styleKindName(entry.kind)));
        }
    std::ranges::sort(out, {}, &KeyMapEntry::key);
    return out;
} catch (const std::bad_alloc&) { return std::unexpected("Not enough memory for analysis."); }

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
