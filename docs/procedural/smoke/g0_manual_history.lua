-- @Title: G0 manual draw and Lua history
-- @Description: Run on the synthetic g0-smoke.otbm fixture after each of two manual brush strokes. Never save.
assert(app.hasMap() and app.map.name == "g0-smoke.otbm", "Use the synthetic fixture")
local map, editor = app.map, app.editor
local grass = Brushes.get("grass")
assert(grass and grass.lookId > 0, "The fixture requires grass")
local function snapshot()
    local result = {}
    for y = 98, 121 do
        for x = 98, 121 do
            local tile = map:getTile(x, y, 7)
            local value = "absent"
            if tile then
                local ids = {}
                for _, item in ipairs(tile.items) do ids[#ids + 1] = tostring(item.id) end
                value = table.concat({"tile", tile.hasGround and tile.ground.id or 0,
                    table.concat(ids, ","), tile.houseId, tile.mapFlags,
                    tostring(tile.hasCreature), tostring(tile.hasSpawn)}, ":")
            end
            result[#result + 1] = value
        end
    end
    return table.concat(result, "|")
end
local function original()
    local result = {}
    for y = 98, 121 do
        for x = 98, 121 do
            local inside = x >= 100 and x <= 119 and y >= 100 and y <= 119
            result[#result + 1] = inside and ("tile:" .. grass.lookId .. "::0:0:false:false") or "absent"
        end
    end
    return table.concat(result, "|")
end
local function hasSentinel()
    local tile = map:getTile(150, 150, 7)
    return tile and tile.hasGround and tile.ground.id == grass.lookId or false
end
if not G0_MANUAL_STAGE then
    assert(editor.historySize == 1 and editor:getHistory()[1].name == "Draw", "Expected one initial manual Draw action")
    assert(snapshot() ~= original(), "Initial manual stroke made no visible data change")
    assert(not map:getTile(150, 150, 7), "Sentinel location occupied")
    G0_MANUAL_FIRST = snapshot()
    app.transaction("G0 mixed history Lua", function()
        map:getOrCreateTile(150, 150, 7).ground = grass.lookId
    end)
    assert(editor.historySize == 2 and hasSentinel(), "Lua transaction did not create its own history entry")
    assert(snapshot() == G0_MANUAL_FIRST, "Lua modified the manual area")
    G0_MANUAL_STAGE = 1
    print("G0 MIXED STAGE 1 PASS: manual Draw and Lua transaction recorded. Draw a second brush stroke, then run again.")
else
    assert(G0_MANUAL_STAGE == 1 and editor.historySize == 3, "Expected three separate actions")
    local history = editor:getHistory()
    assert(history[1].name == "Draw" and history[2].name == "G0 mixed history Lua" and history[3].name == "Draw", "Unexpected history sequence")
    local second = snapshot()
    assert(second ~= G0_MANUAL_FIRST and hasSentinel(), "Second manual stroke missing")
    editor:undo()
    assert(snapshot() == G0_MANUAL_FIRST and hasSentinel(), "Undo merged later manual stroke with Lua")
    editor:undo()
    assert(snapshot() == G0_MANUAL_FIRST and not hasSentinel(), "Undo of Lua affected manual stroke")
    editor:undo()
    assert(snapshot() == original() and not hasSentinel(), "Undo of initial manual stroke did not restore fixture")
    editor:redo()
    assert(snapshot() == G0_MANUAL_FIRST and not hasSentinel(), "Redo of initial manual stroke failed")
    editor:redo()
    assert(snapshot() == G0_MANUAL_FIRST and hasSentinel(), "Redo of Lua transaction failed")
    editor:redo()
    assert(snapshot() == second and hasSentinel(), "Redo of second manual stroke failed")
    editor:undo(); editor:undo(); editor:undo()
    assert(snapshot() == original() and not hasSentinel(), "Cleanup did not restore fixture")
    G0_MANUAL_STAGE = 2
    print("G0 MIXED HISTORY PASS: Draw / Lua / Draw remained separate across Undo and Redo; fixture restored.")
end
