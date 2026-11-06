-- @Title: Test transaction rollback and history
-- @Description: Run manually on a freshly reopened g0-smoke.otbm synthetic fixture. Never save this test.

assert(app.hasMap(), "Open the synthetic g0-smoke.otbm fixture")
local map, editor = app.map, app.editor
assert(map.name == "g0-smoke.otbm" and map.hasFile and map.tileCount == 400,
    "Use only the saved 400-tile synthetic G0 fixture")
assert(editor.historySize == 0, "Reopen the fixture to start with empty history")
local grass = Brushes.get("grass")
assert(grass and grass.type == "ground" and grass.lookId > 0, "Load the fixture's grass brush")
for x = 140, 143 do
    assert(map:getTile(x, 140, 7) == nil, "Regression test area is occupied")
end

local function check(before, after)
    assert(map:getTile(140, 140, 7) == nil and map:getTile(141, 140, 7) == nil,
        "Failed callback left a tile behind")
    for _, entry in ipairs({{142, before}, {143, after}}) do
        local tile = map:getTile(entry[1], 140, 7)
        if entry[2] then
            assert(tile and tile.hasGround and tile.ground.id == grass.lookId, "Successful transaction missing")
        else
            assert(tile == nil, "Unexpected history state")
        end
    end
    assert(map.tileCount == 400 + (before and 1 or 0) + (after and 1 or 0), "Unexpected tile count")
end

app.transaction("G0 rollback sentinel before", function()
    map:getOrCreateTile(142, 140, 7).ground = grass.lookId
end)
local historySize, historyIndex = editor.historySize, editor.historyIndex
assert(historySize == 1, "Successful sentinel must create one history entry")
for attempt = 1, 3 do
    local marker = "G0_CALLBACK_FAILURE_" .. attempt
    local ok, err = pcall(function()
        app.transaction("G0 callback must roll back", function()
            for x = 140, 141 do
                map:getOrCreateTile(x, 140, 7).ground = grass.lookId
            end
            error(marker)
        end)
    end)
    assert(not ok, "Callback error was swallowed")
    assert(tostring(err):find(marker, 1, true), "Original callback error was lost")
    check(true, false)
    assert(editor.historySize == historySize and editor.historyIndex == historyIndex,
        "Failed transaction changed history")
end
print("ROLLBACK PASS: 3 original errors propagated, 6 tile mutations restored, history unchanged.")

app.transaction("G0 rollback sentinel after", function()
    map:getOrCreateTile(143, 140, 7).ground = grass.lookId
    return "ignored callback return", 42
end)
check(true, true)
assert(editor.historySize == 2, "Successful transactions must remain separate history entries")
editor:undo()
check(true, false)
editor:undo()
check(false, false)
assert(not editor:canUndo(), "Failed transactions left an Undo entry")
editor:redo()
check(true, false)
editor:redo()
check(true, true)
assert(not editor:canRedo(), "Failed transactions left a Redo entry")
editor:undo()
editor:undo()
check(false, false)
print("TRANSACTION REGRESSION PASS: successful commits, separate Undo/Redo, no failed entries; fixture restored to 400 tiles.")
