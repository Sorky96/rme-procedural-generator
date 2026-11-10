// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/terrain_validation.h"

#include "app/managers/version_manager.h"

#include <string_view>
#include <wx/thread.h>

namespace rme::procedural {
namespace {

constexpr size_t cancelInterval = 4096;

bool sameConfig(const GenerationConfig& a, const GenerationConfig& b) noexcept { return a == b; }

std::unexpected<PreparedValidationError> fail(PreparedValidationError error) noexcept {
	return std::unexpected(error);
}

} // namespace

std::expected<void, PreparedValidationError> validatePreparedTerrain(
	const Map& map, const PreparedTerrain& prepared, const GenerationConfig& currentConfig,
	const GroundProfile& currentProfile, std::span<const uint8_t> currentMask, std::stop_token stop) {
	if (!wxIsMainThread()) return fail(PreparedValidationError::WrongThread);
	if (stop.stop_requested()) return fail(PreparedValidationError::Cancelled);
	if (!g_version.IsVersionLoaded() || !g_version.getLoadedVersion()) return fail(PreparedValidationError::NoClient);

	const auto& snapshot = prepared.snapshot;
	const auto& input = snapshot.input;
	const auto& plan = prepared.plan;

	// Loaded client and asset epoch (allocation-free view, no string copy).
	const uint64_t assetRevision = g_version.GetAssetRevision();
	if (input.assetRevision != assetRevision || plan.assetRevision != assetRevision ||
		std::string_view(plan.client) != g_version.GetCurrentVersionIDView()) {
		return fail(PreparedValidationError::StaleAssets);
	}
	// Compare current catalog values before any source access can follow cached
	// definition links. A missing owner or changed covered value invalidates it.
	const auto assets = prepared.preview.matchesAssets(stop);
	if (!assets) return fail(assets.error() == AssetFingerprintError::Cancelled ?
		PreparedValidationError::Cancelled : PreparedValidationError::StaleAssets);
	if (!*assets) return fail(PreparedValidationError::StaleAssets);

	// Request: explicit config fields and exact profile names against both captured copies.
	if (!sameConfig(currentConfig, input.config) || !sameConfig(currentConfig, plan.config) ||
		!(currentProfile == plan.profile)) {
		return fail(PreparedValidationError::StaleRequest);
	}

	// Captured shape: plan items, captured mask and protection all cover the same rectangle.
	const auto& region = input.config.region;
	const uint64_t cells = plan.items.size();
	if (uint64_t(region.width) * region.height != cells ||
		input.mask.size() != cells || input.protection.size() != cells) {
		return fail(PreparedValidationError::InvalidPrepared);
	}

	// Captured data must still pass the same geometry/mask validation preparation used.
	if (const auto captured = validateGeneration(input.config, input.extent, { cells, UINT64_MAX },
			input.mask, input.protection, stop); !captured) {
		return fail(captured.error() == GenerationError::Cancelled ?
			PreparedValidationError::Cancelled : PreparedValidationError::InvalidPrepared);
	}

	// The current request against the same extent and the captured (already valid) protection; the
	// config equals the captured one, so only the current mask can fail here. Empty = all ones.
	if (!currentMask.empty() && currentMask.size() != cells) return fail(PreparedValidationError::StaleRequest);
	if (const auto current = validateGeneration(currentConfig, input.extent, { cells, UINT64_MAX },
			currentMask, input.protection, stop); !current) {
		return fail(current.error() == GenerationError::Cancelled ?
			PreparedValidationError::Cancelled : PreparedValidationError::StaleRequest);
	}

	// The request mask must equal the captured mask, including protection-excluded bits.
	for (size_t i = 0; i < input.mask.size(); ++i) {
		if (i % cancelInterval == 0 && stop.stop_requested()) return fail(PreparedValidationError::Cancelled);
		const uint8_t requested = currentMask.empty() ? uint8_t(1) : currentMask[i];
		if (input.mask[i] != requested) return fail(PreparedValidationError::StaleRequest);
	}

	// Provenance: snapshot and plan are the ones this preview was built from.
	if (stop.stop_requested()) return fail(PreparedValidationError::Cancelled);
	if (!prepared.preview.matchesSource(snapshot, plan)) return fail(PreparedValidationError::InvalidPrepared);

	// Source map: instance, revision, header and every captured tile/location (does not poll stop).
	if (stop.stop_requested()) return fail(PreparedValidationError::Cancelled);
	if (!snapshotMatchesMap(map, snapshot)) return fail(PreparedValidationError::StaleMap);

	// Preview buffer still holds exactly the payload it was fingerprinted with (does not poll stop).
	if (stop.stop_requested()) return fail(PreparedValidationError::Cancelled);
	const auto fingerprint = prepared.preview.currentFingerprint();
	if (!fingerprint || *fingerprint != prepared.preview.getFingerprint()) {
		return fail(PreparedValidationError::InvalidPrepared);
	}

    if (plan.plateauFloor.empty()) {
        uint32_t levels = 0;
        for (auto* p = prepared.plateau.get(); p; p = p->plateau.get()) {
            if (++levels > currentConfig.surface.plateauLevels || levels > 3) {
                return fail(PreparedValidationError::InvalidPrepared);
            }
        }
    }
    if (prepared.plateau) {
        auto upperConfig = currentConfig;
        --upperConfig.region.z;
        if (upperConfig.region.z < 0) {
            return fail(PreparedValidationError::InvalidPrepared);
        }
        if (prepared.plateau->cave || prepared.plateau->plan.plateauFloor.empty()) {
            return fail(PreparedValidationError::InvalidPrepared);
        }
        const auto upper = validatePreparedTerrain(map, *prepared.plateau, upperConfig, currentProfile, currentMask, stop);
        if (!upper) {
            return upper;
        }
    } else if (!plan.plateauRamps.empty()) {
        return fail(PreparedValidationError::InvalidPrepared);
    }
    if (prepared.cave) {
		auto lowerConfig = currentConfig;
		++lowerConfig.region.z;
		if (prepared.cave->cave || prepared.cave->plan.caveFloor.empty()) return fail(PreparedValidationError::InvalidPrepared);
		const auto lower = validatePreparedTerrain(map, *prepared.cave, lowerConfig, currentProfile, currentMask, stop);
		if (!lower) return lower;
	} else if (!plan.entrances.empty()) return fail(PreparedValidationError::InvalidPrepared);
	if (stop.stop_requested()) return fail(PreparedValidationError::Cancelled);
	return {};
}

} // namespace rme::procedural
