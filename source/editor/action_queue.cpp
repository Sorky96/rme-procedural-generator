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

#include "editor/action_queue.h"
#include "lua/lua_api.h"
#include "ui/managers/minimap_manager.h"
#include <limits>
#include "lua/lua_script_manager.h"
#include "editor/action.h"
#include "editor/editor.h"
#include "app/settings.h"
#include "map/map.h"
#include "boost/range/adaptor/reversed.hpp"

#include "game/creature.h"
#include "game/spawn.h"

ActionQueue::ActionQueue(Editor& editor) :
	current(0), memory_size(0), editor(editor) {
	////
}

ActionQueue::~ActionQueue() {
	actions.clear();
}

std::unique_ptr<Action> ActionQueue::createAction(ActionIdentifier ident) {
	return std::unique_ptr<Action>(new Action(editor, ident));
}

std::unique_ptr<Action> ActionQueue::createAction(BatchAction* batch) {
	return std::unique_ptr<Action>(new Action(editor, batch->getType()));
}

std::unique_ptr<BatchAction> ActionQueue::createBatch(ActionIdentifier ident) {
	return std::unique_ptr<BatchAction>(new BatchAction(editor, ident));
}

std::string ActionQueue::getActionName(size_t index) const {
	if (index >= actions.size()) {
		return "Unknown";
	}
	const BatchAction* batch = actions[index].get();
	if (batch && !batch->getLabel().empty()) {
		return batch->getLabel();
	}
	switch (batch->getType()) {
		case ACTION_MOVE: return "Move";
		case ACTION_REMOTE: return "Remote";
		case ACTION_SELECT: return "Select";
		case ACTION_DELETE_TILES: return "Delete";
		case ACTION_CUT_TILES: return "Cut";
		case ACTION_PASTE_TILES: return "Paste";
		case ACTION_RANDOMIZE: return "Randomize";
		case ACTION_BORDERIZE: return "Borderize";
		case ACTION_DRAW: return "Draw";
		case ACTION_SWITCHDOOR: return "Switch Door";
		case ACTION_ROTATE_ITEM: return "Rotate Item";
		case ACTION_ROTATE: return "Rotate";
		case ACTION_REPLACE_ITEMS: return "Replace Items";
		case ACTION_CHANGE_PROPERTIES: return "Change Properties";
		case ACTION_HOUSE_PROPERTIES: return "Change House Properties";
		case ACTION_HOUSE_PRESENCE: return "Add/Remove House";
		case ACTION_LUA_SCRIPT: return "Lua Script";
		case ACTION_GENERATE_TERRAIN: return "Generate Terrain";
		default: return "Unknown";
	}
}

void ActionQueue::resetTimer() {
	if (!actions.empty()) {
		actions.back()->resetTimer();
	}
}

void ActionQueue::addBatch(std::unique_ptr<BatchAction> batch, int stacking_delay) {
	if (applyingTerrain || batch->getType() == ACTION_GENERATE_TERRAIN) return;
	ASSERT(batch);
	ASSERT(current <= actions.size());

	if (batch->size() == 0) {
		return;
	}

	// Commit any uncommited actions...
	batch->commit();

	// Update title and notify state change if map changed
	if (editor.map.doChange()) {
		editor.notifyStateChange();
	}

	if (batch->getType() == ACTION_REMOTE) {
		return;
	}

	// Plan the existing Redo discard and eviction policy without changing it.
	// Storage allocation must succeed before any old history is destroyed or
	// its memory accounting is changed.
	size_t retainedMemory = memory_size;
	for (size_t index = current; index < actions.size(); ++index) {
		retainedMemory -= actions[index]->memsize();
	}
	size_t firstRetained = 0;
	const auto memoryLimit = size_t(1024) * 1024 * g_settings.getInteger(Config::UNDO_MEM_SIZE);
	while (retainedMemory > memoryLimit && firstRetained < current) {
		retainedMemory -= actions[firstRetained++]->memsize();
	}
	if (current - firstRetained > size_t(g_settings.getInteger(Config::UNDO_SIZE)) && firstRetained < current) {
		retainedMemory -= actions[firstRetained++]->memsize();
	}

	BatchAction* predecessor = firstRetained < current ? actions[current - 1].get() : nullptr;
	const auto publishedAt = time(nullptr);
	const bool merging = predecessor && predecessor->getType() == batch->getType() &&
		g_settings.getInteger(Config::GROUP_ACTIONS) && publishedAt - stacking_delay < predecessor->timestamp;
	const auto incomingMemory = batch->memsize();
	if (merging) {
		predecessor->batch.reserve(predecessor->batch.size() + batch->batch.size());
	} else {
		// A null slot is unpublished and is filled after the no-throw erasures.
		actions.emplace_back();
	}

	const auto redoEnd = merging ? actions.end() : actions.end() - 1;
	actions.erase(actions.begin() + current, redoEnd);
	for (size_t index = 0; index < firstRetained; ++index) {
		actions.pop_front();
	}
	if (merging) {
		retainedMemory -= predecessor->memsize();
		predecessor->merge(batch.get()); // Capacity is already available; unique_ptr moves do not throw.
		predecessor->timestamp = publishedAt;
		memory_size = retainedMemory + predecessor->memsize(true);
	} else {
		batch->timestamp = publishedAt;
		actions.back() = std::move(batch);
		memory_size = retainedMemory + incomingMemory;
	}
	current = actions.size();
	g_luaScripts.emit("actionChange");
}

void ActionQueue::addAction(std::unique_ptr<Action> action, int stacking_delay) {
	if (applyingTerrain || action->getType() == ACTION_GENERATE_TERRAIN) return;
	if (!action->canApply()) return;
	std::unique_ptr<BatchAction> batch = createBatch(action->getType());
	batch->addAndCommitAction(std::move(action)); // BatchAction takes ownership
	if (batch->size() == 0) {
		return;
	}

	addBatch(std::move(batch), stacking_delay);
}

namespace {
struct TerrainTransition {
	bool& active;
	bool armed = true;
	explicit TerrainTransition(bool& value) noexcept : active(value) { active = true; }
	void release() noexcept { active = false; armed = false; }
	~TerrainTransition() { if (armed) active = false; }
};

void emitTerrainHistoryChange(bool selectionMembershipChanged = false) noexcept {
	if (selectionMembershipChanged) {
		try { g_luaScripts.emit("selectionChange"); } catch (...) {}
	}
	try { g_luaScripts.emit("actionChange"); } catch (...) {}
}
}

// Failures of post-publication observers cannot turn an accepted history change into a failed
// operation. Dirty state is set before tracker listeners run; the minimap slot is preallocated.
void ActionQueue::notifyTerrainTransition(bool selectionCountChanged) noexcept {
	editor.map.doChange();
	try { g_minimap.InvalidateAll(editor.map); } catch (...) {}
	try { editor.map.getChangeTracker().markAllDirty(); } catch (...) {}
	if (selectionCountChanged) {
		try { editor.selection.updateSelectionCount(); } catch (...) {}
	}
	try { editor.notifyStateChange(); } catch (...) {}
}

bool ActionQueue::commitTerrainBatch(std::unique_ptr<BatchAction> batch,
	std::span<const Position> positions, std::stop_token stop) {
	if (applyingTerrain || !batch || batch->getType() != ACTION_GENERATE_TERRAIN ||
		batch->size() != 1 || positions.empty() || editor.live_manager.IsLive() ||
		LuaAPI::isTransactionActive() || editor.selection.isBusy() || editor.selection.isDeferred()) return false;
	const auto incoming = batch->memsize();
	const auto configuredMemory = g_settings.getInteger(Config::UNDO_MEM_SIZE);
	const auto configuredCount = g_settings.getInteger(Config::UNDO_SIZE);
	if (configuredMemory <= 0 || configuredCount <= 0) return false;
	const uint64_t limit = uint64_t(configuredMemory) * 1024 * 1024;
	if (incoming > limit) return false;
	if (terrainPair) {
		if (terrainPairCount >= terrainPairLimit || current != terrainPairStart + terrainPairCount) return false;
		// Admission only: redo and the oldest steps before the pair are dropped by finishTerrainPair(true), so a refused
		// pair leaves the earlier history untouched. The pair itself is never trimmed.
		size_t retained = memory_size;
		for (size_t index = current; index < actions.size(); ++index) {
			const auto bytes = actions[index]->memsize();
			if (bytes > retained) return false;
			retained -= bytes;
		}
		size_t first = 0;
		while (first < terrainPairStart && (retained > limit - incoming || current - first >= size_t(configuredCount))) {
			const auto bytes = actions[first++]->memsize();
			if (bytes > retained) return false;
			retained -= bytes;
		}
		if (retained > limit - incoming || current - first >= size_t(configuredCount)) return false;
	}
	TerrainTransition transition(applyingTerrain);
	// Roll back newly reserved map structure if any subsequent allocation/refusal fails.
	BaseMap::TileReservation locations(editor.map, positions);
	g_minimap.PrepareInvalidationSlot(editor.map);
	if (stop.stop_requested() || !batch->canApply()) return false;
	if (!batch->batch.front()->prepareExactSelection()) return false;
	if (terrainPair) {
		// Retain all prior history, including redo, until every floor has succeeded.
		// Insertion is the last allocation; rollback needs only noexcept swaps and erasure.
		auto slot = actions.emplace(actions.begin() + current, nullptr);
		batch->commit();
		++terrainPairCount;
		*slot = std::move(batch);
		memory_size += incoming;
		++current;
		locations.commit();
		return true;
	}
	size_t retained = memory_size;
	for (size_t index = current; index < actions.size(); ++index) {
		const auto bytes = actions[index]->memsize();
		if (bytes > retained) return false;
		retained -= bytes;
	}
	size_t first = 0;
	while (first < current && (retained > limit - incoming ||
		current - first >= size_t(configuredCount))) {
		const auto bytes = actions[first++]->memsize();
		if (bytes > retained) return false;
		retained -= bytes;
	}
	if (retained > limit - incoming || incoming > std::numeric_limits<size_t>::max() - retained) return false;
	// No observers can see this unpublished slot. This is the last allocation before apply.
	actions.emplace_back();
	batch->commit(); // Terrain Action performs only noexcept prepared tile/selection swaps.
	actions.erase(actions.begin() + current, actions.end() - 1);
	for (size_t index = 0; index < first; ++index) actions.pop_front();
	batch->timestamp = time(nullptr);
	actions.back() = std::move(batch);
	memory_size = retained + incoming;
	current = actions.size();
	locations.commit();
	notifyTerrainTransition();
	transition.release();
	// Script listeners may start a subsequent undoable edit only after the guard is released.
	emitTerrainHistoryChange();
	return true;
}

bool ActionQueue::beginTerrainPair(size_t steps) noexcept {
	if (applyingTerrain || terrainPair || !steps) return false;
	terrainPairLimit = steps;
	terrainPair = true;
	terrainPairStart = current;
	terrainPairCount = 0;
	return true;
}

void ActionQueue::finishTerrainPair(bool success) noexcept {
	ASSERT(terrainPair && current == terrainPairStart + terrainPairCount);
	TerrainTransition transition(applyingTerrain);
	if (!success) {
		while (terrainPairCount) {
			ASSERT(current && actions[current - 1]->getType() == ACTION_GENERATE_TERRAIN);
			auto& batch = actions[current - 1];
			batch->undo(); // exact prepared tile/selection swaps; no allocation or observers
			memory_size -= batch->memsize();
			actions.erase(actions.begin() + --current);
			--terrainPairCount;
		}
	} else {
		for (size_t i = current; i < actions.size(); ++i) memory_size -= actions[i]->memsize();
		actions.erase(actions.begin() + current, actions.end());
		// Same limits as a single terrain step, applied to the steps before the pair (admission already checked they suffice).
		const uint64_t limit = uint64_t(std::max(0, g_settings.getInteger(Config::UNDO_MEM_SIZE))) * 1024 * 1024;
		const auto count = size_t(std::max(0, g_settings.getInteger(Config::UNDO_SIZE)));
		while (current > terrainPairCount && (memory_size > limit || current > count)) {
			memory_size -= actions.front()->memsize();
			actions.pop_front();
			--current;
		}
	}
	terrainPair = false;
	terrainPairCount = 0;
	terrainPairLimit = 0;
	if (success) notifyTerrainTransition();
	transition.release();
	if (success) emitTerrainHistoryChange();
}

bool ActionQueue::undo() {
	if (applyingTerrain) return false;
	if (current > 0 && actions[current - 1]->getType() == ACTION_GENERATE_TERRAIN) {
		if (!actions[current - 1]->canApply()) return false;
		TerrainTransition transition(applyingTerrain);
		bool membershipChanged = false;
		try {
			g_minimap.PrepareInvalidationSlot(editor.map);
			if (!actions[current - 1]->batch.front()->prepareExactSelection(&membershipChanged)) return false;
		}
		catch (const std::bad_alloc&) { return false; }
		const auto selectionCount = editor.selection.size();
		actions[current - 1]->undo();
		--current;
		const bool selectionCountChanged = editor.selection.size() != selectionCount;
		notifyTerrainTransition(selectionCountChanged);
		transition.release();
		emitTerrainHistoryChange(membershipChanged);
		return true;
	}
	if (current > 0 && actions[current - 1]->canApply()) {
		current--;
		BatchAction* batch = actions[current].get();
		batch->undo();
		editor.notifyStateChange();
		g_luaScripts.emit("actionChange");
		return true;
	}
	return false;
}

bool ActionQueue::redo() {
	if (applyingTerrain) return false;
	if (current < actions.size() && actions[current]->getType() == ACTION_GENERATE_TERRAIN) {
		if (!actions[current]->canApply()) return false;
		TerrainTransition transition(applyingTerrain);
		bool membershipChanged = false;
		try {
			g_minimap.PrepareInvalidationSlot(editor.map);
			if (!actions[current]->batch.front()->prepareExactSelection(&membershipChanged)) return false;
		}
		catch (const std::bad_alloc&) { return false; }
		const auto selectionCount = editor.selection.size();
		actions[current]->redo();
		++current;
		const bool selectionCountChanged = editor.selection.size() != selectionCount;
		notifyTerrainTransition(selectionCountChanged);
		transition.release();
		emitTerrainHistoryChange(membershipChanged);
		return true;
	}
	if (current < actions.size() && actions[current]->canApply()) {
		BatchAction* batch = actions[current].get();
		batch->redo();
		current++;
		editor.notifyStateChange();
		g_luaScripts.emit("actionChange");
		return true;
	}
	return false;
}

void ActionQueue::clear() {
	if (applyingTerrain) return;
	actions.clear();
	current = 0;
	memory_size = 0;
	g_luaScripts.emit("actionChange");
}
