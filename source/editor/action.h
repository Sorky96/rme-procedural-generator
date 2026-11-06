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

#ifndef RME_ACTION_H_
#define RME_ACTION_H_

#include "map/position.h"
#include "map/tile.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

class Editor;
class Tile;
class House;
class Waypoint;
class Town;
class Change;
class Action;
class BatchAction;
class ActionQueue;

enum ChangeType {
	CHANGE_NONE,
	CHANGE_TILE,
	CHANGE_MOVE_HOUSE_EXIT,
	CHANGE_MOVE_WAYPOINT,
	CHANGE_MOVE_TOWN_TEMPLE,
	CHANGE_HOUSE_PROPERTIES,
	CHANGE_HOUSE_PRESENCE,
};

struct HouseExitChangeData {
	uint32_t houseId;
	Position pos;
};

struct HousePropertiesChangeData {
	uint32_t currentId;
	uint64_t generation;
	uint32_t targetId;
	std::string name;
	int rent;
	uint32_t townId;
	bool guildhall;
};

struct HousePresenceChangeData {
	uint32_t id;
	uint64_t generation;
	// Ownership moves between the map and history, preserving the same House
	// identity for earlier property/exit actions when a deletion is undone.
	std::unique_ptr<House> house;
};

struct WaypointChangeData {
	std::string name;
	Position pos;
};

struct TownTempleChangeData {
	uint32_t townId;
	Position pos;
};

class Change {
private:
	using Data = std::variant<std::monostate, std::unique_ptr<Tile>, HouseExitChangeData, HousePropertiesChangeData, HousePresenceChangeData, WaypointChangeData, TownTempleChangeData>;
	ChangeType type;
	Position position;
	Data data;
	// Generic actions restore unflagged membership; exact terrain swaps exchange actual membership.
	bool membershipCaptured = false;
	bool restoreUnflaggedMembership = false;

	Change();

public:
	explicit Change(std::unique_ptr<Tile> tile);
	explicit Change(HousePropertiesChangeData properties);
	explicit Change(HousePresenceChangeData presence);
	Change(std::unique_ptr<Tile> tile, const Position& pos);
	static Change* Create(House* house, const Position& where);
	static Change* Create(Waypoint* wp, const Position& where);
	static Change* Create(Town* town, const Position& where);
	~Change();
	void clear();

	ChangeType getType() const {
		return type;
	}
	const Tile* getTile() const;
	const HouseExitChangeData* getHouseExitData() const;
	const WaypointChangeData* getWaypointData() const;
	const TownTempleChangeData* getTownTempleData() const;

	// Get memory footprint
	uint32_t memsize() const;

	friend class Action;
};

using ChangeList = std::vector<std::unique_ptr<Change>>;

class DirtyList;

enum ActionIdentifier {
	ACTION_MOVE,
	ACTION_REMOTE,
	ACTION_SELECT,
	ACTION_DELETE_TILES,
	ACTION_CUT_TILES,
	ACTION_PASTE_TILES,
	ACTION_RANDOMIZE,
	ACTION_BORDERIZE,
	ACTION_DRAW,
	ACTION_SWITCHDOOR,
	ACTION_ROTATE_ITEM,
	ACTION_ROTATE,
	ACTION_REPLACE_ITEMS,
	ACTION_CHANGE_PROPERTIES,
	ACTION_LUA_SCRIPT,
	ACTION_HOUSE_PROPERTIES,
	ACTION_HOUSE_PRESENCE,
	ACTION_GENERATE_TERRAIN,
};

class Action {
public:
	virtual ~Action();

	void addChange(std::unique_ptr<Change> t) {
		changes.push_back(std::move(t));
	}

	// Get memory footprint
	size_t approx_memsize() const;
	size_t memsize() const;
	size_t size() const {
		return changes.size();
	}
	ActionIdentifier getType() const {
		return type;
	}

	void commit(DirtyList* dirty_list);
	bool isCommited() const {
		return commited;
	}
	void undo(DirtyList* dirty_list);
	bool canApply() const;
	void redo(DirtyList* dirty_list) {
		commit(dirty_list);
	}

protected:
	Action(Editor& editor, ActionIdentifier ident);

	bool commited;
	ChangeList changes;
	Editor& editor;
	ActionIdentifier type;
	size_t preparedMemory = 0; // Checked terrain estimate, stable in both swap directions.
	size_t preparedSelectionBytes = 0;
	bool prepareExactSelection(bool* membershipChanged = nullptr);
	void swapExactTiles() noexcept;

	friend class ActionQueue;
	friend class DrawOperations;
};

using ActionVector = std::vector<std::unique_ptr<Action>>;

class BatchAction {
public:
	virtual ~BatchAction();

	void resetTimer() {
		timestamp = 0;
	}

	// Get memory footprint
	size_t memsize(bool resize = false) const;
	size_t size() const {
		return batch.size();
	}
	ActionIdentifier getType() const {
		return type;
	}

	virtual void addAction(std::unique_ptr<Action> action);
	virtual void addAndCommitAction(std::unique_ptr<Action> action);

	void setLabel(const std::string& name) {
		label = name;
	}
	const std::string& getLabel() const {
		return label;
	}
	bool canApply() const;

protected:
	BatchAction(Editor& editor, ActionIdentifier ident);

	virtual void commit();
	virtual void undo();
	virtual void redo();

	void merge(BatchAction* other);

	Editor& editor;
	int timestamp;
	uint32_t memory_size;
	ActionIdentifier type;
	std::string label;
	ActionVector batch;

	friend class ActionQueue;
	friend class DrawOperations;
};

#endif
