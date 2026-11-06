-- @Title: Test map tile reference lifetime
-- @Description: Run on a freshly reopened, isolated g0-metadata.otbm; do not save.

assert(app.hasMap() and app.map.name == "g0-metadata.otbm" and app.map.hasFile,
    "Use the isolated metadata fixture")
local map, editor = app.map, app.editor
assert(editor.historySize == 0, "Reopen the fixture to start with empty history")
local function current() return assert(map:getTile(160, 160, 7)) end
local before = current()
local flags = before.mapFlags
local changed = flags ~ 1

local ok, err = pcall(function()
    app.transaction("tile reference rollback", function()
        before.mapFlags = changed
        error("EXPECTED_TILE_ROLLBACK")
    end)
end)
assert(not ok and tostring(err):find("EXPECTED_TILE_ROLLBACK", 1, true))
assert(current().mapFlags == flags and editor.historySize == 0)
local live, message = pcall(function() return before.mapFlags end)
assert(not live and tostring(message):find("Tile reference is no longer valid", 1, true))

app.transaction("tile reference commit", function() current().mapFlags = changed end)
assert(current().mapFlags == changed and editor.historySize == 1 and editor:canUndo())
local committed = current()
editor:undo()
assert(current().mapFlags == flags)
live, message = pcall(function() return committed.mapFlags end)
assert(not live and tostring(message):find("Tile reference is no longer valid", 1, true))
editor:redo()
assert(current().mapFlags == changed)
editor:undo()
assert(current().mapFlags == flags)
print("TILE REFERENCE PASS: rollback, undo and redo")
