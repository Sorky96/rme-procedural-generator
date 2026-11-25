// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace rme::procedural {

// I59: named generator settings. A schema line, then "name<TAB>value" lines sorted by name, LF or CRLF.
inline constexpr std::string_view terrainPresetSchema = "terrain-preset/1";
inline constexpr size_t maxPresetEntries = 128;
inline constexpr size_t maxPresetValueBytes = 1024;
inline constexpr std::string_view terrainPresetExtension = ".preset";

struct TerrainPreset {
    std::map<std::string, std::string, std::less<>> values; // control name -> value
    bool operator==(const TerrainPreset&) const = default;
};

enum class PresetError { BadSchema, BadName, BadValue, Duplicate, Unsorted, TooMany, Truncated, TooLarge, AllocationFailed };
[[nodiscard]] const char* presetErrorText(PresetError error) noexcept;

// "terrain-" followed by 1-56 lower-case letters, digits or '-'.
[[nodiscard]] bool validPresetKey(std::string_view key) noexcept;
// Printable UTF-8 (no tab, newline or other control character), at most maxPresetValueBytes.
[[nodiscard]] bool validPresetValue(std::string_view value) noexcept;

[[nodiscard]] std::expected<std::string, PresetError> serializePreset(const TerrainPreset& preset);
[[nodiscard]] std::expected<TerrainPreset, PresetError> parsePreset(std::string_view text);

// Files `<name>.preset` in `directory` (names as validLearnedProfileName). Save writes a temporary file under a folder
// lock and renames it; an existing file is replaced only with `overwrite`. Errors are user-facing sentences.
enum class PresetSaveError { InvalidName, InvalidPreset, Exists, Busy, WriteFailed };
[[nodiscard]] std::expected<void, PresetSaveError> saveTerrainPreset(
    const std::filesystem::path& directory, std::string_view name, const TerrainPreset& preset, bool overwrite);
[[nodiscard]] const char* presetSaveErrorText(PresetSaveError error) noexcept;
[[nodiscard]] std::expected<TerrainPreset, PresetError> readTerrainPreset(const std::filesystem::path& path);
// Names (without extension) of the preset files in `directory`, sorted; at most 256.
[[nodiscard]] std::vector<std::string> listTerrainPresets(const std::filesystem::path& directory);
[[nodiscard]] bool deleteTerrainPreset(const std::filesystem::path& directory, std::string_view name);

// I64: a primary biome is a partial preset: terrain weights, the five ground brushes and the vegetation brushes.
// Brush names follow the bundled brush data; the dialog skips any the loaded client lacks.
struct BiomePreset {
    std::string_view name;
    TerrainPreset values;
};
[[nodiscard]] const std::vector<BiomePreset>& biomePresets();

} // namespace rme::procedural
