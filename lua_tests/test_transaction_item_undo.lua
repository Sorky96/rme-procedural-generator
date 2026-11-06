-- @Title: Test transaction item attributes and history
-- @Description: Run only on freshly reopened isolated g0-metadata.otbm. Never save.

assert(app.hasMap() and app.map.name == "g0-metadata.otbm" and app.map.hasFile,
    "Use the isolated metadata fixture")
local map, editor = app.map, app.editor
assert(editor.historySize == 0, "Reopen the fixture to start with empty history")

local function ground()
    local tile = map:getTile(160, 160, 7)
    assert(tile and tile.ground, "Fixture ground is missing")
    return tile.ground
end

local originalTile = map:getTile(160, 160, 7)
local original = ground()
assert(original.actionId == 12007 and original.uniqueId == 12107, "Unexpected fixture attributes")
local originalTier, originalText, originalDescription = original.tier, original.text, original.description

-- Deliberately capture this reference before the transaction.
local ok, err = pcall(function()
    app.transaction("item rollback", function()
        original.actionId = 12008
        original.uniqueId = 12108
        original.tier = 2
        original.text = "G0 rollback text"
        original.description = "G0 rollback description"
        error("EXPECTED_ITEM_ROLLBACK")
    end)
end)
local restored = ground().actionId == 12007 and ground().uniqueId == 12107
    and ground().tier == originalTier and ground().text == originalText
    and ground().description == originalDescription
local historyUnchanged = editor.historySize == 0 and not editor:canUndo()
if not restored then
    -- This is a disposable fixture; leave it in its original in-memory state on failure.
    ground().actionId = 12007
    ground().uniqueId = 12107
    ground().tier = originalTier
    ground().text = originalText
    ground().description = originalDescription
end
assert(not ok and tostring(err):find("EXPECTED_ITEM_ROLLBACK", 1, true), "Callback error was not propagated")
assert(restored and historyUnchanged, "Item attributes were not rolled back without a history entry")

-- A previously captured object must either resolve to the restored state or
-- fail cleanly. It must never expose the discarded item or dereference freed data.
local itemValid, oldItemAid = pcall(function() return original.actionId end)
assert(not itemValid or oldItemAid == 12007, "Old item reference exposes discarded state")
local tileValid, oldTileAid = pcall(function() return originalTile.ground.actionId end)
assert(not tileValid or oldTileAid == 12007, "Old tile reference exposes discarded state")

app.transaction("item commit", function()
    local item = ground()
    item.actionId = 12008
    item.uniqueId = 12108
end)
local changed = ground().actionId == 12008 and ground().uniqueId == 12108
if not editor:canUndo() then
    ground().actionId = 12007
    ground().uniqueId = 12107
    error("Item attribute commit did not create an undo action")
end
assert(changed and editor.historySize == 1, "Item attribute commit did not create one action")
local committedItem = ground()
editor:undo()
assert(ground().actionId == 12007 and ground().uniqueId == 12107, "Undo did not restore item attributes")
local committedValid, committedAid = pcall(function() return committedItem.actionId end)
assert(not committedValid or committedAid == 12007, "Item reference exposes state discarded by Undo")
editor:redo()
assert(ground().actionId == 12008 and ground().uniqueId == 12108, "Redo did not restore item attributes")
editor:undo()
assert(ground().actionId == 12007 and ground().uniqueId == 12107, "Fixture was not restored")

-- A detached clone has no owning tile and must not create a map history entry.
local clone = ground():clone()
assert(clone, "Could not clone fixture ground")
local historyBeforeClone = editor.historySize
app.transaction("detached clone", function()
    clone.actionId = 12009
    clone.uniqueId = 12109
end)
assert(clone.actionId == 12009 and clone.uniqueId == 12109, "Clone mutation failed")
assert(ground().actionId == 12007 and ground().uniqueId == 12107, "Clone mutation changed the map")
assert(editor.historySize == historyBeforeClone, "Clone mutation created map history")
print("ITEM TRANSACTION PASS: rollback, one history entry, undo and redo")
