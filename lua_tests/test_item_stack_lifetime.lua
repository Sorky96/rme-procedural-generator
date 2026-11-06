-- @Title: Test item stack lifetime
-- @Description: Run on isolated g0-metadata.otbm with client 1098-BlackTek; do not save.
assert(app.hasMap() and app.map.name == "g0-metadata.otbm")
local map, editor = app.map, app.editor
local info = assert(Items.getInfo(2148), "Missing test item 2148")
assert(info.isStackable and not info.isGroundTile, "Unexpected client item 2148")
local function source() return assert(map:getTile(100, 100, 7)) end
local function destination() return assert(map:getTile(101, 100, 7)) end
assert(source().itemCount == 0 and destination().itemCount == 0, "Fixture must have empty stacks")

app.transaction("stack fixture", function()
    local item = assert(source():addItem(2148, 3))
    item.actionId = 13001
    item.uniqueId = 13002
    item.count = 4
end)
assert(source().itemCount == 1 and source().items[1].actionId == 13001
    and source().items[1].uniqueId == 13002 and source().items[1].count == 4)
local history = editor.historySize
local removed = source().items[1]
local ok, err = pcall(function()
    app.transaction("stack removal rollback", function()
        assert(source():removeItem(removed))
        local valid = pcall(function() return removed.id end)
        assert(not valid, "Removed item reference remained usable")
        error("EXPECTED_STACK_ROLLBACK")
    end)
end)
assert(not ok and tostring(err):find("EXPECTED_STACK_ROLLBACK", 1, true))
assert(editor.historySize == history and source().itemCount == 1 and source().items[1].actionId == 13001
    and source().items[1].uniqueId == 13002 and source().items[1].count == 4)
assert(not pcall(function() return removed.id end))

local moving = source().items[1]
app.transaction("stack move", function()
    source():moveItem(moving, destination())
    assert(not pcall(function() return moving.id end), "Moved item retained its old owner")
    assert(destination().items[1].actionId == 13001 and destination().items[1].count == 4)
end)
assert(source().itemCount == 0 and destination().itemCount == 1)
local moved = destination().items[1]
editor:undo()
assert(source().itemCount == 1 and destination().itemCount == 0 and source().items[1].actionId == 13001)
assert(not pcall(function() return moved.id end))
editor:redo()
assert(source().itemCount == 0 and destination().items[1].actionId == 13001)
editor:undo()
editor:undo()
assert(source().itemCount == 0 and destination().itemCount == 0, "Fixture was not restored")
print("ITEM STACK LIFETIME PASS")
