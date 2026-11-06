// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/adapter/terrain_preparation.h"

#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>

class Map;

namespace rme::procedural {

enum class PreparedValidationError {
	WrongThread, // Not on the GUI thread; nothing else was read.
	Cancelled, // Stop requested before or between checks.
	NoClient, // No client version is loaded.
	StaleAssets, // Client epoch/ID or covered current catalog state differs from the prepared result.
	StaleRequest, // Current config, profile or mask differs from (or is invalid for) the prepared request.
	StaleMap, // The map instance, revision, header or captured source tiles/locations changed.
	InvalidPrepared, // The prepared result is malformed, tampered, mixed or no longer matches its preview.
};

// Read-only check that a PreparedTerrain still corresponds to the current request, client and map.
// GUI thread only, synchronous, no yield and no allocation. Nothing is regenerated or resolved and
// no map, cache, brush or RNG state is touched. The explicit Map also covers inactive editors.
//
// currentMask is borrowed caller data: empty means the whole rectangle (all ones); otherwise it
// must hold exactly one 0/1 byte per region cell and equal the captured mask, including bits that
// protection excluded. When the request comes from a live selection, validateGenerationSelection()
// must be called separately.
//
// Cancellation is polled before and between stages and every 4096 mask cells. snapshotMatchesMap()
// and TerrainPreview::currentFingerprint() do not poll the token, so those synchronous scans run to
// completion once started; there is no responsiveness guarantee inside them.
//
// Limits: success describes the state at this call only and grants no retained commit authority;
// a future commit must revalidate immediately before mutating, without yielding. It does not prove
// the editor is still active or alive, does not cover raw image/GPU/cache edits or non-ground brush
// internals, does not observe GUI cancel and does not make a commit atomic. The catalog fingerprint
// checks current covered values even without an epoch change. Fingerprints are consistency checks,
// not protection against deliberate forgery.
std::expected<void, PreparedValidationError> validatePreparedTerrain(
	const Map& map, const PreparedTerrain& prepared, const GenerationConfig& currentConfig,
	const GroundProfile& currentProfile, std::span<const uint8_t> currentMask = {}, std::stop_token stop = {});

} // namespace rme::procedural
