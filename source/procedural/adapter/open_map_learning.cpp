// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/open_map_learning.h"
#include "brushes/ground/ground_brush.h"
#include "game/item.h"
#include "procedural/core/doodad_class.h"
#include "map/map.h"
#include "map/tile.h"
#include <wx/thread.h>
namespace rme::procedural {
std::expected<void, std::string> readLearningRow(const Map& source, SemanticMap& destination, DoodadItemCounts& doodads, int row) try {
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
        for (const auto& item : tile->items) {
            if (!item->typeExists() || item->isInvalidOTBMItem()) continue;
            const auto* doodad = item->getDoodadBrush();
            if (!doodad || item->isWall() || item->isDoor() || item->isTable() || item->isCarpet() || item->isBorder()) continue;
            auto name = doodad->getName();
            traits |= decorTraitBit(classifyDoodadBrush(name));
            ++doodads[{key, std::move(name)}];
        }
        if (!destination.set(x, b.y0 + row, b.z, key, traits)) return std::unexpected("Invalid sample cell.");
    }
    return {};
} catch (const std::bad_alloc&) { return std::unexpected("Not enough memory to read the sample."); }
}
