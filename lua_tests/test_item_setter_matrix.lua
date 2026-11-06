-- @Title: Test item setter matrix
-- @Description: Isolated g0-metadata.otbm with 1098-BlackTek and empty history. Never save.
assert(app.hasMap() and app.map.hasFile and app.map.name == "g0-metadata.otbm")
local map, editor = app.map, app.editor
assert(editor.historySize == 0, "Reopen the isolated fixture first")
local function tile() return assert(map:getTile(100, 100, 7)) end
assert(tile().itemCount == 0, "Fixture stack must be empty")
local info = assert(Items.getInfo(2148))
assert(info.isStackable and not info.isGroundTile, "Unexpected test client")
assert(Items.exists(1987), "Missing bag fixture")

app.transaction("setter matrix fixture", function()
    assert(tile():addItem(2148, 3))
    assert(tile():addItem(1987, 1))
end)
local function stackItem(id)
    for _, item in ipairs(tile().items) do
        if item.id == id then return item end
    end
    error("Missing fixture item " .. id)
end
local owners = {
    {name = "ground", get = function() return assert(map:getTile(160,160,7).ground) end},
    {name = "stack", get = function() return stackItem(2148) end},
    {name = "container-root", get = function() return stackItem(1987) end},
}
local properties = {"actionId", "uniqueId", "tier", "text", "description"}
local tested = 0
for _, owner in ipairs(owners) do
    local fields = {}
    for _, field in ipairs(properties) do fields[#fields + 1] = field end
    if owner.name == "stack" then
        fields[#fields + 1] = "count"
        fields[#fields + 1] = "subtype"
    end
    for _, field in ipairs(fields) do
        local original = owner.get()[field]
        local changed = type(original) == "string" and (original .. " G0 matrix") or (original == 42 and 43 or 42)
        local index, size = editor.historyIndex, editor.historySize
        local captured = owner.get() -- Deliberately captured before begin().
        local ok, err = pcall(function()
            app.transaction("setter matrix rollback", function()
                captured[field] = changed
                assert(owner.get()[field] == changed, "Setter did not apply")
                error("EXPECTED_MATRIX_ROLLBACK")
            end)
        end)
        assert(not ok and tostring(err):find("EXPECTED_MATRIX_ROLLBACK", 1, true))
        assert(owner.get()[field] == original and editor.historyIndex == index and editor.historySize == size,
            owner.name .. "." .. field .. " rollback/history mismatch")

        app.transaction("setter matrix commit", function() owner.get()[field] = changed end)
        assert(owner.get()[field] == changed and editor.historyIndex == index + 1,
            owner.name .. "." .. field .. " commit mismatch")
        editor:undo()
        assert(owner.get()[field] == original and editor.historyIndex == index)
        editor:redo()
        assert(owner.get()[field] == changed and editor.historyIndex == index + 1)
        editor:undo()
        assert(owner.get()[field] == original and editor.historyIndex == index)

        -- The same setter without an explicit transaction must have its own Undo.
        owner.get()[field] = changed
        assert(owner.get()[field] == changed and editor.historyIndex == index + 1)
        editor:undo()
        assert(owner.get()[field] == original and editor.historyIndex == index)
        editor:redo()
        assert(owner.get()[field] == changed)
        editor:undo()
        assert(owner.get()[field] == original and editor.historyIndex == index)
        tested = tested + 1
        print("SETTER CASE PASS: " .. owner.name .. "." .. field)
    end
end

local index, size = editor.historyIndex, editor.historySize
local invalid = 0
for _, field in ipairs({"actionId", "uniqueId", "tier", "count", "subtype"}) do
    for _, value in ipairs({-1, 65536}) do
        local original = stackItem(2148)[field]
        assert(not pcall(function() stackItem(2148)[field] = value end), "Invalid setter accepted")
        assert(stackItem(2148)[field] == original and editor.historyIndex == index and editor.historySize == size)
        invalid = invalid + 1
    end
end
assert(not pcall(function() stackItem(2148):setPodiumOutfit(1) end), "Non-podium outfit accepted")
assert(editor.historyIndex == index and editor.historySize == size)
editor:undo() -- Remove only the temporary stack and container fixture.
assert(tile().itemCount == 0 and editor.historyIndex == 0, "Fixture was not restored")
assert(map:getTile(160,160,7).ground.actionId == 12007
    and map:getTile(160,160,7).ground.uniqueId == 12107)
print("ITEM SETTER MATRIX PASS: " .. tested .. " cases, " .. invalid .. " invalid numeric values; fixture restored")
