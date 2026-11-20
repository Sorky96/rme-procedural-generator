// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_learning.h"
#include "procedural/core/doodad_class.h"
#include <algorithm>
#include <fstream>
#include <format>
#include <map>
#include <optional>
#include <numeric>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace rme::procedural {
namespace {
// Smallest share a second brush needs to be worth a mix; rarer ones read as noise, not style.
constexpr uint64_t minMixPermille = 50;

uint32_t permilleOf(uint64_t part, uint64_t whole) { return whole ? uint32_t(std::min<uint64_t>(1000, part * 1000 / whole)) : 0; }

// The most used candidates first, each with its share of `total`, while the share reaches minMixPermille.
template <size_t N>
std::array<StyleMix, N> topMix(std::vector<const KeyTiles*> candidates, uint64_t total) {
    std::ranges::sort(candidates, [](const KeyTiles* a, const KeyTiles* b) { return a->tiles != b->tiles ? a->tiles > b->tiles : a->key < b->key; });
    std::array<StyleMix, N> mix;
    size_t used = 0;
    for (const auto* t : candidates) {
        const uint32_t share = permilleOf(t->tiles, total);
        if (used == N || share < minMixPermille) break;
        mix[used++] = {t->key, share};
    }
    return mix;
}

// Named grass brushes after the palette's one (jungle grass, swamp mapped to Grass), each with its share
// of all grass tiles.
std::array<StyleMix, maxGrassMix> chooseGrassMix(std::span<const KeyMapEntry> mapping, std::span<const KeyTiles> keys, const std::string& primary) {
    if (primary.empty()) return {};
    uint64_t total = 0;
    std::vector<const KeyTiles*> candidates;
    for (const auto& t : keys) {
        const auto entry = std::ranges::find(mapping, t.key, &KeyMapEntry::key);
        if (entry == mapping.end() || entry->kind != StyleKind::Grass) continue;
        total += t.tiles;
        if (t.key != primary && !t.key.starts_with("item ")) candidates.push_back(&t);
    }
    return topMix<maxGrassMix>(std::move(candidates), total);
}

// Tree brushes after the palette's one, each with its share of all tree items.
std::array<StyleMix, maxTreeMix> chooseTreeMix(std::span<const KeyTiles> items, const std::string& primary) {
    if (primary.empty()) return {};
    std::vector<const KeyTiles*> candidates;
    uint64_t total = 0;
    for (const auto& t : items) {
        if (classifyDoodadBrush(t.key) != DecorKind::Tree) continue;
        total += t.tiles;
        if (t.key != primary) candidates.push_back(&t);
    }
    return topMix<maxTreeMix>(std::move(candidates), total);
}
} // namespace

std::expected<LearningSample, std::string> inspectLearningSample(const SemanticMap& map,
    std::span<const KeyMapEntry> mapping, std::span<const GroundDoodadItems> doodadItems, bool permission, std::stop_token stop) try {
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
    auto decor = extractDecorContext(map, mapping, b.z, b.z, stop);
    if (!decor) return std::unexpected("Invalid mapping or analysis cancelled.");
    sample.decor = *decor;
    sample.grassMix = chooseGrassMix(mapping, keys, sample.palette[size_t(StyleKind::Grass)]);
    // Ground key of each mix brush; its tiles keep their own vegetation, the rest is the main style.
    std::array<std::optional<uint16_t>, maxGrassMix> mixKey;
    for (size_t m = 0; m < maxGrassMix; ++m)
        for (size_t k = 0; k < map.groundKeys().size(); ++k)
            if (!sample.grassMix[m].brush.empty() && map.groundKeys()[k] == sample.grassMix[m].brush) mixKey[m] = uint16_t(k);
    const auto slotOf = [&](uint16_t ground) -> std::optional<size_t> {
        for (size_t m = 0; m < maxGrassMix; ++m) if (mixKey[m] == ground) return m;
        return std::nullopt;
    };
    // Generated decoration stands on land, so brushes placed on water never stand in for it. Mixed
    // forests rarely give one brush a quarter of the trees; the most used one still beats the built-in.
    std::map<std::string, uint64_t, std::less<>> mainItems;
    std::array<std::map<std::string, uint64_t, std::less<>>, maxGrassMix> mixItems;
    for (const auto& item : doodadItems) {
        if (item.doodad.find("water") != std::string::npos) continue;
        const auto slot = slotOf(item.ground);
        (slot ? mixItems[*slot] : mainItems)[item.doodad] += item.items;
    }
    const auto asKeyTiles = [](const auto& counts) {
        std::vector<KeyTiles> out;
        for (const auto& [name, items] : counts) out.push_back({name, items});
        return out;
    };
    const auto landItems = asKeyTiles(mainItems);
    sample.decorPalette = chooseDecorPalette(landItems, 100, 0);
    sample.treeMix = chooseTreeMix(landItems, sample.decorPalette[0]);
    // Per-tile rates of trees, bushes and stones on the main grass and on each mix ground.
    const auto ratesOn = [&](uint16_t ground) {
        std::array<uint64_t, 4> counts {}; // tiles, trees, bushes, stones
        const auto& floor = map.floors().front();
        for (size_t i = 0; i < floor.ground.size(); ++i) {
            if (floor.ground[i] != ground || !(floor.traits[i] & bit(Trait::Present))) continue;
            ++counts[0];
            counts[1] += (floor.traits[i] & bit(Trait::TreeDoodad)) != 0;
            counts[2] += (floor.traits[i] & bit(Trait::BushDoodad)) != 0;
            counts[3] += (floor.traits[i] & bit(Trait::StoneDoodad)) != 0;
        }
        return counts;
    };
    std::optional<uint16_t> mainKey;
    for (size_t k = 0; k < map.groundKeys().size(); ++k)
        if (map.groundKeys()[k] == sample.palette[size_t(StyleKind::Grass)]) mainKey = uint16_t(k);
    const auto base = mainKey ? ratesOn(*mainKey) : std::array<uint64_t, 4> {};
    for (size_t m = 0; m < maxGrassMix; ++m) {
        if (!mixKey[m]) continue;
        const auto own = ratesOn(*mixKey[m]);
        auto& d = sample.grassMixDecor[m];
        for (size_t k = 0; k < 3; ++k) {
            // keep = (own rate) / (base rate), both per tile; a base rate of 0 has nothing to thin.
            if (own[0] && base[0] && base[1 + k]) d.keepPermille[k] = uint32_t(std::min<uint64_t>(1000, own[1 + k] * base[0] * 1000 / (own[0] * base[1 + k])));
        }
        const auto palette = chooseDecorPalette(asKeyTiles(mixItems[m]), 10, 0);
        for (size_t k = 0; k < 3; ++k) if (palette[k] != sample.decorPalette[k]) d.brushes[k] = palette[k];
    }
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
    // A kind the sample does not contain at all is not generated: the fit only scores shares and may
    // keep some rock or dirt that improves the neighbour pairs, which reads as foreign terrain.
    if (target->sharesPpm[size_t(StyleKind::Rock)] == 0) { profile.tuning->rockPermille = 0; profile.tuning->riverCount = 0; }
    if (target->sharesPpm[size_t(StyleKind::Dirt)] == 0) profile.tuning->dirtPermille = 0;
    // The generator always rings water with Shore; a sample without any shore meets water with grass,
    // so the ring takes the grass brush instead of the built-in sand.
    auto& shore = profile.palette[size_t(StyleKind::Shore)];
    if (target->sharesPpm[size_t(StyleKind::Shore)] == 0 && shore.empty()) shore = profile.palette[size_t(StyleKind::Grass)];
    const MapDecor own{"user-local-surface", "user-local-surface", sample.decor};
    profile.decorTarget = buildDecorTarget(std::span(&own, 1));
    auto decorTuning = fitDecorTuning(*profile.decorTarget, *profile.tuning, options.eval, stop);
    if (!decorTuning) return std::unexpected(stop.stop_requested() ? "Analysis cancelled." : "Vegetation fit failed.");
    profile.decorTuning = *decorTuning;
    profile.decorPalette = sample.decorPalette;
    profile.grassMix = sample.grassMix;
    profile.grassMixDecor = sample.grassMixDecor;
    profile.treeMix = sample.treeMix;
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
