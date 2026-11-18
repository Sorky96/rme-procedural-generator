// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/copy_guard.h"
#include "procedural/core/reference_header.h"
#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <new>
#include <sstream>

namespace rme::procedural {
namespace {
std::string checksum(std::string_view text) {
    Sha256 hash;
    hash.update(std::as_bytes(std::span(text.data(), text.size())));
    return Sha256::hex(hash.finish());
}
bool sha(std::string_view value) {
    return value.size() == 64 && std::ranges::all_of(value, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
bool valid(const Fingerprint& f) {
    return f.options == FingerprintOptions{} && std::ranges::is_sorted(f.hashes) &&
        std::adjacent_find(f.hashes.begin(), f.hashes.end()) == f.hashes.end() &&
        f.sparseWindows <= f.windows && f.candidateWindows <= f.windows - f.sparseWindows &&
        f.uniformWindows <= f.candidateWindows && f.hashes.size() <= f.candidateWindows - f.uniformWindows;
}
}
const char* copyStateName(CopyState state) noexcept {
    switch (state) {
        case CopyState::NotEvaluated: return "not-evaluated (no index)";
        case CopyState::TooSmall: return "not-evaluated (too few windows)";
        case CopyState::Clear: return "evaluated (below copy thresholds)";
        case CopyState::Copy: return "copy thresholds met; Place blocked";
    }
    return "not-evaluated";
}
std::expected<Fingerprint, SimilarityError> classFingerprint(uint32_t width, uint32_t height,
    std::span<const uint8_t> classes, std::stop_token stop) try {
    if (!width || !height || width > INT32_MAX || height > INT32_MAX || uint64_t(width) * height != classes.size())
        return std::unexpected(SimilarityError::BadFormat);
    const FloorBounds bounds{0, 0, 0, int(width), int(height)};
    auto map = SemanticMap::create(std::span(&bounds, 1), {classes.size()});
    if (!map) return std::unexpected(SimilarityError::AllocationFailed);
    std::array<uint16_t, naturalKindCount> keys{};
    for (size_t k = 0; k < keys.size(); ++k) {
        const auto key = map->internGroundKey(styleKindName(StyleKind(k)));
        if (!key) return std::unexpected(SimilarityError::AllocationFailed);
        keys[k] = *key;
    }
    for (uint32_t y = 0; y < height; ++y) {
        if (stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
        for (uint32_t x = 0; x < width; ++x) {
            const auto c = classes[size_t(y) * width + x];
            if (c == 255) continue;
            if (c >= keys.size()) return std::unexpected(SimilarityError::BadFormat);
            if (!map->set(int(x), int(y), 0, keys[c], 0)) return std::unexpected(SimilarityError::BadFormat);
        }
    }
    return computeFingerprint(*map, {}, stop);
} catch (const std::bad_alloc&) { return std::unexpected(SimilarityError::AllocationFailed); }

std::expected<CopyAssessment, SimilarityError> assessCopy(const Fingerprint& candidate,
    const CopyIndex& index, std::stop_token stop) try {
    if (stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
    if (!valid(candidate)) return std::unexpected(SimilarityError::BadFormat);
    CopyAssessment result;
    result.before = candidate.hashes.size();
    Fingerprint filtered = candidate;
    std::erase_if(filtered.hashes, [&](uint64_t h) { return std::ranges::binary_search(index.generic, h); });
    if (stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
    result.after = filtered.hashes.size();
    bool enough = true, copied = false;
    for (size_t r = 0; r < 2; ++r) {
        if (stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
        const auto& reference = index.references[r].fingerprint;
        if (!valid(reference)) return std::unexpected(SimilarityError::BadFormat);
        result.referenceBefore[r] = reference.hashes.size();
        if (index.generic.size() > reference.hashes.size()) return std::unexpected(SimilarityError::BadFormat);
        result.referenceAfter[r] = reference.hashes.size() - index.generic.size();
        enough &= result.referenceAfter[r] >= 16;
        const auto overlap = compareFingerprints(filtered, reference); // filtered candidate contains no generic hashes
        if (stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
        result.shared[r] = overlap.shared;
        result.permille[r] = overlap.aInBPermille();
        copied |= overlap.shared >= 16 && result.permille[r] >= 100;
    }
    if (!enough) return std::unexpected(SimilarityError::BadFormat);
    result.state = result.after < 16 ? CopyState::TooSmall : copied ? CopyState::Copy : CopyState::Clear;
    return result;
} catch (const std::bad_alloc&) { return std::unexpected(SimilarityError::AllocationFailed); }

std::expected<Fingerprint, SimilarityError> surfaceClassFingerprint(const SurfaceGrid& surface, std::stop_token stop) try {
    if (!surface.valid()) return std::unexpected(SimilarityError::BadFormat);
    std::vector<uint8_t> classes(surface.cells.size(), 255);
    for (size_t i = 0; i < classes.size(); ++i) {
        if ((i % surface.region.width) == 0 && stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
        const auto kind = styleKindOf(surface.cells[i]);
        if (size_t(kind) < naturalKindCount) classes[i] = uint8_t(kind);
    }
    return classFingerprint(surface.region.width, surface.region.height, classes, stop);
} catch (const std::bad_alloc&) { return std::unexpected(SimilarityError::AllocationFailed); }

std::expected<CopyAssessment, SimilarityError> assessCopy(const SurfaceGrid& surface,
    const CopyIndex& index, std::stop_token stop) {
    const auto fp = surfaceClassFingerprint(surface, stop);
    if (!fp) return std::unexpected(fp.error());
    return assessCopy(*fp, index, stop);
}

std::string serializeCopyIndex(const CopyIndex& index) {
    auto text = std::format("i53-index/1 {}\n{} {} {}\n", copyProjection, index.client, index.otbSha256, index.datSha256);
    for (const auto& r : index.references) {
        const auto fp = serializeFingerprint(r.fingerprint);
        text += std::format("{} {} {} {} {}\n", r.alias, r.sourceSha256, r.semanticSha256, r.keymapSha256, fp.size());
        text += fp;
    }
    return text + "sha256 " + checksum(text) + '\n';
}
std::expected<CopyIndex, SimilarityError> parseCopyIndex(std::string_view text) try {
    if (text.size() > (64ULL << 20)) return std::unexpected(SimilarityError::BadFormat);
    if (text.size() < 72 || text.substr(text.size() - 72) != "sha256 " + checksum(text.substr(0, text.size() - 72)) + '\n')
        return std::unexpected(SimilarityError::BadFormat);
    text.remove_suffix(72);
    CopyIndex index;
    auto line = [&]() {
        const auto end = text.find('\n');
        if (end == text.npos) return std::string{};
        std::string value(text.substr(0, end)); text.remove_prefix(end + 1); return value;
    };
    if (line() != std::format("i53-index/1 {}", copyProjection)) return std::unexpected(SimilarityError::BadFormat);
    std::istringstream header(line());
    std::string extra;
    if (!(header >> index.client >> index.otbSha256 >> index.datSha256) || header >> extra ||
        !sha(index.otbSha256) || !sha(index.datSha256)) return std::unexpected(SimilarityError::BadFormat);
    for (size_t i = 0; i < 2; ++i) {
        auto& r = index.references[i];
        uint64_t length = 0;
        std::istringstream row(line());
        if (!(row >> r.alias >> r.sourceSha256 >> r.semanticSha256 >> r.keymapSha256 >> length) || row >> extra ||
            r.alias != (i == 0 ? "M06" : "M19") || !sha(r.sourceSha256) || !sha(r.semanticSha256) ||
            !sha(r.keymapSha256) || length > text.size()) return std::unexpected(SimilarityError::BadFormat);
        const auto payload = text.substr(0, size_t(length));
        // Bound the legacy fingerprint parser's declared reserve BEFORE it allocates.
        // Canonical hashes occupy 17 bytes each; a forged kept count must not escape
        // the index's joint parsing allowance, even with a valid payload checksum.
        auto counts = payload.substr(std::min(payload.size(), payload.find('\n') + 1));
        counts = counts.substr(0, counts.find('\n'));
        const auto at = counts.find("kept=");
        uint64_t kept = 0;
        if (at == counts.npos) return std::unexpected(SimilarityError::BadFormat);
        const auto parsed = std::from_chars(counts.data() + at + 5, counts.data() + counts.size(), kept);
        if (parsed.ec != std::errc{} || kept > payload.size() / 17) return std::unexpected(SimilarityError::BadFormat);
        auto fp = parseFingerprint(payload);
        if (!fp || !valid(*fp) || serializeFingerprint(*fp) != payload) return std::unexpected(SimilarityError::BadFormat);
        r.fingerprint = std::move(*fp);
        text.remove_prefix(size_t(length));
    }
    if (!text.empty() || index.references[0].sourceSha256 == index.references[1].sourceSha256 ||
        index.references[0].keymapSha256 != index.references[1].keymapSha256)
        return std::unexpected(SimilarityError::BadFormat);
    const auto& a = index.references[0].fingerprint.hashes;
    const auto& b = index.references[1].fingerprint.hashes;
    std::ranges::set_intersection(a, b, std::back_inserter(index.generic));
    if (a.size() - index.generic.size() < 16 || b.size() - index.generic.size() < 16)
        return std::unexpected(SimilarityError::BadFormat);
    index.estimatedBytes = 65536 + 16 * (a.size() + b.size() + index.generic.size());
    return index;
} catch (const std::bad_alloc&) { return std::unexpected(SimilarityError::AllocationFailed); }

std::expected<CopyIndex, SimilarityError> readCopyIndex(const std::filesystem::path& path,
    std::string_view client, std::string_view otb, std::string_view dat, uint64_t maxBytes) try {
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec || bytes > (64ULL << 20) || bytes * 16 + 65536 > maxBytes) return std::unexpected(SimilarityError::BadFormat);
    std::ifstream file(path, std::ios::binary);
    std::string text(size_t(bytes), '\0');
    if (!file.read(text.data(), std::streamsize(bytes)) || file.peek() != EOF) return std::unexpected(SimilarityError::BadFormat);
    auto index = parseCopyIndex(text);
    if (!index || index->client != client || index->otbSha256 != otb || index->datSha256 != dat)
        return std::unexpected(SimilarityError::BadFormat);
    // Includes bounded file parsing peak, even though the text is released on return.
    index->estimatedBytes = std::max(index->estimatedBytes, bytes * 16 + 65536);
    return index;
} catch (const std::bad_alloc&) { return std::unexpected(SimilarityError::AllocationFailed); }
} // namespace rme::procedural
