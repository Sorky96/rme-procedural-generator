-- @Title: G0 verify saved smoke map
-- @Description: Read-only content check for the synthetic g0-smoke.otbm fixture.

if not app.hasMap() then return end
local map = app.map
assert(map.name == "g0-smoke.otbm", "G0 verify requires the synthetic smoke fixture")
assert(map.hasFile and map.tileCount == 400, "G0 expected exactly 400 saved tiles")
local grass = Brushes.get("grass")
assert(grass and grass.type == "ground" and grass.lookId > 0, "G0 grass brush missing")
for y = 99, 120 do
    for x = 99, 120 do
        local tile = map:getTile(x, y, 7)
        local inside = x >= 100 and x <= 119 and y >= 100 and y <= 119
        if inside then
            assert(tile and tile.hasGround and tile.ground.id == grass.lookId, "G0 changed ground")
            assert(tile.itemCount == 0 and not tile.hasCreature and not tile.hasSpawn, "G0 changed stack or entities")
            assert(tile.houseId == 0 and tile.mapFlags == 0, "G0 changed house or flags")
        else
            assert(tile == nil, "G0 changed halo")
        end
    end
end
print("G0 SAVED CONTENT PASS: 400 exact ground IDs; empty stacks, entities, house IDs, flags and halo.")
