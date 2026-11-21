// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/open_map_learning.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "game/item.h"
#include "procedural/core/doodad_class.h"
#include "map/map.h"
#include "map/tile.h"
#include <typeinfo>
#include <vector>
#include <wx/thread.h>
namespace rme::procedural {
std::expected<void, std::string> readLearningRow(const Map& source, SemanticMap& destination, SampleCounts& counts, int row) try {
    auto& [doodads, items, overlays, walls] = counts;
    if (!wxIsMainThread() || destination.floors().size() != 1) return std::unexpected("Invalid map read context.");
    const auto b = destination.floors().front().bounds;
    if (b.z < 0 || b.z > 7 || b.width > 512 || b.height > 512 || row < 0 || row >= b.height
        || b.x0 < 0 || b.y0 < 0 || b.x0 + b.width > source.getWidth() || b.y0 + b.height > source.getHeight())
        return std::unexpected("Rectangle must be inside the map, up to 512 x 512, floor 0-7.");
    for (int x = b.x0; x < b.x0 + b.width; ++x) {
        const auto* tile = source.getTile(x, b.y0 + row, b.z);
        if (!tile || tile->empty()) continue;
        uint16_t key = noGroundKey;
        uint32_t traits = 0;
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
        for (const auto& item : tile->items) {
            if (!item->typeExists() || item->isInvalidOTBMItem()) continue;
            // A wall piece one can walk through (spider webs are a wall brush) is decoration, learned as an overlay.
            if (item->isDoor()) { traits |= bit(Trait::Door); continue; }
            if (item->isWall() && item->isBlocking()) {
                traits |= bit(Trait::Wall);
                if (const auto* wall = item->getWallBrush()) ++walls[{key, wall->getName()}];
                continue;
            }
            if (item->isTable() || item->isCarpet() || item->isBorder()) continue;
            const auto* doodad = item->getDoodadBrush();
            auto& seen = items[item->getID()];
            if (!seen.count++) { seen.name = item->getName(); if (doodad) seen.brush = doodad->getName(); }
            // The item's own client name decides its kind when it says one; otherwise the brush that placed it does
            // ("frog leaf" of "swamp plants" is a bush). Vegetation keeps the brush as its source when the brush is of
            // the same kind, for its variants; anything else is learned as that one item.
            DecorKind kind = classifyDoodadBrush(seen.name);
            if (kind == DecorKind::None && doodad) kind = classifyDoodadBrush(doodad->getName());
            const bool brushSource = doodad && kind != DecorKind::None && classifyDoodadBrush(doodad->getName()) == kind;
            auto name = brushSource ? std::string(doodad->getName()) : rawDecorName(item->getID(), kind);
            // Only plain items one can walk through make overlays (see decorItemPlaceable, decorItemPassable).
            if (kind == DecorKind::None) {
                if (typeid(*item) == typeid(Item) && !item->isBlocking()) tileOverlays.push_back(std::move(name));
                continue;
            }
            traits |= decorTraitBit(kind);
            ++doodads[{key, std::move(name)}];
        }
        for (auto& name : tileOverlays) {
            auto& counts = overlays[{key, std::move(name)}];
            ++((traits & bit(Trait::TreeDoodad)) ? counts.first : counts.second);
        }
        if (!destination.set(x, b.y0 + row, b.z, key, traits)) return std::unexpected("Invalid sample cell.");
    }
    return {};
} catch (const std::bad_alloc&) { return std::unexpected("Not enough memory to read the sample."); }
}
