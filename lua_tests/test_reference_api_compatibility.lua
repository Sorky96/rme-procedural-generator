-- @Title: Test reference API compatibility
-- @Description: Run on isolated g0-metadata.otbm; do not save.
assert(app.hasMap() and app.map.name == "g0-metadata.otbm")
local map, selection = app.map, assert(app.selection)
assert(selection.isEmpty, "Fixture selection must be empty")
assert(map:getTile(20, 20, 7) == nil, "Empty location should return nil")

local tileCount = 0
local foundFixture = false
for tile in map.tiles do
    tileCount = tileCount + 1
    if tile.x == 160 and tile.y == 160 and tile.z == 7 then
        foundFixture = tile.hasGround and tile.ground.actionId == 12007
    end
end
assert(tileCount == map.tileCount and tileCount == 402 and foundFixture, "Tile iterator mismatch")

local spawnCount = 0
for tile in map.spawns do
    assert(tile.hasSpawn)
    spawnCount = spawnCount + 1
end
assert(spawnCount >= 1, "Expected fixture spawn")

local tileA = assert(map:getTile(100, 100, 7))
local tileB = assert(map:getTile(101, 100, 7))
local ok, message = pcall(function() selection:add(tileA, tileB.ground) end)
assert(not ok and tostring(message):find("does not belong", 1, true), "Mismatched selection was accepted")
assert(selection.isEmpty, "Mismatched selection changed selection")
selection:add(tileA, tileA.ground)
assert(not selection.isEmpty and #selection.tiles == 1, "Could not select a tile item")
local selected = selection.tiles[1]
assert(selected.x == 100 and selected.y == 100 and selected.ground, "Selection tile did not resolve")
ok, message = pcall(function() selection:remove(selected, assert(map:getTile(101, 100, 7)).ground) end)
assert(not ok and tostring(message):find("does not belong", 1, true), "Mismatched removal was accepted")
assert(not selection.isEmpty, "Mismatched removal changed selection")
selection:remove(selected, selected.ground)
assert(selection.isEmpty, "Selection was not restored")
print("REFERENCE API COMPATIBILITY PASS")
