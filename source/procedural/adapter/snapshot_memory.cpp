//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "procedural/adapter/snapshot_memory.h"

#include "map/tile.h"
#include "map/map_region.h"
#include "game/item.h"
#include "game/complexitem.h"
#include "game/creature.h"
#include "game/spawn.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rme::procedural {

	namespace {

		using Status = std::expected<void, SnapshotMemoryError>;

		// Item::deepCopy() recreates the item through Item::Create() using the current definitions, so
		// the copy's dynamic type may differ from the source; charge every item as the largest subclass.
		constexpr uint64_t kMaxItemObjectSize = (std::max)({ sizeof(Item), sizeof(Container), sizeof(Teleport), sizeof(Door), sizeof(Depot), sizeof(Podium) });

		// std::map node: left/parent/right links plus color/sentinel flags, padded to four pointers
		// (MSVC and libstdc++ layouts), followed by the stored key/value pair.
		constexpr uint64_t kAttributeMapNodeSize = 4 * sizeof(void*) + sizeof(ItemAttributeMap::value_type);

		// A copied heap string may get a capacity rounded up past its size (MSVC rounds to 16 bytes).
		constexpr uint64_t kStringCapacitySlack = 16;

		// True when the string's characters live inside the string object (small-string buffer). Such a
		// string is short enough that its copy is inline as well. Decided without constructing a probe
		// string, which would allocate an iterator-debugging proxy in MSVC debug builds.
		bool usesInlineStorage(const std::string& value) noexcept {
			const void* data = value.data();
			const void* begin = &value;
			const void* end = reinterpret_cast<const unsigned char*>(&value) + sizeof(std::string);
			const std::less<const void*> before;
			return !before(data, begin) && before(data, end);
		}

		class Estimator {
		public:
			Estimator(uint64_t max_bytes, std::stop_token stop) noexcept :
				max_bytes_(max_bytes), stop_(std::move(stop)) { }

			[[nodiscard]] SnapshotTileMemory result() const noexcept {
				return { used_, items_ };
			}

			Status addTile(const Tile& tile) {
				if (auto status = checkStop(); !status) {
					return status;
				}

				// Tile::deepCopy() briefly holds a default owned TileLocation and its clone at the same
				// time; TileOperations::deepCopy() allocates neither. Charge the larger path.
				if (auto status = addAllocation(sizeof(Tile)); !status) {
					return status;
				}
				for (int i = 0; i < 2; ++i) {
					if (auto status = addAllocation(sizeof(TileLocation)); !status) {
						return status;
					}
				}
				if (const HouseExitList* exits = tile.getHouseExits()) {
					if (auto status = addAllocation(sizeof(HouseExitList)); !status) {
						return status;
					}
					if (auto status = addArray(exits->capacity(), sizeof(uint32_t)); !status) {
						return status;
					}
				}

				if (tile.ground) {
					if (auto status = addItem(*tile.ground, 1); !status) {
						return status;
					}
				}

				// Tile::deepCopy() fills the stack with push_back and no reserve: the final buffer holds
				// at most 2n pointers and the previous buffer (at most n) is alive during the last growth.
				const uint64_t stack_size = tile.items.size();
				if (auto status = addArray(stack_size, 2 * sizeof(std::unique_ptr<Item>)); !status) {
					return status;
				}
				if (auto status = addArray(stack_size, sizeof(std::unique_ptr<Item>)); !status) {
					return status;
				}
				for (const auto& item : tile.items) {
					if (!item) {
						return std::unexpected(SnapshotMemoryError::Unsupported);
					}
					if (auto status = addItem(*item, 1); !status) {
						return status;
					}
				}

				if (tile.creature) {
					if (auto status = addCreature(*tile.creature); !status) {
						return status;
					}
				}
				if (tile.spawn) {
					if (auto status = addAllocation(sizeof(Spawn)); !status) {
						return status;
					}
				}
				if (tile.extra) {
					if (auto status = addExtra(*tile.extra); !status) {
						return status;
					}
				}
				return {};
			}

		private:
			Status checkStop() const {
				if (stop_.stop_requested()) {
					return std::unexpected(SnapshotMemoryError::Cancelled);
				}
				return {};
			}

			Status add(uint64_t bytes) {
				if (bytes > max_bytes_ - used_) {
					return std::unexpected(SnapshotMemoryError::Limit);
				}
				used_ += bytes;
				return {};
			}

			Status addAllocation(uint64_t payload_bytes) {
				if (payload_bytes > UINT64_MAX - kSnapshotAllocationOverhead) {
					return std::unexpected(SnapshotMemoryError::Overflow);
				}
				return add(payload_bytes + kSnapshotAllocationOverhead);
			}

			// One heap buffer of count elements; nothing is allocated for an empty array.
			Status addArray(uint64_t count, uint64_t element_size) {
				if (count == 0 || element_size == 0) {
					return {};
				}
				if (count > UINT64_MAX / element_size) {
					return std::unexpected(SnapshotMemoryError::Overflow);
				}
				return addAllocation(count * element_size);
			}

			// Heap buffer of a copied std::string; strings that fit inline allocate nothing.
			Status addStringCopy(const std::string& value) {
				if (usesInlineStorage(value)) {
					return {};
				}
				const uint64_t size = value.size();
				if (size > UINT64_MAX - kStringCapacitySlack - 1) {
					return std::unexpected(SnapshotMemoryError::Overflow);
				}
				const uint64_t capacity = (std::max)(static_cast<uint64_t>(value.capacity()), size + kStringCapacitySlack);
				if (capacity == UINT64_MAX) {
					return std::unexpected(SnapshotMemoryError::Overflow);
				}
				return addAllocation(capacity + 1);
			}

			Status countItem() {
				if (items_ == UINT64_MAX) {
					return std::unexpected(SnapshotMemoryError::Overflow);
				}
				++items_;
				return {};
			}

			Status addItem(const Item& item, uint32_t depth) {
				if (depth > kSnapshotMaxNestingDepth) {
					return std::unexpected(SnapshotMemoryError::DepthLimit);
				}
				if (auto status = checkStop(); !status) {
					return status;
				}
				if (auto status = countItem(); !status) {
					return status;
				}
				if (auto status = addAllocation(kMaxItemObjectSize); !status) {
					return status;
				}
				if (auto status = addAttributes(item); !status) {
					return status;
				}
				if (const InvalidOTBMItemData* invalid = item.getInvalidOTBMData()) {
					if (auto status = addAllocation(sizeof(InvalidOTBMItemData)); !status) {
						return status;
					}
					if (auto status = addArray(invalid->rawInlineBytes.capacity(), sizeof(uint8_t)); !status) {
						return status;
					}
					if (invalid->rawNode) {
						if (auto status = addNodeContents(*invalid->rawNode, 1); !status) {
							return status;
						}
					}
				}
				if (const Container* container = item.asContainer()) {
					// Container::deepCopy() reserves exactly the content count.
					const auto& contents = container->getVector();
					if (auto status = addArray(contents.size(), sizeof(std::unique_ptr<Item>)); !status) {
						return status;
					}
					for (const auto& child : contents) {
						if (!child) {
							return std::unexpected(SnapshotMemoryError::Unsupported);
						}
						if (auto status = addItem(*child, depth + 1); !status) {
							return status;
						}
					}
				}
				return {};
			}

			Status addAttributes(const Item& item) {
				// Item::deepCopy() copies the map whenever it is allocated, even if empty. Charge the map
				// object and the sentinel node std::map implementations may allocate, then each entry.
				const ItemAttributeMap* attributes = item.getAttributesView();
				if (!attributes) {
					return {};
				}
				if (auto status = addAllocation(sizeof(ItemAttributeMap)); !status) {
					return status;
				}
				if (auto status = addAllocation(kAttributeMapNodeSize); !status) {
					return status;
				}
				for (const auto& [key, value] : *attributes) {
					if (auto status = checkStop(); !status) {
						return status;
					}
					if (auto status = addAllocation(kAttributeMapNodeSize); !status) {
						return status;
					}
					if (auto status = addStringCopy(key); !status) {
						return status;
					}
					if (const std::string* text = value.getString()) {
						if (auto status = addStringCopy(*text); !status) {
							return status;
						}
					}
				}
				return {};
			}

			// Heap payload of a preserved node; the node object itself is counted by its owner.
			Status addNodeContents(const PreservedOTBMNode& node, uint32_t depth) {
				if (depth > kSnapshotMaxNestingDepth) {
					return std::unexpected(SnapshotMemoryError::DepthLimit);
				}
				if (auto status = checkStop(); !status) {
					return status;
				}
				if (auto status = addArray(node.rawPayload.capacity(), sizeof(uint8_t)); !status) {
					return status;
				}
				if (auto status = addArray(node.children.capacity(), sizeof(PreservedOTBMNode)); !status) {
					return status;
				}
				for (const PreservedOTBMNode& child : node.children) {
					if (auto status = addNodeContents(child, depth + 1); !status) {
						return status;
					}
				}
				return {};
			}

			Status addCreature(const Creature& creature) {
				if (auto status = addAllocation(sizeof(Creature)); !status) {
					return status;
				}
				if (auto status = addStringCopy(creature.getTypeName()); !status) {
					return status;
				}
				const auto& alternatives = creature.getAlternatives();
				if (auto status = addArray(alternatives.capacity(), sizeof(SpawnAlternative)); !status) {
					return status;
				}
				for (const SpawnAlternative& alternative : alternatives) {
					if (auto status = checkStop(); !status) {
						return status;
					}
					if (auto status = addStringCopy(alternative.name); !status) {
						return status;
					}
				}
				return {};
			}

			Status addExtra(const TileExtraData& extra) {
				if (auto status = addAllocation(sizeof(TileExtraData)); !status) {
					return status;
				}
				// heapSize() is the heap vector object plus its element buffer: two allocations.
				const uint64_t zone_heap = extra.zoneIds.heapSize();
				if (zone_heap > 0) {
					constexpr uint64_t zone_vector_size = sizeof(std::vector<uint16_t>);
					if (auto status = addAllocation(zone_vector_size); !status) {
						return status;
					}
					if (zone_heap > zone_vector_size) {
						if (auto status = addAllocation(zone_heap - zone_vector_size); !status) {
							return status;
						}
					}
				}

				const TileInvalidContent* invalid = extra.invalidContent.get();
				if (!invalid) {
					return {};
				}
				if (auto status = addAllocation(sizeof(TileInvalidContent)); !status) {
					return status;
				}
				if (auto status = addArray(invalid->opaqueTileAttributes.capacity(), sizeof(OpaqueTileAttributeRecord)); !status) {
					return status;
				}
				for (const OpaqueTileAttributeRecord& record : invalid->opaqueTileAttributes) {
					if (auto status = checkStop(); !status) {
						return status;
					}
					if (auto status = addArray(record.rawBytes.capacity(), sizeof(uint8_t)); !status) {
						return status;
					}
				}
				if (auto status = addArray(invalid->opaqueChildNodes.capacity(), sizeof(PreservedOTBMNode)); !status) {
					return status;
				}
				for (const PreservedOTBMNode& node : invalid->opaqueChildNodes) {
					if (auto status = addNodeContents(node, 1); !status) {
						return status;
					}
				}
				return {};
			}

			uint64_t max_bytes_;
			uint64_t used_ = 0;
			uint64_t items_ = 0;
			std::stop_token stop_;
		};

	} // namespace

	uint64_t maximumSnapshotItemAllocation() noexcept {
		return kMaxItemObjectSize + kSnapshotAllocationOverhead;
	}

	std::expected<SnapshotTileMemory, SnapshotMemoryError> estimateSnapshotTileMemory(const Tile& tile, uint64_t max_bytes, std::stop_token stop) {
		Estimator estimator(max_bytes, std::move(stop));
		if (auto status = estimator.addTile(tile); !status) {
			return std::unexpected(status.error());
		}
		return estimator.result();
	}

} // namespace rme::procedural
