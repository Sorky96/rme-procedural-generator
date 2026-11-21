// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/decor_source.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/brush.h"
#include "game/item.h"
#include "item_definitions/core/item_definition_store.h"
#include "procedural/core/doodad_class.h"
#include <string>
#include <typeinfo>
namespace rme::procedural {
uint16_t DecorSource::item(uint64_t roll) const {
	if (raw) return raw;
	const auto* picked = doodad ? doodad->getRandomSingleItem(0, int(roll)) : nullptr;
	return picked ? picked->getID() : 0;
}
DecorSource findDecorSource(std::string_view name, uint64_t maxChance) {
	if (const auto id = rawDecorId(name)) return {nullptr, *id, DecorSource::rawTotal};
	const auto* brush = g_brushes.getBrush(std::string(name));
	const auto* doodad = brush ? brush->as<DoodadBrush>() : nullptr;
	if (!doodad || !doodad->hasSingleObjects(0) || doodad->getSingleChance(0) <= 0 || uint64_t(doodad->getSingleChance(0)) > maxChance) return {};
	return {doodad, 0, uint64_t(doodad->getSingleChance(0))};
}
bool decorItemPlaceable(uint16_t id) {
	const auto definition = id ? g_item_definitions.get(id) : ItemDefinitionView{};
	if (!definition || !definition.clientId() || definition.isMetaItem() || definition.isGroundTile() ||
		definition.attribute(ItemAttributeKey::BorderBaseGroundId) > 0) return false;
	// The preview holds only plain items without attributes; a container, teleport, depot and the like is made
	// as its own Item kind and would be refused there.
	const auto probe = Item::Create(id);
	return probe && typeid(*probe) == typeid(Item) && (!probe->getAttributesView() || probe->getAttributesView()->empty());
}
}
