// Separate probe for the current Zones-based BlackTek. Never call server main.
#include "otpch.h"
#include "item.h"
#include "itemcontainer.h"
#include "tile.h"
#include "iomap.h"
#include "map.h"
#include "zones.h"
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int runProbe(int argc, char** argv) {
    require(argc == 3, "DAT and private map arguments required");
    const auto root = fs::current_path();
    require(fs::is_regular_file(root / ".g0-current-iomap"), "Private fixture guard missing");
    const auto mapPath = fs::weakly_canonical(argv[2]);
    require(mapPath.parent_path() == root && mapPath.filename() == "house-roundtrip.otbm", "Refusing non-fixture map");
    std::cerr << "ISOLATED CURRENT PROBE; no server startup, database or activation\n";
    if (!Item::items.loadFromDat(argv[1])) return 3;
    require(g_config.SetString(ConfigManager::MAP_NAME, "g0-private"), "Private map name rejected");
    require(g_config.SetBoolean(ConfigManager::DISABLE_LEGACY_SPAWN_FILE_AFTER_CONVERSION, false), "Private conversion policy rejected");
    std::vector<std::byte> buffer;
    std::optional<std::pmr::monotonic_buffer_resource> block;
    Map map;
    IOMap loader;
    if (!loader.loadMap(&map, mapPath, buffer, block)) {
        std::cerr << "IOMap failed: " << loader.getLastErrorString() << '\n';
        return 4;
    }
    // The runner pins the OTBM/house XML to approved bytes; housefile is private.
    require(fs::weakly_canonical(map.spawnfile).parent_path() == root, "Spawn companion escaped private fixture");
    if (!IOMap::loadHouses(&map)) return 5;
    const auto* house = map.houses.getHouse(1);
    require(house && house->getName() == "G0 House" && house->getRent() == 4321 &&
        house->getTownId() == 1 && house->getEntryPosition() == Position(105,102,7) &&
        house->getTiles().size() == 1, "house metadata mismatch");
    require(map.getTile(104,102,7) && map.getTile(104,102,7)->getHouse() == house, "house tile mismatch");
    for (int z : {6,7}) {
        const auto& tile = map.getTile(160,160,z);
        require(tile && tile->getGround() && tile->getGround()->getActionId() == 12000+z &&
            tile->getGround()->getUniqueId() == 12100+z, "floor UID/AID mismatch");
        require(std::ranges::any_of(map.legacyZoneFlagTiles, [z](const auto& entry) {
            return entry.position == Position(160,160,z) && (entry.legacy_bits & OTBM_TILEFLAG_PROTECTIONZONE);
        }), "legacy PZ input missing");
    }
    const auto& tile = map.getTile(100,100,7);
    require(tile && tile->getItemList() && tile->getItemList()->size() == 1, "outer item missing");
    const auto outer = tile->getItemList()->front();
    require(outer && outer->getID() == 1987 && outer->getContainer() && outer->getContainer()->getItemList().size() == 1, "outer bag mismatch");
    const auto inner = outer->getContainer()->getItemByIndex(0);
    require(inner && inner->getID() == 1987 && inner->getContainer() && inner->getContainer()->getItemList().size() == 1, "inner bag mismatch");
    const auto coins = inner->getContainer()->getItemByIndex(0);
    require(coins && coins->getID() == 2148 && coins->getItemCount() == 7 && coins->getActionId() == 12042, "coins mismatch");
    Zones::ZoneManager::SmartConvertLegacyZoneFlags(map);
    Zones::ZoneManager::StampWorldZoneFlags();
    for (int z : {6,7}) require(Zones::ZoneManager::HasWorldFlag(Position(160,160,z), Zones::ZoneFlag::Protection), "converted runtime PZ missing");
    std::cerr << "CURRENT_MAP_HOUSE_ITEMS_PZ_PASS\n";
    const bool spawnExists = fs::is_regular_file(map.spawnfile);
    Zones::ZoneManager::ConvertLegacySpawns(map.spawnfile, map.otbmFilePath);
    const auto handle = Zones::ZoneManager::GetZoneByName("ConvertedSpawn_z7_1");
    if (!spawnExists) {
        require(!handle, "Missing spawn unexpectedly produced a zone");
        std::cerr << "CURRENT_MISSING_SPAWN_NO_ZONE\n";
        return 6;
    }
    require(handle.has_value(), "converted spawn zone missing");
    const auto* zone = Zones::ZoneManager::TryGet(*handle);
    require(zone && zone->entity_type == Zones::SpawnType::Monster && zone->policy == Zones::Policy::Fixed &&
        zone->positions.size() == 25 && zone->creature_list.size() == 1, "converted spawn geometry mismatch");
    require(std::ranges::find(zone->positions, Position(158,158,7)) != zone->positions.end() &&
        std::ranges::find(zone->positions, Position(162,162,7)) != zone->positions.end(), "converted bounds mismatch");
    const auto& entry = zone->creature_list.front();
    require(entry && entry->position == Position(2,2,0) && entry->interval == 90000 && entry->creatures == "Rat:100;", "converted spawn record mismatch");
    require(fs::is_regular_file(map.spawnfile.string() + ".converted"), "conversion marker missing");
    std::cerr << "CURRENT_SPAWN_CONVERSION_PASS: Rat, absolute 160/160/7, 90000ms, fixed 158..162 bounds\n";
    const auto convertedPath = root / "data/world/converted/g0-private/g0-private-zones/floor-7.toml";
    const auto converted = toml::parse_file(convertedPath.string());
    const auto* convertedEntry = converted["ConvertedSpawn_z7_1"].as_table();
    require(convertedEntry != nullptr, "converted TOML entry missing");
    require(Zones::ZoneManager::ParseZoneEntry("G0ReloadedSpawn", *convertedEntry).code == Zones::ParseCode::Success,
        "production TOML reader rejected converted spawn");
    const auto reloadedHandle = Zones::ZoneManager::GetZoneByName("G0ReloadedSpawn");
    require(reloadedHandle.has_value(), "reloaded zone missing");
    const auto* reloaded = Zones::ZoneManager::TryGet(*reloadedHandle);
    require(reloaded && reloaded->positions.size() == 25 && reloaded->creature_list.size() == 1 &&
        reloaded->creature_list.front()->position == Position(2,2,0) &&
        reloaded->creature_list.front()->interval == 90000 &&
        reloaded->creature_list.front()->creatures == "Rat:100;", "reloaded spawn record mismatch");
    std::cerr << "CURRENT_SPAWN_TOML_RELOAD_PASS\n";
    std::cerr << "BOUNDARY: converter uses padding 2; legacy radius/direction are not represented; no monster instantiation\n";
    return 0;
}
}
int main(int argc, char** argv) {
    try {
        const int result = runProbe(argc, argv);
        std::cerr << "Local map destruction completed; result=" << result << '\n';
        std::_Exit(result); // No normal server startup occurred; omit global teardown only.
    } catch (const std::exception& error) {
        std::cerr << "CURRENT PROBE FAILED: " << error.what() << '\n';
        std::_Exit(9);
    }
}
