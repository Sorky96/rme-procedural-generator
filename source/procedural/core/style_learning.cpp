// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_learning.h"
#include "procedural/core/doodad_class.h"
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
// Water 4-connected to the edge of the area or to a tile without ground is the sea around an island; the rest
// are ponds. pondEdges counts the (pond tile, land neighbour) pairs: per pond tile it is small for big round
// ponds and large for small ones, so it measures the scale of the ponds.
struct WaterShape { uint64_t sea = 0, pondTiles = 0, pondEdges = 0; };
template <class Present, class Water>
WaterShape waterShape(int w, int h, Present present, Water water) {
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
    WaterShape shape;
    while (!stack.empty()) {
        const size_t i = stack.back(); stack.pop_back(); ++shape.sea;
        const int x = int(i % w), y = int(i / w);
        for (const auto [dx, dy] : steps) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            const size_t j = size_t(ny) * w + nx;
            if (!sea[j] && water(j)) { sea[j] = 1; stack.push_back(j); }
        }
    }
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const size_t i = size_t(y) * w + x;
        if (sea[i] || !water(i)) continue;
        ++shape.pondTiles;
        for (const auto [dx, dy] : steps) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            const size_t j = size_t(ny) * w + nx;
            shape.pondEdges += present(j) && !water(j);
        }
    }
    return shape;
}
// Overlay items on land by use; each kept with at least 20 items and its rates on the land tiles with and
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
        if (counts.first + counts.second >= 20) ranked.emplace_back(counts.first + counts.second, name);
    std::ranges::sort(ranked, [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    std::array<StyleOverlay, maxOverlays> out;
    size_t used = 0;
    for (const auto& [total, name] : ranked) {
        if (used == maxOverlays) break;
        const auto& [onTree, onGround] = byName.find(name)->second;
        const auto rate = [](uint64_t n, uint64_t of) { return of ? uint32_t(std::min<uint64_t>(1000, (n * 1000 + of / 2) / of)) : 0u; };
        StyleOverlay overlay {std::string(name), rate(onTree, treeTiles), rate(onGround, groundTiles)};
        if (overlay.onTreePermille + overlay.onGroundPermille == 0) continue;
        out[used++] = std::move(overlay);
    }
    return out;
}
WaterShape sampleWaterShape(const SemanticMap& map, std::span<const KeyMapEntry> mapping) {
    const auto& floor = map.floors().front();
    std::vector<uint8_t> waterKey(map.groundKeys().size(), 0);
    for (size_t k = 0; k < waterKey.size(); ++k) {
        const auto entry = std::ranges::find(mapping, map.groundKeys()[k], &KeyMapEntry::key);
        waterKey[k] = entry != mapping.end() && entry->kind == StyleKind::Water;
    }
    const auto present = [&](size_t i) { return (floor.traits[i] & bit(Trait::Present)) != 0; };
    const auto water = [&](size_t i) { return present(i) && floor.ground[i] < waterKey.size() && waterKey[floor.ground[i]]; };
    return waterShape(floor.bounds.width, floor.bounds.height, present, water);
}
} // namespace

std::expected<LearningSample, std::string> inspectLearningSample(const SemanticMap& map,
    std::span<const KeyMapEntry> mapping, std::span<const GroundDoodadItems> doodadItems, bool permission, std::stop_token stop,
    std::span<const GroundOverlayItems> overlayItems) try {
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
    const auto shape = sampleWaterShape(map, mapping);
    sample.seaTiles = shape.sea; sample.pondTiles = shape.pondTiles; sample.pondEdges = shape.pondEdges;
    sample.overlays = chooseOverlays(map, mapping, overlayItems);
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
    // The feature size was chosen for the fit's own lake share and scored on all water, the sea included, which
    // favours big smooth shapes. With ponds in the sample it is picked again so that the generated ponds have
    // the same land edge per water tile; otherwise by the neighbour pairs of the final shares.
    {
        constexpr uint32_t sizes[] {6, 8, 12, 16, 24, 32, 48};
        const bool ponds = sample.pondTiles >= 8;
        const uint64_t side = options.eval.side, cells = side * side;
        uint64_t best = UINT64_MAX;
        uint32_t chosen = profile.tuning->featureSize;
        for (const uint32_t size : sizes) {
            auto tuning = *profile.tuning;
            tuning.featureSize = size;
            uint64_t score = UINT64_MAX;
            if (ponds) {
                uint64_t pondTiles = 0, pondEdges = 0;
                for (const uint64_t seed : options.eval.seeds) {
                    const SurfaceConfig config {{100, 100, 7, uint32_t(side), uint32_t(side)}, seed, tuning};
                    const auto grid = generateSurface(config, MapExtent {65001, 65001, 16}, {cells, cells * 8, cells * 64 + (1u << 20)}, {}, {}, stop);
                    if (!grid) return std::unexpected(stop.stop_requested() ? "Analysis cancelled." : "Surface fit failed.");
                    const auto& c = grid->cells;
                    const auto shape = waterShape(int(side), int(side), [&](size_t i) { return c[i] != SurfaceKind::Preserve; },
                        [&](size_t i) { return c[i] == SurfaceKind::Lake || c[i] == SurfaceKind::River; });
                    pondTiles += shape.pondTiles; pondEdges += shape.pondEdges;
                }
                // Edges per 1000 pond tiles, generated against sampled; no ponds at all scores worst.
                if (pondTiles) {
                    const uint64_t made = pondEdges * 1000 / pondTiles, wanted = sample.pondEdges * 1000 / sample.pondTiles;
                    score = made > wanted ? made - wanted : wanted - made;
                }
            } else {
                const auto stats = evaluateTuning(tuning, options.eval, stop);
                if (!stats) return std::unexpected(stop.stop_requested() ? "Analysis cancelled." : "Surface fit failed.");
                if (const auto generated = targetFromStats(*stats)) score = distance(*generated, *target).adjacency;
            }
            if (score < best) { best = score; chosen = size; }
        }
        profile.tuning->featureSize = chosen;
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
