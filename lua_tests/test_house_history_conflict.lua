-- @Title: Test house history conflict
-- @Description: On an isolated map, change house ID 1 to 2 and add a new house with ID 1.
assert(app.hasMap() and app.map.name == "house-roundtrip.otbm", "Open the isolated house fixture")
local editor = assert(app.editor)
local index = editor.historyIndex
assert(index > 0 and editor:canUndo(), "Create the house ID change before the conflict")
local ok, message = pcall(function() editor:undo() end)
assert(not ok and tostring(message):find("target ID is in use", 1, true), "Expected the undo conflict error")
assert(editor.historyIndex == index, "Failed undo changed the history position")
local jump_ok, jump_message = pcall(function() editor:goToHistory(index - 1) end)
assert(not jump_ok and tostring(jump_message):find("target ID is in use", 1, true), "Expected the history navigation conflict error")
assert(editor.historyIndex == index, "Failed history navigation changed the history position")
print("HOUSE HISTORY CONFLICT PASS: Lua undo and navigation kept the history position")
