-- @Title: Test nested transaction rejection
-- @Description: Run manually on a freshly reopened g0-smoke.otbm fixture; never save.

assert(app.hasMap(), "Open the synthetic g0-smoke.otbm fixture")
local map, editor = app.map, app.editor
assert(map.name == "g0-smoke.otbm" and map.hasFile and map.tileCount == 400,
    "Use only the saved 400-tile synthetic G0 fixture")
assert(editor.historySize == 0, "Reopen the fixture to start with empty history")
local grass = Brushes.get("grass")
assert(grass and grass.type == "ground" and grass.lookId > 0, "Load the fixture's grass brush")
for x = 144, 145 do
    assert(map:getTile(x, 140, 7) == nil, "Regression test area is occupied")
end

app.transaction("G0 outer transaction", function()
    map:getOrCreateTile(144, 140, 7).ground = grass.lookId
    local ok, err = pcall(function()
        app.transaction("G0 rejected nested transaction", function()
            error("Nested callback must not run")
        end)
    end)
    assert(not ok and tostring(err):find("Transaction already in progress", 1, true),
        "Expected nested transaction rejection")
    assert(map:getTile(144, 140, 7), "Nested rejection rolled back the outer transaction")
    map:getOrCreateTile(145, 140, 7).ground = grass.lookId
end)

assert(map.tileCount == 402 and editor.historySize == 1, "Outer transaction was not committed once")
editor:undo()
assert(map.tileCount == 400 and not map:getTile(144, 140, 7) and not map:getTile(145, 140, 7),
    "Undo did not restore both outer mutations")
editor:redo()
assert(map.tileCount == 402 and map:getTile(144, 140, 7) and map:getTile(145, 140, 7),
    "Redo did not restore both outer mutations")
editor:undo()
assert(map.tileCount == 400, "Fixture not restored")
print("NESTED TRANSACTION PASS: rejection preserves the outer transaction and its Undo/Redo")
