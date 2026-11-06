#include "otpch.h"
#include "item.h"
#include "tile.h"
#include "position.h"
// Test-only observation of parsed spawn records; no server source is modified.
#define private public
#include "spawn.h"
#undef private
#include "iomap.h"
#include "map.h"
#include "monsters.h"
#include "container.h"
#include <stdexcept>
#include <cstdlib>

extern Monsters g_monsters;

void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

int runProbe(int argc, char** argv)
{
    std::cerr << "ISOLATED PROBE MAIN entered; no server startup\n";
    if (argc != 3) return 2;
    if (!Item::items.loadFromDat(argv[1])) return 3;
    std::cerr << "DAT loaded\n";
    require(g_monsters.loadFromXml(), "monster catalog failed");
    require(g_monsters.getMonsterType("Rat") != nullptr, "Rat not resolved");
    std::vector<std::byte> buffer;
    std::optional<std::pmr::monotonic_buffer_resource> block;
    Map map;
    IOMap loader;
    const auto result = loader.loadMap(&map, argv[2], buffer, block);
    if (!result) {
        std::cerr << "IOMap failed: " << loader.getLastErrorString() << '\n';
        return 4;
    }
    std::cerr << "IOMap loaded " << argv[2] << '\n';
    if (!IOMap::loadHouses(&map)) return 5;
    std::cerr << "Houses loaded\n";
    const auto house = map.houses.getHouse(1);
    require(house && house->getName() == "G0 House" && house->getRent() == 4321 &&
        house->getTownId() == 1 && house->getEntryPosition() == Position(105,102,7) &&
        house->getTiles().size() == 1, "house metadata mismatch");
    const auto houseTile = map.getTile(104,102,7);
    require(houseTile && houseTile->getHouse() == house, "house tile association mismatch");
    for (int z : {6,7}) {
        auto tile = map.getTile(160,160,z);
        require(tile && tile->getGround(), "metadata ground missing");
        require(tile->getGround()->getActionId() == 12000+z &&
            tile->getGround()->getUniqueId() == 12100+z &&
            tile->hasFlag(TILESTATE_PROTECTIONZONE), "ground attributes/flags mismatch");
    }
    auto tile = map.getTile(100,100,7);
    require(tile && tile->getItemList() && tile->getItemList()->size() == 1, "outer bag missing");
    auto bag = tile->getItemList()->front()->getContainer();
    require(bag && bag->getID() == 1987 && bag->size() == 1, "outer bag mismatch");
    auto inner = bag->getItemByIndex(0)->getContainer();
    require(inner && inner->getID() == 1987 && inner->size() == 1, "inner bag mismatch");
    auto coins = inner->getItemByIndex(0);
    require(coins && coins->getID() == 2148 && coins->getItemCount() == 7 &&
        coins->getActionId() == 12042, "coins mismatch");
    if (!IOMap::loadSpawns(&map)) return 6;
    require(std::distance(map.spawns.spawnList.begin(), map.spawns.spawnList.end()) == 1, "spawn count mismatch");
    const auto& spawn = map.spawns.spawnList.front();
    require(spawn.centerPos == Position(160,160,7) && spawn.radius == 3 &&
        spawn.spawnMap.size() == 1, "spawn geometry/block count mismatch");
    const auto& sb = spawn.spawnMap.begin()->second;
    require(sb.pos == Position(160,160,7) && sb.interval == 90000 &&
        sb.direction == DIRECTION_SOUTH && sb.mTypes.size() == 1 &&
        sb.mTypes.front().first->name == "Rat", "spawn monster mismatch");
    std::cerr << "ASSERTIONS PASS: house metadata/tile, floors 6/7 UID/AID/PZ, nested bags/coins, Rat spawn geometry/time/direction\n";
    return 0;
}

int main(int argc, char** argv)
{
    try {
        const int result = runProbe(argc, argv);
        std::cerr << "Local map destruction completed; result=" << result << '\n';
        // Server globals have not undergone normal startup; skip their teardown.
        // runProbe has already destroyed its local map and memory arena.
        std::_Exit(result);
    } catch (const std::exception& error) {
        std::cerr << "PROBE FAILED: " << error.what() << '\n';
        std::_Exit(9);
    }
}
