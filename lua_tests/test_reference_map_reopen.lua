-- @Title: Test reference map reopen
-- @Description: Run on an isolated g0-metadata.otbm, close it, reopen the same copy, then run again. Never reload scripts between stages.
assert(app.hasMap() and app.map.hasFile and app.map.name == "g0-metadata.otbm",
    "Use only the isolated metadata fixture")
local tile = assert(app.map:getTile(160, 160, 7))
local item = assert(tile.ground)
assert(item.actionId == 12007 and item.uniqueId == 12107, "Unexpected fixture")
if not _G.g0_reference_reopen then
    _G.g0_reference_reopen = { tile = tile, item = item }
    print("REFERENCE REOPEN ARMED: close the map and reopen the same isolated file; do not reload scripts")
else
    local saved = _G.g0_reference_reopen
    assert(not pcall(function() return saved.tile.x end), "Closed map tile reference became usable")
    assert(not pcall(function() return saved.item.actionId end), "Closed map item reference became usable")
    assert(tile.x == 160 and item.actionId == 12007, "Fresh references failed after reopening")
    _G.g0_reference_reopen = nil
    print("REFERENCE REOPEN PASS: stale tile/item rejected; fresh references resolve")
end
