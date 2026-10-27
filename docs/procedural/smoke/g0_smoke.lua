-- @Title: G0 procedural integration smoke
-- @Description: Run only on a new synthetic map. Run again after Undo, then after Redo.

if not app.hasMap() then
    print("G0 smoke idle: open a new synthetic map before running this script.")
    return
end

local map = app.map
local stage = G0_SMOKE_STAGE or 0
local function check_region(filled)
    for y = 99, 120 do
        for x = 99, 120 do
            local tile = map:getTile(x, y, 7)
            local inside = x >= 100 and x <= 119 and y >= 100 and y <= 119
            if filled and inside then
                assert(tile and tile.hasGround, "G0 missing generated ground")
                assert(tile.itemCount == 0 and not tile.hasCreature and not tile.hasSpawn, "G0 unexpected contents")
            else
                assert(tile == nil, "G0 unexpected tile in empty region or halo")
            end
        end
    end
end

if stage == 0 then
    assert(map.tileCount == 0, "G0 requires a new empty synthetic map")
    local grass = Brushes.get("grass")
    assert(grass and grass.type == "ground" and grass.lookId > 0, "G0 requires a loaded grass brush")
    check_region(false)
    app.transaction("G0 smoke 20x20", function()
        for y = 100, 119 do
            for x = 100, 119 do
                map:getOrCreateTile(x, y, 7).ground = grass.lookId
            end
        end
    end)
    check_region(true)
    G0_SMOKE_MAP = map
    G0_SMOKE_STAGE = 1
    print("G0 PLACE PASS: 400 ground tiles, halo empty. Undo once, then run this script again.")
elseif stage == 1 then
    assert(map == G0_SMOKE_MAP, "G0 map changed")
    check_region(false)
    G0_SMOKE_STAGE = 2
    print("G0 UNDO PASS: region and halo restored to absent tiles. Redo once, then run again.")
elseif stage == 2 then
    assert(map == G0_SMOKE_MAP, "G0 map changed")
    check_region(true)
    G0_SMOKE_STAGE = 3
    print("G0 REDO PASS: generated grounds restored. Save a copy and reopen for the separate round-trip check.")
else
    print("G0 PLACE/UNDO/REDO stages finished. Reloading scripts resets this harness; do not rerun on a populated map.")
end
