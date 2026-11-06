// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "procedural/core/generation.h"
#include "procedural/adapter/generation_assets.h"
#include "map/basemap.h"
#include <optional>

class DrawOperations;

namespace rme::procedural {
struct GenerationSnapshot;
struct GroundPlan;
struct PreviewBudget { uint64_t maxChanges = 0; uint64_t maxBytes = 0; };
enum class PreviewError { WrongThread, NoClient, StaleSnapshot, InvalidPlan, InvalidItem, Limit, OutOfMask, Cancelled, AllocationFailed };

class TerrainPreview {
public:
	TerrainPreview(TerrainPreview&&) noexcept = default;
	TerrainPreview& operator=(TerrainPreview&&) noexcept = default;
	~TerrainPreview() = default;
	[[nodiscard]] const BaseMap& getBuffer() const noexcept { return *buffer_; }
	[[nodiscard]] std::span<const Position> getChanges() const noexcept { return changes_; }
	[[nodiscard]] uint64_t getFingerprint() const noexcept { return fingerprint_; }
	[[nodiscard]] uint64_t getEstimatedPeakBytes() const noexcept { return estimatedPeakBytes_; }
	// Rejects unexpected payloads instead of hashing only IDs and hiding them.
	[[nodiscard]] std::optional<uint64_t> currentFingerprint() const noexcept;
	// Binds this preview to the owned input/plan it was built from. Consistency
	// fingerprint only; source payload/revision still need snapshotMatchesMap.
	[[nodiscard]] bool matchesSource(const GenerationSnapshot&, const GroundPlan&) const noexcept;
	[[nodiscard]] std::expected<bool, AssetFingerprintError> matchesAssets(std::stop_token stop = {}) const noexcept;
private:
	friend class ::DrawOperations;
	TerrainPreview() = default;
	GenerationConfig config_;
	uint64_t mapGeneration_ = 0, mapRevision_ = 0, assetRevision_ = 0, planFingerprint_ = 0;
	uint64_t fingerprint_ = 0, estimatedPeakBytes_ = 0;
	uint64_t sourceFingerprint_ = 0;
	uint64_t assetsFingerprint_ = 0;
	std::unique_ptr<BaseMap> buffer_;
	std::vector<Position> changes_;
};
} // namespace rme::procedural
