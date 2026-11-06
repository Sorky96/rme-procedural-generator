-- @Title: G0 noise probe
-- @Description: Read-only comparison of seeded scalar and grid noise.

local options = { seed = 1729, frequency = 0.037, noiseType = "simplex" }
local first = noise.generateGrid(10, 20, 29, 39, options)
local repeatGrid = noise.generateGrid(10, 20, 29, 39, options)
local other = noise.generateGrid(10, 20, 29, 39,
    { seed = 1730, frequency = 0.037, noiseType = "simplex" })
local differences = 0
assert(#first == 20 and #first[1] == 20, "Unexpected grid dimensions")
for y = 1, 20 do
    assert(#first[y] == 20 and #repeatGrid[y] == 20, "Unexpected row dimensions")
    for x = 1, 20 do
        local value = first[y][x]
        assert(value == repeatGrid[y][x], "Repeated seed produced different grid")
        assert(value == value and value >= -1 and value <= 1, "Noise outside expected range")
        assert(value == noise.simplex(x + 9, y + 19, 1729, 0.037), "Grid/scalar mismatch")
        if value ~= other[y][x] then differences = differences + 1 end
    end
end
assert(differences > 0, "Different seeds produced identical samples")
print("G0 NOISE PASS: 20x20 seeded simplex grid, scalar agreement, repeat and different seed; changed cells=" .. differences)
