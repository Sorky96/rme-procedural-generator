// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_learning.h"
#include <algorithm>
#include <fstream>
#include <format>
#include <numeric>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace rme::procedural {
std::expected<LearningSample, std::string> inspectLearningSample(const SemanticMap& map,
    std::span<const KeyMapEntry> mapping, bool permission, std::stop_token stop) try {
    if (!permission) return std::unexpected("Confirm your right to use this map before analysis.");
    if (stop.stop_requested()) return std::unexpected("Analysis cancelled.");
    if (map.floors().size() != 1) return std::unexpected("Choose one surface rectangle.");
    const auto& b = map.floors().front().bounds;
    if (b.z < 0 || b.z > 7 || b.width > 512 || b.height > 512)
        return std::unexpected("Choose a surface floor 0-7 and a rectangle up to 512 x 512.");
    for (const auto& entry : mapping)
        if (size_t(entry.kind) >= styleKindCount || (entry.key.empty() && entry.kind != StyleKind::NoGround))
            return std::unexpected("Unresolved ground must remain no-ground; use valid G4 classes for named brushes.");
    if (!serializeKeyMap(mapping)) return std::unexpected("Invalid or duplicate mapping keys.");
    auto stats = accumulateKindStats(map, mapping, b.z, b.z, stop);
    if (!stats) return std::unexpected("Invalid mapping or analysis cancelled.");
    LearningSample sample;
    sample.stats = *stats;
    sample.present = std::accumulate(stats->kindTiles.begin(), stats->kindTiles.end(), uint64_t{0});
    sample.recognized = std::accumulate(stats->kindTiles.begin(), stats->kindTiles.begin() + naturalKindCount, uint64_t{0});
    if (sample.recognized < 1024 || sample.recognized * 2 < sample.present)
        return std::unexpected(std::format("{} recognized, {} skipped, {} present. Need at least 1024 natural tiles and 50% coverage.",
            sample.recognized, sample.present - sample.recognized, sample.present));
    std::vector<KeyTiles> keys;
    const auto summary = summarize(map);
    for (size_t i = 0; i < map.groundKeys().size(); ++i)
        keys.push_back({map.groundKeys()[i], summary.groundKeyStats[i].tiles});
    sample.palette = choosePalette(mapping, keys);
    return sample;
} catch (const std::bad_alloc&) { return std::unexpected("Not enough memory for analysis."); }

std::expected<StyleProfile, std::string> fitLearningSample(const LearningSample& sample,
    std::vector<KeyMapEntry> mapping, std::string client, std::string otb, std::string dat, std::stop_token stop) {
    if (sample.recognized < 1024 || sample.recognized * 2 < sample.present)
        return std::unexpected("Insufficient sample coverage.");
    const auto target = targetFromStats(sample.stats);
    if (!target) return std::unexpected("No natural neighbour pairs in the sample.");
    FitOptions options;
    options.eval = {160, {1, 2, 3, 4, 5, 6}};
    options.maxPasses = 4;
    auto steps = fitTuning(*target, *target, SurfaceTuning{}, options, stop);
    if (!steps) return std::unexpected(stop.stop_requested() ? "Analysis cancelled." : "Surface fit failed.");
    StyleProfile profile;
    profile.client = std::move(client); profile.otbSha256 = std::move(otb); profile.datSha256 = std::move(dat);
    // Generic provenance: never persist map names, paths, coordinates, or reference aliases.
    profile.sources = {{"user-local-surface", 1000}};
    profile.keymap = std::move(mapping); profile.stats = sample.stats; profile.target = target;
    profile.tuning = steps->back().tuning; profile.palette = sample.palette;
    if (!validateProfile(profile)) return std::unexpected("The learned profile is invalid.");
    return profile;
}

bool validLearnedProfileName(std::string_view name) {
    if (name.empty() || name.size() > 64) return false;
    std::string upper;
    for (const unsigned char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
        upper += char(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
    }
    if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL") return false;
    if (upper.size() == 4 && (upper.starts_with("COM") || upper.starts_with("LPT")) && upper[3] >= '1' && upper[3] <= '9') return false;
    return true;
}

std::expected<void, std::string> saveLearnedProfile(const std::filesystem::path& directory,
    std::string_view name, const StyleProfile& profile) {
    if (!validLearnedProfileName(name)) return std::unexpected("Use 1-64 letters, digits, '-' or '_'; no paths or device names.");
    const auto text = serializeProfile(profile);
    if (!text) return std::unexpected("Invalid profile; nothing saved.");
    if (text->size() > (1u << 20)) return std::unexpected("Profile exceeds the generator's 1 MiB read limit; nothing saved.");
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) return std::unexpected("Cannot create the local profile directory.");
    const auto path = directory / (std::string(name) + ".profile");
    const auto lock = directory / ".learning-save.lock";
    if (!std::filesystem::create_directory(lock, ec)) return std::unexpected("Profile folder is busy or unavailable. Retry after the other save finishes.");
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code e; std::filesystem::remove_all(path, e); } } cleanup{lock};
    size_t profiles = 0;
    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().extension() == ".profile" && it->is_regular_file(ec)) ++profiles;
    }
    if (ec) return std::unexpected("Cannot inspect the profile directory.");
    if (profiles >= 64) return std::unexpected("The generator reads up to 64 profile files. Archive unused profiles before saving another.");
    const auto temporary = lock / "pending";
#ifdef _WIN32
    struct File {
        HANDLE handle;
        ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    };
    bool written = false;
    {
        File file{CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (file.handle == INVALID_HANDLE_VALUE) return std::unexpected("Cannot create temporary profile.");
        DWORD bytes = 0;
        written = WriteFile(file.handle, text->data(), static_cast<DWORD>(text->size()), &bytes, nullptr)
            && bytes == text->size() && FlushFileBuffers(file.handle);
    }
    // Without REPLACE_EXISTING: destination races are refused as well as existing files.
    if (!written || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH))
        return std::unexpected("Profile not saved (write error or name already exists). Choose another name.");
#else
    { std::ofstream out(temporary, std::ios::binary); out << *text; out.flush();
      if (!out) return std::unexpected("Cannot write profile."); }
    std::filesystem::create_hard_link(temporary, path, ec);
    if (ec) return std::unexpected("Cannot publish profile; name may already exist.");
#endif
    return {};
}
} // namespace rme::procedural
