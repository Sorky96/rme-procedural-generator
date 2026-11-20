// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/similarity.h"
#include "procedural/core/style_fit.h"
#include <array>
#include <filesystem>

namespace rme::procedural {
// Frozen I53 projection: only G4's five natural classes are present. Path, Floor,
// Bridge, Other, NoGround, Unmapped, Unknown and Preserve are absent.
inline constexpr std::string_view copyProjection = "g4-natural-v1";
// D10: window hashes are canonical over the eight symmetries of the square, so rotated and
// mirrored copies match. Indices of the earlier orientation-sensitive format are refused.
inline constexpr std::string_view copyIndexSchema = "i53-index/2";
inline constexpr std::string_view copyFingerprintKind = "d4-canonical-v1";
inline constexpr std::string_view copyIndexRebuildHint =
    "index format i53-index/1 predates rotation/mirror-invariant fingerprints; rebuild it with "
    "docs/procedural/smoke/g6_build_copy_index.py";
struct CopyReference {
    std::string alias, sourceSha256, semanticSha256, keymapSha256;
    Fingerprint fingerprint;
};
struct CopyIndex {
    std::string client, otbSha256, datSha256;
    std::array<CopyReference, 2> references; // exactly M06, M19, in this order
    std::vector<uint64_t> generic; // intersection of REFERENCES only
    uint64_t estimatedBytes = 0;
};
enum class CopyState : uint8_t { NotEvaluated, TooSmall, Clear, Copy };
struct CopyAssessment {
    CopyState state = CopyState::NotEvaluated;
    uint64_t before = 0, after = 0;
    std::array<uint64_t, 2> referenceBefore{}, referenceAfter{}, shared{};
    std::array<uint32_t, 2> permille{};
    bool operator==(const CopyAssessment&) const = default;
    [[nodiscard]] bool hardFail() const noexcept { return state == CopyState::Copy; }
};
[[nodiscard]] const char* copyStateName(CopyState state) noexcept;
// Common canonical projection for generated surface and offline G4 class rasters.
// Raster values 0..4 are StyleKind Grass..Water, 255 is absent. Other values fail.
// Window values are invariant under rotation by 90/180/270 degrees and mirroring.
[[nodiscard]] std::expected<Fingerprint, SimilarityError> classFingerprint(
    uint32_t width, uint32_t height, std::span<const uint8_t> classes, std::stop_token stop = {});
[[nodiscard]] std::expected<Fingerprint, SimilarityError> surfaceClassFingerprint(
    const SurfaceGrid& surface, std::stop_token stop = {});
[[nodiscard]] std::expected<CopyAssessment, SimilarityError> assessCopy(
    const Fingerprint& candidate, const CopyIndex& index, std::stop_token stop = {});
[[nodiscard]] std::expected<CopyAssessment, SimilarityError> assessCopy(
    const SurfaceGrid& surface, const CopyIndex& index, std::stop_token stop = {});
// Shared series budget must charge this estimate in addition to the retained index.
[[nodiscard]] constexpr uint64_t copyWorkingBytes(uint64_t cells) noexcept {
    return cells > (UINT64_MAX - 65536) / 64 ? UINT64_MAX : cells * 64 + 65536;
}
[[nodiscard]] std::string serializeCopyIndex(const CopyIndex& index);
[[nodiscard]] std::expected<CopyIndex, SimilarityError> parseCopyIndex(std::string_view text);
// Bounded local read, no discovery/download/fallback. A specified bad index is an error.
[[nodiscard]] std::expected<CopyIndex, SimilarityError> readCopyIndex(
    const std::filesystem::path& path, std::string_view client, std::string_view otb, std::string_view dat,
    uint64_t maxBytes = 256ULL << 20);
// True when the file starts with the superseded i53-index/1 header (readCopyIndex refuses it).
[[nodiscard]] bool copyIndexNeedsRebuild(const std::filesystem::path& path);
} // namespace rme::procedural
