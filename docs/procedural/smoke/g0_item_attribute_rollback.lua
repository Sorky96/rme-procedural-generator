-- @Title: G0 item attribute rollback probe
-- @Description: Run only on isolated, saved g0-metadata.otbm. Do not save diagnostic changes.
assert(app.hasMap() and app.map.name == "g0-metadata.otbm" and app.map.hasFile,
    "Use only the isolated metadata fixture")
local map = app.map
local tile = map:getTile(160, 160, 7)
assert(tile and tile.hasGround and tile.ground.actionId == 12007 and tile.ground.uniqueId == 12107,
    "Expected original metadata fixture")
local editor = app.editor
local size, index = editor.historySize, editor.historyIndex
local ok, err = pcall(function()
    app.transaction("G0 item attribute rollback", function()
        tile.ground.actionId = 0
        error("G0_ITEM_EXPECTED_ERROR")
    end)
end)
tile = map:getTile(160, 160, 7)
local restored = tile and tile.ground and tile.ground.actionId == 12007
local historyUnchanged = editor.historySize == size and editor.historyIndex == index
print("G0 ITEM ROLLBACK: propagated=" .. tostring(not ok)
    .. "; action_restored=" .. tostring(not not restored)
    .. "; history_unchanged=" .. tostring(historyUnchanged))
if not restored then tile.ground.actionId = 12007 end
assert(not ok and tostring(err):find("G0_ITEM_EXPECTED_ERROR", 1, true), "Expected error did not propagate")
local beforeSize, beforeIndex = editor.historySize, editor.historyIndex
local success = pcall(function()
    app.transaction("G0 item attribute successful", function() tile.ground.actionId = 0 end)
end)
local changed = tile.ground.actionId == 0
local recorded = editor.historySize ~= beforeSize or editor.historyIndex ~= beforeIndex
print("G0 ITEM COMMIT: success=" .. tostring(success)
    .. "; action_changed=" .. tostring(changed) .. "; history_changed=" .. tostring(recorded))
tile.ground.actionId = 12007
assert(restored and historyUnchanged, "G0 ITEM ROLLBACK FAIL: item attribute survived the failed transaction")
assert(success and changed and recorded, "G0 ITEM COMMIT FAIL: item attribute changed without an Undo entry")
print("G0 ITEM ROLLBACK PASS")
