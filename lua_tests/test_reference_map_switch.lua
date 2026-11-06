-- @Title: Test reference map switch
-- @Description: Run on isolated g0-metadata.otbm, a new empty map, then the fixture again.
assert(app.hasMap())
if not _G.g0_reference_switch then
    assert(app.map.name == "g0-metadata.otbm" and app.map.hasFile, "Start on the isolated fixture")
    local tile = assert(app.map:getTile(160, 160, 7))
    local item = assert(tile.ground)
    assert(item.actionId == 12007)
    _G.g0_reference_switch = { tile = tile, item = item, phase = 1 }
    print("REFERENCE SWITCH ARMED")
elseif _G.g0_reference_switch.phase == 1 then
    assert(not app.map.hasFile, "Switch to a new empty map")
    local saved = _G.g0_reference_switch
    assert(not pcall(function() return saved.tile.x end), "Tile from another map remained usable")
    assert(not pcall(function() return saved.item.actionId end), "Item from another map remained usable")
    saved.phase = 2
    print("REFERENCE SWITCH OTHER MAP PASS")
else
    assert(app.map.name == "g0-metadata.otbm" and app.map.hasFile, "Return to fixture map")
    local saved = _G.g0_reference_switch
    assert(saved.tile.x == 160 and saved.item.actionId == 12007, "Original references did not resolve after return")
    _G.g0_reference_switch = nil
    print("REFERENCE SWITCH RETURN PASS")
end
