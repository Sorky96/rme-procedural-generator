// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/open_map_learning.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "game/item.h"
#include "procedural/core/doodad_class.h"
#include "map/map.h"
#include "map/tile.h"
#include <algorithm>
#include <array>
#include <string_view>
#include <typeinfo>
#include <vector>
#include <wx/thread.h>
namespace rme::procedural {
namespace {
// A piece of a wall laid without a wall brush, by its client name.
bool namedAsWall(std::string_view name) {
    return std::ranges::any_of(std::array<std::string_view, 4> {"wall", "pillar", "column", "railing"},
        [&](std::string_view word) { return name.find(word) != std::string_view::npos; });
}
} // namespace
std::expected<void, std::string> readLearningRow(const Map& source, SemanticMap& destination, SampleCounts& counts, int row) try {
    auto& [doodads, items, overlays, walls, placed, carpets] = counts;
    if (!wxIsMainThread() || destination.floors().size() != 1) return std::unexpected("Invalid map read context.");
    const auto b = destination.floors().front().bounds;
    if (b.z < 0 || b.z > 7 || b.width > 512 || b.height > 512 || row < 0 || row >= b.height
        || b.x0 < 0 || b.y0 < 0 || b.x0 + b.width > source.getWidth() || b.y0 + b.height > source.getHeight())
        return std::unexpected("Rectangle must be inside the map, up to 512 x 512, floor 0-7.");
    for (int x = b.x0; x < b.x0 + b.width; ++x) {
        const auto* tile = source.getTile(x, b.y0 + row, b.z);
        if (!tile || tile->empty()) continue;
        const uint32_t cell = uint32_t(row) * uint32_t(b.width) + uint32_t(x - b.x0);
        uint16_t key = noGroundKey;
        uint32_t traits = 0;
        if (tile->hasOptionalBorder()) traits |= bit(Trait::OptionalBorder);
        if (tile->ground) {
            traits |= bit(Trait::HasGround);
            if (!tile->ground->typeExists() || tile->ground->isInvalidOTBMItem()) traits |= bit(Trait::Unknown);
            else if (const auto* brush = tile->ground->getGroundBrush()) {
                auto interned = destination.internGroundKey(brush->getName());
                if (!interned) return std::unexpected("Too many ground names.");
                key = *interned;
            }
        }
        std::vector<std::string> tileOverlays;
        // The whole stack, for the walls: kept when something on it blocks and it holds no door and no vegetation.
        PlacedStack stack {cell};
        bool blocking = false, door = false, natural = false;
        stack.plainWall = true;
        for (const auto& item : tile->items) {
            if (!item->typeExists() || item->isInvalidOTBMItem()) continue;
            // A wall piece one can walk through (spider webs are a wall brush) is decoration, learned as an overlay.
            if (item->isDoor()) { traits |= bit(Trait::Door); door = true; continue; }
            if (item->isWall() && item->isBlocking()) {
                traits |= bit(Trait::Wall);
                const auto* wall = item->getWallBrush();
                if (wall) ++walls[{key, wall->getName()}];
                stack.items.push_back(item->getID());
                stack.plainWall = stack.plainWall && wall;
                blocking = true;
                continue;
            }
            if (item->isCarpet()) {
                if (const auto* carpet = item->getCarpetBrush()) ++carpets[{key, carpet->getName()}];
                continue;
            }
            if (item->isTable() || item->isBorder()) continue;
            stack.items.push_back(item->getID());
            stack.plainWall = false;
            blocking = blocking || item->isBlocking();
            const auto* doodad = item->getDoodadBrush();
            auto& seen = items[item->getID()];
            if (!seen.count++) { seen.name = item->getName(); if (doodad) seen.brush = doodad->getName(); }
            stack.wallLike = stack.wallLike || (item->isBlocking() && namedAsWall(seen.name));
            // The item's own client name decides its kind when it says one; otherwise the brush that placed it does
            // ("frog leaf" of "swamp plants" is a bush). Vegetation keeps the brush as its source when the brush is of
            // the same kind, for its variants; anything else is learned as that one item.
            DecorKind kind = classifyDoodadBrush(seen.name);
            if (kind == DecorKind::None && doodad) kind = classifyDoodadBrush(doodad->getName());
            natural = natural || kind != DecorKind::None;
            const bool brushSource = doodad && kind != DecorKind::None && classifyDoodadBrush(doodad->getName()) == kind;
            auto name = brushSource ? std::string(doodad->getName()) : rawDecorName(item->getID(), kind);
            // Only plain items one can walk through make overlays (see decorItemPlaceable, decorItemPassable).
            if (kind == DecorKind::None) {
                if (typeid(*item) != typeid(Item)) continue;
                if (!item->isBlocking()) tileOverlays.push_back(std::move(name));
                continue;
            }
            traits |= decorTraitBit(kind);
            ++doodads[{key, std::move(name)}];
        }
        if (blocking && !door && !natural) placed.push_back(std::move(stack));
        // Items under something blocking belong to that structure, not to the open land.
        if (blocking) tileOverlays.clear();
        for (auto& name : tileOverlays) {
            auto& counts = overlays[{key, std::move(name)}];
            ++((traits & bit(Trait::TreeDoodad)) ? counts.first : counts.second);
        }
        if (!destination.set(x, b.y0 + row, b.z, key, traits)) return std::unexpected("Invalid sample cell.");
    }
    return {};
} catch (const std::bad_alloc&) { return std::unexpected("Not enough memory to read the sample."); }
}
