// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/semantic_map.h"

#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace rme::procedural {

// Shift-invariant fingerprints of local map content (plan 4.3: detect copied areas) and the
// overlap between two maps (plan 4.4: keep related maps in one family). A window is a window x window
// block of cells at any offset; its value is a hash of the ground-key names (not indices, so maps
// with different key tables compare) and of tile presence. Position and floor are not hashed, so a
// copy moved to another place or floor is still found. Traits, items and decoration are not part of
// the value. Windows are sampled by content (hash % sampleModulo == 0), so the same copy yields the
// same sample on both sides regardless of offset. Integer math only.

struct FingerprintOptions {
	uint32_t window = 16;            // 2..256
	uint32_t sampleModulo = 4;       // keep windows whose mixed hash % sampleModulo == 0; 1 keeps all
	uint32_t minPresentPercent = 50; // windows with fewer existing tiles are skipped as sparse
	bool operator==(const FingerprintOptions&) const = default;
};

struct Fingerprint {
	FingerprintOptions options;
	uint64_t windows = 0;           // complete windows examined on all floors
	uint64_t sparseWindows = 0;     // skipped: fewer existing tiles than minPresentPercent
	uint64_t candidateWindows = 0;  // not sparse and selected by the content sample
	uint64_t uniformWindows = 0;    // candidates dropped because every cell has the same value
	std::vector<uint64_t> hashes;   // kept candidates, sorted and unique
};

enum class SimilarityError : uint8_t { InvalidOptions, Cancelled, AllocationFailed, BadFormat };

// Checks stop_requested once per row. Working memory about window * (floor width) * 8 bytes
// plus 4 bytes per column and 8 bytes per kept window.
[[nodiscard]] std::expected<Fingerprint, SimilarityError> computeFingerprint(
	const SemanticMap& map, const FingerprintOptions& options, std::stop_token stop);

// Distinct sampled windows. `a` and `b` are the distinct counts of the two fingerprints.
struct Containment {
	bool comparable = false; // same options and both sets non-empty
	uint64_t shared = 0, a = 0, b = 0;
	[[nodiscard]] uint32_t aInBPermille() const noexcept { return a ? static_cast<uint32_t>(shared * 1000 / a) : 0; }
	[[nodiscard]] uint32_t bInAPermille() const noexcept { return b ? static_cast<uint32_t>(shared * 1000 / b) : 0; }
	[[nodiscard]] uint32_t maxPermille() const noexcept { return aInBPermille() > bInAPermille() ? aInBPermille() : bInAPermille(); }
};

[[nodiscard]] Containment compareFingerprints(const Fingerprint& a, const Fingerprint& b);

// Windows that occur in many maps are generic patterns (a coast, a path), not evidence of copying.
// Returns copies of `all` without every window found in at least `genericAtLeast` of the fingerprints
// (0 or 1 removes nothing). Options of the inputs are kept; counters are kept except the hash list.
[[nodiscard]] std::vector<Fingerprint> withoutGenericWindows(std::span<const Fingerprint> all, uint32_t genericAtLeast);

// Text form, one hash per line, for private local files (hashes are not raw tiles, but keep them out of Git).
[[nodiscard]] std::string serializeFingerprint(const Fingerprint& fingerprint);
[[nodiscard]] std::expected<Fingerprint, SimilarityError> parseFingerprint(std::string_view text);

} // namespace rme::procedural
