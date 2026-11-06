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
#include "lua_api_item.h"
#include "lua/lua_item_reference.h"
#include "game/item.h"
#include "game/items.h"
#include "game/complexitem.h"

#include <algorithm>
#include <cctype>

namespace LuaAPI {

	// Helper: convert string to lowercase
	static std::string toLower(const std::string& str) {
		std::string result = str;
		std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });
		return result;
	}

	// Helper: check if string contains another (case-insensitive)
	static bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
		std::string lowerHaystack = toLower(haystack);
		std::string lowerNeedle = toLower(needle);
		return lowerHaystack.find(lowerNeedle) != std::string::npos;
	}

	// Optimized helper when needle is already lowercased
	static bool containsIgnoreCaseOptimized(const std::string& haystack, const std::string& lowerNeedle) {
		std::string lowerHaystack = toLower(haystack);
		return lowerHaystack.find(lowerNeedle) != std::string::npos;
	}

		template <auto Method>
		static auto readItem() {
			return [](const ItemReference& reference) { return (reference.require()->*Method)(); };
		}

		void registerItem(sol::state& lua) {
			lua.new_usertype<ItemReference>("Item", sol::no_constructor,
				"id", sol::property(readItem<&Item::getID>()),
				"clientId", sol::property(readItem<&Item::getClientID>()),
				"name", sol::property(readItem<&Item::getName>()),
				"fullName", sol::property(readItem<&Item::getFullName>()),
				"count", sol::property(readItem<&Item::getCount>(), [](const ItemReference& ref, int value) {
					if (value < 0 || value > 65535) throw sol::error("item.count: value must be between 0 and 65535.");
					ref.mutate([value](Item& item) { item.setSubtype(static_cast<uint16_t>(value)); });
				}),
				"subtype", sol::property(readItem<&Item::getSubtype>(), [](const ItemReference& ref, int value) {
					if (value < 0 || value > 65535) throw sol::error("item.subtype: value must be between 0 and 65535.");
					ref.mutate([value](Item& item) { item.setSubtype(static_cast<uint16_t>(value)); });
				}),
				"actionId", sol::property(readItem<&Item::getActionID>(), [](const ItemReference& ref, int value) {
					if (value < 0 || value > 65535) throw sol::error("item.actionId: value must be between 0 and 65535.");
					ref.mutate([value](Item& item) { item.setActionID(static_cast<uint16_t>(value)); });
				}),
				"uniqueId", sol::property(readItem<&Item::getUniqueID>(), [](const ItemReference& ref, int value) {
					if (value < 0 || value > 65535) throw sol::error("item.uniqueId: value must be between 0 and 65535.");
					ref.mutate([value](Item& item) { item.setUniqueID(static_cast<uint16_t>(value)); });
				}),
				"tier", sol::property(readItem<&Item::getTier>(), [](const ItemReference& ref, int value) {
					if (value < 0 || value > 65535) throw sol::error("item.tier: value must be between 0 and 65535.");
					ref.mutate([value](Item& item) { item.setTier(static_cast<uint16_t>(value)); });
				}),
				"text", sol::property(readItem<&Item::getText>(), [](const ItemReference& ref, const std::string& value) {
					ref.mutate([&](Item& item) { item.setText(value); });
				}),
				"description", sol::property(readItem<&Item::getDescription>(), [](const ItemReference& ref, const std::string& value) {
					ref.mutate([&](Item& item) { item.setDescription(value); });
				}),
				"isSelected", sol::property(readItem<&Item::isSelected>()),
				"select", [](const ItemReference& ref) { ref.require()->select(); },
				"deselect", [](const ItemReference& ref) { ref.require()->deselect(); },
				"isStackable", sol::property(readItem<&Item::isStackable>()),
				"isMoveable", sol::property(readItem<&Item::isMoveable>()),
				"isPickupable", sol::property(readItem<&Item::isPickupable>()),
				"isBlocking", sol::property(readItem<&Item::isBlocking>()),
				"isGroundTile", sol::property(readItem<&Item::isGroundTile>()),
				"isBorder", sol::property(readItem<&Item::isBorder>()),
				"isWall", sol::property(readItem<&Item::isWall>()),
				"isDoor", sol::property(readItem<&Item::isDoor>()),
				"isTable", sol::property(readItem<&Item::isTable>()),
				"isCarpet", sol::property(readItem<&Item::isCarpet>()),
				"isHangable", sol::property(readItem<&Item::isHangable>()),
				"isRoteable", sol::property(readItem<&Item::isRoteable>()),
				"isFluidContainer", sol::property(readItem<&Item::isFluidContainer>()),
				"isSplash", sol::property(readItem<&Item::isSplash>()),
				"hasCharges", sol::property(readItem<&Item::hasCharges>()),
				"isPodium", sol::property([](const ItemReference& ref) { return ref.require()->asPodium() != nullptr; }),
				"setPodiumOutfit", [](const ItemReference& ref, int lookType, sol::optional<int> direction) {
					if (!ref.require()->asPodium()) throw sol::error("setPodiumOutfit: item is not a podium");
					if (lookType < 0) throw sol::error("setPodiumOutfit: lookType must not be negative");
					if (direction && (*direction < 0 || *direction > 255)) throw sol::error("setPodiumOutfit: direction must be between 0 and 255");
					ref.mutate([&](Item& item) {
						Podium* podium = item.asPodium();
						Outfit outfit = podium->getOutfit();
						outfit.lookType = lookType;
						podium->setOutfit(outfit);
						if (direction) podium->setDirection(static_cast<uint8_t>(*direction));
					});
				},
				"hasElevation", sol::property([](const ItemReference& ref) {
					const Item& item = *ref.require();
					return g_items.typeExists(item.getID()) && g_items[item.getID()].hasElevation;
				}),
				"zOrder", sol::property(readItem<&Item::getTopOrder>()),
				"clone", [](const ItemReference& ref) -> std::optional<ItemReference> {
					auto clone = ref.require()->deepCopy();
					if (!clone) return std::nullopt;
					return ItemReference(std::move(clone));
				},
				"rotate", [](const ItemReference& ref) { ref.mutate([](Item& item) { item.doRotate(); }); },
				sol::meta_function::to_string, [](const ItemReference& ref) {
					const Item& item = *ref.require();
					return "Item(id=" + std::to_string(item.getID()) + ", name=\"" + std::string(item.getName()) + "\")";
				}
			);

		// Register Items namespace for item lookup
		sol::table items = lua.create_named_table("Items");

		// Get item info by ID - returns a table with item properties
		items["getInfo"] = [](sol::this_state ts, int id) -> sol::object {
			sol::state_view lua(ts);
			if (!g_items.typeExists(id)) {
				return sol::nil;
			}
			ItemType it = g_items.getItemType(id);
			sol::table info = lua.create_table();
			info["id"] = it.id;
			info["clientId"] = it.clientID;
			info["name"] = it.name;
			info["description"] = it.description;
			info["isStackable"] = it.stackable;
			info["isMoveable"] = it.moveable;
			info["isPickupable"] = it.pickupable;
			info["isGroundTile"] = it.isGroundTile();
			info["isBorder"] = it.isBorder;
			info["isWall"] = it.isWall;
			info["isDoor"] = it.isDoor();
			info["isTable"] = it.isTable;
			info["isCarpet"] = it.isCarpet;
			const auto definition = g_item_definitions.get(id);
			info["isPodium"] = definition && definition.isPodium();
			info["hasElevation"] = it.hasElevation;
			return info;
		};

		// Check if item ID exists
		items["exists"] = [](int id) -> bool {
			return g_items.typeExists(id);
		};

		// Get max item ID
		items["getMaxId"] = []() -> int {
			return g_items.getMaxID();
		};

		// Find items by name (returns array of {id, name} tables)
		items["findByName"] = [](sol::this_state ts, const std::string& searchName, sol::optional<int> maxResults) -> sol::table {
			sol::state_view lua(ts);
			sol::table results = lua.create_table();

			int limit = maxResults.value_or(50);
			int count = 0;
			int maxId = g_items.getMaxID();
			std::string searchLower = toLower(searchName);

			for (int id = 1; id <= maxId && count < limit; ++id) {
				if (!g_items.typeExists(id)) {
					continue;
				}

				ItemType it = g_items.getItemType(id);
				if (it.name.empty()) {
					continue;
				}

				if (containsIgnoreCaseOptimized(it.name, searchLower)) {
					sol::table item = lua.create_table();
					item["id"] = it.id;
					item["name"] = it.name;
					results[++count] = item;
				}
			}

			return results;
		};

		// Find first item matching name exactly (case-insensitive)
		items["findIdByName"] = [](const std::string& searchName) -> sol::optional<int> {
			std::string searchLower = toLower(searchName);
			int maxId = g_items.getMaxID();

			for (int id = 1; id <= maxId; ++id) {
				if (!g_items.typeExists(id)) {
					continue;
				}

				ItemType it = g_items.getItemType(id);
				if (toLower(it.name) == searchLower) {
					return id;
				}
			}

			for (int id = 1; id <= maxId; ++id) {
				if (!g_items.typeExists(id)) {
					continue;
				}

				ItemType it = g_items.getItemType(id);
				if (containsIgnoreCaseOptimized(it.name, searchLower)) {
					return id;
				}
			}

			return sol::nullopt;
		};

		// Get item name by ID
		items["getName"] = [](int id) -> std::string {
			if (g_items.typeExists(id)) {
				return g_items.getItemType(id).name;
			}
			return "";
		};
	}

} // namespace LuaAPI
