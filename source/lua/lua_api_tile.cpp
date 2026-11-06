//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "lua_api_tile.h"
#include "lua_api.h"
#include "lua/lua_item_reference.h"
#include <algorithm>
#include <iterator>
#include <limits>
#include "map/tile.h"
#include "map/tile_operations.h"
#include "game/item.h"
#include "game/creature.h"
#include "game/spawn.h"
#include "brushes/brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/door/door_brush.h"
#include "ui/gui.h"
#include "editor/editor.h"
#include "map/map.h"

namespace LuaAPI {

	static sol::table makePositionTable(sol::this_state ts, const Position& pos) {
		sol::state_view lua(ts);
		sol::table tbl = lua.create_table();
		tbl["x"] = pos.x;
		tbl["y"] = pos.y;
		tbl["z"] = pos.z;
		return tbl;
	}

	// Helper to get items as a Lua table
	static sol::table getTileItems(Tile* tile, sol::this_state ts) {
		sol::state_view lua(ts);
		sol::table items = lua.create_table();

		if (!tile) {
			return items;
		}

		int idx = 1;
		for (const auto& item : tile->items) {
			if (item) {
				if (auto ref = itemReferenceForCurrentMap(tile, item.get())) items[idx++] = std::move(*ref);
			}
		}
		return items;
	}

	// Add item to tile
	static Item* addItemToTile(Tile* tile, int itemId, sol::optional<int> countOpt) {
		if (!tile) {
			throw sol::error("Invalid tile");
		}

		if (itemId < 0) {
			throw sol::error("addItemToTile: itemId must be non-negative");
		}
		if (countOpt && (*countOpt < 0 || *countOpt > 65535)) {
			throw sol::error("addItemToTile: count must be between 0 and 65535");
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		std::unique_ptr<Item> item = Item::Create(static_cast<ServerItemId>(itemId));
		if (!item) {
			throw sol::error("Failed to create item with id " + std::to_string(itemId));
		}

		if (countOpt) {
			item->setSubtype(static_cast<uint16_t>(*countOpt));
		}

		Item* ptr = item.get();
		tile->addItem(std::move(item));
		tile->modify();

		return ptr;
	}

	// Remove item from tile
	static bool removeItemFromTile(Tile* tile, Item* itemToRemove) {
		if (!tile || !itemToRemove) {
			return false;
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		// Find and remove the item
		for (auto it = tile->items.begin(); it != tile->items.end(); ++it) {
			if (it->get() == itemToRemove) {
				tile->items.erase(it);
				tile->modify();
				return true;
			}
		}

		// Check if it's the ground
		if (tile->ground.get() == itemToRemove) {
			tile->ground.reset();
			tile->modify();
			return true;
		}

		return false;
	}

	// Set creature on tile
	static Creature* setTileCreature(Tile* tile, const std::string& creatureName, sol::optional<int> spawnTimeOpt, sol::optional<int> directionOpt) {
		if (!tile) {
			throw sol::error("Invalid tile");
		}

		// Check if creature type exists
		CreatureType* type = g_creatures[creatureName];
		if (!type) {
			throw sol::error("Unknown creature type: " + creatureName);
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		// Create new creature
		tile->creature = std::make_unique<Creature>(creatureName);

		// Set spawn time (default to global setting or 60s)
		int spawnTime = spawnTimeOpt.value_or(g_gui.GetSpawnTime());
		tile->creature->setSpawnTime(spawnTime);

		// Set direction (default SOUTH)
		Direction dir = static_cast<Direction>(directionOpt.value_or(SOUTH));
		if (dir >= DIRECTION_FIRST && dir <= DIRECTION_LAST) {
			tile->creature->setDirection(dir);
		}

		tile->modify();
		return tile->creature.get();
	}

	// Remove creature from tile
	static bool removeTileCreature(Tile* tile) {
		if (!tile || !tile->creature) {
			return false;
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		tile->creature.reset();
		tile->modify();
		return true;
	}

	// Set spawn on tile
	static Spawn* setTileSpawn(Tile* tile, sol::optional<int> sizeOpt) {
		if (!tile) {
			throw sol::error("Invalid tile");
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		// Get map instance
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			throw sol::error("No active editor");
		}
		Map& map = editor->map;

		// Remove existing spawn from map metadata
		if (tile->spawn) {
			map.removeSpawn(tile);
		}

		// Create new spawn with given size (default 3)
		int size = sizeOpt.value_or(3);
		if (size < 1) {
			size = 1;
		}
		if (size > 50) {
			size = 50;
		}

		tile->spawn = std::make_unique<Spawn>(size);

		// Register new spawn with map
		map.addSpawn(tile);

		tile->modify();
		return tile->spawn.get();
	}

	// Remove spawn from tile
	static bool removeTileSpawn(Tile* tile) {
		if (!tile || !tile->spawn) {
			return false;
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		// Get map instance
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			throw sol::error("No active editor");
		}
		Map& map = editor->map;

		// Remove spawn from map metadata
		map.removeSpawn(tile);

		tile->spawn.reset();
		tile->modify();
		return true;
	}

	// Set ground item
	static void setTileGround(Tile* tile, sol::object groundObj) {
		if (!tile) {
			throw sol::error("Invalid tile");
		}

		if (groundObj.is<int>()) {
			int groundId = groundObj.as<int>();
			if (groundId < 1 || groundId > std::numeric_limits<uint16_t>::max()) {
				throw sol::error("setTileGround: ground item id must be between 1 and 65535");
			}
			std::unique_ptr<Item> ground = Item::Create(static_cast<uint16_t>(groundId));
			if (!ground) {
				throw sol::error("setTileGround: failed to create ground item " + std::to_string(groundId));
			}
			markTileForUndo(tile);
			tile->ground = std::move(ground);
			tile->modify();
			return;
		} else if (groundObj.is<ItemReference>()) {
			Item* item = groundObj.as<ItemReference>().require();
			if (!item) {
				throw sol::error("setTileGround: invalid ground item");
			}
			if (!item->getGroundBrush()) {
				throw sol::error("setTileGround: item is not a ground item");
			}
			markTileForUndo(tile);
			tile->ground = item->deepCopy();
			tile->modify();
			return;
		} else if (groundObj.is<sol::nil_t>()) {
			markTileForUndo(tile);
			tile->ground.reset();
			tile->modify();
			return;
		}

		throw sol::error("setTileGround: expected item id, ground item, or nil");
	}

	// Set house ID
	static void setTileHouseId(Tile* tile, uint32_t houseId) {
		if (!tile) {
			return;
		}

		// Mark tile for undo before modification
		markTileForUndo(tile);

		tile->setHouseID(houseId);
		tile->modify();
	}

	// Apply a brush to a tile (with optional auto-bordering)
	static bool applyBrushToTile(Tile* tile, const std::string& brushName, sol::optional<bool> autoBorder) {
		if (!tile) {
			return false;
		}

		Brush* brush = g_brushes.getBrush(brushName);
		if (!brush) {
			return false;
		}

		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return false;
		}

		markTileForUndo(tile);

		bool success = false;

		// Apply the brush based on its type
		if (brush->is<GroundBrush>()) {
			GroundBrush* groundBrush = brush->as<GroundBrush>();
			if (groundBrush) {
				groundBrush->draw(&editor->map, tile, nullptr);
				success = true;
			}
		} else if (brush->is<DoodadBrush>()) {
			DoodadBrush* doodadBrush = brush->as<DoodadBrush>();
			if (doodadBrush) {
				doodadBrush->draw(&editor->map, tile, nullptr);
				success = true;
			}
		} else if (brush->is<WallBrush>()) {
			WallBrush* wallBrush = brush->as<WallBrush>();
			if (wallBrush) {
				wallBrush->draw(&editor->map, tile, nullptr);
				success = true;
			}
		} else if (brush->is<DoorBrush>()) {
			DoorBrush* doorBrush = brush->as<DoorBrush>();
			if (doorBrush) {
				doorBrush->draw(&editor->map, tile, nullptr);
				success = true;
			}
		}

		if (success) {
			// Apply auto-bordering if requested (default: true for ground brushes)
			bool doBorder = autoBorder.value_or(brush->is<GroundBrush>());
			if (doBorder) {
				TileOperations::borderize(tile, &editor->map);
			}
			tile->modify();
		}

		return success;
	}

	void registerTile(sol::state& lua) {
		// Register Tile usertype
		lua.new_usertype<TileReference>(
			"Tile",
			// No public constructor - tiles are obtained from the map
			sol::no_constructor,

			// Position properties (read-only)
			"position", sol::property([](const TileReference& tileRef, sol::this_state ts) -> sol::table { Tile* tile = tileRef.require();
				return makePositionTable(ts, tile ? tile->getPosition() : Position());
			}),
			"x", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile ? tile->getX() : 0; }),
			"y", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile ? tile->getY() : 0; }),
			"z", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile ? tile->getZ() : 0; }),

			// Ground (read/write)
			"ground", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return itemReferenceForCurrentMap(tile, tile->ground.get()); }, [](const TileReference& tileRef, sol::object ground) { setTileGround(tileRef.require(), ground); }),
			"hasGround", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && tile->hasGround(); }),

			// Items collection (read-only - use addItem/removeItem to modify)
			"items", sol::property([](const TileReference& tileRef, sol::this_state ts) { return getTileItems(tileRef.require(), ts); }),
			"itemCount", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile ? static_cast<int>(tile->items.size()) : 0; }),

			// House
			"houseId", sol::property([](const TileReference& tileRef) -> uint32_t { return tileRef.require()->getHouseID(); }, [](const TileReference& tileRef, uint32_t id) { setTileHouseId(tileRef.require(), id); }),
			"isHouseTile", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && tile->isHouseTile(); }),
			"isHouseExit", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && tile->isHouseExit(); }),

			// Flags (read-only for now, some could be made writable)
			"isPZ", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && tile->isPZ(); }),
			"isBlocking", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && tile->isBlocking(); }),
			"hasBorders", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && tile->hasBorders(); }),
			"hasWall", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && tile->hasWall(); }),
			"hasTable", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && tile->hasTable(); }),
			"hasCarpet", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && tile->hasCarpet(); }),
			"groundZOrder", sol::property([](const TileReference& tileRef) -> int { Tile* tile = tileRef.require();
				if (!tile) {
					return 0;
				}
				GroundBrush* brush = tile->getGroundBrush();
				return brush ? brush->getZ() : 0;
			}),
			"isNoPvp", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && (tile->getMapFlags() & TILESTATE_NOPVP); }),
			"isNoLogout", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && (tile->getMapFlags() & TILESTATE_NOLOGOUT); }),
			"isPvpZone", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && (tile->getMapFlags() & TILESTATE_PVPZONE); }),

			// Map flags
			"mapFlags", sol::property([](const TileReference& tileRef) -> uint32_t { Tile* tile = tileRef.require(); return tile ? tile->getMapFlags() : 0; }, [](const TileReference& tileRef, uint32_t flags) { Tile* tile = tileRef.require();
					if (tile) {
						markTileForUndo(tile);
						tile->setMapFlags(flags);
						tile->modify();
					} }),

			// Selection
			"isSelected", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && tile->isSelected(); }),
			"select", [](const TileReference& tileRef) { Tile* tile = tileRef.require(); if (tile) tile->select(); },
			"deselect", [](const TileReference& tileRef) { Tile* tile = tileRef.require(); if (tile) tile->deselect(); },

			// Creature and Spawn (read-only access, use methods to modify)
			"creature", sol::property([](const TileReference& tileRef) -> Creature* { Tile* tile = tileRef.require(); return tile ? tile->creature.get() : nullptr; }),
			"spawn", sol::property([](const TileReference& tileRef) -> Spawn* { Tile* tile = tileRef.require(); return tile ? tile->spawn.get() : nullptr; }),
			"hasCreature", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && tile->creature != nullptr; }),
			"hasSpawn", sol::property([](const TileReference& tileRef) { Tile* tile = tileRef.require(); return tile && tile->spawn != nullptr; }),

			// Creature methods
			"setCreature", sol::overload([](const TileReference& tileRef, const std::string& name) -> Creature* { Tile* tile = tileRef.require(); return setTileCreature(tile, name, sol::nullopt, sol::nullopt); }, [](const TileReference& tileRef, const std::string& name, int spawnTime) -> Creature* { Tile* tile = tileRef.require(); return setTileCreature(tile, name, spawnTime, sol::nullopt); }, [](const TileReference& tileRef, const std::string& name, int spawnTime, int direction) -> Creature* { Tile* tile = tileRef.require(); return setTileCreature(tile, name, spawnTime, direction); }),
			"removeCreature", [](const TileReference& tileRef) { return removeTileCreature(tileRef.require()); },

			// Spawn methods
			"setSpawn", sol::overload([](const TileReference& tileRef) -> Spawn* { Tile* tile = tileRef.require(); return setTileSpawn(tile, sol::nullopt); }, [](const TileReference& tileRef, int size) -> Spawn* { Tile* tile = tileRef.require(); return setTileSpawn(tile, size); }),
			"removeSpawn", [](const TileReference& tileRef) { return removeTileSpawn(tileRef.require()); },

			// Methods
			"addItem", sol::overload([](const TileReference& tileRef, int itemId) { Tile* tile = tileRef.require(); return itemReferenceForCurrentMap(tile, addItemToTile(tile, itemId, sol::nullopt)); }, [](const TileReference& tileRef, int itemId, int count) { Tile* tile = tileRef.require(); return itemReferenceForCurrentMap(tile, addItemToTile(tile, itemId, count)); }),
			"removeItem", [](const TileReference& tileRef, const ItemReference& item) { return removeItemFromTile(tileRef.require(), item.require()); },
			"applyBrush", [](const TileReference& tileRef, const std::string& name, sol::optional<bool> border) { return applyBrushToTile(tileRef.require(), name, border); },
			"borderize", [](const TileReference& tileRef) { Tile* tile = tileRef.require();
				if (!tile) return;
				Editor* editor = g_gui.GetCurrentEditor();
				if (!editor) return;
				markTileForUndo(tile);
				TileOperations::borderize(tile, &editor->map);
				tile->modify(); },
			"wallize", [](const TileReference& tileRef) { Tile* tile = tileRef.require();
				if (!tile) return;
				Editor* editor = g_gui.GetCurrentEditor();
				if (!editor) return;
				markTileForUndo(tile);
				TileOperations::wallize(tile, &editor->map);
				tile->modify(); },
			"moveItem", sol::overload(
							// Index-based move within same tile
							// Semantics: Move item at fromIdx so it ends up at toIdx position
							[](const TileReference& tileRef, int fromIdx, int toIdx) { Tile* tile = tileRef.require();
								if (!tile) {
									return;
								}

								int from = fromIdx - 1;
								int to = toIdx - 1;
								int size = (int)tile->items.size();

								if (from < 0 || from >= size || to < 0 || to >= size || from == to) {
									return;
								}

								markTileForUndo(tile);

								std::unique_ptr<Item> item = std::move(tile->items[from]);
								tile->items.erase(tile->items.begin() + from);

								int insertPos = std::clamp(to, 0, (int)tile->items.size());

								tile->items.insert(tile->items.begin() + insertPos, std::move(item));
								tile->modify();
							},
							// Move specific item object to new index in same tile
							[](const TileReference& tileRef, const ItemReference& itemRef, int toIdx) { Tile* tile = tileRef.require(); Item* item = itemRef.require();
								if (!tile || !item) {
									return;
								}
								auto it = std::find_if(tile->items.begin(), tile->items.end(), [item](const std::unique_ptr<Item>& i) { return i.get() == item; });
								if (it == tile->items.end()) {
									return;
								}

								int from = (int)std::distance(tile->items.begin(), it);
								int to = std::clamp(toIdx - 1, 0, (int)tile->items.size() - 1);
								if (from == to) {
									return;
								}

								markTileForUndo(tile);
								std::unique_ptr<Item> movedItem = std::move(*it);
								tile->items.erase(it);

								int insertPos = std::clamp(to, 0, (int)tile->items.size());
								tile->items.insert(tile->items.begin() + insertPos, std::move(movedItem));
								tile->modify();
							},
							// Move item to DIFFERENT tile
							[](const TileReference& sourceTileRef, const ItemReference& itemRef, const TileReference& destTileRef, sol::optional<int> toIdx) { Tile* sourceTile = sourceTileRef.require(); Item* item = itemRef.require(); Tile* destTile = destTileRef.require();
								if (!sourceTile || !item || !destTile) {
									return;
								}
								auto it = std::find_if(sourceTile->items.begin(), sourceTile->items.end(), [item](const std::unique_ptr<Item>& i) { return i.get() == item; });
								if (it == sourceTile->items.end()) {
									return;
								}

								markTileForUndo(sourceTile);
								markTileForUndo(destTile);

								std::unique_ptr<Item> movedItem = std::move(*it);
								sourceTile->items.erase(it);
								int to = toIdx ? std::clamp(*toIdx - 1, 0, (int)destTile->items.size()) : (int)destTile->items.size();
								destTile->items.insert(destTile->items.begin() + to, std::move(movedItem));

								sourceTile->modify();
								destTile->modify();
							}
						),

			"getPosition", [](const TileReference& tileRef, sol::this_state ts) -> sol::table { Tile* tile = tileRef.require();
				return makePositionTable(ts, tile ? tile->getPosition() : Position());
			},

			"getItemAt", [](const TileReference& tileRef, int index) { Tile* tile = tileRef.require();
				// Lua uses 1-based indexing
				return itemReferenceForCurrentMap(tile, tile->getItemAt(index - 1)); },

			"getTopItem", [](const TileReference& tileRef) { Tile* tile = tileRef.require(); return itemReferenceForCurrentMap(tile, tile->getTopItem()); },

			"getWall", [](const TileReference& tileRef) { Tile* tile = tileRef.require(); return itemReferenceForCurrentMap(tile, tile->getWall()); },

			"getTable", [](const TileReference& tileRef) { Tile* tile = tileRef.require(); return itemReferenceForCurrentMap(tile, tile->getTable()); },

			"getCarpet", [](const TileReference& tileRef) { Tile* tile = tileRef.require(); return itemReferenceForCurrentMap(tile, tile->getCarpet()); },

			// String representation
			sol::meta_function::to_string, [](const TileReference& tileRef) { Tile* tile = tileRef.require();
				if (!tile) return std::string("Tile(invalid)");
				Position pos = tile->getPosition();
				return "Tile(" + std::to_string(pos.x) + ", " +
					std::to_string(pos.y) + ", " + std::to_string(pos.z) + ")"; }
		);
	}
} // namespace LuaAPI
