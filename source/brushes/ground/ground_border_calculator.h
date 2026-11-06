//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_GROUND_BORDER_CALCULATOR_H
#define RME_GROUND_BORDER_CALCULATOR_H

#include "app/main.h"

class BaseMap;
class Tile;

/**
 * @brief Handles the calculation of ground borders.
 */
class GroundBorderCalculator {
public:
	/**
	 * @brief Calculates and applies borders for a specific tile.
	 *
	 * @param map The map where the tile resides.
	 * @param tile The tile to calculate borders for.
	 * @param deterministicSpecificOrder When false (ordinary drawing), specific-case border blocks are
	 *        deduplicated by sorting on their addresses. When true, duplicates are dropped while keeping
	 *        the first-seen neighbour traversal order, so the result does not depend on allocation addresses.
	 */
	static void calculate(BaseMap* map, Tile* tile, bool deterministicSpecificOrder = false);
};

#endif // RME_GROUND_BORDER_CALCULATOR_H
