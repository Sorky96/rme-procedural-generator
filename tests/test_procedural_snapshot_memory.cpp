#include "app/main.h"

#include "game/complexitem.h"
#include "game/creature.h"
#include "game/item.h"
#include "game/spawn.h"
#include "map/map_region.h"
#include "map/tile.h"
#include "procedural/adapter/snapshot_memory.h"
#include "procedural/adapter/generation_snapshot.h"
#include "procedural/adapter/snapshot_equal.h"
#include "map/map.h"
#include <wx/init.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

// Test-only replacement of the global allocation functions. Allocations are counted only while
// recording is enabled, which the zero-allocation test does solely around the estimator call.
// The default nothrow and array forms forward to these, as do the matching deletes.
namespace {
	std::atomic<bool> g_record_allocations { false };
	std::atomic<uint64_t> g_recorded_allocations { 0 };
	thread_local uint64_t g_thread_allocation_ordinal = 0;
	thread_local uint64_t g_fail_allocation = 0;
	thread_local bool g_failure_observed = false;
}

void* operator new(std::size_t size) {
	if (g_record_allocations.load(std::memory_order_relaxed)) {
		g_recorded_allocations.fetch_add(1, std::memory_order_relaxed);
		if (++g_thread_allocation_ordinal == g_fail_allocation) {
			g_failure_observed = true;
			throw std::bad_alloc();
		}
	}
	if (void* memory = std::malloc(size == 0 ? 1 : size)) {
		return memory;
	}
	throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
	return ::operator new(size);
}

void operator delete(void* memory) noexcept {
	std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
	std::free(memory);
}

void operator delete[](void* memory) noexcept {
	std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
	std::free(memory);
}

namespace {

	using rme::procedural::estimateSnapshotTileMemory;
	using rme::procedural::kSnapshotAllocationOverhead;
	using rme::procedural::kSnapshotMaxNestingDepth;
	using rme::procedural::SnapshotMemoryError;
	using rme::procedural::SnapshotTileMemory;

	constexpr uint64_t kNoLimit = UINT64_MAX;
	// Item ids are arbitrary: no client is loaded, and the estimator does not consult definitions.
	constexpr ServerItemId kPlainItemId = 100;
	constexpr ServerItemId kContainerItemId = 200;

	void require(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}

	SnapshotTileMemory estimateOk(const Tile& tile) {
		const auto result = estimateSnapshotTileMemory(tile, kNoLimit);
		require(result.has_value(), "estimate without limit must succeed");
		return *result;
	}

	SnapshotMemoryError estimateError(const Tile& tile, uint64_t max_bytes, std::stop_token stop = {}) {
		const auto result = estimateSnapshotTileMemory(tile, max_bytes, std::move(stop));
		require(!result.has_value(), "estimate was expected to fail");
		return result.error();
	}

	// Grows the tile through one mutation and checks the estimate strictly increases by at least min_growth.
	template <typename Mutation>
	SnapshotTileMemory requireGrowth(Tile& tile, const SnapshotTileMemory& before, uint64_t min_growth, Mutation&& mutate, const char* message) {
		mutate();
		const SnapshotTileMemory after = estimateOk(tile);
		require(after.bytes > before.bytes, message);
		require(after.bytes - before.bytes >= min_growth, message);
		return after;
	}

	// A chain of `length` items: length - 1 nested containers ending with a plain item.
	std::unique_ptr<Item> makeItemChain(uint32_t length) {
		std::unique_ptr<Item> current = std::make_unique<Item>(kPlainItemId, 1);
		for (uint32_t i = 1; i < length; ++i) {
			auto container = std::make_unique<Container>(kContainerItemId);
			container->getVector().push_back(std::move(current));
			current = std::move(container);
		}
		return current;
	}

	// A preserved node tree of exactly `depth` nested nodes.
	PreservedOTBMNode makeNodeChain(uint32_t depth) {
		PreservedOTBMNode current;
		current.rawPayload.assign(4, 0xAB);
		for (uint32_t i = 1; i < depth; ++i) {
			PreservedOTBMNode parent;
			parent.children.push_back(std::move(current));
			current = std::move(parent);
		}
		return current;
	}

	void testEmptyTile() {
		Tile tile(0, 0, 0);
		const SnapshotTileMemory memory = estimateOk(tile);
		require(memory.items == 0, "empty tile copies no items");
		const uint64_t expected = sizeof(Tile) + 2 * sizeof(TileLocation) + 3 * kSnapshotAllocationOverhead;
		require(memory.bytes == expected, "empty tile charges the tile and two owned locations");
	}

	void testPayloadGrowth() {
		Tile tile(0, 0, 0);
		SnapshotTileMemory memory = estimateOk(tile);

		memory = requireGrowth(tile, memory, sizeof(Item), [&] { tile.ground = std::make_unique<Item>(kPlainItemId, 1); }, "ground item grows estimate");
		require(memory.items == 1, "ground counts as one item");

		memory = requireGrowth(tile, memory, sizeof(Item), [&] { tile.items.push_back(std::make_unique<Item>(kPlainItemId, 1)); }, "stack item grows estimate");
		require(memory.items == 2, "stack item counted");

		memory = requireGrowth(tile, memory, sizeof(ItemAttributeMap) + 2 * sizeof(ItemAttributeMap::value_type), [&] {
			tile.items.back()->setAttribute("aid", int32_t { 1000 });
		}, "first attribute charges the map, its sentinel and one node");

		const std::string long_text(500, 'x');
		memory = requireGrowth(tile, memory, long_text.size(), [&] { tile.items.back()->setAttribute("text", long_text); }, "long string attribute charges its buffer");

		const std::string long_key(300, 'k');
		memory = requireGrowth(tile, memory, long_key.size(), [&] { tile.items.back()->setAttribute(long_key, int32_t { 1 }); }, "long attribute key charges its buffer");

		memory = requireGrowth(tile, memory, 2048, [&] {
			InvalidOTBMItemData invalid;
			invalid.kind = InvalidOTBMItemKind::MissingItem;
			invalid.rawInlineBytes.assign(1024, 0x01);
			invalid.rawNode = PreservedOTBMNode {};
			invalid.rawNode->rawPayload.assign(1024, 0x02);
			tile.items.back()->setInvalidOTBMData(std::move(invalid));
		}, "invalid OTBM item payload and raw node are charged");

		const std::string long_name(200, 'c');
		memory = requireGrowth(tile, memory, sizeof(Creature) + long_name.size(), [&] { tile.creature = std::make_unique<Creature>(long_name); }, "creature and its stored name are charged");

		memory = requireGrowth(tile, memory, sizeof(SpawnAlternative) + long_name.size(), [&] {
			tile.creature->addAlternative(SpawnAlternative { long_name, 60, SOUTH, 10 });
		}, "spawn alternatives and their names are charged");

		memory = requireGrowth(tile, memory, sizeof(Spawn), [&] { tile.spawn = std::make_unique<Spawn>(3); }, "spawn is charged");

		memory = requireGrowth(tile, memory, sizeof(TileExtraData), [&] { tile.addZone(1); }, "inline zone storage is charged");

		memory = requireGrowth(tile, memory, sizeof(std::vector<uint16_t>), [&] {
			tile.addZone(2);
			tile.addZone(3);
			tile.addZone(4);
		}, "heap zone storage is charged");

		memory = requireGrowth(tile, memory, sizeof(TileInvalidContent) + 512, [&] {
			tile.addOpaqueTileAttribute(OpaqueTileAttributeRecord { std::vector<uint8_t>(512, 0x03) });
		}, "opaque tile attributes are charged");

		memory = requireGrowth(tile, memory, 4 * sizeof(PreservedOTBMNode), [&] { tile.addOpaqueChildNode(makeNodeChain(4)); }, "opaque child node trees are charged");

		memory = requireGrowth(tile, memory, sizeof(HouseExitList) + 16 * sizeof(uint32_t), [&] {
			HouseExitList* exits = tile.location->createHouseExits();
			for (uint32_t house = 1; house <= 16; ++house) {
				exits->push_back(house);
			}
		}, "house exits of the cloned location are charged");

		require(memory.items == 2, "metadata does not change the item count");
	}

	void testNestedContainers() {
		Tile tile(0, 0, 0);
		const SnapshotTileMemory empty = estimateOk(tile);

		auto outer = std::make_unique<Container>(kContainerItemId);
		auto inner = std::make_unique<Container>(kContainerItemId);
		auto leaf = std::make_unique<Item>(kPlainItemId, 1);
		leaf->setAttribute("text", std::string(400, 'n'));
		inner->getVector().push_back(std::move(leaf));
		inner->getVector().push_back(std::make_unique<Item>(kPlainItemId, 1));
		outer->getVector().push_back(std::move(inner));
		tile.items.push_back(std::move(outer));

		const SnapshotTileMemory nested = estimateOk(tile);
		require(nested.items == 4, "every nested container item is counted");
		require(nested.bytes - empty.bytes >= 4 * sizeof(Item) + 400, "nested items and their attributes are charged");
	}

	void testNullVersusEmptyAttributeMap() {
		Tile tile(0, 0, 0);
		tile.items.push_back(std::make_unique<Item>(kPlainItemId, 1));
		Item& item = *tile.items.back();
		require(item.getAttributesView() == nullptr, "a new item has no attribute map");
		const SnapshotTileMemory without_map = estimateOk(tile);

		// setAttribute allocates the map and eraseAttribute keeps it, leaving an allocated empty map.
		item.setAttribute("aid", int32_t { 1 });
		item.eraseAttribute("aid");
		require(item.getAttributesView() != nullptr && item.getAttributesView()->empty(), "erase leaves an allocated empty map");
		const SnapshotTileMemory empty_map = estimateOk(tile);
		require(empty_map.bytes > without_map.bytes, "an allocated empty map is charged");
		require(empty_map.bytes - without_map.bytes >= sizeof(ItemAttributeMap) + sizeof(ItemAttributeMap::value_type) + 2 * kSnapshotAllocationOverhead, "an empty map charges the map object and its sentinel node");
		require(empty_map.items == without_map.items, "the map does not change the item count");

		item.clearAllAttributes();
		require(item.getAttributesView() == nullptr, "clearAllAttributes releases the map");
		require(estimateOk(tile).bytes == without_map.bytes, "a null map charges nothing");
	}

	void testEstimationDoesNotAllocate() {
		Tile tile(0, 0, 0);
		tile.ground = std::make_unique<Item>(kPlainItemId, 1);
		for (int item_index = 0; item_index < 32; ++item_index) {
			auto item = std::make_unique<Item>(kPlainItemId, 1);
			for (int attribute_index = 0; attribute_index < 24; ++attribute_index) {
				std::string key = std::to_string(attribute_index) + std::string(120, 'k');
				item->setAttribute(key, std::string(300, 'v'));
			}
			item->setAttribute("aid", int32_t { 1000 + item_index });
			tile.items.push_back(std::move(item));
		}
		auto container = std::make_unique<Container>(kContainerItemId);
		auto inner = std::make_unique<Item>(kPlainItemId, 1);
		inner->setAttribute(std::string(200, 'i'), std::string(500, 'j'));
		container->getVector().push_back(std::move(inner));
		tile.items.push_back(std::move(container));

		InvalidOTBMItemData invalid;
		invalid.kind = InvalidOTBMItemKind::MissingItem;
		invalid.rawInlineBytes.assign(256, 0x01);
		invalid.rawNode = makeNodeChain(8);
		tile.items.front()->setInvalidOTBMData(std::move(invalid));

		tile.creature = std::make_unique<Creature>(std::string(200, 'c'));
		tile.creature->addAlternative(SpawnAlternative { std::string(200, 'a'), 60, SOUTH, 10 });
		tile.spawn = std::make_unique<Spawn>(3);
		for (uint16_t zone = 1; zone <= 8; ++zone) {
			tile.addZone(zone);
		}
		tile.addOpaqueTileAttribute(OpaqueTileAttributeRecord { std::vector<uint8_t>(128, 0x02) });
		tile.addOpaqueChildNode(makeNodeChain(8));
		tile.location->createHouseExits()->push_back(7);

		const SnapshotTileMemory expected = estimateOk(tile);
		require(expected.items == 35, "ground, stack, container and nested item are counted");

		std::stop_source stop_source;
		const std::stop_token stop_token = stop_source.get_token();

		g_recorded_allocations.store(0);
		g_record_allocations.store(true);
		const auto unlimited = estimateSnapshotTileMemory(tile, kNoLimit, stop_token);
		const auto limited = estimateSnapshotTileMemory(tile, expected.bytes / 2, stop_token);
		g_record_allocations.store(false);
		const uint64_t allocations = g_recorded_allocations.load();

		require(allocations == 0, "estimation must not allocate");
		require(unlimited.has_value() && unlimited->bytes == expected.bytes, "recorded estimate matches");
		require(!limited.has_value() && limited.error() == SnapshotMemoryError::Limit, "recorded limited estimate fails with Limit");
	}

	void testBudget() {
		Tile tile(0, 0, 0);
		tile.items.push_back(std::make_unique<Item>(kPlainItemId, 1));
		tile.items.back()->setAttribute("text", std::string(100, 't'));
		const SnapshotTileMemory memory = estimateOk(tile);

		const auto exact = estimateSnapshotTileMemory(tile, memory.bytes);
		require(exact.has_value(), "a budget equal to the estimate succeeds");
		require(exact->bytes == memory.bytes && exact->items == memory.items, "budget does not change the estimate");

		require(estimateError(tile, memory.bytes - 1) == SnapshotMemoryError::Limit, "a budget one byte short fails with Limit");
		require(estimateError(tile, 0) == SnapshotMemoryError::Limit, "zero budget fails with Limit");
	}

	void testCancellation() {
		Tile tile(0, 0, 0);
		tile.items.push_back(makeItemChain(8));
		std::stop_source source;
		source.request_stop();
		require(estimateError(tile, kNoLimit, source.get_token()) == SnapshotMemoryError::Cancelled, "a requested stop cancels the estimate");
	}

	void testItemDepthLimit() {
		Tile at_limit(0, 0, 0);
		at_limit.items.push_back(makeItemChain(kSnapshotMaxNestingDepth));
		const SnapshotTileMemory memory = estimateOk(at_limit);
		require(memory.items == kSnapshotMaxNestingDepth, "item chain at the depth limit is accepted");

		Tile too_deep(0, 0, 0);
		too_deep.items.push_back(makeItemChain(kSnapshotMaxNestingDepth + 1));
		require(estimateError(too_deep, kNoLimit) == SnapshotMemoryError::DepthLimit, "item chain past the depth limit is rejected");
	}

	void testNodeDepthLimit() {
		Tile at_limit(0, 0, 0);
		at_limit.addOpaqueChildNode(makeNodeChain(kSnapshotMaxNestingDepth));
		require(estimateSnapshotTileMemory(at_limit, kNoLimit).has_value(), "node chain at the depth limit is accepted");

		Tile too_deep(0, 0, 0);
		too_deep.addOpaqueChildNode(makeNodeChain(kSnapshotMaxNestingDepth + 1));
		require(estimateError(too_deep, kNoLimit) == SnapshotMemoryError::DepthLimit, "node chain past the depth limit is rejected");

		Tile item_node_too_deep(0, 0, 0);
		auto item = std::make_unique<Item>(kPlainItemId, 1);
		InvalidOTBMItemData invalid;
		invalid.kind = InvalidOTBMItemKind::MissingItem;
		invalid.rawNode = makeNodeChain(kSnapshotMaxNestingDepth + 1);
		item->setInvalidOTBMData(std::move(invalid));
		item_node_too_deep.items.push_back(std::move(item));
		require(estimateError(item_node_too_deep, kNoLimit) == SnapshotMemoryError::DepthLimit, "invalid item node chain past the depth limit is rejected");
	}

	void testNullItemUnsupported() {
		Tile tile(0, 0, 0);
		tile.items.push_back(nullptr);
		require(estimateError(tile, kNoLimit) == SnapshotMemoryError::Unsupported, "a null stack entry is rejected");

		Tile nested(0, 0, 0);
		auto container = std::make_unique<Container>(kContainerItemId);
		container->getVector().push_back(nullptr);
		nested.items.push_back(std::move(container));
		require(estimateError(nested, kNoLimit) == SnapshotMemoryError::Unsupported, "a null container entry is rejected");
	}

	void testSnapshotCapture() {
		using namespace rme::procedural;
		Map map;
		const GenerationConfig config {{100,100,7,4,3}, 1729};
		constexpr CoreBudget coreBudget {12,4096};
		constexpr SnapshotBudget budget {100,1024*1024};
		auto* halo = map.createTile(99,99,7);
		halo->ground = std::make_unique<Item>(100,1);
		halo->ground->setUniqueID(142);
		halo->ground->setActionID(986);
		halo->ground->setAttribute("text", std::string("halo metadata"));
		auto* tile = map.createTile(100,100,7);
		tile->creature = std::make_unique<Creature>(std::string("stored creature spelling"));
		tile->creature->markAsNpcSpawn();
		tile->creature->setWeight(7);
		tile->creature->addAlternative({"alternative",90,NORTH,3});
		tile->spawn = std::make_unique<Spawn>(4);
		tile->spawn->setNpcSize(6);
		tile->spawn->addKind(Spawn::NPCS);
		tile->house_id = 37;
		tile->mapflags = TILESTATE_PROTECTIONZONE;
		tile->addZone(7);
		tile->addOpaqueTileAttribute({{0x71,0x72}});
		auto* location = map.createTileL(101,100,7);
		location->increaseSpawnCount();
		location->increaseWaypointCount();
		location->increaseTownCount();
		location->createHouseExits()->push_back(37);
		const auto count = map.getTileCount(), revision = map.getChangeTracker().getGeneration();
		const auto snapshot = captureGenerationSnapshot(map, config, coreBudget, budget);
		require(bool(snapshot), "snapshot capture failed");
		require(snapshot->context == GenerationRegion{98,98,7,8,7} && snapshot->cells.size() == 56, "two-cell halo incorrect");
		require(snapshotMatchesMap(map,*snapshot), "fresh snapshot does not match");
		const auto& haloCopy = snapshot->cells[1 * 8 + 1].original;
		require(haloCopy && haloCopy.get() != halo && haloCopy->getLocation() != halo->getLocation() &&
			snapshotTilesEqual(halo,haloCopy.get()), "halo is not an owned complete copy");
		const auto& metadata = snapshot->cells[2 * 8 + 3];
		require(!metadata.original && metadata.spawnCount == 1 && metadata.waypointCount == 1 && metadata.townCount == 1 &&
			metadata.houseExits == std::vector<uint32_t>{37}, "location-only metadata omitted");
		require(map.getTileCount() == count && map.getChangeTracker().getGeneration() == revision, "capture changed source map");
		g_thread_allocation_ordinal = 0;
		g_record_allocations.store(true);
		const auto baselineCopy = captureGenerationSnapshot(map,config,coreBudget,budget);
		g_record_allocations.store(false);
		const auto allocations = g_thread_allocation_ordinal;
		require(baselineCopy && allocations > 0, "snapshot allocation baseline missing");
		for (uint64_t ordinal = 1; ordinal <= allocations; ++ordinal) {
			g_thread_allocation_ordinal = 0;
			g_fail_allocation = ordinal;
			g_failure_observed = false;
			g_record_allocations.store(true);
			const auto failed = captureGenerationSnapshot(map,config,coreBudget,budget);
			g_record_allocations.store(false);
			g_fail_allocation = 0;
			require(g_failure_observed && !failed && failed.error() == SnapshotError::AllocationFailed,
				"snapshot allocation failure returned partial data or wrong error");
			require(snapshotMatchesMap(map,*snapshot), "failed capture mutated original map");
		}
		std::cout << "snapshot_allocation_faults=" << allocations << '\n';
		halo->ground->setUniqueID(143); // Deliberately no tracker notification: the value comparison must catch it.
		require(!snapshotMatchesMap(map,*snapshot), "untracked halo UID mutation accepted");
		halo->ground->setUniqueID(142);
		require(snapshotMatchesMap(map,*snapshot), "restored halo does not match");
		halo->ground->setActionID(987);
		require(!snapshotMatchesMap(map,*snapshot), "untracked halo AID mutation accepted");
		halo->ground->setActionID(986);
		tile->creature->save();
		require(!snapshotMatchesMap(map,*snapshot), "creature saved state mutation accepted");
		tile->creature->reset();
		tile->extra->invalidContent->opaqueTileAttributes[0].rawBytes[0] = 0x73;
		require(!snapshotMatchesMap(map,*snapshot), "preserved opaque payload mutation accepted");
		tile->extra->invalidContent->opaqueTileAttributes[0].rawBytes[0] = 0x71;
		tile->creature->getAlternatives()[0].weight = 4;
		require(!snapshotMatchesMap(map,*snapshot), "creature alternative mutation accepted");
		tile->creature->getAlternatives()[0].weight = 3;
		tile->spawn->setNpcSize(9);
		require(!snapshotMatchesMap(map,*snapshot), "NPC spawn radius mutation accepted");
		tile->spawn->setNpcSize(6);
		location->increaseTownCount();
		require(!snapshotMatchesMap(map,*snapshot), "location-only mutation accepted");
		location->decreaseTownCount();
		require(snapshotMatchesMap(map,*snapshot), "restored metadata does not match");
		const auto exact = captureGenerationSnapshot(map,config,coreBudget,{100,snapshot->estimatedBytes});
		require(bool(exact), "exact snapshot budget rejected");
		const auto shortBudget = captureGenerationSnapshot(map,config,coreBudget,{100,snapshot->estimatedBytes-1});
		require(!shortBudget && shortBudget.error() == SnapshotError::Limit, "short snapshot budget accepted");
		const auto shortCells = captureGenerationSnapshot(map,config,coreBudget,{55,budget.maxBytes});
		require(!shortCells && shortCells.error() == SnapshotError::Limit, "halo cell budget ignored");
		std::stop_source cancel;
		cancel.request_stop();
		const auto stopped = captureGenerationSnapshot(map,config,coreBudget,budget,{},cancel.get_token());
		require(!stopped && stopped.error() == SnapshotError::Cancelled, "cancelled snapshot returned payload");
		Map other;
		require(!snapshotMatchesMap(other,*snapshot), "snapshot accepted another map");
		map.getChangeTracker().markTileDirty(400,400,7);
		require(!snapshotMatchesMap(map,*snapshot), "snapshot accepted a changed revision");
		Map incompatible;
		auto* unsupportedTile = incompatible.createTile(100,100,7);
		auto container = std::make_unique<Container>(200);
		container->getVector().push_back(std::make_unique<Item>(100,1));
		unsupportedTile->items.push_back(std::move(container));
		const auto rejected = captureGenerationSnapshot(incompatible,config,coreBudget,budget);
		require(!rejected && rejected.error() == SnapshotError::Unsupported &&
			unsupportedTile->items[0]->asContainer()->getItemCount() == 1,
			"lossy copy against absent item definitions accepted or mutated source");
	}

	void testRecursiveEquality() {
		using namespace rme::procedural;
		const auto make = [] {
			auto tile = std::make_unique<Tile>(1,2,7);
			auto container = std::make_unique<Container>(200);
			auto leaf = std::make_unique<Item>(100,1);
			leaf->setUniqueID(12);
			leaf->setAttribute("text", std::string("nested content"));
			container->getVector().push_back(std::move(leaf));
			tile->items.push_back(std::move(container));
			return tile;
		};
		const auto first = make(), second = make();
		require(snapshotTilesEqual(first.get(),second.get()), "equal nested containers rejected");
		auto* leaf = second->items[0]->asContainer()->getItem(0);
		leaf->setUniqueID(13);
		require(!snapshotTilesEqual(first.get(),second.get()), "nested UID mutation omitted");
		leaf->setUniqueID(12);
		leaf->setAttribute("text",std::string("changed"));
		require(!snapshotTilesEqual(first.get(),second.get()), "nested text mutation omitted");
	}

	void testSnapshotScale(uint32_t side) {
		using namespace rme::procedural;
		Map map;
		map.initializeEmpty();
		const GenerationConfig config {{100,100,7,side,side},1729};
		auto* tile = map.createTile(99,99,7);
		tile->ground = std::make_unique<Item>(100,1);
		tile->ground->setUniqueID(21);
		const auto started = std::chrono::steady_clock::now();
		const auto snapshot = captureGenerationSnapshot(map,config,{250000,1024*1024},{260100,64*1024*1024});
		const auto elapsed = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
		require(snapshot && snapshot->cells.size() == size_t(side+4)*(side+4) && snapshotMatchesMap(map,*snapshot),
			"large owned snapshot/halo failed");
		std::cout << "snapshot_size=" << side << " context_cells=" << snapshot->cells.size()
			<< " estimated_bytes=" << snapshot->estimatedBytes << " capture_ms=" << elapsed << '\n';
	}

} // namespace

int main() {
	try {
		wxInitializer initializer;
		require(initializer.IsOk(), "wx initialization failed");
		testEmptyTile();
		testPayloadGrowth();
		testNestedContainers();
		testNullVersusEmptyAttributeMap();
		testEstimationDoesNotAllocate();
		testBudget();
		testCancellation();
		testItemDepthLimit();
		testNodeDepthLimit();
		testNullItemUnsupported();
		testSnapshotCapture();
		testRecursiveEquality();
		testSnapshotScale(100);
		testSnapshotScale(500);
	} catch (const std::exception& error) {
		g_record_allocations.store(false);
		g_fail_allocation = 0;
		std::cout << "test_procedural_snapshot_memory FAILED: " << error.what() << "\n";
		return EXIT_FAILURE;
	}
	std::cout << "test_procedural_snapshot_memory passed\n";
	return EXIT_SUCCESS;
}
