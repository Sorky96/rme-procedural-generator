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

#include "map/tile.h"
#include "map/basemap.h"
#include "map/spatial_hash_grid.h"

#include <stdexcept>
#include <utility>

BaseMap::BaseMap() :
	allocator(),
	tilecount(0),
	grid(*this) {
	////
}

void BaseMap::clear(bool del) {
	std::ranges::for_each(tiles(), [&](auto& loc) {
		if (loc.tile) {
			std::unique_ptr<Tile> t = std::move(loc.tile);
			if (!del) {
				t.release();
			}
			--tilecount;
		}
	});
	change_tracker.markAllDirty();
}

void BaseMap::clearVisible(uint32_t mask) {
	grid.clearVisible(mask);
}

Tile* BaseMap::createTile(int x, int y, int z) {
	ASSERT(z < MAP_LAYERS);
	MapNode* leaf = grid.getLeafForce(x, y);
	TileLocation* loc = leaf->createTile(x, y, z);
	if (loc->get()) {
		return loc->get();
	}
	std::unique_ptr<Tile> t = allocator(loc);
	Tile* ptr = t.get();
	leaf->setTile(x, y, z, std::move(t));
	return ptr;
}

Tile* BaseMap::createTileInCell(size_t cell_idx, int x, int y, int z) {
	ASSERT(z < MAP_LAYERS);
	MapNode* leaf = grid.getLeafForceInCell(cell_idx, x, y);
	if (!leaf) {
		return nullptr;
	}
	Floor* f = leaf->createFloor(x, y, z);
	int offset_x = x & 3;
	int offset_y = y & 3;
	TileLocation* tmp = &f->locs[offset_x * 4 + offset_y];
	if (tmp->tile) {
		return tmp->tile.get();
	}
	std::unique_ptr<Tile> t = allocator(tmp);
	t->setLocation(tmp);
	Tile* ptr = t.get();
	tmp->tile = std::move(t);
	return ptr;
}

Tile* BaseMap::getOrCreateTile(const Position& pos) {
	if (Tile* t = getTile(pos)) {
		return t;
	}

	Tile* newTile = createTile(pos.x, pos.y, pos.z);
	newTile->setLocation(createTileL(pos));
	return newTile;
}

TileLocation* BaseMap::getTileL(int x, int y, int z) {
	ASSERT(z < MAP_LAYERS);
	MapNode* leaf = grid.getLeaf(x, y);
	if (leaf) {
		Floor* floor = leaf->getFloor(z);
		if (floor) {
			return &floor->locs[(x & 3) * 4 + (y & 3)];
		}
	}

	return nullptr;
}

const TileLocation* BaseMap::getTileL(int x, int y, int z) const {
	ASSERT(z < MAP_LAYERS);
	const MapNode* leaf = grid.getLeaf(x, y);
	if (leaf) {
		const Floor* floor = leaf->getFloor(z);
		if (floor) {
			return &floor->locs[(x & 3) * 4 + (y & 3)];
		}
	}

	return nullptr;
}

TileLocation* BaseMap::getTileL(const Position& pos) {
	return getTileL(pos.x, pos.y, pos.z);
}

const TileLocation* BaseMap::getTileL(const Position& pos) const {
	return getTileL(pos.x, pos.y, pos.z);
}

TileLocation* BaseMap::createTileL(int x, int y, int z) {
	ASSERT(z < MAP_LAYERS);

	MapNode* leaf = grid.getLeafForce(x, y);
	Floor* floor = leaf->createFloor(x, y, z);
	uint32_t offsetX = x & 3;
	uint32_t offsetY = y & 3;

	return &floor->locs[offsetX * 4 + offsetY];
}

TileLocation* BaseMap::createTileL(const Position& pos) {
	return createTileL(pos.x, pos.y, pos.z);
}

std::unique_ptr<Tile> BaseMap::setTile(int x, int y, int z, std::unique_ptr<Tile> newtile) {
	ASSERT(!newtile || newtile->getX() == int(x));
	ASSERT(!newtile || newtile->getY() == int(y));
	ASSERT(!newtile || newtile->getZ() == int(z));

	change_tracker.markTileDirty(x, y, z);
	MapNode* leaf = grid.getLeafForce(x, y);
	return leaf->setTile(x, y, z, std::move(newtile));
}

std::unique_ptr<Tile> BaseMap::swapTile(int x, int y, int z, std::unique_ptr<Tile> newtile) {
	ASSERT(z < MAP_LAYERS);
	ASSERT(!newtile || newtile->getX() == int(x));
	ASSERT(!newtile || newtile->getY() == int(y));
	ASSERT(!newtile || newtile->getZ() == int(z));

	change_tracker.markTileDirty(x, y, z);
	MapNode* leaf = grid.getLeafForce(x, y);
	return leaf->setTile(x, y, z, std::move(newtile));
}

void BaseMap::swapReservedTile(const Position& pos, std::unique_ptr<Tile>& other) noexcept {
	TileLocation* location = getTileL(pos);
	ASSERT(location);
	ASSERT(!other || other->getPosition() == pos);
	if (!location) {
		return;
	}
	if (other) {
		other->setLocation(location);
	}
	const bool had = location->tile != nullptr;
	const bool has = other != nullptr;
	std::swap(location->tile, other);
	if (has && !had) {
		++tilecount;
	} else if (had && !has) {
		--tilecount;
	}
}

// Tile reservation

namespace {
	using GridCell = SpatialHashGrid::GridCell;

	int nodeIndexInCell(int x, int y) noexcept {
		const int nx = (x >> SpatialHashGrid::NODE_SHIFT) & (SpatialHashGrid::NODES_PER_CELL - 1);
		const int ny = (y >> SpatialHashGrid::NODE_SHIFT) & (SpatialHashGrid::NODES_PER_CELL - 1);
		return ny * SpatialHashGrid::NODES_PER_CELL + nx;
	}

	// Same bit layout as SpatialHashGrid::getLeafForce: one bit per 16x16 chunk of the cell.
	uint16_t chunkBitInCell(int x, int y) noexcept {
		const int nx = (x >> SpatialHashGrid::NODE_SHIFT) & (SpatialHashGrid::NODES_PER_CELL - 1);
		const int ny = (y >> SpatialHashGrid::NODE_SHIFT) & (SpatialHashGrid::NODES_PER_CELL - 1);
		return static_cast<uint16_t>(1u << (((ny >> 2) << 2) | (nx >> 2)));
	}
} // namespace

BaseMap::TileReservation::TileReservation(BaseMap& map, std::span<const Position> positions) :
	map_(map) {
	for (const Position& position : positions) {
		if (!position.isValid() || position.z >= MAP_LAYERS) {
			throw std::invalid_argument("TileReservation: position outside map limits");
		}
	}
	// All bookkeeping is allocated before the grid is touched.
	records_.reserve(positions.size());

	try {
		for (const Position& position : positions) {
			// Recorded before creation so a partially created cell/node is also rolled back.
			records_.push_back(map.probeTileReservation(position));
			map.createTileL(position);
		}
	} catch (...) {
		// The destructor does not run for a failed construction.
		map.rollbackTileReservation(records_);
		throw;
	}
}

BaseMap::TileReservation::~TileReservation() {
	if (armed_) {
		map_.rollbackTileReservation(records_);
	}
}

BaseMap::TileReservation::Record BaseMap::probeTileReservation(const Position& position) const noexcept {
	TileReservation::Record record;
	record.position = position;
	const size_t index = grid.findCellIndex(SpatialHashGrid::makeKey(position.x, position.y));
	const GridCell* cell = index < grid.cells_.size() ? grid.cells_[index].cell.get() : nullptr;
	const MapNode* node = cell ? cell->nodes[nodeIndexInCell(position.x, position.y)].get() : nullptr;
	record.cellMissing = cell == nullptr;
	record.nodeMissing = node == nullptr;
	record.floorMissing = node == nullptr || node->array[position.z] == nullptr;
	record.chunkBitWasSet = cell && (cell->floor_chunk_mask[position.z] & chunkBitInCell(position.x, position.y)) != 0;
	return record;
}

void BaseMap::rollbackTileReservation(std::span<const TileReservation::Record> records) noexcept {
	const auto pristine = [](const Floor& floor) noexcept {
		for (const TileLocation& location : floor.locs) {
			if (location.tile || location.spawn_count || location.waypoint_count || location.town_count || location.house_exits) {
				return false;
			}
		}
		return true;
	};

	for (auto it = records.rbegin(); it != records.rend(); ++it) {
		const Position& position = it->position;
		const size_t index = grid.findCellIndex(SpatialHashGrid::makeKey(position.x, position.y));
		if (index == grid.cells_.size() || !grid.cells_[index].cell) {
			continue;
		}
		GridCell& cell = *grid.cells_[index].cell;
		std::unique_ptr<MapNode>& node = cell.nodes[nodeIndexInCell(position.x, position.y)];

		if (it->floorMissing && node && node->array[position.z] && pristine(*node->array[position.z])) {
			node->array[position.z].reset();
		}

		// The chunk bit is restored to its recorded value unless a floor still exists in that chunk.
		const uint16_t bit = chunkBitInCell(position.x, position.y);
		// The 4x4 nodes of this 16x16 chunk.
		const int chunkNx = ((position.x >> SpatialHashGrid::NODE_SHIFT) & (SpatialHashGrid::NODES_PER_CELL - 1)) & ~3;
		const int chunkNy = ((position.y >> SpatialHashGrid::NODE_SHIFT) & (SpatialHashGrid::NODES_PER_CELL - 1)) & ~3;
		bool chunkHasFloor = false;
		for (int ny = chunkNy; ny < chunkNy + 4 && !chunkHasFloor; ++ny) {
			for (int nx = chunkNx; nx < chunkNx + 4 && !chunkHasFloor; ++nx) {
				const auto& chunkNode = cell.nodes[ny * SpatialHashGrid::NODES_PER_CELL + nx];
				chunkHasFloor = chunkNode && chunkNode->array[position.z];
			}
		}
		if (it->chunkBitWasSet || chunkHasFloor) {
			cell.floor_chunk_mask[position.z] |= bit;
		} else {
			cell.floor_chunk_mask[position.z] &= static_cast<uint16_t>(~bit);
		}

		if (it->nodeMissing && node && std::ranges::none_of(node->array, [](const auto& floor) { return floor != nullptr; })) {
			node.reset();
		}
		if (it->cellMissing && std::ranges::none_of(cell.nodes, [](const auto& entry) { return entry != nullptr; })) {
			grid.cells_.erase(grid.cells_.begin() + std::ptrdiff_t(index));
		}
	}
	// Cell indices may have shifted; drop the grid's one-entry lookup cache.
	grid.last_valid_ = false;
}

// Iterators

MapIterator::MapIterator(BaseMap* _map) :
	cell_it(),
	node_i(0),
	floor_i(0),
	tile_i(0),
	current_tile(nullptr),
	map(_map) {
	if (map) {
		cell_it = map->grid.cells_.begin();
	}
}

// Copy constructor for flat iterator design - copies all stateful members
MapIterator::MapIterator(const MapIterator& other) :
	cell_it(other.cell_it),
	node_i(other.node_i),
	floor_i(other.floor_i),
	tile_i(other.tile_i),
	map(other.map),
	current_tile(other.current_tile) {
}

MapIterator BaseMap::begin() {
	MapIterator it(this);
	it.cell_it = grid.cells_.begin();
	it.node_i = 0;
	it.floor_i = 0;
	it.tile_i = 0;
	it.current_tile = nullptr;

	if (it.cell_it != grid.cells_.end()) {
		if (!it.findNext()) {
			return end();
		}
	} else {
		return end();
	}
	return it;
}

MapIterator BaseMap::end() {
	MapIterator it(this);
	it.cell_it = grid.cells_.end();
	it.current_tile = nullptr;
	return it;
}

TileLocation& MapIterator::operator*() const noexcept {
	return *current_tile;
}

bool MapIterator::operator==(const MapIterator& other) const noexcept {
	if (map != other.map) {
		return false;
	}
	if (cell_it == other.cell_it) {
		if (map && cell_it == map->getGrid().cells_.end()) {
			return true;
		}
		return node_i == other.node_i && floor_i == other.floor_i && tile_i == other.tile_i;
	}
	return false;
}

bool MapIterator::findNext() {
	while (cell_it != map->grid.cells_.end()) {
		SpatialHashGrid::GridCell* cell = cell_it->cell.get();
		if (!cell) {
			++cell_it;
			continue;
		}
		while (node_i < SpatialHashGrid::NODES_PER_CELL * SpatialHashGrid::NODES_PER_CELL) {
			MapNode* node = cell->nodes[node_i].get();
			if (node) {
				while (floor_i < MAP_LAYERS) {
					Floor* floor = node->array[floor_i].get();
					if (floor) {
						while (tile_i < SpatialHashGrid::TILES_PER_NODE) {
							TileLocation& t = floor->locs[tile_i];
							if (t.get()) {
								current_tile = &t;
								return true;
							}
							tile_i++;
						}
					}
					floor_i++;
					tile_i = 0;
				}
			}
			node_i++;
			floor_i = 0;
			tile_i = 0;
		}
		++cell_it;
		node_i = 0;
		floor_i = 0;
		tile_i = 0;
	}
	current_tile = nullptr;
	return false;
}

MapIterator& MapIterator::operator++() noexcept {
	if (current_tile) {
		tile_i++;
		findNext();
	}
	return *this;
}

MapIterator MapIterator::operator++(int) noexcept {
	MapIterator i(*this);
	++*this;
	return i;
}
