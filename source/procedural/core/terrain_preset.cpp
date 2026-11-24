// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/terrain_preset.h"
#include "procedural/core/style_learning.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <new>

namespace rme::procedural {

namespace {
constexpr std::string_view keyPrefix = "terrain-";
// Schema line plus the largest possible entries.
constexpr size_t maxPresetFileBytes = 64 + maxPresetEntries * (64 + 1 + maxPresetValueBytes + 2);
} // namespace

const char* presetErrorText(PresetError error) noexcept {
    switch (error) {
        case PresetError::BadSchema: return "not a terrain-preset/1 file";
        case PresetError::BadName: return "a setting has an invalid name";
        case PresetError::BadValue: return "a value has a tab, newline or control character or is too long";
        case PresetError::Duplicate: return "a setting appears twice";
        case PresetError::Unsorted: return "settings are not sorted by name";
        case PresetError::TooMany: return "more than 128 settings";
        case PresetError::Truncated: return "the file is truncated";
        case PresetError::TooLarge: return "the file is too large";
        case PresetError::AllocationFailed: return "not enough memory";
    }
    return "invalid preset";
}

bool validPresetKey(std::string_view key) noexcept {
    if (!key.starts_with(keyPrefix) || key.size() == keyPrefix.size() || key.size() > 64) return false;
    return std::ranges::all_of(key.substr(keyPrefix.size()), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
}

bool validPresetValue(std::string_view value) noexcept {
    return value.size() <= maxPresetValueBytes &&
        std::ranges::none_of(value, [](char c) { const auto u = static_cast<unsigned char>(c); return u < 0x20 || u == 0x7F; });
}

std::expected<std::string, PresetError> serializePreset(const TerrainPreset& preset) try {
    if (preset.values.size() > maxPresetEntries) return std::unexpected(PresetError::TooMany);
    std::string out(terrainPresetSchema);
    out += '\n';
    for (const auto& [key, value] : preset.values) {
        if (!validPresetKey(key)) return std::unexpected(PresetError::BadName);
        if (!validPresetValue(value)) return std::unexpected(PresetError::BadValue);
        out += key; out += '\t'; out += value; out += '\n';
    }
    return out;
} catch (const std::bad_alloc&) { return std::unexpected(PresetError::AllocationFailed); }

std::expected<TerrainPreset, PresetError> parsePreset(std::string_view text) try {
    if (text.size() > maxPresetFileBytes) return std::unexpected(PresetError::TooLarge);
    if (!text.ends_with('\n')) return std::unexpected(PresetError::Truncated);
    TerrainPreset preset;
    bool first = true;
    std::string_view previous;
    while (!text.empty()) {
        const size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text.remove_prefix(end + 1);
        if (line.ends_with('\r')) line.remove_suffix(1);
        if (first) {
            if (line != terrainPresetSchema) return std::unexpected(PresetError::BadSchema);
            first = false;
            continue;
        }
        const size_t tab = line.find('\t');
        if (tab == std::string_view::npos) return std::unexpected(PresetError::Truncated);
        const auto key = line.substr(0, tab), value = line.substr(tab + 1);
        if (!validPresetKey(key)) return std::unexpected(PresetError::BadName);
        if (!validPresetValue(value)) return std::unexpected(PresetError::BadValue);
        if (key == previous) return std::unexpected(PresetError::Duplicate);
        if (!previous.empty() && key < previous) return std::unexpected(PresetError::Unsorted);
        if (preset.values.size() == maxPresetEntries) return std::unexpected(PresetError::TooMany);
        preset.values.emplace(std::string(key), std::string(value));
        previous = key;
    }
    if (first) return std::unexpected(PresetError::BadSchema);
    return preset;
} catch (const std::bad_alloc&) { return std::unexpected(PresetError::AllocationFailed); }

const char* presetSaveErrorText(PresetSaveError error) noexcept {
    switch (error) {
        case PresetSaveError::InvalidName: return "Use 1-64 letters, digits, '-' or '_' for the preset name; no paths or device names.";
        case PresetSaveError::InvalidPreset: return "The settings cannot be stored as a preset; nothing saved.";
        case PresetSaveError::Exists: return "A preset with this name exists.";
        case PresetSaveError::Busy: return "The preset folder is busy. Retry after the other save finishes.";
        case PresetSaveError::WriteFailed: return "Cannot write the preset; any previous file is unchanged.";
    }
    return "Cannot save the preset.";
}

std::expected<void, PresetSaveError> saveTerrainPreset(
    const std::filesystem::path& directory, std::string_view name, const TerrainPreset& preset, bool overwrite) {
    if (!validLearnedProfileName(name)) return std::unexpected(PresetSaveError::InvalidName);
    const auto text = serializePreset(preset);
    if (!text) return std::unexpected(PresetSaveError::InvalidPreset);
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) return std::unexpected(PresetSaveError::WriteFailed);
    const auto lock = directory / ".preset-save.lock";
    if (!std::filesystem::create_directory(lock, ec)) return std::unexpected(PresetSaveError::Busy);
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code e; std::filesystem::remove_all(path, e); } } cleanup {lock};
    const auto path = directory / (std::string(name) + std::string(terrainPresetExtension));
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) return std::unexpected(PresetSaveError::WriteFailed);
    if (exists && !overwrite) return std::unexpected(PresetSaveError::Exists);
    const auto temporary = lock / "pending";
    {
        std::ofstream out(temporary, std::ios::binary);
        out.write(text->data(), std::streamsize(text->size()));
        out.flush();
        if (!out) return std::unexpected(PresetSaveError::WriteFailed);
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) return std::unexpected(PresetSaveError::WriteFailed);
    return {};
}

std::expected<TerrainPreset, PresetError> readTerrainPreset(const std::filesystem::path& path) try {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) return std::unexpected(PresetError::Truncated);
    if (size > maxPresetFileBytes) return std::unexpected(PresetError::TooLarge);
    std::ifstream in(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!in.eof() && in.fail()) return std::unexpected(PresetError::Truncated);
    return parsePreset(text);
} catch (const std::bad_alloc&) { return std::unexpected(PresetError::AllocationFailed); }

std::vector<std::string> listTerrainPresets(const std::filesystem::path& directory) {
    std::vector<std::string> names;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end && names.size() < 256; it.increment(ec)) {
        if (it->path().extension() != terrainPresetExtension || !it->is_regular_file(ec)) continue;
        auto name = it->path().stem().string();
        if (validLearnedProfileName(name)) names.push_back(std::move(name));
    }
    std::ranges::sort(names);
    return names;
}

bool deleteTerrainPreset(const std::filesystem::path& directory, std::string_view name) {
    if (!validLearnedProfileName(name)) return false;
    std::error_code ec;
    return std::filesystem::remove(directory / (std::string(name) + std::string(terrainPresetExtension)), ec) && !ec;
}

} // namespace rme::procedural
