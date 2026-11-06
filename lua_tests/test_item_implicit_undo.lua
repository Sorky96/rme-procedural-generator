-- @Title: Test implicit item undo
-- @Description: Run on isolated g0-metadata.otbm; do not save.
assert(app.hasMap() and app.map.name == "g0-metadata.otbm")
local map, editor = app.map, app.editor
local function ground() return assert(assert(map:getTile(160, 160, 7)).ground) end
local before = ground().actionId
local after = before == 12010 and 12011 or 12010
ground().actionId = after
assert(ground().actionId == after and editor:canUndo(), "Setter did not create an undoable change")
editor:undo()
assert(ground().actionId == before, "Undo did not restore actionId")
editor:redo()
assert(ground().actionId == after, "Redo did not restore actionId")
editor:undo()
assert(ground().actionId == before, "Fixture was not restored")
print("IMPLICIT ITEM UNDO PASS")
