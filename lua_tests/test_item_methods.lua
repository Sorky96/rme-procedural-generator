-- @Title: Test item methods on a private synthetic profile
-- @Description: Run only through g0_rme_item_methods_probe; never save this map.
assert(g0_methods and app.hasMap() and app.map.name == "house-roundtrip.otbm")
local map, editor = app.map, app.editor
assert(editor.historyIndex == 0 and editor.historySize == 0)
local function tile() return assert(map:getTile(110, 110, 7)) end
assert(tile().itemCount == 0)
assert(Items.getInfo(1443).isPodium, "Private XML definition was not loaded")
app.transaction("method fixture", function()
    assert(tile():addItem(1442, 1))
    assert(tile():addItem(1443, 1))
end)
local function item(id)
    for _, value in ipairs(tile().items) do if value.id == id then return value end end
    error("Missing method-test item " .. id)
end
local function equal(actual, expected)
    for key, value in pairs(expected) do assert(actual[key] == value, "State mismatch: " .. key) end
    for key in pairs(actual) do assert(expected[key] ~= nil, "Unexpected state field " .. key) end
end
local function cycle(name, get, change, before, after)
    local index, size = editor.historyIndex, editor.historySize
    local captured = get()
    local ok, err = pcall(function()
        app.transaction(name .. " rollback", function()
            change(captured)
            after()
            error("EXPECTED_METHOD_ROLLBACK")
        end)
    end)
    assert(not ok and tostring(err):find("EXPECTED_METHOD_ROLLBACK", 1, true))
    before()
    assert(editor.historyIndex == index and editor.historySize == size)
    app.transaction(name .. " commit", function() change(get()) end)
    after()
    assert(editor.historyIndex == index + 1 and editor.historySize == index + 1)
    editor:undo(); before(); assert(editor.historyIndex == index)
    editor:redo(); after(); assert(editor.historyIndex == index + 1)
    editor:undo(); before(); assert(editor.historyIndex == index)
    change(get()) -- The public method must also create its own implicit action.
    after(); assert(editor.historyIndex == index + 1 and editor.historySize == index + 1)
    editor:undo(); before(); assert(editor.historyIndex == index)
    editor:redo(); after(); assert(editor.historyIndex == index + 1)
    editor:undo(); before(); assert(editor.historyIndex == index)
    print("ITEM_METHOD_CASE_PASS\t" .. name)
end
cycle("rotate", function() return item(1442) end, function(value) value:rotate() end,
    function() assert(item(1442).id == 1442) end,
    function() assert(item(1476).id == 1476) end)
local original = g0_methods.podiumState()
assert(original.lookType == 0 and original.direction == 0)
local function changed(look, direction)
    local result = {}
    for key, value in pairs(original) do result[key] = value end
    result.lookType, result.direction = look, direction
    return result
end
cycle("podium-direction", function() return item(1443) end,
    function(value) value:setPodiumOutfit(128, 3) end,
    function() equal(g0_methods.podiumState(), original) end,
    function() equal(g0_methods.podiumState(), changed(128, 3)) end)
-- A nonzero baseline proves the optional argument preserves direction.
app.transaction("podium baseline", function() item(1443):setPodiumOutfit(128, 3) end)
cycle("podium-optional-direction", function() return item(1443) end,
    function(value) value:setPodiumOutfit(129) end,
    function() equal(g0_methods.podiumState(), changed(128, 3)) end,
    function() equal(g0_methods.podiumState(), changed(129, 3)) end)
local index, size = editor.historyIndex, editor.historySize
for _, args in ipairs({{-1}, {128, -1}, {128, 256}}) do
    assert(not pcall(function() item(1443):setPodiumOutfit(table.unpack(args)) end))
    equal(g0_methods.podiumState(), changed(128, 3))
    assert(editor.historyIndex == index and editor.historySize == size)
end
assert(not pcall(function() item(1442):setPodiumOutfit(128) end))
assert(editor.historyIndex == index and editor.historySize == size)
print("ITEM_METHOD_INVALID_PASS")
editor:undo() -- Nonzero podium baseline.
equal(g0_methods.podiumState(), original)
editor:undo() -- Both temporary items.
assert(editor.historyIndex == 0 and tile().itemCount == 0 and map.tileCount == 402)
print("ITEM_METHOD_RESTORE_PASS")
