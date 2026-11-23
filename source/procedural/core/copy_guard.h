// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/semantic_map.h"
#include "procedural/core/similarity.h"
#include "procedural/core/style_fit.h"
#include <filesystem>

namespace rme::procedural {
// Frozen I53 projection: only G4's five natural classes are present. Path, Floor,
// Bridge, Other, NoGround, Unmapped, Unknown and Preserve are absent.
inline constexpr std::string_view copyProjection = "g4-natural-v1";
// D10: window hashes are canonical over the eight symmetries of the square, so rotated and
// mirrored copies match. Indices of the earlier orientation-sensitive format are refused.
// I57: i53-index/3 holds 1..maxCopyReferences named references (written by the editor).
// i53-index/2 (exactly M06 and M19, from g6_build_copy_index.py) is still read.
inline constexpr std::string_view copyIndexSchema = "i53-index/3";
inline constexpr std::string_view copyIndexSchemaV2 = "i53-index/2";
inline constexpr size_t maxCopyReferences = 16;
inline constexpr uint64_t minCopyReferenceWindows = 16;
inline constexpr std::string_view copyFingerprintKind = "d4-canonical-v1";
inline constexpr std::string_view copyIndexRebuildHint =
    "index format i53-index/1 predates rotation/mirror-invariant fingerprints; rebuild it with "
    "docs/procedural/smoke/g6_build_copy_index.py";
struct CopyReference {
    std::string alias, sourceSha256, semanticSha256, keymapSha256;
    Fingerprint fingerprint;
};
struct CopyIndex {
    std::string schema{copyIndexSchema}; // as read; serialization always writes copyIndexSchema
    std::string client, otbSha256, datSha256;
    std::vector<CopyReference> references; // i53-index/2: exactly M06, M19, in this order
    std::vector<uint64_t> generic; // windows in every reference when there are two or more; empty for one
    uint64_t estimatedBytes = 0;
};
enum class CopyState : uint8_t { NotEvaluated, TooSmall, Clear, Copy };
struct CopyAssessment {
    CopyState state = CopyState::NotEvaluated;
    uint64_t before = 0, after = 0;
    std::vector<uint64_t> referenceBefore, referenceAfter, shared; // per reference, in index order
    std::vector<uint32_t> permille;
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
// Same rule as profile names: 1-64 ASCII letters, digits, '-' or '_', no device names.
[[nodiscard]] bool validCopyAlias(std::string_view alias);
enum class CopyEditError : uint8_t { BadAlias, DuplicateAlias, DuplicateSample, TooMany, TooSmall, WrongClient, AllocationFailed };
[[nodiscard]] const char* copyEditErrorText(CopyEditError error) noexcept;
// Appends one reference and recomputes `generic` and the estimate. An index read as i53-index/2
// becomes i53-index/3 (same references and assessments).
[[nodiscard]] std::expected<CopyIndex, CopyEditError> addCopyReference(CopyIndex index, CopyReference reference);
// One floor of a learning sample projected like the G4 class rasters: StyleKind Grass..Water of the
// cell's ground key through `mapping`, 255 for any other kind, an unmapped key or a missing tile.
[[nodiscard]] std::expected<std::vector<uint8_t>, SimilarityError> sampleClassRaster(
    const SemanticMap& sample, std::span<const KeyMapEntry> mapping);
// `<name>.i53` in `directory`. Writes under a folder lock to a sibling temporary file, then renames it.
// `expectedSha256`: SHA-256 of the file the edit started from, or empty when it must not exist yet;
// a file changed or created meanwhile is not overwritten.
[[nodiscard]] std::expected<void, std::string> saveCopyIndex(const std::filesystem::path& directory,
    std::string_view name, const CopyIndex& index, std::string_view expectedSha256);
[[nodiscard]] std::string fileSha256(const std::filesystem::path& path); // empty when unreadable
struct CopyIndexFile {
    std::filesystem::path path;
    std::string name;           // file stem
    std::string problem;        // empty when it can be used with the given client
    size_t references = 0;
};
// `.i53` files of `directory` (at most 64), sorted by name, each read and checked against the client.
[[nodiscard]] std::vector<CopyIndexFile> listCopyIndices(const std::filesystem::path& directory,
    std::string_view client, std::string_view otb, std::string_view dat);
// True when the file starts with the superseded i53-index/1 header (readCopyIndex refuses it).
[[nodiscard]] bool copyIndexNeedsRebuild(const std::filesystem::path& path);
} // namespace rme::procedural
