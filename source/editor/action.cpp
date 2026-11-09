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

#include "map/tile_operations.h"
#include "app/main.h"

#include "editor/action.h"
#include "editor/dirty_list.h"
#include "app/settings.h"
#include "map/map.h"
#include "map/tile.h"
#include "game/town.h"
#include "editor/editor.h"
#include "ui/gui.h"
#include "lua/lua_api.h"
#include "brushes/managers/brush_manager.h"
#include "brushes/house/house_brush.h"
#include "brushes/house/house_exit_brush.h"

#include <algorithm>
#include <ranges>
#include <ctime>

namespace {

void updateCommittedTile(Tile* tile, ActionIdentifier type) {
	if (type == ACTION_SELECT) {
		TileOperations::updateSelectionState(tile);
		return;
	}

	TileOperations::update(tile);
	tile->modify();
}

void updateUndoTile(Tile* tile, ActionIdentifier type) {
	if (type == ACTION_SELECT) {
		TileOperations::updateSelectionState(tile);
		return;
	}

	TileOperations::update(tile);
}

} // namespace

Change::Change() :
	type(CHANGE_NONE), data(std::monostate {}) {
	////
}

Change::Change(std::unique_ptr<Tile> t) :
	type(CHANGE_TILE),
	position(t ? t->getPosition() : Position()),
	data(std::move(t)) {
}

Change::Change(std::unique_ptr<Tile> t, const Position& pos) :
	type(CHANGE_TILE),
	position(pos),
	data(std::move(t)) {
}

Change* Change::Create(House* house, const Position& where) {
	Change* c = newd Change();
	c->type = CHANGE_MOVE_HOUSE_EXIT;
	c->position = where;
	c->data = HouseExitChangeData { .houseId = house->getID(), .pos = where };
	return c;
}

Change::Change(HousePropertiesChangeData properties) :
	type(CHANGE_HOUSE_PROPERTIES), data(std::move(properties)) {}

Change::Change(HousePresenceChangeData presence) :
	type(CHANGE_HOUSE_PRESENCE), data(std::move(presence)) {}

Change* Change::Create(Waypoint* wp, const Position& where) {
	Change* c = newd Change();
	c->type = CHANGE_MOVE_WAYPOINT;
	c->position = where;
	c->data = WaypointChangeData { .name = wp->name, .pos = where };
	return c;
}

Change* Change::Create(Town* town, const Position& where) {
	Change* c = newd Change();
	c->type = CHANGE_MOVE_TOWN_TEMPLE;
	c->position = where;
	c->data = TownTempleChangeData { .townId = town->getID(), .pos = where };
	return c;
}

Change::~Change() {
	clear();
}

void Change::clear() {
	type = CHANGE_NONE;
	data = std::monostate {};
}

const Tile* Change::getTile() const {
	if (auto* t = std::get_if<std::unique_ptr<Tile>>(&data)) {
		return t->get();
	}
	return nullptr;
}

const HouseExitChangeData* Change::getHouseExitData() const {
	return std::get_if<HouseExitChangeData>(&data);
}

const WaypointChangeData* Change::getWaypointData() const {
	return std::get_if<WaypointChangeData>(&data);
}

const TownTempleChangeData* Change::getTownTempleData() const {
	return std::get_if<TownTempleChangeData>(&data);
}

uint32_t Change::memsize() const {
	uint32_t mem = sizeof(*this);
	if (auto* t = std::get_if<std::unique_ptr<Tile>>(&data)) {
		if (*t) {
			mem += (*t)->memsize();
		}
	} else if (auto* wp = std::get_if<WaypointChangeData>(&data)) {
		mem += wp->name.capacity();
	} else if (auto* house = std::get_if<HouseExitChangeData>(&data)) {
		mem += sizeof(HouseExitChangeData);
	} else if (auto* house = std::get_if<HousePropertiesChangeData>(&data)) {
		mem += house->name.capacity();
	} else if (auto* presence = std::get_if<HousePresenceChangeData>(&data); presence && presence->house) {
		mem += sizeof(House) + presence->house->name.capacity() +
			presence->house->getTiles().size() * sizeof(Position);
	} else if (auto* temple = std::get_if<TownTempleChangeData>(&data)) {
		mem += sizeof(TownTempleChangeData);
	}
	return mem;
}

namespace {
bool canSwapHousePresence(const Map& map, const HousePresenceChangeData& change) {
	const House* current = map.houses.getHouse(change.id);
	if (!change.house) {
		return current && current->getGeneration() == change.generation;
	}
	if (current || change.house->getID() != change.id || change.house->getGeneration() != change.generation) {
		return false;
	}
	// Imports or other out-of-history changes must not be overwritten by Undo.
	for (const auto& pos : change.house->getTiles()) {
		const auto tile = map.getTile(pos);
		if (!tile || tile->getHouseID() != 0) return false;
	}
	return change.house->getExit() == Position() || map.getTile(change.house->getExit());
}

void swapHousePresence(Editor& editor, HousePresenceChangeData& change) {
	auto& map = editor.map;
	if (editor.live_manager.IsLive() || !canSwapHousePresence(map, change)) return;
	House* house = change.house ? change.house.get() : map.houses.getHouse(change.id);
	const bool restoring = change.house != nullptr;
	if (restoring) {
		if (!map.houses.addHouse(std::move(change.house))) return;
		for (const auto& pos : house->getTiles()) {
			map.getTile(pos)->setHouse(house);
		}
		if (house->getExit() != Position()) {
			TileOperations::addHouseExit(map.getTile(house->getExit()), house);
		}
	} else {
		if (g_gui.GetCurrentEditor() == &editor) {
			if (g_brush_manager.house_brush && g_brush_manager.house_brush->getHouseID() == change.id) {
				g_brush_manager.house_brush->setHouse(nullptr);
			}
			if (g_brush_manager.house_exit_brush && g_brush_manager.house_exit_brush->getHouseID() == change.id) {
				g_brush_manager.house_exit_brush->setHouse(nullptr);
			}
		}
		house->clean();
		auto entry = map.houses.find(change.id);
		change.house = std::move(entry->second);
		map.houses.erase(entry);
	}
	for (const auto& pos : house->getTiles()) {
		map.getChangeTracker().markTileDirty(pos);
		g_minimap.MarkTileDirty(map, pos);
	}
	if (house->getExit() != Position()) {
		map.getChangeTracker().markTileDirty(house->getExit());
		g_minimap.MarkTileDirty(map, house->getExit());
	}
	map.doChange();
	if (g_gui.GetCurrentEditor() == &editor) {
		g_gui.RefreshPalettes();
		g_gui.SelectBrush();
	}
}

bool canSwapHouseProperties(const Map& map, const HousePropertiesChangeData& change) {
	const House* house = map.houses.getHouse(change.currentId);
	return house && house->getGeneration() == change.generation &&
		(change.targetId == change.currentId || !map.houses.getHouse(change.targetId));
}

void swapHouseProperties(Editor& editor, HousePropertiesChangeData& change) {
	Map& map = editor.map;
	if (editor.live_manager.IsLive() || !canSwapHouseProperties(map, change)) {
		return;
	}
	House* house = map.houses.getHouse(change.currentId);

	if (change.targetId != change.currentId) {
		map.convertHouseTiles(change.currentId, change.targetId);
		Tile* exitTile = house->getExit() == Position() ? nullptr : map.getTile(house->getExit());
		if (exitTile) TileOperations::removeHouseExit(exitTile, house);
		map.houses.changeId(house, change.targetId);
		if (exitTile) TileOperations::addHouseExit(exitTile, house);
		std::swap(change.currentId, change.targetId);
		map.getChangeTracker().markAllDirty();
	}
	std::swap(house->name, change.name);
	std::swap(house->rent, change.rent);
	std::swap(house->townid, change.townId);
	std::swap(house->guildhall, change.guildhall);
	map.doChange();
	if (g_gui.GetCurrentEditor() == &editor) {
		g_gui.RefreshPalettes();
	}
}
}

Action::Action(Editor& editor, ActionIdentifier ident) :
	commited(false),
	editor(editor),
	type(ident) {
}

Action::~Action() {
	changes.clear();
}

bool Action::canApply() const {
	if (type == ACTION_GENERATE_TERRAIN) {
		if (editor.live_manager.IsLive() || editor.selection.isBusy() || editor.selection.isDeferred() ||
			LuaAPI::isTransactionActive()) return false;
		const auto plain = [](const Tile* tile) {
			return !tile || (!tile->house_id && !tile->spawn && !tile->creature &&
				!tile->mapflags && !tile->extra);
		};
		for (const auto& change : changes) {
			if (change->type != CHANGE_TILE) return false;
			const auto* location = editor.map.getTileL(change->position);
			if (!location || location->getSpawnCount() || location->getWaypointCount() ||
				location->getTownCount() || (location->getHouseExits() && !location->getHouseExits()->empty()) ||
				!plain(location->get()) || !plain(change->getTile())) return false;
		}
		return true;
	}
	for (const auto& change : changes) {
		if (const auto* properties = std::get_if<HousePropertiesChangeData>(&change->data)) {
			if (editor.live_manager.IsLive() || !canSwapHouseProperties(editor.map, *properties)) return false;
		} else if (const auto* presence = std::get_if<HousePresenceChangeData>(&change->data)) {
			if (editor.live_manager.IsLive() || !canSwapHousePresence(editor.map, *presence)) return false;
		}
	}
	return true;
}

size_t Action::approx_memsize() const {
	if (type == ACTION_GENERATE_TERRAIN) return preparedMemory;
	uint32_t mem = sizeof(*this);
	mem += changes.size() * (sizeof(Change) + sizeof(Tile) + sizeof(Item) + 6 /* approx overhead*/);
	return mem;
}

size_t Action::memsize() const {
	if (type == ACTION_GENERATE_TERRAIN) return preparedMemory;
	uint32_t mem = sizeof(*this);
	mem += sizeof(Change*) * 3 * changes.size();
	for (const auto& c : changes) {
		mem += c->memsize();
	}
	return mem;
}

bool Action::prepareExactSelection(bool* membershipChanged) {
	size_t additions = 0;
	if (membershipChanged) *membershipChanged = false;
	for (const auto& change : changes) {
		const bool outgoingMember = editor.selection.containsExact(editor.map.getTile(change->position));
		const bool incomingMember = change->membershipCaptured ? change->restoreUnflaggedMembership : outgoingMember;
		if (membershipChanged && (incomingMember && change->getTile()) != outgoingMember) *membershipChanged = true;
		if (incomingMember && change->getTile() && !outgoingMember) ++additions;
	}
	return editor.selection.reserveExactReplacements(additions,preparedSelectionBytes);
}

void Action::swapExactTiles() noexcept {
	for (const auto& change : changes) {
		auto& tile = std::get<std::unique_ptr<Tile>>(change->data);
		auto* outgoing = editor.map.getTile(change->position);
		const bool outgoingMember = editor.selection.containsExact(outgoing);
		const bool incomingMember = change->membershipCaptured ? change->restoreUnflaggedMembership : outgoingMember;
		editor.selection.replaceExact(outgoing, tile.get(), incomingMember);
		change->membershipCaptured = true;
		change->restoreUnflaggedMembership = outgoingMember;
		editor.map.swapReservedTile(change->position, tile);
	}
}

void Action::commit(DirtyList* dirty_list) {
	if (type == ACTION_GENERATE_TERRAIN) {
		swapExactTiles();
		commited = true;
		return;
	}
	editor.selection.start(Selection::INTERNAL);
	ChangeList::const_iterator it = changes.begin();
	while (it != changes.end()) {
		Change* c = it->get();
		switch (c->type) {
			case CHANGE_TILE: {
				auto& uptr = std::get<std::unique_ptr<Tile>>(c->data);
				Position pos = uptr ? uptr->getPosition() : c->position;

				if (editor.live_manager.IsClient()) {
					MapNode* nd = editor.map.getLeaf(pos.x, pos.y);
					if (!nd || !nd->isVisible(pos.z > GROUND_LAYER)) {
						// Delete all changes that affect tiles outside our view
						c->clear();
						++it;
						continue;
					}
				}

				if (!c->membershipCaptured) {
						const auto* original = editor.map.getTile(pos);
						c->restoreUnflaggedMembership = original && !original->isSelected() && editor.selection.containsExact(original);
						c->membershipCaptured = true;
					}
					std::unique_ptr<Tile> displacedTile = editor.map.swapTile(pos, std::move(uptr));
				Tile* insertedTile = editor.map.getTile(pos);
				Tile* displaced = displacedTile.get();

				// Update other nodes in the network
				if (editor.live_manager.IsServer() && dirty_list) {
					dirty_list->AddPosition(pos.x, pos.y, pos.z);
				}

				if (insertedTile && displaced) {
					if (insertedTile->getHouseID() != displaced->getHouseID()) {
						// oooooomggzzz we need to add it to the appropriate house!
						House* house = editor.map.houses.getHouse(displaced->getHouseID());
						if (house) {
							house->removeTile(displaced);
						}

						house = editor.map.houses.getHouse(insertedTile->getHouseID());
						if (house) {
							house->addTile(insertedTile);
						}
					}
					if (displaced->spawn) {
						if (insertedTile->spawn) {
							if (*displaced->spawn != *insertedTile->spawn) {
								editor.map.removeSpawn(displaced);
								editor.map.addSpawn(insertedTile);
							}
						} else {
							// Spawn has been removed
							editor.map.removeSpawn(displaced);
						}
					} else if (insertedTile->spawn) {
						editor.map.addSpawn(insertedTile);
					}

					if (editor.map.supportsZones() && !std::ranges::equal(insertedTile->getZoneIds(), displaced->getZoneIds())) {
						editor.map.updateTileZones(pos, displaced->getZoneIds(), insertedTile->getZoneIds());
					}

					editor.selection.removeInternal(displaced);
					updateCommittedTile(insertedTile, type);
					if (insertedTile->isSelected()) {
						editor.selection.addInternal(insertedTile);
					}
				} else if (insertedTile) {
					updateCommittedTile(insertedTile, type);
					if (insertedTile->isSelected()) {
						editor.selection.addInternal(insertedTile);
					}
					if (insertedTile->getHouseID() != 0) {
						// oooooomggzzz we need to add it to the appropriate house!
						House* house = editor.map.houses.getHouse(insertedTile->getHouseID());
						if (house) {
							house->addTile(insertedTile);
						}
					}

					if (insertedTile->spawn) {
						editor.map.addSpawn(insertedTile);
					}

					if (editor.map.supportsZones()) {
						for (uint16_t zid : insertedTile->getZoneIds()) {
							editor.map.addTileToZone(zid, pos);
						}
					}
				} else if (displaced) {
					editor.selection.removeInternal(displaced);
					if (displaced->getHouseID() != 0) {
						House* house = editor.map.houses.getHouse(displaced->getHouseID());
						if (house) {
							house->removeTile(displaced);
						}
					}
					if (displaced->spawn) {
						editor.map.removeSpawn(displaced);
					}

					if (editor.map.supportsZones()) {
						for (uint16_t zid : displaced->getZoneIds()) {
							editor.map.removeTileFromZone(zid, pos);
						}
					}
				}

				uptr = std::move(displacedTile);

				// Update client dirty list
				if (editor.live_manager.IsClient() && dirty_list && type != ACTION_REMOTE) {
					// Local action, assemble changes
					dirty_list->AddChange(c);
				}
				if (type != ACTION_SELECT) {
					editor.map.getChangeTracker().markTileDirty(pos);
					g_minimap.MarkTileDirty(editor.map, pos);
				}
				break;
			}

			case CHANGE_MOVE_HOUSE_EXIT: {
				auto& p = std::get<HouseExitChangeData>(c->data);
				House* whathouse = editor.map.houses.getHouse(p.houseId);

				if (whathouse) {
					Position oldpos = whathouse->getExit();
					Position newpos = p.pos;
					whathouse->setExit(newpos);
					p.pos = oldpos;

					if (oldpos != Position() && oldpos.isValid()) {
						editor.map.getChangeTracker().markTileDirty(oldpos);
						g_minimap.MarkTileDirty(editor.map, oldpos);
					}
					if (newpos != Position() && newpos.isValid()) {
						editor.map.getChangeTracker().markTileDirty(newpos);
						g_minimap.MarkTileDirty(editor.map, newpos);
					}
				}
				break;
			}

			case CHANGE_HOUSE_PROPERTIES:
				swapHouseProperties(editor, std::get<HousePropertiesChangeData>(c->data));
				break;

			case CHANGE_HOUSE_PRESENCE:
				swapHousePresence(editor, std::get<HousePresenceChangeData>(c->data));
				break;

			case CHANGE_MOVE_WAYPOINT: {
				auto& p = std::get<WaypointChangeData>(c->data);
				Waypoint* wp = editor.map.waypoints.getWaypoint(p.name);

				if (wp) {
					// Change the tiles
					TileLocation* oldtile = editor.map.getTileL(wp->pos);
					TileLocation* newtile = editor.map.getTileL(p.pos);

					// Only need to remove from old if it actually exists
					if (wp->pos != Position()) {
						if (oldtile && oldtile->getWaypointCount() > 0) {
							oldtile->decreaseWaypointCount();
						}
					}

					if (p.pos != Position() && newtile) {
						newtile->increaseWaypointCount();
					}

					// Update shit
					Position oldpos = wp->pos;
					Position newpos = p.pos;
					wp->pos = newpos;
					p.pos = oldpos;

					if (oldpos != Position() && oldpos.isValid()) {
						editor.map.getChangeTracker().markTileDirty(oldpos);
						g_minimap.MarkTileDirty(editor.map, oldpos);
					}
					if (newpos != Position() && newpos.isValid()) {
						editor.map.getChangeTracker().markTileDirty(newpos);
						g_minimap.MarkTileDirty(editor.map, newpos);
					}
				}
				break;
			}

			case CHANGE_MOVE_TOWN_TEMPLE: {
				auto& p = std::get<TownTempleChangeData>(c->data);
				Town* town = editor.map.towns.getTown(p.townId);

				if (town) {
					TileLocation* oldtile = editor.map.getTileL(town->getTemplePosition());
					TileLocation* newtile = editor.map.getTileL(p.pos);

					if (town->getTemplePosition() != Position()) {
						if (oldtile && oldtile->getTownCount() > 0) {
							oldtile->decreaseTownCount();
						}
					}

					if (p.pos != Position() && newtile) {
						newtile->increaseTownCount();
					}

					Position oldpos = town->getTemplePosition();
					Position newpos = p.pos;
					town->setTemplePosition(newpos);
					p.pos = oldpos;

					if (oldpos != Position() && oldpos.isValid()) {
						editor.map.getChangeTracker().markTileDirty(oldpos);
						g_minimap.MarkTileDirty(editor.map, oldpos);
					}
					if (newpos != Position() && newpos.isValid()) {
						editor.map.getChangeTracker().markTileDirty(newpos);
						g_minimap.MarkTileDirty(editor.map, newpos);
					}
				}
				break;
			}

			default:
				break;
		}
		++it;
	}
	editor.selection.finish(Selection::INTERNAL);
	commited = true;
}

void Action::undo(DirtyList* dirty_list) {
	if (type == ACTION_GENERATE_TERRAIN) {
		swapExactTiles();
		commited = false;
		return;
	}
	if (changes.empty()) {
		return;
	}

	editor.selection.start(Selection::INTERNAL);
	ChangeList::reverse_iterator it = changes.rbegin();

	while (it != changes.rend()) {
		Change* c = it->get();
		switch (c->type) {
			case CHANGE_TILE: {
				auto& uptr = std::get<std::unique_ptr<Tile>>(c->data);
				Position pos = uptr ? uptr->getPosition() : c->position;

				if (editor.live_manager.IsClient()) {
					MapNode* nd = editor.map.getLeaf(pos.x, pos.y);
					if (!nd || !nd->isVisible(pos.z > GROUND_LAYER)) {
						// Delete all changes that affect tiles outside our view
						c->clear();
						++it;
						continue;
					}
				}

				if (uptr) {
					std::unique_ptr<Tile> old_uptr = std::move(uptr);
					Tile* oldtile = old_uptr.get();
					std::unique_ptr<Tile> newtile_uptr = editor.map.swapTile(pos, std::move(old_uptr));
					Tile* newtile = newtile_uptr.get();

					// Update server side change list (for broadcast)
					if (editor.live_manager.IsServer() && dirty_list) {
						dirty_list->AddPosition(pos.x, pos.y, pos.z);
					}

					if (newtile) {
						editor.selection.removeInternal(newtile);
					}

					if (oldtile && newtile) {
						if (newtile->getHouseID() != oldtile->getHouseID()) {
							// oooooomggzzz we need to remove it from the appropriate house!
							House* house = editor.map.houses.getHouse(newtile->getHouseID());
							if (house) {
								house->removeTile(newtile);
							} else {
								// Set tile house to 0, house has been removed
								newtile->setHouse(nullptr);
							}

							house = editor.map.houses.getHouse(oldtile->getHouseID());
							if (house) {
								house->addTile(oldtile);
							}
						}

						if (oldtile->spawn) {
							if (newtile->spawn) {
								if (*oldtile->spawn != *newtile->spawn) {
									editor.map.removeSpawn(newtile);
									editor.map.addSpawn(oldtile);
								}
							} else {
								editor.map.addSpawn(oldtile);
							}
						} else if (newtile->spawn) {
							editor.map.removeSpawn(newtile);
						}

						if (editor.map.supportsZones() && !std::ranges::equal(newtile->getZoneIds(), oldtile->getZoneIds())) {
							editor.map.updateTileZones(pos, newtile->getZoneIds(), oldtile->getZoneIds());
						}

						updateUndoTile(oldtile, type);
						if (oldtile->isSelected() || c->restoreUnflaggedMembership) {
							editor.selection.addInternal(oldtile);
						}

						uptr = std::move(newtile_uptr);
					} else if (oldtile) {
						if (oldtile->getHouseID() != 0) {
							House* house = editor.map.houses.getHouse(oldtile->getHouseID());
							if (house) {
								house->addTile(oldtile);
							}
						}
						if (oldtile->spawn) {
							editor.map.addSpawn(oldtile);
						}
						if (editor.map.supportsZones()) {
							for (uint16_t zid : oldtile->getZoneIds()) {
								editor.map.addTileToZone(zid, pos);
							}
						}
						updateUndoTile(oldtile, type);
						if (oldtile->isSelected() || c->restoreUnflaggedMembership) {
							editor.selection.addInternal(oldtile);
						}
						uptr = std::move(newtile_uptr);
					} else {
						if (newtile && editor.map.supportsZones()) {
							for (uint16_t zid : newtile->getZoneIds()) {
								editor.map.removeTileFromZone(zid, pos);
							}
						}
						uptr = std::move(newtile_uptr);
					}
				} else {
					std::unique_ptr<Tile> removedTile = editor.map.swapTile(pos, std::unique_ptr<Tile>());
					Tile* removed = removedTile.get();

					// Update server side change list (for broadcast)
					if (editor.live_manager.IsServer() && dirty_list) {
						dirty_list->AddPosition(pos.x, pos.y, pos.z);
					}

					if (removed) {
						editor.selection.removeInternal(removed);
						if (removed->getHouseID() != 0) {
							House* house = editor.map.houses.getHouse(removed->getHouseID());
							if (house) {
								house->removeTile(removed);
							}
						}
						if (removed->spawn) {
							editor.map.removeSpawn(removed);
						}
						for (uint16_t zid : removed->getZoneIds()) {
							editor.map.removeTileFromZone(zid, pos);
						}
					}
					uptr = std::move(removedTile);
				}

				// Update client dirty list
				if (editor.live_manager.IsClient() && dirty_list && type != ACTION_REMOTE) {
					// Local action, assemble changes
					dirty_list->AddChange(c);
				}
				if (type != ACTION_SELECT) {
					editor.map.getChangeTracker().markTileDirty(pos);
					g_minimap.MarkTileDirty(editor.map, pos);
				}
				break;
			}

			case CHANGE_MOVE_HOUSE_EXIT: {
				auto& p = std::get<HouseExitChangeData>(c->data);
				House* whathouse = editor.map.houses.getHouse(p.houseId);
				if (whathouse) {
					Position oldpos = whathouse->getExit();
					Position newpos = p.pos;
					whathouse->setExit(newpos);
					p.pos = oldpos;

					if (oldpos != Position() && oldpos.isValid()) {
						editor.map.getChangeTracker().markTileDirty(oldpos);
						g_minimap.MarkTileDirty(editor.map, oldpos);
					}
					if (newpos != Position() && newpos.isValid()) {
						editor.map.getChangeTracker().markTileDirty(newpos);
						g_minimap.MarkTileDirty(editor.map, newpos);
					}
				}
				break;
			}

			case CHANGE_HOUSE_PROPERTIES:
				swapHouseProperties(editor, std::get<HousePropertiesChangeData>(c->data));
				break;

			case CHANGE_HOUSE_PRESENCE:
				swapHousePresence(editor, std::get<HousePresenceChangeData>(c->data));
				break;

			case CHANGE_MOVE_WAYPOINT: {
				auto& p = std::get<WaypointChangeData>(c->data);
				Waypoint* wp = editor.map.waypoints.getWaypoint(p.name);

				if (wp) {
					// Change the tiles
					TileLocation* oldtile = editor.map.getTileL(wp->pos);
					TileLocation* newtile = editor.map.getTileL(p.pos);

					// Only need to remove from old if it actually exists
					if (wp->pos != Position()) {
						if (oldtile && oldtile->getWaypointCount() > 0) {
							oldtile->decreaseWaypointCount();
						}
					}

					if (p.pos != Position() && newtile) {
						newtile->increaseWaypointCount();
					}

					// Update shit
					Position oldpos = wp->pos;
					Position newpos = p.pos;
					wp->pos = newpos;
					p.pos = oldpos;

					if (oldpos != Position() && oldpos.isValid()) {
						editor.map.getChangeTracker().markTileDirty(oldpos);
						g_minimap.MarkTileDirty(editor.map, oldpos);
					}
					if (newpos != Position() && newpos.isValid()) {
						editor.map.getChangeTracker().markTileDirty(newpos);
						g_minimap.MarkTileDirty(editor.map, newpos);
					}
				}
				break;
			}

			case CHANGE_MOVE_TOWN_TEMPLE: {
				auto& p = std::get<TownTempleChangeData>(c->data);
				Town* town = editor.map.towns.getTown(p.townId);

				if (town) {
					TileLocation* oldtile = editor.map.getTileL(town->getTemplePosition());
					TileLocation* newtile = editor.map.getTileL(p.pos);

					if (town->getTemplePosition() != Position()) {
						if (oldtile && oldtile->getTownCount() > 0) {
							oldtile->decreaseTownCount();
						}
					}

					if (p.pos != Position() && newtile) {
						newtile->increaseTownCount();
					}

					Position oldpos = town->getTemplePosition();
					Position newpos = p.pos;
					town->setTemplePosition(newpos);
					p.pos = oldpos;

					if (oldpos != Position() && oldpos.isValid()) {
						editor.map.getChangeTracker().markTileDirty(oldpos);
						g_minimap.MarkTileDirty(editor.map, oldpos);
					}
					if (newpos != Position() && newpos.isValid()) {
						editor.map.getChangeTracker().markTileDirty(newpos);
						g_minimap.MarkTileDirty(editor.map, newpos);
					}
				}
				break;
			}

			default:
				break;
		}
		++it;
	}
	editor.selection.finish(Selection::INTERNAL);
	commited = false;
}

BatchAction::BatchAction(Editor& editor, ActionIdentifier ident) :
	editor(editor), timestamp(0), memory_size(0), type(ident), label("") {
	////
}

BatchAction::~BatchAction() {
	// batch is vector<unique_ptr>, destruction handled automatically
	batch.clear();
}

bool BatchAction::canApply() const {
	if (type != ACTION_HOUSE_PROPERTIES && type != ACTION_HOUSE_PRESENCE && type != ACTION_GENERATE_TERRAIN) return true;
	return std::ranges::all_of(batch, [](const auto& action) { return action->canApply(); });
}

size_t BatchAction::memsize(bool recalc) const {
	// Expensive operation, only evaluate once (won't change anyways)
	if (!recalc && memory_size > 0) {
		return memory_size;
	}

	uint32_t mem = sizeof(*this);
	mem += sizeof(Action*) * 3 * batch.size();

	for (const auto& action : batch) {
#ifdef __USE_EXACT_MEMSIZE__
		mem += action->memsize();
#else
		// Less exact but MUCH faster
		mem += action->approx_memsize();
#endif
	}

	const_cast<BatchAction*>(this)->memory_size = mem;
	return mem;
}

void BatchAction::addAction(std::unique_ptr<Action> action) {
	// A cave pair retains an empty first floor as its own generator history step.
	if (action->size() == 0 && type != ACTION_GENERATE_TERRAIN) {
		return;
	}

	ASSERT(action->getType() == type);

	if (editor.live_manager.IsClient()) {
		return;
	}

	// Add it!
	batch.push_back(std::move(action));
	timestamp = time(nullptr);
}

void BatchAction::addAndCommitAction(std::unique_ptr<Action> action) {
	// If empty, do nothing.
	if (action->size() == 0) {
		return;
	}

	if (editor.live_manager.IsClient()) {
		return;
	}

	// Add it!
	action->commit(nullptr);
	batch.push_back(std::move(action));
	timestamp = time(nullptr);
}

void BatchAction::commit() {
	for (const auto& action : batch) {
		if (!action->isCommited()) {
			action->commit(nullptr);
		}
	}
}

void BatchAction::undo() {
	for (auto& action : std::ranges::reverse_view(batch)) {
		action->undo(nullptr);
	}
}

void BatchAction::redo() {
	for (auto& action : batch) {
		action->redo(nullptr);
	}
}

void BatchAction::merge(BatchAction* other) {
	batch.insert(batch.end(), std::make_move_iterator(other->batch.begin()), std::make_move_iterator(other->batch.end()));
	other->batch.clear();
}
