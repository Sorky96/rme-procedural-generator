// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/decor_stats.h"
#include "procedural/core/decoration.h"
#include "procedural/core/generation.h"
#include "procedural/core/semantic_map.h"

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace rme::procedural {

// Versioned style profile (plan 4.1 step 5, 4.3: profile per client, level and assets). It stores
// the client and asset fingerprints it was learned with, which floors it covers, a key map from
// the client's ground brush names to coarse kinds (no item ids), and integer statistics projected
// through that map. A profile is refused for another client or other assets.

// Coarse ground kinds of the style profile. Water is not split into lake and river: the key does
// not say which, so that split belongs to shape features (D9) and the generator mapping (G4-C).
enum class StyleKind : uint8_t {
	Grass, Dirt, Rock, Shore, Water,
	Other,     // valid ground with no generator kind: floors, roofs, stairs, hazards, biome grounds
	Unmapped,  // no rule matched; reported, never guessed
	NoGround   // the tile has no ground brush
};
inline constexpr size_t styleKindCount = 8;

[[nodiscard]] const char* styleKindName(StyleKind kind) noexcept;
[[nodiscard]] bool parseStyleKind(std::string_view name, StyleKind& out) noexcept;

// A rule fires when `token` is one of the lowercase alphanumeric words of the brush name. Rules are
// tried in order, so qualifiers (roof, floor, hole) come before base grounds (grass, dirt).
struct KeyRule {
	std::string_view token;
	StyleKind kind;
};
[[nodiscard]] std::span<const KeyRule> defaultKeyRules() noexcept;

struct KeyMapEntry {
	std::string key;              // ground brush name; empty = no ground; no tab or newline
	StyleKind kind = StyleKind::Unmapped;
	std::string rule;             // token that decided it, empty when none or set by hand
	bool operator==(const KeyMapEntry&) const = default;
};

// Order of the output follows `keys`. The empty key is always NoGround; a name no rule matches is Unmapped.
[[nodiscard]] std::vector<KeyMapEntry> proposeKeyMap(std::span<const std::string> keys, std::span<const KeyRule> rules);

enum class ProfileError : uint8_t {
	BadFormat, UnsupportedSchema, UnknownKind, DuplicateKey, InvalidRange, EmptyField, InvalidKey
};

inline constexpr std::string_view keyMapSchema = "g4-keymap/1";

// Text form of a key map alone, reviewed and edited by hand: `g4-keymap/1`, then one line per key:
// key, kind, rule (or -), key text; a key may be empty but contains no tab or newline.
[[nodiscard]] std::expected<std::string, ProfileError> serializeKeyMap(std::span<const KeyMapEntry> keymap);
[[nodiscard]] std::expected<std::vector<KeyMapEntry>, ProfileError> parseKeyMap(std::string_view text);

enum class StyleLevel : uint8_t { Surface, Underground };

struct StyleStats {
	std::array<uint64_t, styleKindCount> kindTiles {};
	// pairs[a][b]: tiles of kind a with an existing east or south neighbour of kind b, counted once per pair.
	std::array<std::array<uint64_t, styleKindCount>, styleKindCount> pairs {};
	bool operator==(const StyleStats&) const = default;
};

struct ProfileSource {
	std::string alias;
	uint32_t weightPermille = 0;
	bool operator==(const ProfileSource&) const = default;
};

// The five natural kinds (Grass..Water) are the ones the surface generator can produce and a
// target is defined on. Other, Unmapped and NoGround are left out of shares and adjacency.
inline constexpr size_t naturalKindCount = 5;

// Distribution a generator is fitted to, in parts per million of natural tiles / natural neighbour pairs.
struct StyleTarget {
	std::array<uint32_t, naturalKindCount> sharesPpm {};
	// Symmetric: adjacencyPpm[a][b] == adjacencyPpm[b][a]; all entries sum to about 1 000 000.
	std::array<std::array<uint32_t, naturalKindCount>, naturalKindCount> adjacencyPpm {};
	bool operator==(const StyleTarget&) const = default;
};

// Target of one set of counts, or nullopt when it has no natural tile or no natural neighbour pair.
[[nodiscard]] std::optional<StyleTarget> targetFromStats(const StyleStats& stats) noexcept;

struct MapStats {
	std::string alias;
	std::string family;
	StyleStats stats;
};

struct TargetBuild {
	StyleTarget target;
	std::vector<ProfileSource> used;     // weightPermille of each map that counted, in input order
	std::vector<std::string> excluded;   // aliases with too few natural tiles or no pairs
	uint32_t familyCount = 0;
};

enum class TargetError : uint8_t { NothingUsable, AllocationFailed };

// Every family counts the same, every map inside a family the same, so a large map or a family with
// many maps does not dominate. A map counts only with at least minNaturalTiles natural tiles.
[[nodiscard]] std::expected<TargetBuild, TargetError> buildTarget(std::span<const MapStats> maps, uint64_t minNaturalTiles);

inline constexpr std::string_view statsSchema = "g4-kindstats/1";
[[nodiscard]] std::string serializeStats(const StyleStats& stats);
[[nodiscard]] std::expected<StyleStats, ProfileError> parseStats(std::string_view text);

// Ground brush a style used most for each natural kind, so that choosing the profile can also choose the
// brushes (winter: snowy mountain). Entry i belongs to StyleKind i (Grass..Water); empty = the style has no
// brush that clearly carries that kind (most of its tiles are ground items no brush owns, or too few tiles).
using StylePalette = std::array<std::string, naturalKindCount>;

struct KeyTiles {
	std::string key;
	uint64_t tiles = 0;
};

// Per kind (through `keymap`): the brushed key with the most tiles, but only when it has at least minTiles
// tiles and at least minSharePercent percent of all tiles of that kind. Keys that start with "item " are
// ground items without a brush and are never chosen, though they count towards the kind's total.
// `usable`, when given, lists the brush names this client's generator can place at all (some brushes, e.g.
// ones with outer borders, always draw outside the generated area and are refused); only those are chosen,
// though every tile still counts towards the kind's total.
[[nodiscard]] StylePalette choosePalette(std::span<const KeyMapEntry> keymap, std::span<const KeyTiles> tiles,
	uint64_t minTiles = 200, uint32_t minSharePercent = 25, const std::set<std::string>* usable = nullptr);

// Doodad brush a style used most for trees, bushes and stones (entry 0, 1, 2), by the name classification of
// classifyDoodadBrush; empty = none clearly carries that kind. `items` are surface items per doodad brush name.
// Same thresholds and `usable` rule as choosePalette; a brush that is not classified as that kind never counts.
using StyleDecorPalette = std::array<std::string, 3>;
[[nodiscard]] StyleDecorPalette chooseDecorPalette(std::span<const KeyTiles> items,
	uint64_t minItems = 100, uint32_t minSharePercent = 25, const std::set<std::string>* usable = nullptr);

inline constexpr std::string_view profileSchema = "g4-profile/6";

struct StyleMix {
	std::string brush;
	uint32_t permille = 0; // 1..1000 when brush is set, else 0
	bool operator==(const StyleMix&) const = default;
};
inline constexpr size_t maxTreeMix = 3, maxGrassMix = 3;

struct StyleProfile {
	std::string client;
	std::string otbSha256, datSha256;   // lowercase hex of the client's items.otb and Tibia.dat
	StyleLevel level = StyleLevel::Surface;
	int zMin = 0, zMax = 7;             // Tibia floors: 0..7 surface, 8..15 underground
	std::vector<ProfileSource> sources;
	std::vector<KeyMapEntry> keymap;
	StyleStats stats;                          // raw sums over the sources
	std::optional<StyleTarget> target;         // weighted over the sources
	std::optional<SurfaceTuning> tuning;       // generator parameters fitted to the target
	std::optional<DecorContext> decorTarget;   // vegetation rates by context, weighted over the sources (virtual units)
	std::optional<DecorTuning> decorTuning;    // vegetation chances fitted to decorTarget
	StylePalette palette;                      // brushes the style used (see StylePalette)
	StyleDecorPalette decorPalette;            // doodad brushes the style used (see StyleDecorPalette)
	// Learned mixes (empty brush = none): further grass brushes in patches, each covering `permille` of the grass,
	// and further tree brushes, each standing in for the tree brush on `permille` of the trees.
	std::array<StyleMix, maxGrassMix> grassMix;
	std::array<StyleMix, maxTreeMix> treeMix;
	bool operator==(const StyleProfile&) const = default;
};

// Refuses tab/newline in keys, duplicate keys, an unordered or out-of-range floor span and empty client/hashes.
[[nodiscard]] std::expected<void, ProfileError> validateProfile(const StyleProfile& profile);
[[nodiscard]] std::expected<std::string, ProfileError> serializeProfile(const StyleProfile& profile);
[[nodiscard]] std::expected<StyleProfile, ProfileError> parseProfile(std::string_view text);

enum class ProfileCompatibility : uint8_t { Compatible, WrongClient, WrongAssets, WrongLevel };

// `floorZ` is the floor the profile would be applied to.
[[nodiscard]] ProfileCompatibility checkProfile(const StyleProfile& profile, std::string_view client,
	std::string_view otbSha256, std::string_view datSha256, int floorZ) noexcept;

enum class StatsError : uint8_t { KeyNotMapped, InvalidRange, Cancelled, AllocationFailed };

// Counts tiles and neighbour pairs of the floors zMin..zMax through `keymap`. A ground key of the
// map that the key map does not list is an error, not Unmapped. Checks stop once per row; no allocation per tile.
[[nodiscard]] std::expected<StyleStats, StatsError> accumulateKindStats(
	const SemanticMap& map, std::span<const KeyMapEntry> keymap, int zMin, int zMax, std::stop_token stop);

} // namespace rme::procedural
