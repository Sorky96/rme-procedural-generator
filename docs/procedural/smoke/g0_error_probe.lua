-- @Title: G0 transaction error probe
-- @Description: Diagnostic on the synthetic saved fixture only. Do not save after a failed rollback.

if not app.hasMap() then return end
local map = app.map
assert(map.name == "g0-smoke.otbm" and map.hasFile and map.tileCount == 400,
    "G0 requires the saved 400-tile synthetic fixture")
assert(map:getTile(140, 140, 7) == nil, "G0 probe location must be absent")
local grass = Brushes.get("grass")
assert(grass and grass.type == "ground" and grass.lookId > 0, "G0 grass missing")
local ok, err = pcall(function()
    app.transaction("G0 expected callback error", function()
        map:getOrCreateTile(140, 140, 7).ground = grass.lookId
        error("G0_EXPECTED_CALLBACK_ERROR")
    end)
end)
local tile = map:getTile(140, 140, 7)
print("G0 ERROR PROBE: propagated=" .. tostring(not ok)
    .. "; tile_present=" .. tostring(tile ~= nil)
    .. "; tile_count=" .. tostring(map.tileCount))
if not ok then print(tostring(err)) end
assert(not ok and tile == nil and map.tileCount == 400,
    "G0 ROLLBACK FAIL: callback error must propagate and restore the map. Undo the probe; do not save.")
print("G0 ROLLBACK CONTENT PASS; history still requires a separate check.")
