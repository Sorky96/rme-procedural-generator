// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/similarity.h"

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rme::procedural {

// Reference manifest and the train/validation/test split by family of maps (plan 4.4).
// Whole maps are the unit, never neighbouring tiles of one map, so no spatial buffer is needed;
// instead maps that share content are merged into one family, measured with fingerprints.

enum class ReferenceRole : uint8_t {
	Style,   // may teach style: member of train / validation / test
	Sample,  // tileset samples and concepts: never teach or test style, kept as memorization controls
	Shape    // shape-only maps: same as Sample
};

[[nodiscard]] const char* referenceRoleName(ReferenceRole role) noexcept;

struct ManifestEntry {
	std::string alias;   // private stable name, no file names
	std::string sha256;  // of the OTBM file, lowercase hex
	std::string client;  // client id the map must be read with
	ReferenceRole role = ReferenceRole::Style;
	std::string family;  // declared family; entries sharing it are one family
};

struct Manifest {
	std::vector<ManifestEntry> entries;
};

enum class ManifestError : uint8_t { BadFormat, UnknownRole, DuplicateAlias, EmptyField, MixedClients };

inline constexpr std::string_view manifestSchema = "g4-manifest/1";

// Line format: first line `g4-manifest/1`; then one entry per line, tab separated:
// alias, sha256, client, role (style|sample|shape), family. Empty lines and lines starting with # are skipped.
// One manifest holds one client only (maps are analysed with the client they were made with).
[[nodiscard]] std::expected<Manifest, ManifestError> parseManifest(std::string_view text);
[[nodiscard]] std::string renderManifest(const Manifest& manifest);

struct FamilyMerge {
	size_t a = 0, b = 0;        // entry indices
	uint32_t permille = 0;      // Containment::maxPermille
};

struct FamilyOptions {
	uint32_t mergePermille = 100;  // containment (either direction) at or above this joins two families
	uint64_t minSharedWindows = 16; // ...but only with at least this many shared windows, so tiny sets cannot merge by chance
};

// Family id of every entry after merging declared families and every pair whose containment
// is at least mergePermille. Ids are the smallest declared family name of the component, so
// they do not depend on entry order.
struct FamilyAssignment {
	std::vector<std::string> family;     // per entry
	std::vector<FamilyMerge> byOverlap;  // overlap pairs that were not already in one declared family
};

// fingerprints[i] belongs to manifest.entries[i]. Pairs that are not comparable never merge.
[[nodiscard]] FamilyAssignment assignFamilies(
	const Manifest& manifest, std::span<const Fingerprint> fingerprints, const FamilyOptions& options);

// One fold of leave-one-family-out: `test` is one style family, `validation` the next style family
// in sorted order (cyclic), `train` all other style families. Entries are manifest indices.
struct Fold {
	std::string testFamily, validationFamily;
	std::vector<size_t> test, validation, train;
	uint32_t maxLeakPermille = 0; // largest containment between test and (validation + train) maps
};

enum class SplitError : uint8_t { TooFewFamilies, InvalidInput };

// Needs at least three style families. maxLeakPermille counts only pairs with at least minSharedWindows
// shared windows; one that reaches mergePermille is impossible by construction and is reported to make the check visible.
[[nodiscard]] std::expected<std::vector<Fold>, SplitError> makeFolds(
	const Manifest& manifest, const FamilyAssignment& families, std::span<const Fingerprint> fingerprints,
	const FamilyOptions& options);

} // namespace rme::procedural
