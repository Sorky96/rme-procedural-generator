// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/snapshot_equal.h"
#include "procedural/adapter/snapshot_memory.h"

#include "map/tile.h"
#include "game/complexitem.h"
#include "game/creature.h"
#include "game/spawn.h"
#include <algorithm>
#include <bit>
#include <typeinfo>
#include <tuple>

namespace rme::procedural {
namespace {
bool attributeEqual(const ItemAttribute& a, const ItemAttribute& b) noexcept {
	if (a.type != b.type) return false;
	switch (a.type) {
		case ItemAttribute::STRING: return a.getString() && b.getString() && *a.getString() == *b.getString();
		case ItemAttribute::INTEGER: return a.getInteger() && b.getInteger() && *a.getInteger() == *b.getInteger();
		case ItemAttribute::BOOLEAN: return a.getBoolean() && b.getBoolean() && *a.getBoolean() == *b.getBoolean();
		case ItemAttribute::FLOAT:
		case ItemAttribute::DOUBLE: return a.getFloat() && b.getFloat() &&
			std::bit_cast<uint64_t>(*a.getFloat()) == std::bit_cast<uint64_t>(*b.getFloat());
		case ItemAttribute::NONE: return !a.getString() && !a.getInteger() && !a.getBoolean() && !a.getFloat() &&
			!b.getString() && !b.getInteger() && !b.getBoolean() && !b.getFloat();
		default: return false;
	}
}

bool boundedOpaqueTree(const PreservedOTBMNode& node, unsigned depth = 1) noexcept {
	if (depth > kSnapshotMaxNestingDepth) return false;
	for (const auto& child : node.children) if (!boundedOpaqueTree(child, depth + 1)) return false;
	return true;
}

bool attributesEqual(const Item& a, const Item& b) noexcept {
	const auto* first = a.getAttributesView();
	const auto* second = b.getAttributesView();
	if (!first || first->empty()) return !second || second->empty();
	if (!second || first->size() != second->size()) return false;
	return std::ranges::equal(*first, *second, [](const auto& left, const auto& right) {
		return left.first == right.first && attributeEqual(left.second, right.second);
	});
}

bool itemsEqual(const Item* a, const Item* b, unsigned depth) noexcept {
	if (a == b) return true;
	if (!a || !b || depth > kSnapshotMaxNestingDepth || typeid(*a) != typeid(*b) || a->getID() != b->getID() ||
		a->getSubtype() != b->getSubtype() || a->isSelected() != b->isSelected() || !attributesEqual(*a, *b)) return false;
	if (typeid(*a) != typeid(Item) && typeid(*a) != typeid(Container) && typeid(*a) != typeid(Teleport) &&
		typeid(*a) != typeid(Door) && typeid(*a) != typeid(Depot) && typeid(*a) != typeid(Podium)) return false;
	const auto* invalidA = a->getInvalidOTBMData();
	const auto* invalidB = b->getInvalidOTBMData();
	if ((invalidA && invalidA->rawNode && !boundedOpaqueTree(*invalidA->rawNode)) ||
		(invalidB && invalidB->rawNode && !boundedOpaqueTree(*invalidB->rawNode))) return false;
	if (bool(invalidA) != bool(invalidB) || (invalidA && *invalidA != *invalidB)) return false;
	if (const auto* container = a->asContainer()) {
		const auto* other = b->asContainer();
		if (!other || container->getItemCount() != other->getItemCount()) return false;
		for (size_t i = 0; i < container->getItemCount(); ++i) {
			if (!itemsEqual(container->getItem(i), other->getItem(i), depth + 1)) return false;
		}
	}
	if (const auto* teleport = a->asTeleport(); teleport && teleport->getDestination() != b->asTeleport()->getDestination()) return false;
	if (const auto* door = a->asDoor(); door && door->getDoorID() != b->asDoor()->getDoorID()) return false;
	if (const auto* depot = a->asDepot(); depot && depot->getDepotID() != b->asDepot()->getDepotID()) return false;
	if (const auto* podium = a->asPodium()) {
		const auto* other = b->asPodium();
		if (podium->getDirection() != other->getDirection() || podium->getShowOutfit() != other->getShowOutfit() ||
			podium->getShowMount() != other->getShowMount() || podium->getShowPlatform() != other->getShowPlatform()) return false;
		const auto& first = podium->getOutfit();
		const auto& second = other->getOutfit();
		const auto fields = [](const Outfit& outfit) {
			return std::tie(outfit.lookType, outfit.lookItem, outfit.lookMount, outfit.lookAddon,
				outfit.lookHead, outfit.lookBody, outfit.lookLegs, outfit.lookFeet,
				outfit.lookMountHead, outfit.lookMountBody, outfit.lookMountLegs, outfit.lookMountFeet);
		};
		if (fields(first) != fields(second)) return false;
	}
	return true;
}

bool tilesEqualImpl(const Tile* first, const Tile* second, bool derivedState) noexcept {
	if (first == second) return true;
	for (const auto* tile : {first, second}) {
		if (tile && tile->extra && tile->extra->invalidContent) {
			for (const auto& node : tile->extra->invalidContent->opaqueChildNodes) if (!boundedOpaqueTree(node)) return false;
		}
	}
	constexpr uint16_t derivedFlags = TILESTATE_UNIQUE | TILESTATE_BLOCKING | TILESTATE_HAS_TABLE |
		TILESTATE_HAS_CARPET | TILESTATE_HOOK_SOUTH | TILESTATE_HOOK_EAST | TILESTATE_HAS_LIGHT;
	const uint16_t flags = derivedState ? UINT16_MAX : uint16_t(~derivedFlags);
	if (!first || !second || first->house_id != second->house_id || first->mapflags != second->mapflags ||
		(first->statflags & flags) != (second->statflags & flags) || (derivedState && first->minimapColor != second->minimapColor) ||
		first->items.size() != second->items.size() || bool(first->extra) != bool(second->extra) ||
		(first->extra && (first->extra->zoneIds != second->extra->zoneIds ||
			bool(first->extra->invalidContent) != bool(second->extra->invalidContent) ||
			(first->extra->invalidContent && *first->extra->invalidContent != *second->extra->invalidContent))) ||
		!itemsEqual(first->ground.get(), second->ground.get(), 1)) return false;
	for (size_t i = 0; i < first->items.size(); ++i) {
		if (!itemsEqual(first->items[i].get(), second->items[i].get(), 1)) return false;
	}
	if (bool(first->creature) != bool(second->creature) ||
		(first->creature && !first->creature->hasSameState(*second->creature))) return false;
	if (bool(first->spawn) != bool(second->spawn)) return false;
	if (first->spawn && (first->spawn->getSize() != second->spawn->getSize() ||
		first->spawn->getNpcSize() != second->spawn->getNpcSize() || first->spawn->getKinds() != second->spawn->getKinds() ||
		first->spawn->isSelected() != second->spawn->isSelected())) return false;
	return true;
}
} // namespace

bool snapshotTilesEqual(const Tile* first, const Tile* second) noexcept { return tilesEqualImpl(first,second,true); }
bool snapshotTileContentsEqual(const Tile* first, const Tile* second) noexcept { return tilesEqualImpl(first,second,false); }

} // namespace rme::procedural
