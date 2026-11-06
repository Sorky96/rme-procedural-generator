-- @Title: G0 algo geo probe
-- @Description: Read-only seeded cave and geometry checks.

local options = { seed = 1729, fillProbability = 0.45, iterations = 2 }
local cave = algo.generateCave(20, 20, options)
local repeatCave = algo.generateCave(20, 20, options)
local other = algo.generateCave(20, 20,
    { seed = 1730, fillProbability = 0.45, iterations = 2 })
local differences = 0
assert(#cave == 20, "Unexpected cave height")
for y = 1, 20 do
    assert(#cave[y] == 20, "Unexpected cave width")
    for x = 1, 20 do
        local value = cave[y][x]
        assert(value == 0 or value == 1, "Unexpected cave cell")
        assert(value == repeatCave[y][x], "Cave seed is not repeatable")
        if x == 1 or x == 20 or y == 1 or y == 20 then
            assert(value == 1, "Cave edge is not a wall")
        end
        if value ~= other[y][x] then differences = differences + 1 end
    end
end
assert(differences > 0, "Different cave seeds produced identical samples")

local line = geo.bresenhamLine(2, 3, 6, 5)
assert(#line == 5, "Unexpected line length")
assert(line[1].x == 2 and line[1].y == 3, "Line start mismatch")
assert(line[5].x == 6 and line[5].y == 5, "Line end mismatch")
for i = 2, #line do
    local dx = math.abs(line[i].x - line[i - 1].x)
    local dy = math.abs(line[i].y - line[i - 1].y)
    assert(dx <= 1 and dy <= 1 and dx + dy > 0, "Line has a discontinuity")
end
print("G0 ALGO GEO PASS: seeded cave repeat, changed seed, boundary walls and connected line")
