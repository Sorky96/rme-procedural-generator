//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_LUA_ITEM_REFERENCE_H_
#define RME_LUA_ITEM_REFERENCE_H_

#include "lua/lua_object_registry.h"
#include "map/position.h"
#include <cstdint>
#include <optional>
#include <functional>
#include <memory>

class Item;
class Map;
class Tile;
class LuaEngine;

namespace LuaAPI {

// A handle resolves only while the same tile is installed on the same map.
// The object registry also distinguishes a replacement allocated at the same
// address. Dereference must be serialized with map and object destruction.
class TileReference {
public:
	TileReference(ObjectRegistry<Tile>::Handle tile, Map* map, Position position, uint64_t mapGeneration);
	// The caller must supply a live tile and serialize creation with map edits.
	[[nodiscard]] static std::optional<TileReference> create(LuaEngine& engine, Map* map, Tile* tile);
	[[nodiscard]] Tile* get() const;
	[[nodiscard]] Tile* require() const;

private:
	ObjectRegistry<Tile>::Handle tile;
	Map* map;
	Position position;
	uint64_t mapGeneration;
};

// A non-owning item reference tied to the map and tile from which it came.
class ItemReference {
public:
	ItemReference(ObjectRegistry<Item>::Handle item, TileReference tile);
	explicit ItemReference(std::unique_ptr<Item> detached);
	// The caller must supply live objects and serialize creation with map edits.
	[[nodiscard]] static std::optional<ItemReference> create(LuaEngine& engine, Map* map, Tile* tile, Item* item);

	[[nodiscard]] Item* get() const;
	[[nodiscard]] Item* require() const;
	[[nodiscard]] bool belongsTo(Tile* owner) const;
	void mutate(const std::function<void(Item&)>& mutation) const;
private:
	[[nodiscard]] Tile* currentTile() const;
	ObjectRegistry<Item>::Handle item;
	std::optional<TileReference> tile;
	// Copies of a Lua reference can outlive one another; detached clones have
	// no map owner, so these independent handles share ownership of the clone.
	std::shared_ptr<Item> detached;
};

// Called only with a live tile obtained from the active map.
[[nodiscard]] std::optional<TileReference> tileReferenceForCurrentMap(Tile* tile);
[[nodiscard]] std::optional<ItemReference> itemReferenceForCurrentMap(Tile* tile, Item* item);

} // namespace LuaAPI

#endif
