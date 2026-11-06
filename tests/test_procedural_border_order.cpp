#include "app/main.h"

#include "brushes/ground/auto_border.h"
#include "brushes/ground/ground_border_calculator.h"
#include "brushes/ground/ground_brush.h"
#include "game/item.h"
#include "item_definitions/core/item_definition_fragments.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/basemap.h"
#include "map/tile.h"

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Exercises the real GroundBorderCalculator with synthetic item definitions and ground brushes
// (no client assets). Two specific-case border blocks overlap: the outcome depends on the order
// they run in, and one of them is reached twice through a wildcard rule.
//
// Layout around the centre tile (brush A), in the calculator's neighbour traversal order:
//   index 1 (north) = brush B -> block X (wildcard rule)
//   index 3 (west)  = brush C -> block Y (rule for C)
//   index 4 (east)  = brush D -> block X again (wildcard rule)
// so specificList is [X, Y, X] before deduplication.
//
// Block X places border E1 (autoborder) and has two cases: E1 -> E2, then E3 -> E4.
// Block Y has one case: E2 -> E3.
//   [X, Y]    -> E3  (first-seen traversal order, each block once)
//   [Y, X]    -> E2
//   [X, Y, X] -> E4  (a duplicate block executed twice)

namespace {

	void require(bool condition, const std::string& message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}

	constexpr ServerItemId kGroundA = 100;
	constexpr ServerItemId kGroundB = 101;
	constexpr ServerItemId kGroundC = 102;
	constexpr ServerItemId kGroundD = 103;
	constexpr ServerItemId kBorderE1 = 200;
	constexpr ServerItemId kBorderE2 = 201;
	constexpr ServerItemId kBorderE3 = 202;
	constexpr ServerItemId kBorderE4 = 203;

	constexpr int kCenterX = 10;
	constexpr int kCenterY = 10;
	constexpr int kFloor = 7;

	// Populates the protected brush fields the loader would normally set from XML.
	class TestGroundBrush : public GroundBrush {
	public:
		void enableInnerBorder() {
			has_inner_border = true;
		}
		std::vector<std::unique_ptr<BorderBlock>>& mutableBorders() {
			return borders;
		}
	};

	void addGroundDefinition(ServerItemId id, GroundBrush& brush) {
		ResolvedItemDefinitionRow row;
		row.server_id = id;
		row.client_id = 0;
		row.group = ITEM_GROUP_GROUND;
		g_item_definitions.append(std::move(row));

		ItemEditorData editor_data;
		editor_data.brush = &brush;
		g_item_definitions.setEditorData(id, editor_data);
	}

	void addBorderDefinition(ServerItemId id) {
		ResolvedItemDefinitionRow row;
		row.server_id = id;
		row.client_id = 0;
		row.flags = flagMask(ItemFlag::IsBorder);
		g_item_definitions.append(std::move(row));
	}

	std::unique_ptr<GroundBrush::SpecificCaseBlock> makeReplaceCase(uint16_t match_and_replace, uint16_t with) {
		auto specific_case = std::make_unique<GroundBrush::SpecificCaseBlock>();
		specific_case->items_to_match.push_back(match_and_replace);
		specific_case->to_replace_id = match_and_replace;
		specific_case->with_id = with;
		return specific_case;
	}

	// Gives the two existing blocks their roles without reallocating them, so their addresses stay fixed.
	// A's border list is ordered [Y, X] so getBrushTo() picks Y for C and the wildcard X for B and D.
	void assignRoles(TestGroundBrush& brush_a, AutoBorder& border_e1, uint32_t brush_c_id, bool x_is_lower_address) {
		auto& borders = brush_a.mutableBorders();
		const std::less<const GroundBrush::BorderBlock*> before;
		const bool first_is_lower = before(borders[0].get(), borders[1].get());
		// borders[1] becomes X; swap so X has the requested relative address.
		if (first_is_lower == x_is_lower_address) {
			std::swap(borders[0], borders[1]);
		}

		GroundBrush::BorderBlock& y = *borders[0];
		GroundBrush::BorderBlock& x = *borders[1];

		x.outer = false;
		x.super = false;
		x.to = Brush::WILDCARD_ID;
		x.autoborder = &border_e1;
		x.specific_cases.clear();
		x.specific_cases.push_back(makeReplaceCase(kBorderE1, kBorderE2));
		x.specific_cases.push_back(makeReplaceCase(kBorderE3, kBorderE4));

		y.outer = false;
		y.super = false;
		y.to = brush_c_id;
		y.autoborder = nullptr;
		y.specific_cases.clear();
		y.specific_cases.push_back(makeReplaceCase(kBorderE2, kBorderE3));

		require(before(&x, &y) == x_is_lower_address, "role assignment must follow the observed address order");
	}

	std::vector<ServerItemId> borderIds(const Tile& tile) {
		std::vector<ServerItemId> ids;
		for (const auto& item : tile.items) {
			ids.push_back(item->getID());
		}
		return ids;
	}

	std::string describe(const std::vector<ServerItemId>& ids) {
		std::string text = "[";
		for (ServerItemId id : ids) {
			text += std::to_string(id) + " ";
		}
		return text + "]";
	}

	void requireSingleBorder(const Tile& tile, ServerItemId expected, const std::string& context) {
		const std::vector<ServerItemId> ids = borderIds(tile);
		require(ids.size() == 1 && ids.front() == expected, context + ": expected [" + std::to_string(expected) + " ] got " + describe(ids));
	}

	void runScenario(BaseMap& map, Tile& center, TestGroundBrush& brush_a, AutoBorder& border_e1, uint32_t brush_c_id, bool x_is_lower_address) {
		assignRoles(brush_a, border_e1, brush_c_id, x_is_lower_address);
		const std::string label = x_is_lower_address ? "X at lower address" : "X at higher address";

		GroundBorderCalculator::calculate(&map, &center, true);
		requireSingleBorder(center, kBorderE3, label + ", deterministic order");

		// Recalculating replaces the previous borders and must give the same result.
		GroundBorderCalculator::calculate(&map, &center, true);
		requireSingleBorder(center, kBorderE3, label + ", deterministic order repeated");

		// The default keeps its address sort: X first only when X is at the lower address.
		GroundBorderCalculator::calculate(&map, &center);
		requireSingleBorder(center, x_is_lower_address ? kBorderE3 : kBorderE2, label + ", default address order");
	}

	void testSharedBorderOrder() {
		GroundBrush::init();
		g_item_definitions.clear();

		// Declared before the brushes so it outlives the blocks that point at it.
		AutoBorder border_e1(1);
		for (size_t direction = NORTH_HORIZONTAL; direction < border_e1.tiles.size(); ++direction) {
			border_e1.tiles[direction] = kBorderE1;
		}

		TestGroundBrush brush_a;
		TestGroundBrush brush_b;
		TestGroundBrush brush_c;
		TestGroundBrush brush_d;
		brush_a.enableInnerBorder();
		brush_a.mutableBorders().push_back(std::make_unique<GroundBrush::BorderBlock>());
		brush_a.mutableBorders().push_back(std::make_unique<GroundBrush::BorderBlock>());

		addGroundDefinition(kGroundA, brush_a);
		addGroundDefinition(kGroundB, brush_b);
		addGroundDefinition(kGroundC, brush_c);
		addGroundDefinition(kGroundD, brush_d);
		for (ServerItemId id : { kBorderE1, kBorderE2, kBorderE3, kBorderE4 }) {
			addBorderDefinition(id);
		}

		constexpr uint32_t x_alignment = (1u << 1) | (1u << 4);
		require(GroundBrush::border_types[x_alignment] != 0, "border table must place a border for the north/east neighbours");

		{
			BaseMap map;
			Tile* center = map.createTile(kCenterX, kCenterY, kFloor);
			center->ground = Item::Create(kGroundA);
			map.createTile(kCenterX, kCenterY - 1, kFloor)->ground = Item::Create(kGroundB);
			map.createTile(kCenterX - 1, kCenterY, kFloor)->ground = Item::Create(kGroundC);
			map.createTile(kCenterX + 1, kCenterY, kFloor)->ground = Item::Create(kGroundD);

			require(center->getGroundBrush() == &brush_a, "centre tile resolves to brush A");
			require(map.getTile(kCenterX, kCenterY - 1, kFloor)->getGroundBrush() == &brush_b, "north tile resolves to brush B");
			require(map.getTile(kCenterX - 1, kCenterY, kFloor)->getGroundBrush() == &brush_c, "west tile resolves to brush C");
			require(map.getTile(kCenterX + 1, kCenterY, kFloor)->getGroundBrush() == &brush_d, "east tile resolves to brush D");

			runScenario(map, *center, brush_a, border_e1, brush_c.getID(), true);
			runScenario(map, *center, brush_a, border_e1, brush_c.getID(), false);
		}

		// The definitions point at the brushes above; drop them before the brushes go away.
		g_item_definitions.clear();
	}

} // namespace

int main() {
	try {
		testSharedBorderOrder();
	} catch (const std::exception& error) {
		std::cout << "test_procedural_border_order FAILED: " << error.what() << "\n";
		g_item_definitions.clear();
		return EXIT_FAILURE;
	}
	std::cout << "test_procedural_border_order passed\n";
	return EXIT_SUCCESS;
}
