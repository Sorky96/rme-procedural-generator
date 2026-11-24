// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/terrain_preset.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace rme::procedural;

namespace {

void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

const std::string header = std::string(terrainPresetSchema) + "\n";

bool refused(std::string_view text, PresetError error) {
    const auto parsed = parsePreset(text);
    return !parsed && parsed.error() == error;
}

void format() {
    TerrainPreset preset;
    preset.values["terrain-width"] = "256";
    preset.values["terrain-grass-brush"] = "grass";
    preset.values["terrain-seed"] = "18446744073709551615";
    preset.values["terrain-profile"] = "Zw\xC3\xB3l desert"; // UTF-8 and spaces are kept
    const auto text = serializePreset(preset);
    require(text && *text == header + "terrain-grass-brush\tgrass\nterrain-profile\tZw\xC3\xB3l desert\n"
        "terrain-seed\t18446744073709551615\nterrain-width\t256\n", "sorted output");
    require(parsePreset(*text) == preset, "round trip");
    std::string crlf;
    for (const char c : *text) { if (c == '\n') crlf += '\r'; crlf += c; }
    require(parsePreset(crlf) == preset, "CRLF reads like LF");
    require(parsePreset(header) && parsePreset(header)->values.empty(), "an empty preset is valid");
    require(parsePreset(header + "terrain-seed\t\n")->values.at("terrain-seed").empty(), "an empty value is valid");

    require(refused("terrain-preset/2\n", PresetError::BadSchema), "wrong schema");
    require(refused("", PresetError::Truncated), "empty file");
    require(refused(header + "terrain-seed\t1", PresetError::Truncated), "missing final newline");
    require(refused(header + "terrain-seed 1\n", PresetError::Truncated), "line without a tab");
    require(refused(header + "terrain-seed\t1\nterrain-seed\t2\n", PresetError::Duplicate), "duplicate name");
    require(refused(header + "terrain-width\t1\nterrain-seed\t2\n", PresetError::Unsorted), "unsorted names");
    require(refused(header + "terrain-Seed\t1\n", PresetError::BadName), "upper-case name");
    require(refused(header + "other-seed\t1\n", PresetError::BadName), "name without the prefix");
    require(refused(header + "terrain-seed\t1\t2\n", PresetError::BadValue), "tab in a value");
    require(refused(header + "terrain-seed\t1\x01\n", PresetError::BadValue), "control character in a value");
    require(refused(header + "terrain-seed\t" + std::string(maxPresetValueBytes + 1, 'a') + "\n", PresetError::BadValue), "value over 1024 bytes");
    require(parsePreset(header + "terrain-seed\t" + std::string(maxPresetValueBytes, 'a') + "\n").has_value(), "value of 1024 bytes");

    std::string many = header;
    for (size_t i = 0; i <= maxPresetEntries; ++i) many += std::format("terrain-k{:03}\tv\n", i);
    require(refused(many, PresetError::TooMany), "more than 128 entries");
    TerrainPreset bad;
    bad.values["terrain-seed"] = "a\nb";
    require(serializePreset(bad).error() == PresetError::BadValue, "serialize refuses a newline");
    bad.values.clear();
    bad.values["../escape"] = "1";
    require(serializePreset(bad).error() == PresetError::BadName, "serialize refuses a bad name");
}

void files() {
    const auto dir = std::filesystem::temp_directory_path() / "rme-i59-preset-test";
    std::filesystem::remove_all(dir);
    TerrainPreset preset;
    preset.values["terrain-seed"] = "7";
    require(saveTerrainPreset(dir, "../escape", preset, true).error() == PresetSaveError::InvalidName, "path name refused");
    require(saveTerrainPreset(dir, "CON", preset, true).error() == PresetSaveError::InvalidName, "device name refused");
    require(saveTerrainPreset(dir, "hills", preset, false).has_value(), "save new");
    preset.values["terrain-seed"] = "8";
    require(saveTerrainPreset(dir, "hills", preset, false).error() == PresetSaveError::Exists, "no silent overwrite");
    require(readTerrainPreset(dir / "hills.preset")->values.at("terrain-seed") == "7", "refused overwrite keeps the file");
    require(saveTerrainPreset(dir, "hills", preset, true).has_value(), "overwrite when asked");
    require(readTerrainPreset(dir / "hills.preset")->values.at("terrain-seed") == "8", "overwritten");
    require(saveTerrainPreset(dir, "coast", preset, false).has_value(), "second preset");
    { std::ofstream(dir / "notes.txt") << "x"; }
    require(listTerrainPresets(dir) == std::vector<std::string> {"coast", "hills"}, "listing");
    std::filesystem::create_directory(dir / ".preset-save.lock");
    require(saveTerrainPreset(dir, "dunes", preset, false).error() == PresetSaveError::Busy, "a held lock refuses");
    std::filesystem::remove(dir / ".preset-save.lock");
    require(deleteTerrainPreset(dir, "coast") && !deleteTerrainPreset(dir, "coast"), "delete once");
    require(!deleteTerrainPreset(dir, "../hills"), "delete refuses paths");
    require(listTerrainPresets(dir) == std::vector<std::string> {"hills"}, "listing after delete");
    { std::ofstream(dir / "broken.preset", std::ios::binary) << "terrain-preset/1\nterrain-seed"; }
    require(readTerrainPreset(dir / "broken.preset").error() == PresetError::Truncated, "truncated file refused");
    std::filesystem::remove_all(dir);
}

} // namespace

int main() try {
    format();
    files();
    std::cout << "TERRAIN_PRESET_PASS\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAILED " << error.what() << '\n';
    return 1;
}
