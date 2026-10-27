-- @Title: G0 metadata and floors round-trip
-- @Description: Isolated synthetic G0 copy only. Create, save as g0-metadata.otbm, reopen with BlackTek, then run again.
assert(app.hasMap(), "Open the synthetic G0 fixture")
local map = app.map
local grass = Brushes.get("grass")
assert(grass and grass.lookId > 0, "G0 grass brush missing")
local function verify()
    assert(map.tileCount == 402, "Expected 400 base tiles plus two fixture tiles")
    for _, z in ipairs({6, 7}) do
        local tile = map:getTile(160, 160, z)
        assert(tile and tile.hasGround and tile.ground.id == grass.lookId, "Ground or floor missing")
        assert(tile.ground.actionId == 12000 + z and tile.ground.uniqueId == 12100 + z,
            "Ground action/unique ID changed")
        assert(tile.mapFlags == 1 and tile.houseId == 0 and tile.itemCount == 0, "Flags or item stack changed")
        if z == 7 then
            assert(tile.hasSpawn and tile.spawn.radius == 3, "Spawn changed")
            assert(tile.hasCreature and tile.creature.name == "Rat" and tile.creature.spawnTime == 90,
                "Creature or spawn time changed")
        else
            assert(not tile.hasSpawn and not tile.hasCreature, "Unexpected upstairs entity")
        end
    end
    for y = 99, 120 do
        for x = 99, 120 do
            local tile = map:getTile(x, y, 7)
            if x >= 100 and x <= 119 and y >= 100 and y <= 119 then
                assert(tile and tile.hasGround and tile.ground.id == grass.lookId and tile.itemCount == 0,
                    "Base fixture changed")
            else
                assert(tile == nil, "Base halo changed")
            end
        end
    end
end
if map.name == "g0-smoke.otbm" then
    assert(map.tileCount == 400 and not map:getTile(160, 160, 6) and not map:getTile(160, 160, 7),
        "Start from the unchanged 400-tile synthetic fixture")
    app.transaction("G0 metadata fixture", function()
        for _, z in ipairs({6, 7}) do
            local tile = map:getOrCreateTile(160, 160, z)
            tile.ground = grass.lookId
            tile.ground.actionId = 12000 + z
            tile.ground.uniqueId = 12100 + z
            tile.mapFlags = 1
            if z == 7 then
                tile:setSpawn(3)
                tile:setCreature("Rat", 90, 2)
            end
        end
    end)
    verify()
    print("G0 METADATA CREATED PASS: floors 6/7, UID/AID, flags, spawn and Rat. Save As g0-metadata.otbm, close and reopen.")
elseif map.name == "g0-metadata.otbm" then
    assert(map.hasFile, "Expected saved metadata fixture")
    verify()
    print("G0 METADATA ROUND-TRIP PASS: floors 6/7, UID/AID, flags, spawn, Rat and base halo preserved.")
else
    error("Use only the synthetic g0-smoke.otbm or g0-metadata.otbm fixture")
end
