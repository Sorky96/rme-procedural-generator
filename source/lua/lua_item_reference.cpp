//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "lua/lua_item_reference.h"
#include "lua/lua_engine.h"
#include "lua/lua_script_manager.h"
#include "lua/lua_api.h"
#include "editor/editor.h"
#include "game/complexitem.h"
#include "map/map.h"
#include "map/tile.h"
#include "ui/gui.h"

namespace LuaAPI {
namespace {
	bool containsItem(const Item* root, const Item* target, unsigned depth = 0) {
		if (!root) return false;
		if (root == target) return true;
		if (depth >= 256) return false;
		const Container* container = root->asContainer();
		if (!container) return false;
		for (const auto& child : container->getVector()) {
			if (containsItem(child.get(), target, depth + 1)) return true;
		}
		return false;
	}
}

TileReference::TileReference(ObjectRegistry<Tile>::Handle tileHandle, Map* ownerMap,
	Position ownerPosition, uint64_t generation) :
	tile(std::move(tileHandle)), map(ownerMap), position(ownerPosition), mapGeneration(generation) {}

std::optional<TileReference> TileReference::create(LuaEngine& engine, Map* map, Tile* tile) {
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor || !map || !tile || editor->getMap() != map) return std::nullopt;
	const Position position = tile->getPosition();
	if (map->getTile(position) != tile) return std::nullopt;
	return TileReference(engine.tileReferences().watch(tile), map, position, map->getGeneration());
}

Tile* TileReference::get() const {
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor || !map || editor->getMap() != map || map->getGeneration() != mapGeneration) return nullptr;
	Tile* expected = tile.get();
	return expected && map->getTile(position) == expected ? expected : nullptr;
}

Tile* TileReference::require() const {
	Tile* result = get();
	if (!result) throw sol::error("Tile reference is no longer valid");
	return result;
}

ItemReference::ItemReference(ObjectRegistry<Item>::Handle itemHandle, TileReference owner) :
	item(std::move(itemHandle)), tile(std::move(owner)) {}

ItemReference::ItemReference(std::unique_ptr<Item> owned) : detached(std::move(owned)) {}

std::optional<ItemReference> ItemReference::create(LuaEngine& engine, Map* map, Tile* tile, Item* item) {
	if (!item) return std::nullopt;
	auto owner = TileReference::create(engine, map, tile);
	if (!owner) return std::nullopt;
	ItemReference reference(engine.itemReferences().watch(item), std::move(*owner));
	if (!reference.get()) return std::nullopt;
	return reference;
}

Tile* ItemReference::currentTile() const {
	return tile ? tile->get() : nullptr;
}

Item* ItemReference::get() const {
	if (detached) return detached.get();
	Tile* owner = currentTile();
	Item* target = item.get();
	if (!owner || !target) return nullptr;
	if (containsItem(owner->ground.get(), target)) return target;
	for (const auto& root : owner->items) {
		if (containsItem(root.get(), target)) return target;
	}
	return nullptr;
}

Item* ItemReference::require() const {
	Item* result = get();
	if (!result) throw sol::error("Item reference is no longer valid");
	return result;
}

bool ItemReference::belongsTo(Tile* owner) const {
	return owner && currentTile() == owner && get() != nullptr;
}

void ItemReference::mutate(const std::function<void(Item&)>& mutation) const {
	Item* object = require();
	if (detached) {
		mutation(*object);
		return;
	}
	Tile* owner = currentTile();
	if (!owner) throw sol::error("Item owner is no longer on the current map");
	mutateTrackedItem(owner, [&] { mutation(*object); });
}

std::optional<TileReference> tileReferenceForCurrentMap(Tile* tile) {
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) return std::nullopt;
	return TileReference::create(g_luaScripts.getEngine(), editor->getMap(), tile);
}

std::optional<ItemReference> itemReferenceForCurrentMap(Tile* tile, Item* item) {
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) return std::nullopt;
	return ItemReference::create(g_luaScripts.getEngine(), editor->getMap(), tile, item);
}

} // namespace LuaAPI
